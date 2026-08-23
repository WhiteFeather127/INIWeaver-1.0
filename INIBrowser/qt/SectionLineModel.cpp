// SectionLineModel.cpp
// 行列表模型实现：遍历后端 IBB_Section 提取行数据，对应 ImGui RenderUI_Lines
#include "SectionLineModel.h"
#include "WorkspaceController.h"  // m_workspace->refresh()（链接修改后同步全量刷新）
#include "Global.h"
#include "IBR_Project.h"
#include "IBR_Misc.h"
#include "IBB_Ini.h"
#include "IBB_IniLine.h"   // D14：IBB_IniLine_Data_String/Data_IIF 定义（用于 IICStatus 读写）
#include "IBB_RegType.h"
#include "IBR_LinkNode.h"
#include "IBG_InputType_Defines.h"
#include "FromEngine/ImGuiDeps.h"
#include "FromEngine/RFBump.h"
// D13：行标题四选一文本所需依赖
#include "IBR_Localization.h"      // loc/locw 宏
#include "IBB_PropStringPool.h"    // PoolCStr 宏
#include "FromEngine/global_tool_func.h"  // UTF8toUnicode/UnicodetoUTF8
#include "IBG_InputType_Derived.h"  // IIC_* 输入组件类型（IIF 多分量导出/渲染）
#include "IifComponentHelper.h"    // 画布/侧边栏共用 IIF 分量导出辅助（统一类型判定）
#include "IBB_Components.h"        // IBB_Section_Desc
#include <algorithm>
#include <ranges>
#include <set>
#include <QFile>
#include <QTextStream>
#include <QCoreApplication>

// Acceptor_CheckLinkType/CheckSecType 自由函数前向声明（定义在 IBR_SectionData.cpp:438-465）
bool Acceptor_CheckLinkType(StrPoolID SourceReg, StrPoolID TargetReg, StrPoolID LinkType);
bool Acceptor_CheckSecType(StrPoolID SourceReg, StrPoolID SecType);
// D13：SplitParamCached 前向声明（定义在 IBR_LinkNode.cpp:743）
const std::vector<std::string>& SplitParamCached(const std::string& Text);

SectionLineModel::SectionLineModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

int SectionLineModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid()) return 0;
    return static_cast<int>(m_entries.size());
}

QHash<int, QByteArray> SectionLineModel::roleNames() const
{
    static const QHash<int, QByteArray> names = {
        {LineKeyRole,    "lineKey"},
        {OnShowRole,     "onShow"},
        {DescLongRole,   "descLong"},
        {SubSecNameRole, "subSecName"},
        {SubSecTypeRole, "subSecType"},
        {HasLinkNodeRole,"hasLinkNode"},
        {LinkColRole,    "linkCol"},
        {LinkLimitRole,  "linkLimit"},
        {LinkTypeRole,   "linkType"},
        {IsEmptyRole,    "isEmpty"},
        {IsInheritRole,  "isInherit"},
        {IsImportRole,   "isImport"},
        {LineIdxRole,    "lineIdx"},
        {LineMultRole,   "lineMult"},
        {CompIdxRole,    "compIdx"},
        {SessionIdRole,  "sessionId"},
        {LinksRole,      "links"},
        {IsCollapsedRole,"isCollapsed"},
        {ExportValueRole,"exportValue"},
        {IsInputModeRole,"isInputMode"},
        {IsMultipleRole, "isMultiple"},
        {SpecialAcceptRole,"specialAccept"},
        {InputOnShowRole, "inputOnShow"},
        {KeyTypeRole,    "keyType"},
        {BoolCheckedRole, "boolChecked"},
    };
    return names;
}

QVariant SectionLineModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid()) return {};
    int row = index.row();
    if (row < 0 || row >= static_cast<int>(m_entries.size())) return {};
    const auto &e = m_entries[row];

    switch (role) {
    case LineKeyRole:     return e.keyName;
    case OnShowRole:      return e.onShow;
    case DescLongRole:    return e.descLong;
    case SubSecNameRole:  return e.subSecName;
    case SubSecTypeRole:  return e.subSecType;
    case HasLinkNodeRole: return e.hasLinkNode;
    case LinkColRole:     return e.linkCol;
    case LinkLimitRole:   return e.linkLimit;
    case LinkTypeRole:    return e.linkType;
    case IsEmptyRole:     return e.isEmpty;
    case IsInheritRole:   return e.isInherit;
    case IsImportRole:    return e.isImport;
    case LineIdxRole:     return e.lineIdx;
    case LineMultRole:    return e.lineMult;
    case CompIdxRole:     return e.compIdx;
    case SessionIdRole:   return static_cast<qulonglong>(e.sessionId);
    case LinksRole:       return e.links;
    case IsCollapsedRole: return e.isCollapsed;
    case ExportValueRole: return e.exportValue;
    case IsInputModeRole: return e.isInputMode;
    case IsMultipleRole:  return e.isMultiple;
    case SpecialAcceptRole: return m_specialAccept.value(e.keyId, false);
    case InputOnShowRole: return m_inputOnShow.value(e.keyId, false);
    case KeyTypeRole:     return e.keyType;
    case BoolCheckedRole: return e.boolChecked;
    }
    return {};
}

void SectionLineModel::setSectionId(qulonglong id)
{
    // 修复：不能用 m_sectionId==id 跳过。m_sectionId 初始为 0，合法模块 ID 也可能是 0，
    // 若 lineModel 创建时即绑定 ID=0 的模块，setSectionId(0) 会命中此 return 不 refresh，
    // 导致 ID=0 模块画布无行。改为：值相同且已构建过（m_sectionId 在 map 中）才跳过。
    if (m_sectionId == id) {
        auto it = IBR_Inst_Project.IBR_SectionMap.find(static_cast<ModuleID_t>(id));
        if (it == IBR_Inst_Project.IBR_SectionMap.end()) return;  // 真正无激活，无需刷新
        // id 在 map 中（合法），即使值相同也继续 refresh（首次构建场景）
    }
    m_sectionId = id;
    emit sectionIdChanged();
    refresh();
}

void SectionLineModel::setShowRegName(bool v)
{
    if (m_showRegName == v) return;
    m_showRegName = v;
    emit showRegNameChanged();
    refresh();
}

void SectionLineModel::refresh()
{
    const int oldCount = static_cast<int>(m_entries.size());
    rebuildEntries();

    // IIF 行在 QML 侧走 iifComponents()（函数式 + iifRevision 门控），内容来自实时
    // 读 form 分量状态，只有 iifDataChanged 能触发 QML 重读。而侧边栏改 IIF 值只变更
    // form 内分量状态、不改行值串（GetString/format 不变），快照 diff 检测不到 →
    // 必须无条件对每个 IIF 行补发 iifDataChanged，否则“侧边栏改值→画布模块不同步”。
    // 对齐画布直接改值路径（setIifComponentValue 尾部也显式 emit），二者互补不冲突。
    for (size_t i = 0; i < m_entries.size(); ++i) {
        if (m_entries[i].keyType == 2) {
            emit iifDataChanged(static_cast<int>(i));
        }
    }

    // 性能优化：行内容未变时跳过信号。
    // 删除/新建模块等触发全量 refreshSections 时，无关节点的行数据不变，
    // 不发 dataChanged/resetModel 可避免 QML 行绑定重新求值（全量重建卡顿主因）
    if (m_entries.size() == m_entriesSnapshot.size()
        && std::equal(m_entries.begin(), m_entries.end(), m_entriesSnapshot.begin()))
    {
        // 内容相同：快照同步（新 entries 与旧等价），直接返回
        m_entriesSnapshot = m_entries;
        return;
    }
    m_entriesSnapshot = m_entries;

    // 行数不变时用 dataChanged 局部更新（连线/取消连线只改链接，行数不变）：
    // 避免 resetModel 触发 QML 行重建，行重建瞬间圆点坐标未就绪会污染 acceptCenter
    // 缓存 → 该模块所有连线起点画到第一个节点。行数变化才 resetModel。
    const int newCount = static_cast<int>(m_entries.size());
    if (newCount != oldCount)
    {
        beginResetModel();
        endResetModel();
    }
    else if (newCount > 0)
    {
        emit dataChanged(index(0), index(newCount - 1));
    }
}

