// EditPanelController.cpp
// 编辑面板控制器实现：桥接 IBR_EditFrame 命名空间（IBR_Misc.cpp:587-940）
#include "EditPanelController.h"
#include "Global.h"
#include "IBR_Project.h"
#include "IBR_Misc.h"
#include "IBB_Ini.h"
#include "IBB_PropStringPool.h"
#include "IBG_UndoTree.h"
#include "IBG_InputType.h"
#include "FromEngine/RFBump.h"
#include "FromEngine/global_tool_func.h"
#include <QString>
#include <QTimer>
#include <QDebug>
#include <unordered_map>

// IBR_EditFrame 命名空间的成员声明（定义在 IBR_Misc.cpp:587-594）
// EditBuf 大小固定为 100000（对应 IBR_Misc.cpp:592 的 char EditBuf[100000]）
static constexpr size_t EditBufSize = 100000;
namespace IBR_EditFrame
{
    extern IBR_Section CurSection;
    extern bool Empty;
    extern bool OnTextEdit;
    extern char EditBuf[];
    extern bool TextEditError;
    extern bool TextEditReset;
    extern std::unordered_map<StrPoolID, SidebarLine> EditLines;
    extern std::string NewLineKey;
    extern std::string NewLineValue;
    extern std::vector<ClipIICStatus> EditClipIICStatus;

    void SetActive(ModuleID_t id);
    void SwitchToText();
    void ExitTextEdit(bool Save);
    void ResetEdit(IBB_Section* rsc);
}

// EmptyOnShowDesc（定义在 Global.cpp:41）
extern const char* EmptyOnShowDesc;

// NeedtoMangle 函数声明（定义在 IBR_Misc.cpp:757-769）
namespace IBR_EditFrame { bool NeedtoMangle(IBB_Section* pbk); }

EditPanelController::EditPanelController(QObject *parent)
    : QObject(parent)
{
}

void EditPanelController::setActive(qulonglong sectionId)
{
    // 对应 IBR_EditFrame::SetActive（IBR_Misc.cpp:615-640）
    // 注意：SetActive 在 R 线程执行（通过 IBRF_CoreBump.SendToR）
    // 这里在 UI 线程调用，仅更新显示状态，后端 SetActive 由 WorkspaceController 投递
    ModuleID_t id = static_cast<ModuleID_t>(sectionId);

    // 修复：验证 sectionId 有效，避免访问无效内存导致崩溃
    auto it = IBR_Inst_Project.IBR_SectionMap.find(id);
    if (it == IBR_Inst_Project.IBR_SectionMap.end()) {
        // 无效 ID，清空状态
        if (!m_isEmpty) {
            m_currentSectionId = 0;
            emit currentSectionIdChanged();
            m_displayName.clear();
            emit displayNameChanged();
            m_isEmpty = true;
            emit isEmptyChanged();
            m_editLines.clear();
            emit editLinesChanged();
        }
        return;
    }

    if (m_currentSectionId == sectionId && !m_isEmpty) return;

    m_currentSectionId = sectionId;
    emit currentSectionIdChanged();

    // 读取模块显示名（对应 IBR_SectionData::DisplayName）
    m_displayName = QString::fromUtf8(it->second.DisplayName.c_str());
    emit displayNameChanged();

    m_isEmpty = false;
    emit isEmptyChanged();

    // 重置文本编辑模式
    if (m_onTextEdit) {
        m_onTextEdit = false;
        emit onTextEditChanged();
    }

    // 性能优化：异步重建键值列表，避免阻塞 UI 线程
    // 快速切换时去重：只保留一个待执行的 rebuild 任务，执行时读取最新的 m_currentSectionId
    // 安全性：rebuildEditLines 只读 IBR_SectionMap（结构稳定）和 pbk 的 SubSecs/Lines（R 线程在 SetActive 中不修改这些）
    if (!m_pendingRebuild) {
        m_pendingRebuild = true;
        QMetaObject::invokeMethod(this, [this]() {
            m_pendingRebuild = false;
            rebuildEditLines();
        }, Qt::QueuedConnection);
    }
}

void EditPanelController::clear()
{
    // 对应 IBR_EditFrame::Clear（IBR_Misc.cpp:942-946）
    // Empty=true → RenderUI 直接 return，侧边栏不显示编辑内容
    if (m_isEmpty) return;

    m_currentSectionId = 0;
    emit currentSectionIdChanged();
    m_displayName.clear();
    emit displayNameChanged();
    m_isEmpty = true;
    emit isEmptyChanged();
    m_editLines.clear();
    emit editLinesChanged();

    // 重置文本编辑模式
    if (m_onTextEdit) {
        m_onTextEdit = false;
        emit onTextEditChanged();
    }
}

