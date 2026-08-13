// DialogController.cpp
#include "DialogController.h"
#include <QFile>
#include <QTextStream>
#include <QRandomGenerator>
#include "IBB_RegType.h"
#include "IBR_Project.h"
#include "IBB_Components.h"
#include "Global.h"
#include <algorithm>
#include <unordered_set>

DialogController::DialogController(QObject *parent)
    : QObject(parent)
{
    // 阶段 10.3：启动时加载 hint.txt（对应 IBR_Components.cpp:818-845 IBR_HintManager::Load）
    loadHintPool();
}

void DialogController::showConfirm(const QString &title, const QString &message, const QString &actionId)
{
    emit confirmRequested(title, message, actionId);
}

void DialogController::showConfirm3(const QString &title, const QString &message, const QString &actionId)
{
    // 阶段 11.3：三态确认（保存/不保存/取消）
    emit confirm3Requested(title, message, actionId);
}

void DialogController::showExportDialog()
{
    emit exportRequested();
}

void DialogController::showImportDialog(const QString &path)
{
    emit importRequested(path);
}

void DialogController::showWaiting(const QString &message)
{
    emit waitingShown(message);
}

void DialogController::hideWaiting()
{
    emit waitingHidden();
}

void DialogController::setHint(const QString &text)
{
    emit hintSet(text);
}

void DialogController::setHintWithType(const QString &text, int type, int durationMs)
{
    emit hintSetWithType(text, type, durationMs);
}

void DialogController::onPopupHooked(const QString &title, const QStringList &texts, bool modal, bool canClose, const QSizeF &size)
{
    // 阶段 11.5：弹窗被请求时设置 hasPopup=true
    setHasPopup(true);
    // v3 批次 2.2：转发 size（{0,0} 表示自动尺寸，QML 用默认值兜底）
    emit popupRequested(title, texts, modal, canClose, size);
}

void DialogController::onRightClickMenuHooked(const QString &title, const QStringList &items, int posX, int posY)
{
    // 阶段 11.4：右键菜单被请求时设置 hasRightClickMenu=true
    setHasRightClickMenu(true);
    emit rightClickMenuRequested(title, items, posX, posY);
}

void DialogController::onPopupCleared()
{
    // 阶段 11.5：ClearCurrentPopup 被调用时同步状态
    setHasPopup(false);
}

void DialogController::onRightClickMenuCleared()
{
    // 阶段 11.5：ClearRightClickMenu 被调用时同步状态
    setHasRightClickMenu(false);
}

void DialogController::notifyConfirmResult(const QString &actionId, bool accepted)
{
    emit confirmCompleted(actionId, accepted);
}

void DialogController::notifyConfirmResult3(const QString &actionId, int result)
{
    // 阶段 11.3：result: 0=取消, 1=保存, 2=不保存
    emit confirm3Completed(actionId, result);
}

void DialogController::notifyRightClickMenuResult(int index)
{
    // 阶段 11.4：index=-1 表示用户取消
    setHasRightClickMenu(false);
    emit rightClickMenuCompleted(index);
}

// ===== 阶段 11.2 新增：错误队列实现（对应 IBR_ErrorCollector ErrorList） =====

void DialogController::onErrorPushed(const QString &title, const QString &errorStr,
                                     const QString &info, bool forJson)
{
    // 对应 IBR_ErrorCollector.cpp:46 ErrorList.push_back(...)
    m_errorQueue.push_back({ title, errorStr, info, forJson });
    emit errorQueueSizeChanged();
    // 第一条错误入队时通知 QML 显示
    if (m_errorQueue.size() == 1) {
        emit currentErrorChanged();
    }
}