void SectionLineModel::rebuildEntries()
{
    std::vector<LineEntry> newEntries;

    if (m_sectionId == 0) {
        // 修复：0 既是无激活标记也是合法模块 ID。用 IBR_SectionMap.find 区分：
        // 0 不在 map（真正无激活）→ 清空；0 在 map（合法模块 ID=0）→ 继续构建行。
        auto it0 = IBR_Inst_Project.IBR_SectionMap.find(static_cast<ModuleID_t>(m_sectionId));
        if (it0 == IBR_Inst_Project.IBR_SectionMap.end()) {
            m_entries = std::move(newEntries);
            return;
        }
    }

    ModuleID_t sid = static_cast<ModuleID_t>(m_sectionId);
    auto rsec = IBR_Inst_Project.GetSectionFromID(sid);
    auto bsec = rsec.GetBack_Unsafe();
    if (bsec) {
        // 对应 ImGui RenderUI_Lines（IBR_SectionData.cpp:953）
        // CheckSubsecOrder 懒初始化 SubSecOrder（项目加载后首次访问时填充）
        // 缺失此调用会导致 SubSecOrder 为空，行数据遍历不执行，节点显示"无行数据"
        bsec->CheckSubsecOrder();

        // 对应 ImGui RenderUI_Lines（IBR_SectionData.cpp:955-974）
        for (auto subIdx : bsec->SubSecOrder) {
            auto &sub = bsec->SubSecs[subIdx];
            if (!sub.Default) continue;
            // 跳过 Inherit 且无继承源（对应 RenderUI_Lines:961-962）
            if (sub.Default->Type == IBB_SubSec_Default::Inherit && bsec->Inherit.empty())
                continue;

            for (auto keyId : bsec->LineOrder) {
                if (!sub.CanOwnKey(keyId)) continue;
                // OnShow=false 的行折叠不显示（对应 RenderUI_Lines:966 coll=!IsOnShow → continue）
                // IsOnShow=true 才显示在画布上
                if (!bsec->IsOnShow(keyId)) continue;

                auto *line = bsec->GetLineFromSubSecs(keyId);
                if (!line || !line->Default) continue;

                // 共享字段（所有分量相同，提取到循环外避免重复计算）
                const auto &onShowDesc = bsec->GetOnShow(keyId);
                // D13：行标题四选一文本（对应 ImGui IBR_SectionData.cpp:421-432 RenderUI_Line）
                // 顺序：InheritKeyID 优先 → ShowRegName → EmptyOnShowDesc 比较 → OnShow
                QString onShowText;
                if (keyId == InheritKeyID()) {
                    // 继承描述：InheritStr lambda（IBR_SectionData.cpp:396-414）
                    // ShowInherit = line->Indexed(0)->FirstIsLink()
                    bool showInherit = line->Indexed(0)->FirstIsLink();
                    onShowText = buildInheritStr(bsec, rsec, m_showRegName, showInherit);
                } else if (m_showRegName) {
                    // 注册键名：PoolCStr（非 PoolStr，保持 ImGui 一致）
                    onShowText = QString::fromUtf8(PoolCStr(keyId));
                } else if (onShowDesc.empty() || onShowDesc == EmptyOnShowDesc) {
                    // 短描述：EmptyOnShowDesc("\r\n\r\n\r\n") 视为空
                    onShowText = QString::fromUtf8(PoolDesc(line->Default->DescShort));
                } else {
                    // 用户 OnShow 描述
                    onShowText = QString::fromUtf8(onShowDesc.c_str());
                }
                QString descLongText = QString::fromUtf8(PoolDesc(line->Default->DescLong));

                const auto &ln = line->Default->LinkNode;
                QString linkTypeStr = QString::fromUtf8(PoolStr(ln.LinkType));
                int linkLimitVal = ln.LinkLimit;
                bool hasLinkNodeVal = IBB_DefaultRegType::HasRegType(ln.LinkType);

                // 修复：lineIdx 必须用 keyId 在 sub.Lines_ByName 中的位置索引，
                // 对应 IBB_SubSec::UpdateAll 中 `for (auto&& [LineIdx, L] : zip(iota(0u), Lines_ByName))`
                // 之前用 static_cast<size_t>(keyId)（StrPoolID 值，如 644/54838）是错误的，
                // 导致 GetSessionIdx 算出的 SessionID 与 link.SourceID 不匹配，LastCenter 回写失效。
                size_t lineIdx = 0;
                bool foundIdx = false;
                for (size_t i = 0; i < sub.Lines_ByName.size(); ++i) {
                    if (sub.Lines_ByName[i] == keyId) {
                        lineIdx = i;
                        foundIdx = true;
                        break;
                    }
                }
                if (!foundIdx) continue;  // keyId 不在此 SubSec 中，跳过

                bool sectionIgnored = false;
                auto sd = rsec.GetSectionData();
                if (sd) sectionIgnored = sd->Ignore;

                // 阶段 5：遍历多分量（对应 IBB_IniLine::ForEachWithIdx）
                // ImGui 中 ForEachWithIdx 对每个分量调 Data->RenderUI，每个分量独立 LinkNode
                size_t multCount = line->Count();
                if (multCount == 0) multCount = 1;  // 安全兜底
                for (size_t mult = 0; mult < multCount; ++mult) {
                    LineEntry e;
                    e.keyId = keyId;
                    e.keyName = QString::fromUtf8(PoolStr(keyId));
                    e.subSecName = QString::fromUtf8(sub.Default->Name);
                    e.subSecType = static_cast<int>(sub.Default->Type);
                    e.isInherit = (sub.Default->Type == IBB_SubSec_Default::Inherit);
                    e.isImport = (sub.Default->Type == IBB_SubSec_Default::Import);
                    e.onShow = onShowText;
                    e.descLong = descLongText;
                    e.linkType = linkTypeStr;
                    e.linkLimit = linkLimitVal;
                    e.hasLinkNode = hasLinkNodeVal;
                    e.lineIdx = static_cast<int>(lineIdx);
                    e.lineMult = static_cast<int>(mult);  // 多分量索引
                    e.compIdx = 0;

                    // SessionID（对应 IBR_NodeSession::GetSessionIdx）
                    e.sessionId = IBR_NodeSession::GetSessionIdx(
                        bsec->GetThisID(), sub.Default->Name, lineIdx, mult, 0);

                    // 链接状态
                    auto [begin, end] = sub.GetLink(lineIdx, mult, 0);
                    e.isEmpty = (begin == end);
                    e.links = collectLinks(sub, lineIdx, mult, 0);

                    // 节点颜色
                    e.linkCol = computeNodeColor(ln.LinkCol, e.isEmpty, e.isInherit,
                                                 e.hasLinkNode, sectionIgnored);

                    // 显示值（Input 态用，按分量索引）：GetString 无副作用。
                    // FinalExportString 是导出专用，内部 OnExport Regen + SetValue
                    // + UpdateAll/LimitFix 会把链接分量目标名污染成 NaN/截断数字
                    if (auto evData = line->Indexed(mult))
                        e.exportValue = QString::fromUtf8(evData->GetString().c_str());

                    // D14：IICStatus 持久化（从业务层 Data 读取，对应 ImGui Status_Workspace/ComponentStatus）
                    // Data_String: Status_Workspace.InputMethod（所有分量共享）
                    // Data_IIF: ComponentStatus[mult].InputMethod（每分量独立）
                    // Data_Bool: 无状态机，永远 Input 态
                    auto dataPtr = line->Indexed(mult);
                    if (dataPtr) {
                        auto strData = dataPtr->GetData<IBB_IniLine_Data_String>();
                        if (strData) {
                            e.isInputMode = !(strData->Status_Workspace.InputMethod == IICStatus::Link);
                            e.keyType = 0;  // String
                        } else {
                            auto iifData = dataPtr->GetData<IBB_IniLine_Data_IIF>();
                            if (iifData && iifData->Value) {
                                // 多分量 IIF：整行始终渲染其分量 Flow（各分量自身的 Link/Input
                                // 状态由 iifComponents 的 isLink 决定 → 节点/输入框）。行级 isInputMode
                                // 不能由 ComponentStatus[mult]（分量状态数组按分量索引起始）推断，
                                // 否则组件数量与值数量不对齐时会把"多节点 IIF"（如两个 Type:Link 分量）
                                // 折叠成行级单节点 → 三分量结构消失。故 IIF 恒定 Input 态显示 Flow。
                                e.isInputMode = true;
                                e.keyType = 2;  // IIF
                            } else {
                                // Data_Bool 或未知类型：永远 Input 态
                                e.isInputMode = true;
                                auto bData = dataPtr->GetData<IBB_IniLine_Data_Bool>();
                                if (bData) {
                                    e.keyType = 1;
                                    e.boolChecked = bData->Value;
                                } else {
                                    e.keyType = 0;  // 未知类型按 String 处理
                                }
                            }
                        }
                    }

                    e.isCollapsed = false;
                    // IsMultiple：InputType.Multiple 标志（对应 ImGui "+" 增行按钮显示条件）
                    e.isMultiple = line->IsMultiple();

                    newEntries.push_back(std::move(e));
                }
            }
        }
    }

    m_entries = std::move(newEntries);
}

QVariantList SectionLineModel::collectLinks(const IBB_SubSec &sub,
                                            size_t lineIdx, size_t lineMult, size_t compIdx) const
{
    QVariantList links;
    auto [begin, end] = sub.GetLink(lineIdx, lineMult, compIdx);
    for (auto it = begin; it != end; ++it) {
        size_t linkIdx = it->second;
        if (linkIdx >= sub.NewLinkTo.size()) continue;
        const auto &link = sub.NewLinkTo[linkIdx];

        QVariantMap info;
        info["linkIdx"] = static_cast<int>(linkIdx);
        info["destSectionId"] = static_cast<qulonglong>(link.ToLoc.Sec.ID);
        info["destKey"] = QString::fromUtf8(PoolStr(link.ToLoc.Key));
        info["destMult"] = static_cast<int>(link.ToLoc.Mult);
        info["isSelfLink"] = (link.FromLoc.Sec == link.ToLoc.Sec);

        // 目标显示名
        auto dstRsec = IBR_Inst_Project.GetSection(link.ToLoc.Sec);
        info["destDisplayName"] = QString::fromUtf8(dstRsec.GetDisplayName());

        links.append(info);
    }
    return links;
}