void EditPanelController::refreshLines()
{
    rebuildEditLines();
}

void EditPanelController::rebuildEditLines()
{
    // 对应 IBR_EditFrame::ResetEdit + RenderUI_Lines
    // 性能优化：直接从 IBR_Inst_Project.IBR_SectionMap + pbk->SubSecs 读取
    // 不读 IBR_EditFrame::EditLines（避免与 R 线程 ResetEdit 竞争）
    // 不调 pbk->RecheckLineOrder()（UI 线程不修改后端数据，由 R 线程 SetActive 负责）
    if (m_isEmpty) {
        m_editLines.clear();
        emit editLinesChanged();
        return;
    }

    auto it = IBR_Inst_Project.IBR_SectionMap.find(static_cast<ModuleID_t>(m_currentSectionId));
    if (it == IBR_Inst_Project.IBR_SectionMap.end()) {
        m_editLines.clear();
        emit editLinesChanged();
        return;
    }
    IBB_Section* pbk = it->second.GetBack_Inl();
    if (!pbk) {
        m_editLines.clear();
        emit editLinesChanged();
        return;
    }

    // 更新 NeedtoMangle（对应 RenderUI_UseOwnName）
    bool newMangle = IBR_EditFrame::NeedtoMangle(pbk);
    if (newMangle != m_needtoMangle) {
        m_needtoMangle = newMangle;
        emit needtoMangleChanged();
    }

    m_editLines.clear();

    // 遍历 LineOrder（对应 RenderUI_Lines，IBR_Misc.cpp:874）
    // 安全性：调用时序保证 R 线程的 SetActive/RecheckLineOrder 尚未执行
    for (auto& K : pbk->LineOrder) {
        auto [pLine, pSub] = pbk->GetLineFromSubSecsEx2(K);

        QVariantMap entry;
        entry["keyName"] = QString::fromUtf8(PoolCStr(K));
        entry["keyId"] = static_cast<qulonglong>(K);
        entry["onShow"] = pbk->IsOnShow(K);
        std::string onShowDesc = pbk->GetOnShow(K);
        if (onShowDesc == EmptyOnShowDesc) onShowDesc.clear();
        entry["onShowDesc"] = QString::fromUtf8(onShowDesc.c_str());

        // 缺失行数据（对应 ImGui TextColored IllegalLineColor，IBR_Misc.cpp:903）
        if (!pLine || !pLine->Default) {
            entry["missing"] = true;
            entry["value"] = QString();
            entry["hint"] = QString();
            m_editLines.append(entry);
            continue;
        }

        entry["missing"] = false;
        // 直接从 Default->DescLong 读取 Hint（对应 ResetEdit 里 Line.Hint = V.Default->DescLong）
        entry["hint"] = QString::fromUtf8(PoolDesc(pLine->Default->DescLong));
        entry["isMultiple"] = pLine->IsMultiple();
        entry["lineCount"] = static_cast<int>(pLine->Count());

        if (pLine->IsMultiple()) {
            QVariantList values;
            for (size_t i = 0; i < pLine->Count(); ++i) {
                values.append(QString::fromUtf8(pLine->FinalExportString(static_cast<int>(i)).c_str()));
            }
            entry["values"] = values;
            entry["value"] = values.isEmpty() ? QString() : values.first().toString();
        } else {
            QString val = QString::fromUtf8(pLine->FinalExportString(0).c_str());
            entry["value"] = val;
            entry["values"] = QVariantList{val};
        }

        m_editLines.append(entry);
    }

    emit editLinesChanged();
}

