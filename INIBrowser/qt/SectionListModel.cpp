// SectionListModel.cpp
// Qt6 Section 列表模型实现
#include "SectionListModel.h"
#include "Global.h"
#include "IBR_Project.h"
#include "IBR_Misc.h"
#include "IBB_RegType.h"
#include "IBB_PropStringPool.h"
#include <QCollator>
#include <QRegularExpression>
#include <algorithm>

SectionListModel::SectionListModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

int SectionListModel::rowCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent)
    return static_cast<int>(m_entries.size());
}

QVariant SectionListModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= static_cast<int>(m_entries.size()))
        return {};
    const auto &e = m_entries[index.row()];
    switch (role)
    {
    // 阶段 9.1：DisplayNameRole 根据 showRegName 切换显示（对应 IBR_ListView.cpp:294）
    case DisplayNameRole:  return m_showRegName ? e.secName : e.displayName;
    case SectionIdRole:    return static_cast<qulonglong>(e.sectionId);
    case IniNameRole:      return e.iniName;
    case SecNameRole:      return e.secName;
    case FrozenRole:       return e.frozen;
    case HiddenRole:       return e.hidden;
    case IgnoredRole:      return e.ignored;
    case SelectedRole:     return e.selected;
    case RegisterTypeRole: return e.registerType;
    case RegisterColorRole:return e.registerColor;
    case IsCommentRole:    return e.isComment;
    // 阶段 4 新增：工作区坐标（供双击跳转）
    case EqXRole:          return e.eqX;
    case EqYRole:          return e.eqY;
    case EqWRole:          return e.eqW;
    case EqHRole:          return e.eqH;
    }
    return {};
}

bool SectionListModel::setData(const QModelIndex &index, const QVariant &value, int role)
{
    if (!index.isValid() || index.row() < 0 || index.row() >= static_cast<int>(m_entries.size()))
        return false;
    auto &e = m_entries[index.row()];
    bool changed = false;
    bool needStatsRefresh = false;
    switch (role)
    {
    case FrozenRole:  e.frozen = value.toBool(); changed = true; needStatsRefresh = e.selected; break;
    case HiddenRole:  e.hidden = value.toBool(); changed = true; needStatsRefresh = e.selected; break;
    case IgnoredRole: e.ignored = value.toBool(); changed = true; break;
    case SelectedRole: e.selected = value.toBool(); changed = true; needStatsRefresh = true; break;
    }
    if (changed) emit dataChanged(index, index, {role});
    // 阶段 9.1：选中/冻结/隐藏状态变化需刷新统计量
    if (needStatsRefresh) refreshSelectionStats();
    return changed;
}

QHash<int, QByteArray> SectionListModel::roleNames() const
{
    return {
        {DisplayNameRole,   "displayName"},
        {SectionIdRole,     "sectionId"},
        {IniNameRole,       "iniName"},
        {SecNameRole,       "secName"},
        {FrozenRole,        "frozen"},
        {HiddenRole,        "hidden"},
        {IgnoredRole,       "ignored"},
        {SelectedRole,      "selected"},
        {RegisterTypeRole,  "registerType"},
        {RegisterColorRole, "registerColor"},
        {IsCommentRole,     "isComment"},
        {EqXRole,           "eqX"},
        {EqYRole,           "eqY"},
        {EqWRole,           "eqW"},
        {EqHRole,           "eqH"},
    };
}

void SectionListModel::refresh()
{
    // 全量刷新（由 Q_INVOKABLE 修改操作直接调用）
    m_dirty = true;
    refreshFromTimer();
}

void SectionListModel::refreshFromTimer()
{
    // 性能优化：检查 SectionMap 数量是否变化，无变化时跳过 rebuild
    size_t curCount = IBR_Inst_Project.IBR_SectionMap.size();
    if (!m_dirty && m_lastSectionCount == curCount)
    {
        return;
    }
    m_dirty = false;
    m_lastSectionCount = curCount;
    rebuild();
}