QColor SectionLineModel::computeNodeColor(uint32_t linkColRaw, bool isEmpty, bool isInherit,
                                          bool hasLinkNode, bool sectionIgnored) const
{
    // 对应 IBR_LinkNode::AdjustNodeCol（IBR_LinkNode.cpp:987-1018）
    if (!hasLinkNode) {
        // RenderUI_Node_Disabled：灰色不可交互
        return QColor(90, 90, 90);
    }
    if (sectionIgnored) {
        // TempWbg：白色半透明
        return QColor(255, 255, 255, 128);
    }
    if (isEmpty && isInherit) {
        // FocusLineColor：蓝色
        return QColor(79, 195, 247);  // #4fc3f7
    }
    if (isEmpty) {
        // IllegalLineColor：红色
        return QColor(214, 59, 59);  // #d63b3b
    }
    if (linkColRaw != 0) {
        // 自定义颜色（ImU32 ABGR 打包）
        return QColor(
            static_cast<int>((linkColRaw >> 0) & 0xFF),
            static_cast<int>((linkColRaw >> 8) & 0xFF),
            static_cast<int>((linkColRaw >> 16) & 0xFF),
            static_cast<int>((linkColRaw >> 24) & 0xFF)
        );
    }
    // CheckMark 默认色
    return QColor(204, 204, 204);  // #cccccc
}

// D13：构建继承描述文本（对应 ImGui IBR_SectionData.cpp:396-414 InheritStr lambda）
// 按 ShowRegName 决定显示注册名或解析后的显示名，最终格式化为本地化字符串
QString SectionLineModel::buildInheritStr(const IBB_Section *bsec, const IBR_Section &rsec,
                                          bool showReg, bool showInherit) const
{
    if (!bsec) return QString();

    // 获取当前 Section 的 Ini 文件名（用于 GetSecIndex 的 PriorIni 参数）
    auto curDesc = rsec.GetSectionDesc();
    const std::string &curIni = curDesc.Ini;

    std::wstring w;
    for (const auto &Q : SplitParamCached(bsec->Inherit)) {
        if (showReg) {
            // ShowRegName 模式：直接显示注册名（原始字符串）
            w += UTF8toUnicode(Q);
        } else {
            // 解析为显示名：GetSecIndex → GetSection → GetDisplayName
            auto Idx = IBF_Inst_Project.Project.GetSecIndex(Q, curIni);
            auto RSec = IBR_Inst_Project.GetSection(Idx);
            if (RSec.HasBack()) w += UTF8toUnicode(RSec.GetDisplayName());
            else w += UTF8toUnicode(Q);
            w.push_back(L',');
        }
    }
    // 去掉末尾逗号（非 ShowRegName 模式下会多加一个）
    if (!w.empty() && !showReg) w.pop_back();

    if (w.empty()) {
        // 无继承：显示 "无继承"
        return QString::fromUtf8(loc("GUI_NoInherit"));
    }
    // 格式化：showInherit ? 继承列表 : 空字符串
    // 对应 ImGui: std::vformat(locw("GUI_InheritFrom"), std::make_wformat_args(ShowInherit() ? w : Nul))
    std::wstring Nul;
    const std::wstring &arg = showInherit ? w : Nul;
    std::string formatted = UnicodetoUTF8(std::vformat(locw("GUI_InheritFrom"), std::make_wformat_args(arg)));
    return QString::fromUtf8(formatted.c_str());
}

void SectionLineModel::setLinkNodeCenter(int row, qreal x, qreal y)
{
    if (row < 0 || row >= static_cast<int>(m_entries.size())) return;
    const auto &e = m_entries[row];
    // 回写到 IBR_NodeSession::SessionValue.LastCenter
    // 对应 ImGui PushLinkForDraw → SetSessionStatus
    IBR_NodeSession::SetSessionStatus(e.sessionId, ImVec2(static_cast<float>(x), static_cast<float>(y)), false);
    // 通知 WorkspaceController 端点表需要重建
    // Qt 版本无每帧渲染，LastCenter 更新后需主动触发 rebuildLinkEndpoints
    emit linkNodeCenterChanged();
}

// IIF 分量节点坐标回写：compIdx 定位分量，sessionId 按 compIdx 重算（与 UpdateAll 的 Comp=cidx 一致）
// 写回后 rebuildLinkEndpoints 的 priority 2（sv.LastCenter）读到该分量圆点坐标，连线起点/终点正确。
// 注意：IIF 分量节点不写 acceptCenterByKey（多分量同名会互相覆盖），故 priority 1 恒 miss → 走 priority 2。
void SectionLineModel::setLinkNodeCenterAt(int row, int compIdx, qreal x, qreal y)
{
    if (row < 0 || row >= static_cast<int>(m_entries.size())) return;
    qulonglong sid = sessionIdFor(row, compIdx);
    if (sid == 0) return;
    IBR_NodeSession::SetSessionStatus(sid, ImVec2(static_cast<float>(x), static_cast<float>(y)), false);
    emit linkNodeCenterChanged();
}

qulonglong SectionLineModel::sessionIdFor(int row, int compIdx) const
{
    if (row < 0 || row >= static_cast<int>(m_entries.size())) return 0;
    const auto &e = m_entries[row];
    if (compIdx <= 0) return e.sessionId;  // 快速路径：compIdx==0 即 e.sessionId
    ModuleID_t sid = static_cast<ModuleID_t>(m_sectionId);
    auto bsec = IBR_Inst_Project.GetSectionFromID(sid).GetBack_Unsafe();
    if (!bsec) return 0;
    for (auto subIdx : bsec->SubSecOrder) {
        auto &sub = bsec->SubSecs[subIdx];
        if (!sub.CanOwnKey(e.keyId)) continue;
        if (e.lineIdx < 0 || e.lineIdx >= static_cast<int>(sub.Lines_ByName.size())) return 0;
        if (sub.Lines_ByName[static_cast<size_t>(e.lineIdx)] != e.keyId) return 0;
        // 对应 UpdateAll 中 IIF 行链接建 Session：Comp = 分量索引
        return IBR_NodeSession::GetSessionIdx(
            bsec->GetThisID(), sub.Default->Name, static_cast<size_t>(e.lineIdx),
            static_cast<size_t>(e.lineMult), static_cast<size_t>(compIdx));
    }
    return 0;
}

void SectionLineModel::setAcceptCenter(int row, qreal x, qreal y)
{
    // 阶段 3：行级接受点回写（对应 ImGui ActiveLines[key].AcceptCenter[mult]）
    // 该坐标作为连线终点 pb 的行精确值
    // 按 keyName@mult 复合键存储（而非 row 索引）：rebuildEntries 重建 m_entries 后 row 顺序可能变化，
    // 按索引会取到错误行的坐标导致串线；多分量键（Multiple 行）同名 key 的每个分量有独立圆点，
    // 仅按 keyName 会把多分量合并到最后一个坐标（起点错位），故复合 keyName+mult。
    if (row < 0 || row >= static_cast<int>(m_entries.size())) {
        return;
    }
    const auto &e = m_entries[row];
    m_acceptCentersByKey.insert(
        QStringLiteral("%1@%2").arg(e.keyName).arg(e.lineMult), QPointF(x, y));
}

void SectionLineModel::setAcceptCenterByKey(const QString &keyName, int lineMult, qreal x, qreal y)
{
    // 阶段 3（修复）：按 keyName@mult 直接存接受点，不查 m_entries。
    // 修复 root：rebuildEntries 重建 m_entries 后 QML Repeater 旧 delegate 的 rowIndex 可能错位，
    // setAcceptCenter(row) 用旧 row 查新 entries → 存到错误 key（如 Warhead 行被 Projectile 坐标覆盖），
    // 导致链接起点/终点画到相邻行圆点上。keyName/mult 由 LineRow delegate 直接绑定，天然正确。
    m_acceptCentersByKey.insert(
        QStringLiteral("%1@%2").arg(keyName).arg(lineMult), QPointF(x, y));
}

QPointF SectionLineModel::acceptCenterByKey(const QString &keyName, int lineMult) const
{
    // 阶段 3：按 keyName@mult 复合键查询行接受点
    // 对应 ImGui RenderUI_Links 中 RSD->ActiveLines[DestKey].AcceptCenter[LineMult]
    // 同一行的多键值（lineMult 不同）各有独立圆点坐标
    auto it = m_acceptCentersByKey.constFind(
        QStringLiteral("%1@%2").arg(keyName).arg(lineMult));
    if (it != m_acceptCentersByKey.constEnd()) return it.value();
    return QPointF();  // 未找到（QML 尚未回写该行接受点）
}

QStringList SectionLineModel::acceptCenterKeys() const
{
    QStringList keys = m_acceptCentersByKey.keys();
    keys.sort();
    return keys;
}