void EditPanelController::addLine(const QString &key, const QString &value)
{
    // 对应 RenderUI_NewLine 的 "＋" 按钮逻辑（IBR_Misc.cpp:701-727）
    std::string keyStr = key.toUtf8().toStdString();
    std::string valStr = value.toUtf8().toStdString();
    auto pbk = IBR_EditFrame::CurSection.GetBack();
    if (!pbk) return;

    IBRF_CoreBump.SendToR({ [keyStr, valStr, pbk]() {
        IBG_Undo.SomethingShouldBeHere();
        StrPoolID NewKeyID = NewPoolStr(keyStr);

        std::string finalVal = valStr;
        // Value 为空时，从默认类型列表获取初始值（对应 IBR_Misc.cpp:717-724）
        if (finalVal.empty())
        {
            auto pLine = IBF_Inst_DefaultTypeList.List.KeyBelongToLine(NewKeyID, pbk->Register);
            if (pLine)
            {
                auto& Input = pLine->GetInputType();
                finalVal = Input.Form->GetFormattedString();
            }
        }

        // 默认在画布上显示
        pbk->OnShow[NewKeyID] = EmptyOnShowDesc;
        // 添加或替换
        pbk->MergeLine(NewKeyID, Index_AlwaysNew, finalVal, IBB_IniMergeMode::Replace);
        // 调整到最前面，方便用户看到
        pbk->OrderKey(NewKeyID, 0);
        IBF_Inst_Project.UpdateAll();
        IBR_EditFrame::ResetEdit(pbk);
    } });

    // 刷新显示（延迟到 R 线程完成后）
    QTimer::singleShot(50, this, [this]() {
        rebuildEditLines();
        emit sectionDataChanged(m_currentSectionId);
    });
}

QString EditPanelController::getInitialValue(const QString &key) const
{
    // 对应 RenderUI_NewLine 的 TextDisabled 提示（IBR_Misc.cpp:737-748）
    // 当 Value 为空时，从默认类型列表查询 Key 的初始值，用于显示提示
    if (m_isEmpty) return {};

    auto it = IBR_Inst_Project.IBR_SectionMap.find(static_cast<ModuleID_t>(m_currentSectionId));
    if (it == IBR_Inst_Project.IBR_SectionMap.end()) return {};

    IBB_Section* pbk = it->second.GetBack_Inl();
    if (!pbk) return {};

    StrPoolID KeyID = NewPoolStr(key.toUtf8().toStdString());
    auto pLine = IBF_Inst_DefaultTypeList.List.KeyBelongToLine(KeyID, pbk->Register);
    if (!pLine) return {};

    auto& Input = pLine->GetInputType();
    const std::string& Str = Input.Form->GetFormattedString();
    if (Str.empty()) return {};

    return QString::fromUtf8(Str.c_str());
}

void EditPanelController::removeLine(const QString &key)
{
    // 对应 RenderUI_OnShow 的 "移除行" 按钮（IBR_Misc.cpp:858-864）
    std::string keyStr = key.toUtf8().toStdString();
    StrPoolID K = NewPoolStr(keyStr);
    auto pbk = IBR_EditFrame::CurSection.GetBack();
    if (!pbk) return;

    IBRF_CoreBump.SendToR({ [pbk, K]() {
        IBG_Undo.SomethingShouldBeHere();
        pbk->RemoveLine(K);
        IBR_EditFrame::EditLines.erase(K);
        IBF_Inst_Project.UpdateAll();
    } });

    QTimer::singleShot(50, this, [this]() {
        rebuildEditLines();
        emit sectionDataChanged(m_currentSectionId);
    });
}

void EditPanelController::toggleOnShow(const QString &key)
{
    // 对应 RenderUI_OnShow 的 RadioButton（IBR_Misc.cpp:827-832）
    // 按用户要求：按钮和是否显示键都用 OnShow 变量判断，按下按钮时先把变量取否，然后刷新渲染
    // 同步修改 OnShow 状态（对应 ImGui RenderUI_OnShow 直接操作 pbk->OnShow[K]）
    std::string keyStr = key.toUtf8().toStdString();
    StrPoolID K = NewPoolStr(keyStr);
    auto pbk = IBR_EditFrame::CurSection.GetBack();
    if (!pbk) return;
#ifdef INIWEAVER_DIAG
    qDebug() << "[ONSHOW-DIAG] toggleOnShow sectionId=" << m_currentSectionId
             << "key=" << key << "beforeShow=" << pbk->IsOnShow(K);
#endif

    // 先把变量取否
    IBG_Undo.SomethingShouldBeHere();
    if (pbk->OnShow[K].empty()) pbk->OnShow[K] = EmptyOnShowDesc;
    else pbk->OnShow[K].clear();

    // 然后刷新渲染：rebuildEditLines 更新侧边栏 modelData.onShow，
    // sectionDataChanged 触发 WorkspaceController::refreshSectionLines 刷新画布
    rebuildEditLines();
    emit sectionDataChanged(m_currentSectionId);
}

