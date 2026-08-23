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
#include "IBG_InputType_Derived.h"
#include "IifComponentHelper.h"    // 画布/侧边栏共用 IIF 分量导出辅助（统一类型判定）
#include "IBB_IniLine.h"
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

// ========== 侧边栏 IIF 分量导出辅助（对齐 SectionLineModel::iifComponents + imgui 各控件字段） ==========
static bool IifStrTrue(const std::string& v)
{
    auto t = v;
    if (t.empty()) return false;
    char c = t[0];
    return !(c == '0' || c == 'n' || c == 'N' || c == 'f' || c == 'F');
}

// choice/combo/radio 的选项数组：[{key,label,desc}]（显示名取 ShowReg ? key : DisplayName）
static QVariantList ExportIifOpts(const std::unordered_map<std::string, IICDescStr>& options,
                                  const std::vector<std::string>& order, bool showReg)
{
    QVariantList out;
    for (auto& k : order) {
        auto it = options.find(k);
        if (it == options.end()) continue;
        QVariantMap o;
        o["key"] = QString::fromUtf8(k.c_str());
        o["label"] = QString::fromUtf8(showReg ? k.c_str() : it->second.Short.c_str());  // DisplayName=Short
        o["desc"]  = QString::fromUtf8(it->second.Long.c_str());                          // DescLong=Long
        out << o;
    }
    return out;
}

// 解析链接分量目标显示文本（对齐画布 collectLinks）：ShowReg ? destKey : destDisplayName，
// 用链接表（sub->GetLink/NewLinkTo）解析，而非 value 容器里可能残留的 "0"/占位值
static QString ExportIifLinkText(IBB_SubSec *sub, size_t lineIdx, size_t mult, size_t comp)
{
    if (!sub) return {};
    auto [begin, end] = sub->GetLink(lineIdx, mult, comp);
    if (begin == end) return {};
    const bool showReg = IBR_WorkSpace::ShowRegName;
    QStringList names;
    for (auto it = begin; it != end; ++it) {
        size_t linkIdx = it->second;
        if (linkIdx >= sub->NewLinkTo.size()) continue;
        const auto &link = sub->NewLinkTo[linkIdx];
        auto dstRsec = IBR_Inst_Project.GetSection(link.ToLoc.Sec);
        QString n = showReg ? QString::fromUtf8(PoolStr(link.ToLoc.Key))
                            : QString::fromUtf8(dstRsec.GetDisplayName());
        if (!n.isEmpty()) names << n;
    }
    return names.join(QStringLiteral(", "));
}

// 导出单个值(mult)的 IIF 分量列表（统一走 IifExportComponents，侧边栏 isWorkSpace=false）
static QVariantList ExportIifComponents(IBB_IniLine_Data_IIF *ii, IBB_SubSec *sub = nullptr,
                                        size_t lineIdx = 0, size_t mult = 0)
{
    // 先恢复链接分量目标名（对齐 ImGui RenderUI_Node 每帧从链接表读目标名），
    // 避免 GetFormattedString 的 UpdateValue 用错误 FormatValue 覆盖 link 分量（>100→100、文本→0）。
    if (ii && ii->Value && sub)
        IifSyncLinkTargets(*ii->Value, sub, lineIdx, mult);
    // 统一导出（按 ImGui 原版组件驱动模型，画布/侧边栏共用）：链接分量只读目标名。
    return IifExportComponents(ii, sub, lineIdx, mult, /*isWorkSpace=*/false);
}

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