bool SectionLineModel::checkLinkType(int row, qulonglong destSectionId) const
{
    if (row < 0 || row >= static_cast<int>(m_entries.size())) return false;
    const auto &e = m_entries[row];

    ModuleID_t srcId = static_cast<ModuleID_t>(m_sectionId);
    ModuleID_t dstId = static_cast<ModuleID_t>(destSectionId);

    auto srcRsec = IBR_Inst_Project.GetSectionFromID(srcId);
    auto dstRsec = IBR_Inst_Project.GetSectionFromID(dstId);
    auto srcBsec = srcRsec.GetBack_Unsafe();
    auto dstBsec = dstRsec.GetBack_Unsafe();
    if (!srcBsec || !dstBsec) return false;

    StrPoolID srcReg = srcBsec->Register;
    StrPoolID dstReg = dstBsec->Register;
    StrPoolID linkType = e.keyId;  // 临时用 keyId，实际应取 line->Default->LinkNode.LinkType
    // 重新取 LinkType
    auto *line = srcBsec->GetLineFromSubSecs(e.keyId);
    if (line && line->Default) linkType = line->Default->LinkNode.LinkType;

    return Acceptor_CheckLinkType(srcReg, dstReg, linkType);
}

bool SectionLineModel::checkSecType(qulonglong destSectionId) const
{
    ModuleID_t srcId = static_cast<ModuleID_t>(m_sectionId);
    ModuleID_t dstId = static_cast<ModuleID_t>(destSectionId);

    auto srcRsec = IBR_Inst_Project.GetSectionFromID(srcId);
    auto dstRsec = IBR_Inst_Project.GetSectionFromID(dstId);
    auto srcBsec = srcRsec.GetBack_Unsafe();
    auto dstBsec = dstRsec.GetBack_Unsafe();
    if (!srcBsec || !dstBsec) return false;

    // SecType 校验：源行的 SecType 与目标 Section 的 Register 匹配
    // 简化：直接用目标 Register 作为 SecType
    return Acceptor_CheckSecType(srcBsec->Register, dstBsec->Register);
}

bool SectionLineModel::createLink(int row, qulonglong destSectionId, const QString &destKey)
{
    if (row < 0 || row >= static_cast<int>(m_entries.size())) return false;
    const auto &e = m_entries[row];

    ModuleID_t srcId = static_cast<ModuleID_t>(m_sectionId);
    ModuleID_t dstId = static_cast<ModuleID_t>(destSectionId);

    // 类型校验
    if (!checkLinkType(row, destSectionId)) return false;

    StrPoolID dstKeyId = 0;
    if (!destKey.isEmpty()) {
        dstKeyId = NewPoolStr(destKey.toUtf8().constData());
    }

    StrPoolID srcKeyId = e.keyId;
    size_t srcLineMult = static_cast<size_t>(e.lineMult);
    size_t srcLineIdx = static_cast<size_t>(e.lineIdx);

    IBRF_CoreBump.SendToR({ [srcId, dstId, srcKeyId, srcLineMult, srcLineIdx, dstKeyId, this]() {
        auto srcRsec = IBR_Inst_Project.GetSectionFromID(srcId);
        auto dstRsec = IBR_Inst_Project.GetSectionFromID(dstId);
        auto srcBsec = srcRsec.GetBack_Unsafe();
        auto dstBsec = dstRsec.GetBack_Unsafe();
        if (!srcBsec || !dstBsec) return;

        auto *line = srcBsec->GetLineFromSubSecs(srcKeyId);
        if (!line || !line->Default) return;

        auto dstData = dstRsec.GetSectionData();
        if (!dstData) return;

        // 目标 key：若未指定，用目标的 DLK（DefaultLinkKey）
        StrPoolID finalDstKey = dstKeyId;
        if (finalDstKey == 0) {
            finalDstKey = dstBsec->GetDLK(dstBsec->Register);
            if (finalDstKey == 0) finalDstKey = NewPoolStr("default");
        }

        // D12 修正：改走 ModifyAndShow 路径——修改行值而非直接 push NewLinkTo
        // 对应 ImGui RenderUI_Node 的 NotifyValueToMerge 处理（IBR_LinkNode.cpp:596-631）
        // 1. 收集现有链接的 TargetValue 列表
        // 2. 追加新目标（若未存在）
        // 3. 遵守 LinkLimit
        // 4. 写入行值，UpdateAll 重建 NewLinkTo
        std::string newTarget = TargetValueStr(dstData->Desc.Sec, finalDstKey, 0);

        IBB_SubSec *foundSub = nullptr;
        for (auto subIdx : srcBsec->SubSecOrder) {
            auto &sub = srcBsec->SubSecs[subIdx];
            if (sub.CanOwnKey(srcKeyId)) {
                foundSub = &sub;
                break;
            }
        }

        std::vector<std::string> values;
        if (foundSub) {
            auto [begin, end] = foundSub->GetLink(srcLineIdx, srcLineMult, 0);
            for (auto it = begin; it != end; ++it) {
                if (it->second < foundSub->NewLinkTo.size()) {
                    values.push_back(foundSub->NewLinkTo[it->second].TargetValue());
                }
            }
        }

        // 追加新目标（若未存在）
        bool exists = std::find(values.begin(), values.end(), newTarget) != values.end();
        if (!exists) values.push_back(newTarget);

        // 遵守 LinkLimit（对应 ImGui IBR_LinkNode.cpp:612-628）
        int limit = line->Default->GetLinkLimit();
        if (limit > 0 && static_cast<int>(values.size()) > limit) {
            if (limit == 1) {
                values = { values.back() };
            } else {
                values.erase(values.begin(),
                             values.begin() + (values.size() - static_cast<size_t>(limit)));
            }
        }

        // 构建逗号分隔的值字符串并写入行值
        std::string str;
        for (size_t i = 0; i < values.size(); ++i) {
            if (i > 0) str += ",";
            str += values[i];
        }

        line->Merge(srcLineMult, str, IBB_IniMergeMode::Replace);
        srcBsec->UpdateAll();
        IBR_Inst_Project.RefreshLinkList = true;
        // 修复：业务修改完成后再刷新/通知（SendToR 队列在下一 QTimer tick 才执行 lambda，
        // 原来的立即 refresh/emit 读到旧数据，导致连线与侧边栏不更新）
        refresh();
        emit sectionDataChanged(m_sectionId);
    } });

    return true;
}

// IIF 分量节点建链（comIdx 定位分量）：把目标链接串写入该分量 Value 并重算格式化串落盘
// 对应 imgui IIC_InputText(Link) RenderUI_Node 的 ModifyFunc（IBG_InputType.cpp:1407）：
// 写 IIS_String = 目标串 → NeedsUpdate → RegenFormat → UpdateAll 重建链接列表。
bool SectionLineModel::createLinkAt(int row, int compIdx, qulonglong destSectionId, const QString &destKey)
{
    if (row < 0 || row >= static_cast<int>(m_entries.size())) return false;
    const auto &e = m_entries[row];
#ifdef INIWEAVER_DIAG
    qDebug("[IIF-WRITE] createLinkAt key='%s' comp=%d dst=%llu", PoolCStr(e.keyId), compIdx, (unsigned long long)destSectionId);
#endif

    ModuleID_t srcId = static_cast<ModuleID_t>(m_sectionId);
    StrPoolID srcKeyId = e.keyId;
    int mult = e.lineMult;
    ModuleID_t dstId = static_cast<ModuleID_t>(destSectionId);

    auto rsec = IBR_Inst_Project.GetSectionFromID(srcId);
    auto bsec = rsec.GetBack_Unsafe();
    auto drsec = IBR_Inst_Project.GetSectionFromID(dstId);
    auto dbsec = drsec.GetBack_Unsafe();
    auto ddata = drsec.GetSectionData();
    if (!bsec || !dbsec || !ddata) return false;

    // 源分量链接类型（LinkNode.Type 决定 Accept 校验）
    StrPoolID compLinkType = 0;
    auto *ln = bsec->GetLineFromSubSecs(srcKeyId);
    if (!ln || !ln->Default) return false;
    auto dataPtr = ln->Indexed(static_cast<size_t>(mult));
    if (!dataPtr) return false;
    auto iedit = dataPtr->GetData<IBB_IniLine_Data_IIF>();
    if (!iedit || !iedit->Value) return false;
    auto &comps = *iedit->Value->InputComponents;
    if (compIdx < 0 || compIdx >= static_cast<int>(comps.size())) return false;
    compLinkType = comps[static_cast<size_t>(compIdx)]->NodeSetting.LinkType;

    if (!Acceptor_CheckLinkType(bsec->Register, dbsec->Register, compLinkType)) return false;

    // 目标 key：未指定用目标的 DLK（DefaultLinkKey）
    StrPoolID finalDstKey = 0;
    if (!destKey.isEmpty() && destKey != QLatin1String("default")) {
        finalDstKey = NewPoolStr(destKey.toUtf8().constData());
    }
    if (finalDstKey == 0) {
        finalDstKey = dbsec->GetDLK(dbsec->Register);
        if (finalDstKey == 0) finalDstKey = NewPoolStr("default");
    }
    std::string newTarget = TargetValueStr(ddata->Desc.Sec, finalDstKey, 0);

    IBRF_CoreBump.SendToR({ [srcId, srcKeyId, mult, compIdx, newTarget, row, this]() {
        if (!IBR_Inst_Project.GetSectionFromID(srcId).HasBack()) return;
        // 与 writeIifCompString 同步执行（避免 m_entries 在 refresh 后错位）：
        // 这里在渲染线程 lambda 内直接写业务数据，保持与 setIifComponentValue 一致
        auto srcRsec = IBR_Inst_Project.GetSectionFromID(srcId);
        auto srcBsec = srcRsec.GetBack_Unsafe();
        if (!srcBsec) return;
        auto *line = srcBsec->GetLineFromSubSecs(srcKeyId);
        if (!line) return;
        auto dPtr = line->Indexed(static_cast<size_t>(mult));
        if (!dPtr) return;
        auto ii = dPtr->GetData<IBB_IniLine_Data_IIF>();
        if (!ii || !ii->Value) return;

        IBG_InputForm &form = *ii->Value;
        auto &comps = *form.InputComponents;
        if (compIdx < 0 || compIdx >= static_cast<int>(comps.size())) return;
        auto *comp = comps[static_cast<size_t>(compIdx)].get();
        int vid = comp->GetCurrentTargetValueID();
        if (vid < 0) return;

        auto &V = form.GetValue(vid);
        if (auto st = V.StateValue<IIS_String>()) st->Text = newTarget;
        else V.ResetState<IIS_String>(newTarget);
        V.Value = newTarget;
        V.NeedsUpdate(form.GetValues(), *comp);
        form.RegenFormattedString();
        srcBsec->UpdateAll();
        IBR_Inst_Project.RefreshLinkList = true;
        refresh();
        emit iifDataChanged(row);
        emit sectionDataChanged(m_sectionId);
    } });
    return true;
}