QVariantMap DialogController::currentError() const
{
    QVariantMap m;
    if (m_errorQueue.empty()) {
        m["valid"] = false;
        return m;
    }
    const auto &e = m_errorQueue.front();
    m["valid"] = true;
    m["title"] = e.title;
    m["errorStr"] = e.errorStr;
    m["info"] = e.info;
    m["forJson"] = e.forJson;
    // 阶段 11.2/B5：JsonParse ±4 行上下文（对应 IBR_ErrorCollector.cpp:71-92）
    // 在 C++ 侧预处理上下文行，QML 侧只需渲染字符串列表
    if (e.forJson) {
        QStringList contextLines;
        const QStringList lines = e.errorStr.split('\n', Qt::SkipEmptyParts);
        int tgIdx = -1;
        for (int i = 0; i < lines.size(); ++i) {
            // 对应 loc("Error_JsonParseErrorPos") 标记
            if (lines[i].contains(QStringLiteral("Error_JsonParseErrorPos")) ||
                lines[i].contains(QStringLiteral("at line"))) {
                tgIdx = i + 1;  // 1-based
                break;
            }
        }
        if (tgIdx > 0) {
            // 输出 [TgIdx-4, TgIdx+4] 范围
            int start = std::max(0, tgIdx - 1 - 4);
            int end = std::min<int>(static_cast<int>(lines.size()), tgIdx + 4);
            for (int i = start; i < end; ++i) {
                contextLines.append(lines[i]);
            }
        }
        m["contextLines"] = contextLines;
        m["errorLine"] = tgIdx;
    }
    return m;
}

void DialogController::advanceErrorQueue()
{
    // 对应 IBR_ErrorCollector.cpp:58 ErrorListShown++（用 pop_front 替代游标）
    if (!m_errorQueue.empty()) {
        m_errorQueue.erase(m_errorQueue.begin());
        emit errorQueueSizeChanged();
        emit currentErrorChanged();
    }
}

void DialogController::clearErrorQueue()
{
    // 对应 IBR_ErrorCollector.cpp:124-129 ClearDelayed
    if (!m_errorQueue.empty()) {
        m_errorQueue.clear();
        emit errorQueueSizeChanged();
        emit currentErrorChanged();
    }
}

// ===== 阶段 11.5 私有辅助 =====

void DialogController::setHasPopup(bool v)
{
    if (m_hasPopup == v) return;
    m_hasPopup = v;
    emit hasPopupChanged();
}

void DialogController::setHasRightClickMenu(bool v)
{
    if (m_hasRightClickMenu == v) return;
    m_hasRightClickMenu = v;
    emit hasRightClickMenuChanged();
}

// ===== 阶段 10.3 新增：Hint 队列轮播实现（对应 IBR_Components.cpp:801-908 IBR_HintManager） =====

void DialogController::loadHintPool()
{
    // 对应 IBR_Components.cpp:818-845 Load()
    // 读取 .\Resources\hint.txt 逐行存入 m_hintPool，删除首行
    QFile file(QStringLiteral(".\\Resources\\hint.txt"));
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        // 文件不存在时静默失败（对应 ImGui 侧 Load 失败仅写日志）
        return;
    }
    QTextStream in(&file);
    in.setEncoding(QStringConverter::Utf8);
    while (!in.atEnd())
    {
        QString line = in.readLine();
        if (!line.isEmpty())
            m_hintPool.append(line);
    }
    file.close();
    // 对应 ImGui Hint.erase(Hint.begin())：删除首行
    if (!m_hintPool.isEmpty())
        m_hintPool.removeFirst();
}

QString DialogController::firstHint() const
{
    // 对应 ImGui UseHint 初始值=0，首帧 HintTimer.GetMilli()=0 < 5000 不切换，
    // 直接渲染 Hint[0]（Load 已 erase 首行，故 m_hintPool[0] 是 hint.txt 第二行）
    if (m_hintPool.isEmpty())
        return QString();
    return m_hintPool.first();
}

QString DialogController::nextRandomHint() const
{
    // 对应 IBR_Components.cpp:860 UseHint = rand() % Hint.size()
    if (m_hintPool.isEmpty())
        return QString();
    int idx = QRandomGenerator::global()->bounded(m_hintPool.size());
    return m_hintPool.at(idx);
}