// IIF 分量悬停 Hint：交互分量用 Hint.Long；纯文本分量无 Hint，回退用其自身文本
// 保证 IIF 每个分量在侧边栏都能列出一条提示（每分量一行，用 \n 分隔）
static std::string Edit_IIC_HintLong(const IBG_InputComponent *comp)
{
    if (auto t = dynamic_cast<const IIC_InputText*>(comp)) return t->Hint.Long;
    if (auto t = dynamic_cast<const IIC_InputInt*>(comp)) return t->Hint.Long;
    if (auto t = dynamic_cast<const IIC_Bool*>(comp)) return t->Hint.Long;
    if (auto t = dynamic_cast<const IIC_MultipleChoice*>(comp)) return t->Hint.Long;
    if (auto t = dynamic_cast<const IIC_EnumCombo*>(comp)) return t->Hint.Long;
    if (auto t = dynamic_cast<const IIC_EnumRadio*>(comp)) return t->Hint.Long;
    if (auto t = dynamic_cast<const IIC_ColorPanel*>(comp)) return t->Hint.Long;
    if (auto t = dynamic_cast<const IIC_SliderInt*>(comp)) return t->Hint.Long;
    // 纯文本类分量：无专属 Hint，回退为其渲染文本
    if (auto t = dynamic_cast<const IIC_PureText*>(comp)) return t->Text;
    if (auto t = dynamic_cast<const IIC_LocalizedText*>(comp)) return t->FallbackText;
    if (auto t = dynamic_cast<const IIC_Setter_String*>(comp)) return t->Value;
    return "";
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
        // IIF 键：键级 DescLong 为空时，改用分量 Hint.Long，让侧边栏悬停也能显示说明
        if (entry["hint"].toString().isEmpty()) {
            if (auto dataPtr = pLine->Indexed(0)) {
                if (auto ii = dataPtr->GetData<IBB_IniLine_Data_IIF>(); ii && ii->Value) {
                    QStringList hints;
                    for (auto &comp : *ii->Value->InputComponents) {
                        QString h = QString::fromUtf8(Edit_IIC_HintLong(comp.get()).c_str());
                        if (!h.isEmpty()) hints << h;
                    }
                    if (!hints.isEmpty()) entry["hint"] = hints.join(QStringLiteral("\n"));
                }
            }
        }
        entry["isMultiple"] = pLine->IsMultiple();
        entry["lineCount"] = static_cast<int>(pLine->Count());

        // keyType + IIF 分量导出（对齐画布 SectionLineModel）：
        // 0=String, 1=Bool, 2=IIF；IIF 键的 iifValues 按每个值(mult)导出分量列表，供侧边栏逐分量编辑。
        entry["keyType"] = 0;
        // 计算该键在 SubSec 里的 lineIdx（链接目标解析需要）
        size_t lineIdx = 0;
        if (pSub) for (size_t k = 0; k < pSub->Lines_ByName.size(); ++k)
            if (pSub->Lines_ByName[k] == K) { lineIdx = k; break; }
        if (auto d0 = pLine->Indexed(0)) {
            if (d0->GetData<IBB_IniLine_Data_IIF>()) {
                entry["keyType"] = 2;
                QVariantList iifValues;
                const size_t n = std::max<size_t>(static_cast<size_t>(pLine->Count()), 1u);
                for (size_t m = 0; m < n; ++m) {
                    QVariantMap vrec;
                    vrec["mult"] = static_cast<int>(m);
                    auto dm = pLine->Indexed(m);
                    vrec["comps"] = (dm && dm->GetData<IBB_IniLine_Data_IIF>())
                        ? ExportIifComponents(dm->GetData<IBB_IniLine_Data_IIF>(), pSub, lineIdx, m) : QVariantList();
                    iifValues << vrec;
                }
                entry["iifValues"] = iifValues;
            } else if (d0->GetData<IBB_IniLine_Data_Bool>()) {
                entry["keyType"] = 1;
            }
        }

        if (pLine->IsMultiple()) {
            QVariantList values;
            for (size_t i = 0; i < pLine->Count(); ++i) {
                // 显示用 GetString（无副作用）：FinalExportString 是导出专用，
                // 内部 OnExport Regen + SetValue + UpdateAll/LimitFix 会污染链接分量目标名
                auto dm = pLine->Indexed(i);
                values.append(QString::fromUtf8(dm ? dm->GetString().c_str() : ""));
            }
            entry["values"] = values;
            entry["value"] = values.isEmpty() ? QString() : values.first().toString();
        } else {
            // 非多值键：只填单值，不设 values（空 → 侧边栏多值 Repeater model 恒 0，
            // 从根上杜绝该 Repeater 对非 Multiple 键产生额外输入框）
            auto dm0 = pLine->Indexed(0);
            QString val = QString::fromUtf8(dm0 ? dm0->GetString().c_str() : "");
            entry["value"] = val;
            entry["values"] = QVariantList();
        }

        m_editLines.append(entry);
    }

    emit editLinesChanged();
}