// IIF 分量节点清空链接（解除该分量链接）：写入空串 + 重算格式化串
void SectionLineModel::deleteAllLinksAt(int row, int compIdx)
{
    if (row < 0 || row >= static_cast<int>(m_entries.size())) return;
    const auto &e = m_entries[row];
#ifdef INIWEAVER_DIAG
    qDebug("[IIF-WRITE] deleteAllLinksAt key='%s' comp=%d", PoolCStr(e.keyId), compIdx);
#endif

    ModuleID_t srcId = static_cast<ModuleID_t>(m_sectionId);
    StrPoolID srcKeyId = e.keyId;
    int mult = e.lineMult;

    IBRF_CoreBump.SendToR({ [srcId, srcKeyId, mult, compIdx, row, this]() {
        auto srcRsec = IBR_Inst_Project.GetSectionFromID(srcId);
        auto srcBsec = srcRsec.GetBack_Unsafe();
        if (!srcBsec) return;
        auto *line = srcBsec->GetLineFromSubSecs(srcKeyId);
        if (!line) return;
        auto dPtr = line->Indexed(static_cast<size_t>(mult));
        if (!dPtr) return;
        auto ii = dPtr->GetData<IBB_IniLine_Data_IIF>();
        if (!ii || !ii->Value) return;

        IBG_InputForm &form = *ii->Value;
        auto &comps = *form.InputComponents;
        if (compIdx < 0 || compIdx >= static_cast<int>(comps.size())) return;
        auto *comp = comps[static_cast<size_t>(compIdx)].get();
        int vid = comp->GetCurrentTargetValueID();
        if (vid < 0) return;
        if (form.GetValue(vid).Value.empty()) return;  // 本就空，无需改动

        auto &V = form.GetValue(vid);
        if (auto st = V.StateValue<IIS_String>()) st->Text.clear();
        else V.ResetState<IIS_String>("");
        V.Value.clear();
        V.NeedsUpdate(form.GetValues(), *comp);
        form.RegenFormattedString();
        srcBsec->UpdateAll();
        IBR_Inst_Project.RefreshLinkList = true;
        refresh();
        emit iifDataChanged(row);
        emit sectionDataChanged(m_sectionId);
    } });
}

bool SectionLineModel::deleteLink(int row, int linkIdx)
{
    if (row < 0 || row >= static_cast<int>(m_entries.size())) return false;
    const auto &e = m_entries[row];

    ModuleID_t srcId = static_cast<ModuleID_t>(m_sectionId);
    StrPoolID srcKeyId = e.keyId;
    size_t lineIdx = static_cast<size_t>(e.lineIdx);
    size_t lineMult = static_cast<size_t>(e.lineMult);

    IBRF_CoreBump.SendToR({ [srcId, srcKeyId, lineIdx, lineMult, linkIdx, this]() {
        auto srcRsec = IBR_Inst_Project.GetSectionFromID(srcId);
        auto srcBsec = srcRsec.GetBack_Unsafe();
        if (!srcBsec) return;

        auto *line = srcBsec->GetLineFromSubSecs(srcKeyId);
        if (!line) return;

        IBB_SubSec *foundSub = nullptr;
        for (auto subIdx : srcBsec->SubSecOrder) {
            auto &sub = srcBsec->SubSecs[subIdx];
            if (sub.CanOwnKey(srcKeyId)) {
                foundSub = &sub;
                break;
            }
        }
        if (!foundSub) return;

        // D12 修正：改走行值修改路径（对应 ImGui Session.NewValue 重建）
        // 收集该行所有链接的 TargetValue，排除要删除的那条，重建值字符串
        auto [begin, end] = foundSub->GetLink(lineIdx, lineMult, 0);
        std::vector<size_t> indices;
        for (auto it = begin; it != end; ++it) {
            indices.push_back(it->second);
        }
        if (linkIdx < 0 || linkIdx >= static_cast<int>(indices.size())) return;

        size_t toRemove = indices[linkIdx];
        std::string str;
        bool first = true;
        for (size_t i = 0; i < indices.size(); ++i) {
            if (indices[i] == toRemove) continue;
            if (indices[i] >= foundSub->NewLinkTo.size()) continue;
            if (!first) str += ",";
            str += foundSub->NewLinkTo[indices[i]].TargetValue();
            first = false;
        }

        line->Merge(lineMult, str, IBB_IniMergeMode::Replace);
        srcBsec->UpdateAll();
        IBR_Inst_Project.RefreshLinkList = true;
        // 修复：业务修改完成后再刷新/通知（增量按后端，避免全量 resetModel 坐标污染）
        refresh();
        emit sectionDataChanged(m_sectionId);
        if (m_workspace) m_workspace->refreshLinkIncremental();
    } });

    return true;
}

void SectionLineModel::deleteAllLinks(int row)
{
    if (row < 0 || row >= static_cast<int>(m_entries.size())) return;
    const auto &e = m_entries[row];
#ifdef INIWEAVER_DIAG
    qDebug("[IIF-WRITE] deleteAllLinks key='%s'", PoolCStr(e.keyId));
#endif

    ModuleID_t srcId = static_cast<ModuleID_t>(m_sectionId);
    StrPoolID srcKeyId = e.keyId;
    size_t lineMult = static_cast<size_t>(e.lineMult);

    // 同步执行：QML 调用本就在主线程，业务修改 + 前端刷新即时完成，
    // 避免 SendToR 队列等下一 tick（~40ms）才执行导致的取消连线延迟
    auto srcRsec = IBR_Inst_Project.GetSectionFromID(srcId);
    auto srcBsec = srcRsec.GetBack_Unsafe();
    if (!srcBsec) return;

    auto *line = srcBsec->GetLineFromSubSecs(srcKeyId);
    if (!line) return;

    // D12 修正：写空值清除所有链接（对应 ImGui Session.NewValue = ""）
    line->Merge(lineMult, "", IBB_IniMergeMode::Replace);
    srcBsec->UpdateAll();
    IBR_Inst_Project.RefreshLinkList = true;

    // 后端已改值。前端刷新（模型重建/侧边栏/工作区连线）统一 queued 到当前
    // QML 事件处理完成后执行：onClicked 栈内同步 beginResetModel 会销毁正在
    // 执行回调的 delegate（崩溃风险）；queued 后几乎无感（下一事件迭代），
    // 远快于原 SendToR 队列（下一 tick ~40ms）
    const qulonglong sid = m_sectionId;
    QMetaObject::invokeMethod(this, [this, sid]() {
        refresh();
        emit sectionDataChanged(sid);
        if (m_workspace) m_workspace->refreshLinkIncremental();
    }, Qt::QueuedConnection);
}