void DialogController::setHintCustom(const QString &text)
{
    // 对应 IBR_HintManager::SetHintCustom
    if (m_customHint == text)
        return;
    m_customHint = text;
    emit customHintChanged();
}

void DialogController::clearHintCustom()
{
    // 对应 IBR_HintManager::Clear
    if (m_customHint.isEmpty())
        return;
    m_customHint.clear();
    emit customHintChanged();
}

// ===== 阶段 11.1 新增：导入预览状态管理实现（对应 IBR_ImportPreview::Open） =====

void DialogController::onImportPreviewRequested(ImportedIniFile &&file,
                                                std::function<void(const IBR_ImportResult &)> callback)
{
    // 对应 IBR_ImportIni.cpp:36-52 Open()：存储文件 + callback，初始化辅助数据
    m_importFile = std::move(file);
    m_importCallback = std::move(callback);

    // 对应 IBR_ImportIni.cpp:44 g_IniTypeOptions = IBB_DefaultRegType::GetIniTypeList()
    m_iniTypeOptions.clear();
    for (const auto &s : IBB_DefaultRegType::GetIniTypeList())
        m_iniTypeOptions.append(QString::fromUtf8(s));

    // 对应 IBR_ImportIni.cpp:47-51 g_AvailableRegTypes：收集并排序所有注册表类型名
    m_availableRegTypes.clear();
    std::vector<std::string> regTypes;
    for (auto &[Name, Reg] : IBB_DefaultRegType::RegisterTypes)
        regTypes.push_back(Name);
    std::sort(regTypes.begin(), regTypes.end());
    for (const auto &s : regTypes)
        m_availableRegTypes.append(QString::fromUtf8(s));

    // 对应 IBR_ImportIni.cpp:136-157 冲突检测：列出当前项目已存在的同名 section
    m_conflictSections.clear();
    for (const auto &sec : m_importFile.Sections)
    {
        if (sec.IsRegistryList) continue;
        IBB_Section_Desc desc{ DefaultIniName, sec.SectionName };
        if (IBR_Inst_Project.HasSection(desc))
            m_conflictSections.append(QString::fromUtf8(sec.SectionName));
    }

    // 阶段 11.5：标记有弹窗
    setHasPopup(true);

    // 通知 QML 显示导入预览弹窗
    emit importPreviewRequested();
}

QVariantMap DialogController::importPreviewData() const
{
    // 对应 IBR_ImportIni.cpp:68-456 RenderUI 中读取的 g_File 数据
    QVariantMap data;
    if (!m_importCallback)
    {
        data["valid"] = false;
        return data;
    }
    data["valid"] = true;

    // sections 列表
    QVariantList sections;
    for (const auto &sec : m_importFile.Sections)
    {
        QVariantMap s;
        s["index"] = static_cast<qlonglong>(sec.Index);
        s["sectionName"] = QString::fromUtf8(sec.SectionName);
        s["matchStatus"] = static_cast<int>(sec.MatchStatus);  // 0=Matched, 1=LinkMatched, 2=Unmatched
        s["matchedRegType"] = QString::fromUtf8(sec.MatchedRegType);
        s["isRegistryList"] = sec.IsRegistryList;
        s["selected"] = sec.Selected;  // 默认 true
        s["linkMatchSource"] = QString::fromUtf8(sec.LinkMatchSource);

        // keyValues 列表（对应 IBR_ImportIni.cpp:214-225 KV 展开视图）
        QVariantList kvs;
        for (const auto &kv : sec.KeyValues)
        {
            if (kv.Value.empty()) continue;  // 对应 ImGui 版跳过空值
            QVariantMap kvMap;
            kvMap["key"] = QString::fromUtf8(kv.Key);
            kvMap["value"] = QString::fromUtf8(kv.Value);
            kvs.append(kvMap);
        }
        s["keyValues"] = kvs;
        s["keyCount"] = static_cast<int>(kvs.size());
        sections.append(s);
    }
    data["sections"] = sections;
    data["iniTypeOptions"] = m_iniTypeOptions;
    data["availableRegTypes"] = m_availableRegTypes;
    data["defaultIniType"] = QString::fromUtf8(DefaultIniName);
    data["conflictSections"] = m_conflictSections;

    return data;
}