void SectionListModel::syncSelectionFromWorkspace()
{
    // workspace 框选/全选/点击节点后，ExtendMassSelect 已改写 Dynamic.Selected
    // 列表需同步勾选显示，但不重建行（保留滚动位置/避免闪烁）
    std::vector<size_t> newSelected;
    bool anyChanged = false;
    for (size_t i = 0; i < m_entries.size(); ++i)
    {
        bool sel = false;
        auto sec = IBR_Inst_Project.GetSectionFromID(m_entries[i].sectionId);
        if (sec.HasBack())
        {
            auto *back = sec.GetBack_Unsafe();
            if (back) sel = back->Dynamic.Selected;
        }
        if (sel != m_entries[i].selected)
        {
            m_entries[i].selected = sel;
            anyChanged = true;
        }
        if (sel) newSelected.push_back(i);
    }
    if (anyChanged)
    {
        // 批量通知：全行刷新 SelectedRole（QML delegate 的 selected 绑定更新）
        QModelIndex first = index(0);
        QModelIndex last = index(static_cast<int>(m_entries.size()) - 1);
        emit dataChanged(first, last, {SelectedRole});
    }
    m_selectedRows = std::move(newSelected);
    refreshSelectionStats();
}

void SectionListModel::select(int row, bool single)
{
    if (row < 0 || row >= static_cast<int>(m_entries.size())) return;
    if (single)
    {
        // 清除其他选中
        for (size_t i = 0; i < m_entries.size(); ++i)
        {
            if (static_cast<int>(i) != row && m_entries[i].selected)
            {
                m_entries[i].selected = false;
                QModelIndex idx = index(static_cast<int>(i));
                emit dataChanged(idx, idx, {SelectedRole});
            }
        }
        m_selectedRows.clear();
    }
    m_entries[row].selected = !m_entries[row].selected;
    QModelIndex idx = index(row);
    emit dataChanged(idx, idx, {SelectedRole});
    if (m_entries[row].selected)
        m_selectedRows.push_back(row);
    else
        m_selectedRows.erase(std::remove(m_selectedRows.begin(), m_selectedRows.end(), row), m_selectedRows.end());

    // 阶段 4 新增：同步选中状态到工作区（对应 IBR_ListView.cpp:99-104）
    std::vector<ModuleID_t> ids;
    ids.reserve(m_selectedRows.size());
    for (size_t r : m_selectedRows)
    {
        if (r < m_entries.size())
            ids.push_back(m_entries[r].sectionId);
    }
    IBR_WorkSpace::MassSelect(ids);
    // 阶段 9.1：选中变化后刷新统计量
    refreshSelectionStats();
}

void SectionListModel::freeze(int row, bool frozen)
{
    if (row < 0 || row >= static_cast<int>(m_entries.size())) return;
    // 同步到业务层
    auto &sd = IBR_Inst_Project.IBR_SectionMap;
    auto it = sd.find(m_entries[row].sectionId);
    if (it != sd.end())
    {
        it->second.Frozen = frozen;
        m_entries[row].frozen = frozen;
        QModelIndex idx = index(row);
        emit dataChanged(idx, idx, {FrozenRole});
        // 阶段 9.1：选中项的 Frozen 变化需刷新统计量
        if (m_entries[row].selected) refreshSelectionStats();
    }
}

void SectionListModel::hide(int row, bool hidden)
{
    if (row < 0 || row >= static_cast<int>(m_entries.size())) return;
    auto &sd = IBR_Inst_Project.IBR_SectionMap;
    auto it = sd.find(m_entries[row].sectionId);
    if (it != sd.end())
    {
        it->second.Hidden = hidden;
        m_entries[row].hidden = hidden;
        // 对应 IBR_ListView.cpp:268-270 Show 时置 UpdatePosByEq = true
        if (!hidden) it->second.UpdatePosByEq = true;
        QModelIndex idx = index(row);
        emit dataChanged(idx, idx, {HiddenRole});
        // 阶段 9.1：选中项的 Hidden 变化需刷新统计量
        if (m_entries[row].selected) refreshSelectionStats();
    }
}