bool SectionLineModel::applyLinkStates(int row, QVariantList keepLinkIdxs)
{
    if (row < 0 || row >= static_cast<int>(m_entries.size())) return false;
    const auto &e = m_entries[row];
#ifdef INIWEAVER_DIAG
    qDebug("[IIF-WRITE] applyLinkStates key='%s' keep=%d", PoolCStr(e.keyId), (int)keepLinkIdxs.size());
#endif

    ModuleID_t srcId = static_cast<ModuleID_t>(m_sectionId);
    StrPoolID srcKeyId = e.keyId;
    size_t lineIdx = static_cast<size_t>(e.lineIdx);
    size_t lineMult = static_cast<size_t>(e.lineMult);

    // 转为 set 便于查找
    std::set<int> keepSet;
    for (const auto &v : keepLinkIdxs) keepSet.insert(v.toInt());

    // 同步执行：QML 调用本就在主线程，业务修改 + 前端刷新即时完成，避免队列延迟
    auto srcRsec = IBR_Inst_Project.GetSectionFromID(srcId);
    auto srcBsec = srcRsec.GetBack_Unsafe();
    if (!srcBsec) return false;

    auto *line = srcBsec->GetLineFromSubSecs(srcKeyId);
    if (!line) return false;

    IBB_SubSec *foundSub = nullptr;
    for (auto subIdx : srcBsec->SubSecOrder) {
        auto &sub = srcBsec->SubSecs[subIdx];
        if (sub.CanOwnKey(srcKeyId)) {
            foundSub = &sub;
            break;
        }
    }
    if (!foundSub) return false;

    // D17：按 keepLinkIdxs 过滤，仅保留 checked 的链接（对应 ImGui RadioButton UseLink 切换）
    auto [begin, end] = foundSub->GetLink(lineIdx, lineMult, 0);
    std::vector<size_t> indices;
    for (auto it = begin; it != end; ++it) {
        indices.push_back(it->second);
    }

    std::string str;
    bool first = true;
    for (int i = 0; i < static_cast<int>(indices.size()); ++i) {
        if (keepSet.find(i) == keepSet.end()) continue;
        if (indices[i] >= foundSub->NewLinkTo.size()) continue;
        if (!first) str += ",";
        str += foundSub->NewLinkTo[indices[i]].TargetValue();
        first = false;
    }

    line->Merge(lineMult, str, IBB_IniMergeMode::Replace);
    srcBsec->UpdateAll();
    IBR_Inst_Project.RefreshLinkList = true;

    // 后端已改值。前端刷新（模型重建/侧边栏/工作区连线）统一 queued 到当前
    // QML 事件处理完成后执行（onClicked 栈内 beginResetModel 会销毁 delegate）
    const qulonglong sid = m_sectionId;
    QMetaObject::invokeMethod(this, [this, sid]() {
        refresh();
        emit sectionDataChanged(sid);
        if (m_workspace) m_workspace->refreshLinkIncremental();
    }, Qt::QueuedConnection);
    return true;
}

bool SectionLineModel::modifyValue(int row, const QString &newText)
{
    if (row < 0 || row >= static_cast<int>(m_entries.size())) return false;
    const auto &e = m_entries[row];
#ifdef INIWEAVER_DIAG
    qDebug("[IIF-WRITE] modifyValue key='%s' new='%s'", PoolCStr(e.keyId), newText.toUtf8().constData());
#endif

    ModuleID_t srcId = static_cast<ModuleID_t>(m_sectionId);
    StrPoolID srcKeyId = e.keyId;
    std::string value = newText.toUtf8().constData();

    IBRF_CoreBump.SendToR({ [srcId, srcKeyId, value, this]() {
        auto srcRsec = IBR_Inst_Project.GetSectionFromID(srcId);
        auto srcBsec = srcRsec.GetBack_Unsafe();
        if (!srcBsec) return;

        auto *line = srcBsec->GetLineFromSubSecs(srcKeyId);
        if (!line) return;

        // 用 Merge 替换第 0 个分量的值（对应 ImGui ModifyAndShow）
        line->Merge(0, value, IBB_IniMergeMode::Replace);
        srcBsec->UpdateAll();
        // 修复：业务修改完成后再刷新/通知
        refresh();
        emit sectionDataChanged(m_sectionId);
    } });

    return true;
}

// D14：切换 Input/Link 态（对应 ImGui 双击翻转 IICStatus.InputMethod）
// 业务层 Data 为 Source of Truth，直接读写 Data 的 Status_Workspace/ComponentStatus
void SectionLineModel::toggleInputMode(int row)
{
    if (row < 0 || row >= static_cast<int>(m_entries.size())) return;
    auto &e = m_entries[row];

    ModuleID_t srcId = static_cast<ModuleID_t>(m_sectionId);
    auto rsec = IBR_Inst_Project.GetSectionFromID(srcId);
    auto bsec = rsec.GetBack_Unsafe();
    if (!bsec) return;

    auto *line = bsec->GetLineFromSubSecs(e.keyId);
    if (!line) return;

    auto dataPtr = line->Indexed(static_cast<size_t>(e.lineMult));
    if (!dataPtr) return;

    // Data_String: 翻转 Status_Workspace.InputMethod（所有分量共享）
    auto strData = dataPtr->GetData<IBB_IniLine_Data_String>();
    if (strData) {
        strData->Status_Workspace.InputMethod =
            (strData->Status_Workspace.InputMethod == IICStatus::Input) ? IICStatus::Link : IICStatus::Input;
        e.isInputMode = !(strData->Status_Workspace.InputMethod == IICStatus::Link);
    } else {
        // Data_IIF: 翻转 ComponentStatus[mult].InputMethod（每分量独立）
        auto iifData = dataPtr->GetData<IBB_IniLine_Data_IIF>();
        if (iifData && iifData->Value) {
            auto& cs = iifData->Value->GetComponentStatus();
            auto idx = static_cast<size_t>(e.lineMult);
            if (idx < cs.size()) {
                cs[idx].InputMethod = (cs[idx].InputMethod == IICStatus::Input) ? IICStatus::Link : IICStatus::Input;
                e.isInputMode = !(cs[idx].InputMethod == IICStatus::Link);
            }
        }
        // Data_Bool：无状态机，不切换
    }

    // 通知 QML 该行数据变化
    QModelIndex idx = index(row);
    emit dataChanged(idx, idx, {IsInputModeRole});
    // 通知侧边栏刷新（画布上切换 Input/Link 态后，侧边栏需同步显示）
    emit sectionDataChanged(m_sectionId);
}

// 行级右键菜单：翻转 SpecialAccept 临时态（对应 IBR_Misc.cpp:217-234）
// 画布会话级状态，不持久化到 INI，按 keyId 索引跨 rebuild 保留
void SectionLineModel::toggleSpecialAccept(int row)
{
    if (row < 0 || row >= static_cast<int>(m_entries.size())) return;
    auto &e = m_entries[row];
    m_specialAccept[e.keyId] = !m_specialAccept.value(e.keyId, false);
    QModelIndex idx = index(row);
    emit dataChanged(idx, idx, {SpecialAcceptRole});
}

// 行级右键菜单：删除整行（对应 IBR_Misc.cpp:235-244 pbk->RemoveLine(Key)）
void SectionLineModel::removeLine(int row)
{
    if (row < 0 || row >= static_cast<int>(m_entries.size())) return;
    auto &e = m_entries[row];

    ModuleID_t srcId = static_cast<ModuleID_t>(m_sectionId);
    StrPoolID keyId = e.keyId;

    IBRF_CoreBump.SendToR({ [srcId, keyId, this]() {
        auto rsec = IBR_Inst_Project.GetSectionFromID(srcId);
        auto bsec = rsec.GetBack_Unsafe();
        if (!bsec) return;
        bsec->RemoveLine(keyId);
        // 删行可能带走该行链接，标记重建 LinkList
        IBR_Inst_Project.RefreshLinkList = true;
        // 修复：业务修改完成后再刷新/通知
        refresh();
        emit sectionDataChanged(m_sectionId);
    } });

    m_specialAccept.remove(keyId);
    m_inputOnShow.remove(keyId);
}

// 行级右键菜单：翻转 InputOnShow 编辑态（对应 IBR_Misc.cpp:245-250 CC->InputOnShow = !CC->InputOnShow）
// 画布会话级状态，不持久化到 INI，按 keyId 索引跨 rebuild 保留
// 仅切换显示/隐藏编辑框，不写入业务层；用户在编辑框按 Enter/失焦时由 editDesc 提交
void SectionLineModel::toggleInputOnShow(int row)
{
    if (row < 0 || row >= static_cast<int>(m_entries.size())) return;
    auto &e = m_entries[row];
    m_inputOnShow[e.keyId] = !m_inputOnShow.value(e.keyId, false);
    QModelIndex idx = index(row);
    emit dataChanged(idx, idx, {InputOnShowRole});
}

// 本模块是否有行处于 InputOnShow 描述编辑态
bool SectionLineModel::hasActiveInputOnShow() const
{
    for (auto it = m_inputOnShow.constBegin(); it != m_inputOnShow.constEnd(); ++it)
        if (it.value()) return true;
    return false;
}