void DialogController::confirmImportPreview(const QString &iniType,
                                            const QVariantList &selectedIndices,
                                            const QVariantMap &unmatchedRegTypes)
{
    if (!m_importCallback)
        return;

    // 对应 IBR_ImportIni.cpp:410-426：应用用户选择
    // 1. 设置每个 section 的 Selected 状态
    // 将 selectedIndices 转为 set 加速查找
    std::unordered_set<qlonglong> selectedSet;
    for (const auto &idx : selectedIndices)
        selectedSet.insert(idx.toLongLong());

    for (auto &sec : m_importFile.Sections)
    {
        if (sec.IsRegistryList) continue;
        sec.Selected = (selectedSet.find(static_cast<qlonglong>(sec.Index)) != selectedSet.end());
    }

    // 2. 对未匹配 section 应用用户选择的 RegType（对应 IBR_ImportIni.cpp:411-426）
    for (auto &sec : m_importFile.Sections)
    {
        if (sec.MatchStatus != IniImportMatchStatus::Unmatched) continue;
        QString key = QString::number(static_cast<qlonglong>(sec.Index));
        if (unmatchedRegTypes.contains(key))
        {
            std::string regType = unmatchedRegTypes.value(key).toString().toStdString();
            if (!regType.empty())
                SetSectionRegType(sec, regType);
            else
                SetSectionRegType(sec, AnyTypeName);
        }
        else
        {
            // 用户未选择则设为 _AnyType（对应 IBR_ImportIni.cpp:423）
            SetSectionRegType(sec, AnyTypeName);
        }
    }

    // 3. 构建结果并回调（对应 IBR_ImportIni.cpp:428-436）
    IBR_ImportResult result;
    result.Confirmed = true;
    result.File = std::move(m_importFile);
    result.INIType = iniType.toStdString();

    auto cb = std::move(m_importCallback);
    m_importCallback = nullptr;
    m_importFile = ImportedIniFile{};

    // 阶段 11.5：清除弹窗状态
    setHasPopup(false);

    cb(result);
}

void DialogController::cancelImportPreview()
{
    // 对应 IBR_ImportIni.cpp:444-456：用户取消
    if (!m_importCallback)
        return;

    IBR_ImportResult result;
    result.Confirmed = false;
    result.File = std::move(m_importFile);

    auto cb = std::move(m_importCallback);
    m_importCallback = nullptr;
    m_importFile = ImportedIniFile{};

    // 阶段 11.5：清除弹窗状态
    setHasPopup(false);

    cb(result);
}

// ===== 阶段 13.3.2：SHP 文件类型选择弹窗（对应 IBR_ProjManager.cpp:1129-1217） =====
void DialogController::requestShpTypeSelection(const QStringList &names)
{
    m_shpFileNames = names;
    // 阶段 11.5：标记弹窗存在
    setHasPopup(true);
    emit shpTypeSelectionRequested();
}

void DialogController::confirmShpTypeSelection(const QVariantList &types)
{
    if (!m_shpCallback)
    {
        setHasPopup(false);
        return;
    }

    // 将 QVariantList 转为 std::vector<int>
    std::vector<int> typeVec;
    typeVec.reserve(types.size());
    for (const auto &v : types)
        typeVec.push_back(v.toInt());

    auto cb = std::move(m_shpCallback);
    m_shpCallback = nullptr;
    m_shpFileNames.clear();

    // 阶段 11.5：清除弹窗状态
    setHasPopup(false);

    cb(typeVec);
}

void DialogController::cancelShpTypeSelection()
{
    // 用户取消：清空状态，不创建任何模块
    m_shpCallback = nullptr;
    m_shpFileNames.clear();

    // 阶段 11.5：清除弹窗状态
    setHasPopup(false);
}