void EditPanelController::addLine(const QString &key, const QString &value)
{
    // 对应 RenderUI_NewLine 的 "＋" 按钮逻辑（IBR_Misc.cpp:701-727）
    // 用 IBR_SectionMap[m_currentSectionId] 取 pbk（与 rebuildEditLines 一致，避免 CurSection 错位）
    std::string keyStr = key.toUtf8().toStdString();
    std::string valStr = value.toUtf8().toStdString();
    if (m_isEmpty) return;
    auto it = IBR_Inst_Project.IBR_SectionMap.find(static_cast<ModuleID_t>(m_currentSectionId));
    if (it == IBR_Inst_Project.IBR_SectionMap.end()) return;
    IBB_Section* pbk = it->second.GetBack_Inl();
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
    // 用 IBR_SectionMap[m_currentSectionId] 取 pbk（与 rebuildEditLines 一致，避免 CurSection 错位）
    std::string keyStr = key.toUtf8().toStdString();
    StrPoolID K = NewPoolStr(keyStr);
    if (m_isEmpty) return;
    auto it = IBR_Inst_Project.IBR_SectionMap.find(static_cast<ModuleID_t>(m_currentSectionId));
    if (it == IBR_Inst_Project.IBR_SectionMap.end()) return;
    IBB_Section* pbk = it->second.GetBack_Inl();
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
    // 修复：必须用 IBR_SectionMap[m_currentSectionId].GetBack_Inl() 取 pbk（与 rebuildEditLines 一致），
    // 不能用 IBR_EditFrame::CurSection.GetBack()。后者在关闭项目后残留旧指针/错位，
    // 导致 toggleOnShow 改的是 CurSection 指向的模块，rebuildEditLines 读的是 m_currentSectionId
    // 对应的模块，两者不一致 → CheckBox 点了没反应（改了 A，UI 读 B 弹回旧状态）。
    std::string keyStr = key.toUtf8().toStdString();
    StrPoolID K = NewPoolStr(keyStr);
    if (m_isEmpty) return;
    auto it = IBR_Inst_Project.IBR_SectionMap.find(static_cast<ModuleID_t>(m_currentSectionId));
    if (it == IBR_Inst_Project.IBR_SectionMap.end()) return;
    IBB_Section* pbk = it->second.GetBack_Inl();
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
    // 用 IBR_SectionMap[m_currentSectionId] 取 pbk（与 rebuildEditLines 一致，避免 CurSection 错位）
    std::string keyStr = key.toUtf8().toStdString();
    std::string descStr = desc.toUtf8().toStdString();
    StrPoolID K = NewPoolStr(keyStr);
    if (m_isEmpty) return;
    auto it = IBR_Inst_Project.IBR_SectionMap.find(static_cast<ModuleID_t>(m_currentSectionId));
    if (it == IBR_Inst_Project.IBR_SectionMap.end()) return;
    IBB_Section* pbk = it->second.GetBack_Inl();
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
    // 用 IBR_SectionMap[m_currentSectionId] 取 pbk（与 rebuildEditLines 一致，避免 CurSection 错位）
    std::string keyStr = key.toUtf8().toStdString();
    std::string valStr = value.toUtf8().toStdString();
    StrPoolID K = NewPoolStr(keyStr);
    if (m_isEmpty) return;
    auto it = IBR_Inst_Project.IBR_SectionMap.find(static_cast<ModuleID_t>(m_currentSectionId));
    if (it == IBR_Inst_Project.IBR_SectionMap.end()) return;
    IBB_Section* pbk = it->second.GetBack_Inl();
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

void EditPanelController::setLineValueAt(const QString &key, int index, const QString &value)
{
    // 对应 setLineValue，但按指定分量索引写回（同名多行键 isMultiple 的每个值独立编辑）
    std::string keyStr = key.toUtf8().toStdString();
    std::string valStr = value.toUtf8().toStdString();
    StrPoolID K = NewPoolStr(keyStr);
    if (m_isEmpty) return;
    auto it = IBR_Inst_Project.IBR_SectionMap.find(static_cast<ModuleID_t>(m_currentSectionId));
    if (it == IBR_Inst_Project.IBR_SectionMap.end()) return;
    IBB_Section* pbk = it->second.GetBack_Inl();
    if (!pbk) return;

    IBRF_CoreBump.SendToR({ [pbk, K, index, valStr]() {
        IBG_Undo.SomethingShouldBeHere();
        pbk->MergeLine(K, index, valStr, IBB_IniMergeMode::Replace);
        IBF_Inst_Project.UpdateAll();
    } });

    QTimer::singleShot(30, this, [this]() {
        rebuildEditLines();
        emit sectionDataChanged(m_currentSectionId);
    });
}

void EditPanelController::setIifValue(const QString &key, int mult, int idx, const QString &v)
{
    // 侧边栏 IIF 分量写回（锚 imgui mf：写分量 V.Value 后 RegenFormattedString）
    // 对 input/int/choice/combo/radio/color/slider 的"选中值/编辑值"统一为写该分量 value 串。
    std::string keyStr = key.toUtf8().toStdString();
    std::string valStr = v.toUtf8().toStdString();
    StrPoolID K = NewPoolStr(keyStr);
    if (m_isEmpty) return;
    auto it = IBR_Inst_Project.IBR_SectionMap.find(static_cast<ModuleID_t>(m_currentSectionId));
    if (it == IBR_Inst_Project.IBR_SectionMap.end()) return;
    IBB_Section* pbk = it->second.GetBack_Inl();
    if (!pbk) return;

    IBRF_CoreBump.SendToR({ [pbk, K, mult, idx, valStr, keyStr]() {
        IBG_Undo.SomethingShouldBeHere();
        auto *line = pbk->GetLineFromSubSecs(K);
        if (!line) return;
        auto d = line->Indexed(static_cast<size_t>(mult));
        if (!d) return;
        auto *ii = d->GetData<IBB_IniLine_Data_IIF>();
        if (!ii || !ii->Value) return;
        IBG_InputForm &form = *ii->Value;
        auto &comps = *form.InputComponents;
        if (idx < 0 || idx >= static_cast<int>(comps.size())) return;
        auto *comp = comps[static_cast<size_t>(idx)].get();
        int vid = comp->GetCurrentTargetValueID();
        if (vid < 0) return;
        qDebug("[SET-IIF] ENTER key=%s mult=%d idx=%d INPUT='%s' vid=%d compType=%s",
               keyStr.c_str(), mult, idx, valStr.c_str(), vid, typeid(*comp).name());
        // 全链路诊断：打印所有分量的 StateValPtr/Value/Dirty
        for (size_t ci = 0; ci < comps.size(); ++ci) {
            int cvid = comps[ci]->GetCurrentTargetValueID();
            if (cvid < 0) continue;
            auto &cv = form.GetValue(cvid);
            std::string stTxt, stType = cv.StateValPtr ? typeid(*cv.StateValPtr).name() : "(null)";
            if (auto st = cv.StateValue<IIS_String>()) stTxt = st->Text;
            else if (auto st = cv.StateValue<IIS_Int>()) stTxt = std::to_string(st->Value);
            else if (auto st = cv.StateValue<IIS_Bool>()) stTxt = st->Value ? "true" : "false";
            qDebug("[SET-IIF]   BEFORE comp=%d type=%s vid=%d state=%s stTxt='%s' raw='%s' dirty=%d",
                   (int)ci, typeid(*comps[ci]).name(), cvid, stType.c_str(), stTxt.c_str(), cv.Value.c_str(), (int)cv.Dirty);
        }
        // 对齐 ImGui 原版写回：直接在原始 Value 上改目标分量 State，
        // 然后 GetFormattedString（只对 Dirty 目标分量 FormatValue）+ UpdateAll（重建链接）。
        // 不 Duplicate / 不 RegenFormattedString（强制全量 FormatValue，会把 IIS_Int 态链接分量
        // 目标名格式化成数字）/ 不 SetValue（不触发 ParseFromString 全量重解析）。
        auto &newV = form.GetValue(vid);
        if (auto st = newV.StateValue<IIS_String>()) st->Text = valStr;
        else newV.ResetState<IIS_String>(valStr);
        newV.Value = valStr;
        newV.NeedsUpdate(form.GetValues(), *comp);
        qDebug("[SET-IIF]   OnExport=%d Key=%.20s exportingLine=%p exportingSec=%p",
               (int)ExportContext::OnExport, PoolStr(ExportContext::Key), (void*)ExportContext::ExportingLine, (void*)ExportContext::ExportingSection);
        qDebug("[SET-IIF]   P1-afterset vid=%d stTxt='%s' raw='%s'", vid,
               (newV.StateValue<IIS_String>() ? newV.StateValue<IIS_String>()->Text.c_str() : "?"), newV.Value.c_str());
        form.GetFormattedString();        // 只格式化 Dirty 的目标分量（其余用缓存 V.Value，不截断）
        auto &gv = form.GetValue(vid);
        qDebug("[SET-IIF]   P2-aftfmt vid=%d stTxt='%s' raw='%s'", vid,
               (gv.StateValue<IIS_String>() ? gv.StateValue<IIS_String>()->Text.c_str() : "?"), gv.Value.c_str());
        qDebug("[SET-IIF]   LINE='%s'", form.GetFormattedString().c_str());
        pbk->UpdateAll();                // 重建链接表
        IBR_Inst_Project.RefreshLinkList = true;
        {
            auto &v2 = form.GetValue(vid);
            std::string stTxt, stType = v2.StateValPtr ? typeid(*v2.StateValPtr).name() : "(null)";
            if (auto st = v2.StateValue<IIS_String>()) stTxt = st->Text;
            else if (auto st = v2.StateValue<IIS_Int>()) stTxt = std::to_string(st->Value);
            else if (auto st = v2.StateValue<IIS_Bool>()) stTxt = st->Value ? "true" : "false";
            qDebug("[SET-IIF]   P3-aftupdall vid=%d state=%s stTxt='%s' raw='%s'", vid, stType.c_str(), stTxt.c_str(), v2.Value.c_str());
        }
    } });
    QTimer::singleShot(30, this, [this]() {
        rebuildEditLines();
        emit sectionDataChanged(m_currentSectionId);
    });
}

void EditPanelController::setIifBool(const QString &key, int mult, int idx, bool val)
{
    // 侧边栏 IIF bool 分量写回（IIS_Bool + IIC_Bool::FmtType）
    std::string keyStr = key.toUtf8().toStdString();
    StrPoolID K = NewPoolStr(keyStr);
    if (m_isEmpty) return;
    auto it = IBR_Inst_Project.IBR_SectionMap.find(static_cast<ModuleID_t>(m_currentSectionId));
    if (it == IBR_Inst_Project.IBR_SectionMap.end()) return;
    IBB_Section* pbk = it->second.GetBack_Inl();
    if (!pbk) return;

    IBRF_CoreBump.SendToR({ [pbk, K, mult, idx, val]() {
        IBG_Undo.SomethingShouldBeHere();
        auto *line = pbk->GetLineFromSubSecs(K);
        if (!line) return;
        auto d = line->Indexed(static_cast<size_t>(mult));
        if (!d) return;
        auto *ii = d->GetData<IBB_IniLine_Data_IIF>();
        if (!ii || !ii->Value) return;
        IBG_InputForm &form = *ii->Value;
        auto &comps = *form.InputComponents;
        if (idx < 0 || idx >= static_cast<int>(comps.size())) return;
        auto *b = dynamic_cast<IIC_Bool*>(comps[static_cast<size_t>(idx)].get());
        if (!b) return;
        int vid = b->GetCurrentTargetValueID();
        if (vid < 0) return;
        // 统一写入：副本上改 bool 分量 → RegenFormattedString 得新整行串 → d->SetValue
        // 注意：输入值不做任何检测/筛选，直接写回（不做链接表同步，避免覆盖用户输入）。
        auto newForm = ii->Value->Duplicate();
        if (!newForm) return;
        auto &newV = newForm->GetValue(vid);
        if (auto st = newV.StateValue<IIS_Bool>()) { st->Value = val; st->FmtType = b->FmtType; }
        else newV.ResetState<IIS_Bool>(val, b->FmtType);
        newV.NeedsUpdate(newForm->GetValues(), *b);
        const std::string newLine = newForm->RegenFormattedString();
        d->SetValue(newLine);            // SetValue 内部 ParseFromString（CurSub 非空时 UpdateAll）
        pbk->UpdateAll();                // 确保链接重建
        IBR_Inst_Project.RefreshLinkList = true;
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
    // 用 IBR_SectionMap[m_currentSectionId] 取 pbk（与 rebuildEditLines 一致，避免 CurSection 错位）
    if (m_isEmpty) return;
    auto it = IBR_Inst_Project.IBR_SectionMap.find(static_cast<ModuleID_t>(m_currentSectionId));
    if (it == IBR_Inst_Project.IBR_SectionMap.end()) return;
    IBB_Section* pbk = it->second.GetBack_Inl();
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