// 行级右键菜单：编辑 OnShow 描述（对应 IBR_Misc.cpp:245-250 EditDesc）
// 写 bsec->OnShow[key]，空串写 EmptyOnShowDesc 标记
void SectionLineModel::editDesc(int row, const QString& text)
{
    if (row < 0 || row >= static_cast<int>(m_entries.size())) return;
    auto &e = m_entries[row];

    ModuleID_t srcId = static_cast<ModuleID_t>(m_sectionId);
    StrPoolID keyId = e.keyId;
    std::string utf8 = text.toUtf8().toStdString();

    IBRF_CoreBump.SendToR({ [srcId, keyId, utf8, this]() {
        auto rsec = IBR_Inst_Project.GetSectionFromID(srcId);
        auto bsec = rsec.GetBack_Unsafe();
        if (!bsec) return;
        // 空串写 EmptyOnShowDesc 标记（对应 ImGui: if(EditOnShow.empty()) OnShow = EmptyOnShowDesc）
        bsec->OnShow[keyId] = utf8.empty() ? std::string(EmptyOnShowDesc) : utf8;
        // 修复：业务修改完成后再刷新/通知
        refresh();
        emit sectionDataChanged(m_sectionId);
    } });
}

// 行级增行按钮：对 Multiple 行追加新分量（对应 IBR_Misc.cpp:373-385 "+" 按钮）
// 调 bsec->MergeLine(Key, Index_AlwaysNew, Form->GetFormattedString(), Replace)
void SectionLineModel::addLine(int row)
{
    if (row < 0 || row >= static_cast<int>(m_entries.size())) return;
    auto &e = m_entries[row];
    if (!e.isMultiple) return;

    ModuleID_t srcId = static_cast<ModuleID_t>(m_sectionId);
    StrPoolID keyId = e.keyId;

    IBRF_CoreBump.SendToR({ [srcId, keyId, this]() {
        auto rsec = IBR_Inst_Project.GetSectionFromID(srcId);
        auto bsec = rsec.GetBack_Unsafe();
        if (!bsec) return;
        auto *line = bsec->GetLineFromSubSecs(keyId);
        if (!line || !line->Default) return;
        // Form->GetFormattedString() 提供格式化空值模板（对应 ImGui def->GetInputType().Form->GetFormattedString()）
        const std::string& formStr = line->Default->GetInputType().Form->GetFormattedString();
        bsec->MergeLine(keyId, Index_AlwaysNew, formStr, IBB_IniMergeMode::Replace);
        // 修复：业务修改完成后再刷新/通知
        refresh();
        emit sectionDataChanged(m_sectionId);
    } });
}

// 键类型：Bool（勾选框）翻转值（对应 ImGui IIC_Bool 对话框切换）
// 读取对应分量 Data_Bool 当前 bool，翻转后按 StrBoolType 格式写回业务层
bool SectionLineModel::toggleBoolValue(int row)
{
    if (row < 0 || row >= static_cast<int>(m_entries.size())) return false;
    const auto &e = m_entries[row];
#ifdef INIWEAVER_DIAG
    qDebug("[IIF-WRITE] toggleBoolValue key='%s'", PoolCStr(e.keyId));
#endif

    ModuleID_t srcId = static_cast<ModuleID_t>(m_sectionId);
    StrPoolID keyId = e.keyId;
    int lineMult = e.lineMult;

    IBRF_CoreBump.SendToR({ [srcId, keyId, lineMult, this]() {
        auto rsec = IBR_Inst_Project.GetSectionFromID(srcId);
        auto bsec = rsec.GetBack_Unsafe();
        if (!bsec) return;
        auto *line = bsec->GetLineFromSubSecs(keyId);
        if (!line) return;
        auto dataPtr = line->Indexed(static_cast<size_t>(lineMult));
        if (!dataPtr) return;
        auto bData = dataPtr->GetData<IBB_IniLine_Data_Bool>();
        if (!bData) return;
        // 翻转 bool，并按 StrBoolType 格式化后写回
        bool next = !bData->Value;
        std::string nextStr = StrBoolImpl(next, bData->Type);
        bData->SetValue(nextStr);
        bsec->UpdateAll();
        refresh();
        emit sectionDataChanged(m_sectionId);
    } });

    return true;
}

// IIF 分量 Hint（对齐 imgui RenderIICInputText）：Short 是可见标签文本，Long 是悬停提示
static IICDescStr IIC_Hint(const IBG_InputComponent *comp)
{
    if (auto t = dynamic_cast<const IIC_InputText*>(comp)) return t->Hint;
    else if (auto t = dynamic_cast<const IIC_InputInt*>(comp)) return t->Hint;
    else if (auto t = dynamic_cast<const IIC_Bool*>(comp)) return t->Hint;
    else if (auto t = dynamic_cast<const IIC_MultipleChoice*>(comp)) return t->Hint;
    else if (auto t = dynamic_cast<const IIC_EnumCombo*>(comp)) return t->Hint;
    else if (auto t = dynamic_cast<const IIC_EnumRadio*>(comp)) return t->Hint;
    else if (auto t = dynamic_cast<const IIC_ColorPanel*>(comp)) return t->Hint;
    else if (auto t = dynamic_cast<const IIC_SliderInt*>(comp)) return t->Hint;
    return {};
}

// IIF 多分量导出：返回该行各分量描述，信息对齐 imgui IBG_InputForm::RenderUI
// 每分量：idx(=compIdx)/type(细粒度)/label(Short)/tooltip(Long||Short)/value/isLink/readOnly/disabled
// 链接分量（isLink && SupportLinks）type="link"，附节点元数据供 LinkNodePoint 建链/坐标回写。
QVariantList SectionLineModel::iifComponents(int row) const
{
    QVariantList out;
    if (row < 0 || row >= static_cast<int>(m_entries.size())) return out;
    const auto &e = m_entries[row];

    auto rsec = IBR_Inst_Project.GetSectionFromID(static_cast<ModuleID_t>(m_sectionId));
    auto bsec = rsec.GetBack_Unsafe();
    if (!bsec) return out;
    auto *line = bsec->GetLineFromSubSecs(e.keyId);
    if (!line) return out;
    auto dataPtr = line->Indexed(static_cast<size_t>(e.lineMult));
    if (!dataPtr) return out;
    auto ii = dataPtr->GetData<IBB_IniLine_Data_IIF>();
    if (!ii || !ii->Value) return out;

    IBG_InputForm &form = *ii->Value;
    // 节点元数据所需的 SubSec / lineIdx / sectionIgnored（链接列表、颜色、坐标用）
    IBB_SubSec *sub = nullptr;
    size_t lineIdx = 0;
    for (auto subIdx : bsec->SubSecOrder) {
        auto &ss = bsec->SubSecs[subIdx];
        if (!ss.CanOwnKey(e.keyId)) continue;
        sub = &ss;
        for (size_t k = 0; k < ss.Lines_ByName.size(); ++k)
            if (ss.Lines_ByName[k] == e.keyId) { lineIdx = k; break; }
        break;
    }
    bool sectionIgnored = false;
    auto sd = rsec.GetSectionData();
    if (sd) sectionIgnored = sd->Ignore;

    // 与侧边栏一致：先恢复链接分量目标名（对齐 ImGui RenderUI_Node 每帧从链接表读目标名），
    // 避免 GetFormattedString/FormatValue 用 IIS_Int 态或错误格式把混合目标名截断成数字。
    if (sub)
        IifSyncLinkTargets(form, sub, lineIdx, static_cast<size_t>(e.lineMult));
    // 先确保各分量 Value 已按格式化串就绪（Dirty 时重算）
    form.GetFormattedString();
    auto &comps = *form.InputComponents;

    // 统一导出（按 ImGui 原版组件驱动模型，画布/侧边栏共用）：输出分量类型/值/参数/链接目标名。
    // 链接分量值从 StateValPtr 目标名读取（IifExportComponents 内部），不读 raw 覆盖。
    out = IifExportComponents(ii, sub, lineIdx, static_cast<size_t>(e.lineMult), /*isWorkSpace=*/true);

    // 画布补充链接节点元数据（连线列表/颜色/坐标/会话键），对应 ImGui IBR_LinkNode::RenderUI_Node
    for (size_t i = 0; i < comps.size() && i < static_cast<size_t>(out.size()); ++i) {
        auto &p = comps[i];
        auto m = out[static_cast<int>(i)].toMap();
        bool isLink = m["isLink"].toBool();
        if (isLink && p->SupportLinks() && m["type"] != "text") {
            m["type"] = "link";
            m["value"] = QString();   // 链接节点显示由 links 表驱动，不用文本值
            auto hasNode = IBB_DefaultRegType::HasRegType(p->NodeSetting.LinkType);
            bool empty = true;
            QVariantList compLinks;
            if (sub) {
                auto [cb, ce] = sub->GetLink(lineIdx, e.lineMult, i);
                empty = (cb == ce);
                compLinks = collectLinks(*sub, lineIdx, e.lineMult, i);
            }
            m["hasLinkNode"] = hasNode;
            m["linkLimit"] = p->NodeSetting.LinkLimit;
            m["linkType"] = QString::fromUtf8(PoolStr(p->NodeSetting.LinkType));
            m["linkCol"] = computeNodeColor(p->NodeSetting.LinkCol, empty, false, hasNode, sectionIgnored);
            m["isEmpty"] = empty;
            m["links"] = compLinks;
            m["lineMult"] = static_cast<int>(e.lineMult);
            m["sessionId"] = QString::number(static_cast<qulonglong>(sessionIdFor(row, static_cast<int>(i))));
#ifdef INIWEAVER_DIAG
            qDebug("[IIF-SEL] key='%s' mult=%d comp=%d isEmpty=%d links=%d hasNode=%d sess=%s",
                   PoolCStr(e.keyId), static_cast<int>(e.lineMult), static_cast<int>(i),
                   static_cast<int>(empty), static_cast<int>(compLinks.size()),
                   static_cast<int>(hasNode), m["sessionId"].toString().toUtf8().constData());
#endif
        }
        out[static_cast<int>(i)] = m;
    }
    return out;
}