void SectionListModel::ignore(int row, bool ignored)
{
    if (row < 0 || row >= static_cast<int>(m_entries.size())) return;
    auto &sd = IBR_Inst_Project.IBR_SectionMap;
    auto it = sd.find(m_entries[row].sectionId);
    if (it != sd.end())
    {
        it->second.Ignore = ignored;
        m_entries[row].ignored = ignored;
        QModelIndex idx = index(row);
        emit dataChanged(idx, idx, {IgnoredRole});
    }
}

void SectionListModel::deleteSection(int row)
{
    if (row < 0 || row >= static_cast<int>(m_entries.size())) return;
    uint64_t id = m_entries[row].sectionId;
    // 委托给业务层删除
    IBR_Inst_Project.DeleteSection(IBB_SectionID{id});
    rebuild();
}

void SectionListModel::deleteSelected()
{
    if (m_selectedRows.empty()) return;
    // 收集所有选中项的 sectionId（ModuleID_t = uint64_t）
    // 直接匹配 IBR_Project::DeleteSection(const std::vector<ModuleID_t>&)
    std::vector<ModuleID_t> ids;
    ids.reserve(m_selectedRows.size());
    for (size_t row : m_selectedRows)
    {
        if (row < m_entries.size())
            ids.push_back(m_entries[row].sectionId);
    }
    IBR_Inst_Project.DeleteSection(ids);
    m_selectedRows.clear();
    rebuild();
}

void SectionListModel::clearSelection()
{
    for (size_t i = 0; i < m_entries.size(); ++i)
    {
        if (m_entries[i].selected)
        {
            m_entries[i].selected = false;
            QModelIndex idx = index(static_cast<int>(i));
            emit dataChanged(idx, idx, {SelectedRole});
        }
    }
    m_selectedRows.clear();
    // 阶段 9.1：清空选中后刷新统计量
    refreshSelectionStats();
}

int SectionListModel::selectedCount() const
{
    return static_cast<int>(m_selectedRows.size());
}

void SectionListModel::rebuild()
{
    beginResetModel();
    m_entries.clear();
    m_selectedRows.clear();

    auto &sd = IBR_Inst_Project.IBR_SectionMap;
    m_entries.reserve(sd.size());
    for (const auto & [id, data] : sd)
    {
        Entry e;
        e.sectionId = id;
        e.displayName = QString::fromUtf8(data.DisplayName);
        e.iniName = QString::fromUtf8(data.Desc.Ini);
        e.secName = QString::fromUtf8(data.Desc.Sec);
        e.frozen = data.Frozen;
        e.hidden = data.Hidden;
        e.ignored = data.Ignore;
        e.isComment = data.IsComment;
        // 阶段 4 新增：工作区坐标（供双击跳转）
        e.eqX = data.EqPos.x;
        e.eqY = data.EqPos.y;
        e.eqW = data.EqSize.x;
        e.eqH = data.EqSize.y;

        // 寄存器类型与颜色：从后端 IBB_Section 读取 Register，查 IBB_DefaultRegType
        auto sec = IBR_Inst_Project.GetSectionFromID(id);
        if (sec.HasBack())
        {
            auto *back = sec.GetBack_Unsafe();
            if (back)
            {
                // 读取 Dynamic.Selected（对应 ImGui 每帧重读 sec.Dynamic.Selected）
                // workspace 框选/全选通过 ExtendMassSelect 改 Dynamic.Selected，
                // rebuild 时需读取以保持列表与工作区选中状态一致
                e.selected = back->Dynamic.Selected;
                auto regName = IBB_Inst_StrPool.GetCStr(back->Register);
                if (regName && *regName)
                {
                    e.registerType = QString::fromUtf8(regName);
                    if (IBB_DefaultRegType::HasRegType(regName))
                    {
                        auto &rt = IBB_DefaultRegType::GetRegType(regName);
                        const auto &fc = rt.FrameColor.Value;
                        e.registerColor = QColor::fromRgbF(fc[0], fc[1], fc[2], fc[3]);
                    }
                }
            }
        }
        if (e.selected)
            m_selectedRows.push_back(m_entries.size());
        m_entries.push_back(std::move(e));
    }
    // 应用排序和筛选（对应 IBR_ListView.cpp:317-453）
    applySortAndFilter();
    endResetModel();
    // rebuild 会清空 m_selectedRows，需刷新选中统计量并发 selectedCountChanged
    refreshSelectionStats();
}