void EditPanelController::setOnShowDesc(const QString &key, const QString &desc)
{
    // 对应 RenderUI_OnShow 的描述编辑框（IBR_Misc.cpp:849-856）
    std::string keyStr = key.toUtf8().toStdString();
    std::string descStr = desc.toUtf8().toStdString();
    StrPoolID K = NewPoolStr(keyStr);
    auto pbk = IBR_EditFrame::CurSection.GetBack();
    if (!pbk) return;

    IBRF_CoreBump.SendToR({ [pbk, K, descStr]() {
        IBG_Undo.SomethingShouldBeHere();
        bool show = pbk->IsOnShow(K);
        if (show && descStr.empty()) pbk->OnShow[K] = EmptyOnShowDesc;
        else pbk->OnShow[K] = descStr;
    } });

    QTimer::singleShot(30, this, [this]() {
        rebuildEditLines();
        emit sectionDataChanged(m_currentSectionId);
    });
}

void EditPanelController::setLineValue(const QString &key, const QString &value)
{
    // 对应 SidebarLine::RenderUI → IBB_IniLine::Merge 的值编辑
    std::string keyStr = key.toUtf8().toStdString();
    std::string valStr = value.toUtf8().toStdString();
    StrPoolID K = NewPoolStr(keyStr);
    auto pbk = IBR_EditFrame::CurSection.GetBack();
    if (!pbk) return;

    IBRF_CoreBump.SendToR({ [pbk, K, valStr]() {
        IBG_Undo.SomethingShouldBeHere();
        pbk->MergeLine(K, 0, valStr, IBB_IniMergeMode::Replace);
        IBF_Inst_Project.UpdateAll();
    } });

    QTimer::singleShot(30, this, [this]() {
        rebuildEditLines();
        emit sectionDataChanged(m_currentSectionId);
    });
}

void EditPanelController::switchToText()
{
    // 对应 IBR_EditFrame::SwitchToText（IBR_Misc.cpp:649-661）
    IBRF_CoreBump.SendToR({ []() {
        IBR_EditFrame::SwitchToText();
    } });

    QTimer::singleShot(50, this, [this]() {
        m_onTextEdit = true;
        emit onTextEditChanged();
        m_textEditContent = QString::fromUtf8(IBR_EditFrame::EditBuf);
        emit textEditContentChanged();
    });
}

void EditPanelController::exitTextEdit(bool save)
{
    // 对应 IBR_EditFrame::ExitTextEdit（IBR_Misc.cpp:663-685）
    // 先把 QML 编辑框内容写回 EditBuf
    if (save) {
        std::string content = m_textEditContent.toUtf8().toStdString();
        IBRF_CoreBump.SendToR({ [content]() {
            strncpy(IBR_EditFrame::EditBuf, content.c_str(), EditBufSize - 1);
            IBR_EditFrame::EditBuf[EditBufSize - 1] = '\0';
            IBR_EditFrame::ExitTextEdit(true);
            IBF_Inst_Project.UpdateAll();
        } });
    } else {
        IBRF_CoreBump.SendToR({ []() {
            IBR_EditFrame::ExitTextEdit(false);
        } });
    }

    QTimer::singleShot(50, this, [this]() {
        m_onTextEdit = false;
        emit onTextEditChanged();
        rebuildEditLines();
        emit sectionDataChanged(m_currentSectionId);
    });
}

void EditPanelController::toggleUseOwnName()
{
    // 对应 RenderUI_UseOwnName（IBR_Misc.cpp:771-793）
    auto pbk = IBR_EditFrame::CurSection.GetBack();
    if (!pbk) return;

    bool N = IBR_EditFrame::NeedtoMangle(pbk);
    bool P = !N;  // 切换

    IBRF_CoreBump.SendToR({ [pbk, P]() {
        if (P) {
            // Set 1：刷新注册名
            pbk->VarList.Value["_OldInitSecName"] = pbk->VarList.Value["_InitialSecName"];
            pbk->VarList.Value.erase("_InitialSecName");
        } else {
            // Set 0：不刷新
            auto& Str = pbk->VarList.Value["_OldInitSecName"];
            if (Str.empty()) Str = GenerateModuleTag() + RandStr(4);
            pbk->VarList.Value["_InitialSecName"] = GenerateModuleTag() + RandStr(4);
        }
    } });

    QTimer::singleShot(30, this, [this]() {
        rebuildEditLines();
        emit sectionDataChanged(m_currentSectionId);
    });
}