// IIF 分量值写回：修改 input/int 分量 state 后按格式化串重建整行并写回业务层
void SectionLineModel::setIifComponentValue(int row, int compIdx, const QString &rawText)
{
    if (row < 0 || row >= static_cast<int>(m_entries.size())) return;
    const auto &e = m_entries[row];

    ModuleID_t srcId = static_cast<ModuleID_t>(m_sectionId);
    StrPoolID keyId = e.keyId;
    int mult = e.lineMult;
    std::string text = rawText.toUtf8().constData();

    IBRF_CoreBump.SendToR({ [row, srcId, keyId, mult, text, compIdx, this]() {
        auto rsec = IBR_Inst_Project.GetSectionFromID(srcId);
        auto bsec = rsec.GetBack_Unsafe();
        if (!bsec) return;
        auto *line = bsec->GetLineFromSubSecs(keyId);
        if (!line) return;
        auto dataPtr = line->Indexed(static_cast<size_t>(mult));
        if (!dataPtr) return;
        auto ii = dataPtr->GetData<IBB_IniLine_Data_IIF>();
        if (!ii || !ii->Value) return;

        IBG_InputForm &form = *ii->Value;
        auto &comps = *form.InputComponents;
        if (compIdx < 0 || compIdx >= static_cast<int>(comps.size())) return;
        auto *comp = comps[static_cast<size_t>(compIdx)].get();
        int vid = comp->GetCurrentTargetValueID();
        if (vid < 0) { qDebug("[IIF-DIAG] vid<0 compIdx=%d kind=%d", compIdx, (int)row); return; }
        qDebug("[CANVAS-SET] ENTER row=%d key=%s mult=%d compIdx=%d INPUT='%s' vid=%d compType=%s",
               row, PoolStr(keyId), mult, compIdx, text.c_str(), vid, typeid(*comp).name());
        for (size_t ci = 0; ci < comps.size(); ++ci) {
            int cvid = comps[ci]->GetCurrentTargetValueID();
            if (cvid < 0) continue;
            auto &cv = form.GetValue(cvid);
            std::string stTxt, stType = cv.StateValPtr ? typeid(*cv.StateValPtr).name() : "(null)";
            if (auto st = cv.StateValue<IIS_String>()) stTxt = st->Text;
            else if (auto st = cv.StateValue<IIS_Int>()) stTxt = std::to_string(st->Value);
            else if (auto st = cv.StateValue<IIS_Bool>()) stTxt = st->Value ? "true" : "false";
            qDebug("[CANVAS-SET]   BEFORE comp=%d type=%s vid=%d state=%s stTxt='%s' raw='%s' dirty=%d",
                   (int)ci, typeid(*comps[ci]).name(), cvid, stType.c_str(), stTxt.c_str(), cv.Value.c_str(), (int)cv.Dirty);
        }

        // 统一写入：在 Duplicate 副本上改目标分量 → RegenFormattedString 得新整行值串，
        // 再用原版 IBB_IniLine_Data_IIF::SetValue → ParseFromString 应用到原始行，
        // 由 ImGui 原版解析逻辑同步所有分量的 StateValPtr/V.Value（链接分量按链接目标名解析，
        // 不再被 FormatValue/ProcessOnExport 错误覆盖为 NaN/0）。
        // 找 sub/lineIdx（链接分量目标名从链接表同步需要）
        IBB_SubSec* sub = nullptr;
        size_t lineIdx = 0;
        for (auto subIdx : bsec->SubSecOrder) {
            auto &ss = bsec->SubSecs[subIdx];
            if (!ss.CanOwnKey(keyId)) continue;
            sub = &ss;
            for (size_t k = 0; k < ss.Lines_ByName.size(); ++k)
                if (ss.Lines_ByName[k] == keyId) { lineIdx = k; break; }
            break;
        }
        // 对齐 ImGui 原版写回：直接在原始 Value 上改目标分量 State，
        // 然后 GetFormattedString（只对 Dirty 目标分量 FormatValue）+ UpdateAll（重建链接）。
        // 不 Duplicate / 不 RegenFormattedString（强制全量 FormatValue，会把 IIS_Int 态链接分量
        // 目标名格式化成数字）/ 不 SetValue（不触发 ParseFromString 全量重解析）。
        (void)sub; (void)lineIdx;   // 不再需要链接表同步（ImGui 原版不这么做）
        auto &newV = form.GetValue(vid);
        if (auto st = newV.StateValue<IIS_String>()) st->Text = text;
        else newV.ResetState<IIS_String>(text);
        newV.Value = text;
        newV.NeedsUpdate(form.GetValues(), *comp);
        form.GetFormattedString();        // 只格式化 Dirty 的目标分量（其余用缓存 V.Value，不截断）
        qDebug("[CANVAS-SET]   LINE='%s'", form.GetFormattedString().c_str());
        bsec->UpdateAll();               // 重建链接表
        {
            auto &v2 = form.GetValue(vid);
            std::string stTxt, stType = v2.StateValPtr ? typeid(*v2.StateValPtr).name() : "(null)";
            if (auto st = v2.StateValue<IIS_String>()) stTxt = st->Text;
            else if (auto st = v2.StateValue<IIS_Int>()) stTxt = std::to_string(st->Value);
            else if (auto st = v2.StateValue<IIS_Bool>()) stTxt = st->Value ? "true" : "false";
            qDebug("[CANVAS-SET]   AFTER vid=%d state=%s stTxt='%s' raw='%s'", vid, stType.c_str(), stTxt.c_str(), v2.Value.c_str());
        }
        refresh();
        emit iifDataChanged(row);        // 通知 QML 重读 IIF 分量（refresh 快照可能 SKIP）
        emit sectionDataChanged(m_sectionId);
    } });
}

void SectionLineModel::setIifComponentValueBool(int row, int compIdx, bool val)
{
    if (row < 0 || row >= static_cast<int>(m_entries.size())) return;
    const auto &e = m_entries[row];

    ModuleID_t srcId = static_cast<ModuleID_t>(m_sectionId);
    StrPoolID keyId = e.keyId;
    int mult = e.lineMult;

    IBRF_CoreBump.SendToR({ [row, srcId, keyId, mult, compIdx, val, this]() {
        auto rsec = IBR_Inst_Project.GetSectionFromID(srcId);
        auto bsec = rsec.GetBack_Unsafe();
        if (!bsec) return;
        auto *line = bsec->GetLineFromSubSecs(keyId);
        if (!line) return;
        auto dataPtr = line->Indexed(static_cast<size_t>(mult));
        if (!dataPtr) return;
        auto ii = dataPtr->GetData<IBB_IniLine_Data_IIF>();
        if (!ii || !ii->Value) return;

        IBG_InputForm &form = *ii->Value;
        auto &comps = *form.InputComponents;
        if (compIdx < 0 || compIdx >= static_cast<int>(comps.size())) return;
        auto *b = dynamic_cast<IIC_Bool*>(comps[static_cast<size_t>(compIdx)].get());
        if (!b) return;
        int vid = b->GetCurrentTargetValueID();
        if (vid < 0) return;

        // 找 sub/lineIdx（链接分量目标名从链接表同步需要）
        IBB_SubSec* sub = nullptr;
        size_t lineIdx = 0;
        for (auto subIdx : bsec->SubSecOrder) {
            auto &ss = bsec->SubSecs[subIdx];
            if (!ss.CanOwnKey(keyId)) continue;
            sub = &ss;
            for (size_t k = 0; k < ss.Lines_ByName.size(); ++k)
                if (ss.Lines_ByName[k] == keyId) { lineIdx = k; break; }
            break;
        }
        // 统一写入：副本上改 bool 分量 → RegenFormattedString 得新整行串 → dataPtr->SetValue
        auto newForm = ii->Value->Duplicate();
        if (!newForm) return;
        // 从链接表恢复链接分量的目标名（对齐 ImGui RenderUI_Node），避免链接分量被污染成 0/NaN
        IifSyncLinkTargets(*newForm, sub, lineIdx, static_cast<size_t>(mult));
        auto &newV = newForm->GetValue(vid);
        if (auto st = newV.StateValue<IIS_Bool>()) { st->Value = val; st->FmtType = b->FmtType; }
        else newV.ResetState<IIS_Bool>(val, b->FmtType);
        newV.NeedsUpdate(newForm->GetValues(), *b);
        const std::string newLine = newForm->RegenFormattedString();
        dataPtr->SetValue(newLine);      // 原版 SetValue → ParseFromString + CurSub->UpdateAll()
        refresh();
        emit iifDataChanged(row);        // 通知 QML 重读 IIF 分量（refresh 快照可能 SKIP）
        emit sectionDataChanged(m_sectionId);
    } });
}