// ===== 阶段 4 新增：排序/筛选/选择方法实现 =====

void SectionListModel::setSortKey(int key)
{
    if (m_sortKey == key) return;
    m_sortKey = key;
    emit sortKeyChanged();
    beginResetModel();
    applySortAndFilter();
    endResetModel();
}

void SectionListModel::setSortReverse(bool rev)
{
    if (m_sortReverse == rev) return;
    m_sortReverse = rev;
    emit sortReverseChanged();
    beginResetModel();
    applySortAndFilter();
    endResetModel();
}

void SectionListModel::setFilterText(const QString &text)
{
    if (m_filterText == text) return;
    m_filterText = text;
    emit filterTextChanged();
    beginResetModel();
    applySortAndFilter();
    endResetModel();
}

void SectionListModel::setFilterByRegistry(bool byReg)
{
    if (m_filterByRegistry == byReg) return;
    m_filterByRegistry = byReg;
    emit filterByRegistryChanged();
    beginResetModel();
    applySortAndFilter();
    endResetModel();
}

// ===== 阶段 9.1 新增：筛选模式 setter（对应 IBR_ListView.cpp:188, 191, 193 三个 Checkbox） =====

void SectionListModel::setFilterFull(bool v)
{
    if (m_filterFull == v) return;
    m_filterFull = v;
    emit filterFullChanged();
    beginResetModel();
    applySortAndFilter();
    endResetModel();
}

void SectionListModel::setFilterCaseSensitive(bool v)
{
    if (m_filterCaseSensitive == v) return;
    m_filterCaseSensitive = v;
    emit filterCaseSensitiveChanged();
    beginResetModel();
    applySortAndFilter();
    endResetModel();
}

void SectionListModel::setFilterRegex(bool v)
{
    if (m_filterRegex == v) return;
    m_filterRegex = v;
    emit filterRegexChanged();
    beginResetModel();
    applySortAndFilter();
    endResetModel();
}

void SectionListModel::setShowRegName(bool v)
{
    if (m_showRegName == v) return;
    m_showRegName = v;
    emit showRegNameChanged();
    // 显示模式切换不改变筛选/排序结果，只更新所有行的 DisplayNameRole
    if (!m_entries.empty())
    {
        QModelIndex first = index(0);
        QModelIndex last = index(static_cast<int>(m_entries.size()) - 1);
        emit dataChanged(first, last, {DisplayNameRole});
    }
}

// ===== 阶段 9.1 新增：批量操作 Q_INVOKABLE 实现（对应 IBR_ListView.cpp:136-137, 144-150） =====

void SectionListModel::duplicate()
{
    // 对应 IBR_ListView.cpp:137, 144 Duplicate 按钮 → IBR_WorkSpace::DuplicateSelected
    // 先 MassSelect 当前选中项再调用 DuplicateSelected
    std::vector<ModuleID_t> ids;
    ids.reserve(m_selectedRows.size());
    for (size_t r : m_selectedRows)
    {
        if (r < m_entries.size())
            ids.push_back(m_entries[r].sectionId);
    }
    if (ids.empty()) return;
    IBR_WorkSpace::MassSelect(ids);
    IBR_WorkSpace::DuplicateSelected();
    rebuild();
}

void SectionListModel::freezeAll(bool frozen)
{
    // 对应 IBR_ListView.cpp:146-147 智能切换 → 批量 Freeze/Unfreeze 选中项
    // frozen=true 表示冻结所有选中，frozen=false 表示解冻所有选中
    auto &sd = IBR_Inst_Project.IBR_SectionMap;
    for (size_t r : m_selectedRows)
    {
        if (r >= m_entries.size()) continue;
        auto it = sd.find(m_entries[r].sectionId);
        if (it != sd.end())
        {
            it->second.Frozen = frozen;
            m_entries[r].frozen = frozen;
            QModelIndex idx = index(static_cast<int>(r));
            emit dataChanged(idx, idx, {FrozenRole});
        }
    }
    refreshSelectionStats();
}

void SectionListModel::hideAll(bool hidden)
{
    // 对应 IBR_ListView.cpp:149-150 智能切换 → 批量 Hide/Show 选中项
    // hidden=true 表示隐藏所有选中，hidden=false 表示显示所有选中
    auto &sd = IBR_Inst_Project.IBR_SectionMap;
    for (size_t r : m_selectedRows)
    {
        if (r >= m_entries.size()) continue;
        auto it = sd.find(m_entries[r].sectionId);
        if (it != sd.end())
        {
            it->second.Hidden = hidden;
            m_entries[r].hidden = hidden;
            // 对应 IBR_ListView.cpp:268-270 Show 时置 UpdatePosByEq = true
            if (!hidden)
                it->second.UpdatePosByEq = true;
            QModelIndex idx = index(static_cast<int>(r));
            emit dataChanged(idx, idx, {HiddenRole});
        }
    }
    refreshSelectionStats();
}

void SectionListModel::applySortAndFilter()
{
    // 筛选（对应 IBR_ListView.cpp:26-58 StringMatch + 276-279）
    // 阶段 9.1：改用 matchFilter 支持 Full × CaseSensitive × Regex 共 8 种组合
    if (!m_filterText.isEmpty())
    {
        m_entries.erase(
            std::remove_if(m_entries.begin(), m_entries.end(),
                [&](const Entry &e) {
                    // 选择搜索字段（对应 Search_ByRegistry）
                    QString field = m_filterByRegistry ? e.secName : e.displayName;
                    return !matchFilter(field);
                }),
            m_entries.end());
    }

    // 排序（对应 IBR_ListView.cpp:346-378）
    QCollator collator;
    collator.setCaseSensitivity(Qt::CaseInsensitive);
    collator.setLocale(QLocale::Chinese);  // 对应 StrCmpZHCN 的 zh-CN.UTF-8 locale

    auto compare = [&](const Entry &a, const Entry &b) -> bool {
        switch (m_sortKey)
        {
        case 1:  // RegName：按 secName 字典序
            return collator.compare(a.secName, b.secName) < 0;
        case 2:  // DisplayName：按 displayName 中文排序
            return collator.compare(a.displayName, b.displayName) < 0;
        case 3:  // RegType：按 registerType 字典序
            return collator.compare(a.registerType, b.registerType) < 0;
        case 0:  // Default：先按 RegType 分组，组内按 DisplayName 排序
        default:
        {
            int typeCmp = collator.compare(a.registerType, b.registerType);
            if (typeCmp != 0) return typeCmp < 0;
            return collator.compare(a.displayName, b.displayName) < 0;
        }
        }
    };

    std::sort(m_entries.begin(), m_entries.end(), compare);
    if (m_sortReverse)
        std::reverse(m_entries.begin(), m_entries.end());

    // 阶段 9.1：筛选/排序后刷新选中统计量
    refreshSelectionStats();
}

// 阶段 9.1 新增：判断单条 Entry 是否匹配当前筛选条件
// 对应 IBR_ListView.cpp:26-58 StringMatch 函数，支持 Full × CaseSensitive × Regex 共 8 种组合
bool SectionListModel::matchFilter(const QString &field) const
{
    const QString &filter = m_filterText;
    if (filter.isEmpty()) return true;

    // Regex 模式（对应 IBR_ListView.cpp:38-52 RegexFull_Nothrow / RegexNotNone_Nothrow）
    if (m_filterRegex)
    {
        QRegularExpression::PatternOptions opts = QRegularExpression::NoPatternOption;
        if (!m_filterCaseSensitive)
            opts |= QRegularExpression::CaseInsensitiveOption;
        QRegularExpression re(filter, opts);
        if (!re.isValid()) return false;
        auto m = re.match(field);
        if (m_filterFull)
        {
            // Full + Regex：整个字符串必须完全匹配（对应 RegexFull_Nothrow）
            return m.hasMatch() && m.capturedStart(0) == 0 && m.capturedLength(0) == field.length();
        }
        else
        {
            // 非 Full + Regex：部分匹配即可（对应 RegexNotNone_Nothrow）
            return m.hasMatch();
        }
    }

    // 非 Regex 模式
    Qt::CaseSensitivity cs = m_filterCaseSensitive ? Qt::CaseSensitive : Qt::CaseInsensitive;
    if (m_filterFull)
    {
        // Full + 非 Regex：字符串完全相等（对应 IBR_ListView.cpp:43 str == match）
        return field.compare(filter, cs) == 0;
    }
    else
    {
        // 非 Full + 非 Regex：子串匹配（对应 IBR_ListView.cpp:46 str.find(match) != npos）
        return field.contains(filter, cs);
    }
}

// 阶段 9.1 新增：刷新选中统计量并发信号（对应 IBR_ListView.cpp:234-235, 272-273）
void SectionListModel::refreshSelectionStats()
{
    int frozenN = 0, hiddenN = 0;
    int selN = static_cast<int>(m_selectedRows.size());
    for (const auto &e : m_entries)
    {
        if (e.selected)
        {
            if (e.frozen) ++frozenN;
            if (e.hidden) ++hiddenN;
        }
    }
    if (m_selAndFrozenN != frozenN)
    {
        m_selAndFrozenN = frozenN;
        emit selAndFrozenNChanged();
    }
    if (m_selAndHiddenN != hiddenN)
    {
        m_selAndHiddenN = hiddenN;
        emit selAndHiddenNChanged();
    }
    // 选中数变化时 emit selectedCountChanged 让 ListPanel 标题栏刷新
    if (m_lastSelectedCount != selN)
    {
        m_lastSelectedCount = selN;
        emit selectedCountChanged();
    }
}

void SectionListModel::selectAll()
{
    // 对应 IBR_ListView.cpp:127-132 SelectAll
    for (size_t i = 0; i < m_entries.size(); ++i)
    {
        if (!m_entries[i].selected)
        {
            m_entries[i].selected = true;
            QModelIndex idx = index(static_cast<int>(i));
            emit dataChanged(idx, idx, {SelectedRole});
        }
    }
    // 重建选中行列表
    m_selectedRows.clear();
    for (size_t i = 0; i < m_entries.size(); ++i)
    {
        if (m_entries[i].selected)
            m_selectedRows.push_back(i);
    }
    // 同步到工作区
    std::vector<ModuleID_t> ids;
    ids.reserve(m_entries.size());
    for (const auto &e : m_entries)
        ids.push_back(e.sectionId);
    IBR_WorkSpace::MassSelect(ids);
    // 阶段 9.1：刷新统计量
    refreshSelectionStats();
}

void SectionListModel::selectInvert()
{
    // 对应 IBR_ListView.cpp:127-132 SelectInvert
    m_selectedRows.clear();
    for (size_t i = 0; i < m_entries.size(); ++i)
    {
        m_entries[i].selected = !m_entries[i].selected;
        QModelIndex idx = index(static_cast<int>(i));
        emit dataChanged(idx, idx, {SelectedRole});
        if (m_entries[i].selected)
            m_selectedRows.push_back(i);
    }
    // 同步到工作区
    std::vector<ModuleID_t> ids;
    for (const auto &e : m_entries)
    {
        if (e.selected) ids.push_back(e.sectionId);
    }
    IBR_WorkSpace::MassSelect(ids);
    // 阶段 9.1：刷新统计量
    refreshSelectionStats();
}

QVariantList SectionListModel::selectedSectionIds() const
{
    QVariantList ids;
    for (const auto &e : m_entries)
    {
        if (e.selected)
            ids.append(static_cast<qulonglong>(e.sectionId));
    }
    return ids;
}

void SectionListModel::jumpToSection(int row)
{
    // 对应 IBR_ListView.cpp:303-307 ArrowButton 跳转
    if (row < 0 || row >= static_cast<int>(m_entries.size())) return;
    const auto &e = m_entries[row];
    // 居中到该 Section 并进入编辑模式
    emit jumpRequested(e.eqX + e.eqW / 2.0f, e.eqY + e.eqH / 2.0f, static_cast<qulonglong>(e.sectionId));
}
