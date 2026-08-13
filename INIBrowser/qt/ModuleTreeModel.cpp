// ModuleTreeModel.cpp
// Qt6 模块树模型实现
#include "ModuleTreeModel.h"
#include "IBB_ModuleAlt.h"
#include "IBR_Project.h"
#include "FromEngine/global_tool_func.h"
#include "Global.h"

ModuleTreeModel::ModuleTreeModel(QObject *parent)
    : QAbstractListModel(parent)
{
    // 延迟到 loadComplete 后再 rebuild，避免业务层未初始化
}

int ModuleTreeModel::rowCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent)
    return static_cast<int>(m_nodes.size());
}

QVariant ModuleTreeModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= static_cast<int>(m_nodes.size()))
        return {};
    const auto &node = m_nodes[index.row()];
    switch (role)
    {
    case NameRole:      return node.name;
    case DescLongRole:  return node.descLong;
    case DepthRole:     return node.depth;
    case IsFolderRole:  return node.isFolder;
    case ExpandedRole:  return node.expanded;
    case HasChildrenRole: return node.childCount > 0;
    case ModuleKeyRole: return node.moduleKey;
    case ChildCountRole: return node.childCount;
    }
    return {};
}

QHash<int, QByteArray> ModuleTreeModel::roleNames() const
{
    return {
        {NameRole,       "name"},
        {DescLongRole,   "descLong"},
        {DepthRole,      "depth"},
        {IsFolderRole,   "isFolder"},
        {ExpandedRole,   "expanded"},
        {HasChildrenRole,"hasChildren"},
        {ModuleKeyRole,  "moduleKey"},
        {ChildCountRole, "childCount"},
    };
}

void ModuleTreeModel::setIncludeSpecial(bool v)
{
    if (m_includeSpecial == v) return;
    m_includeSpecial = v;
    emit includeSpecialChanged();
    rebuild();
}

// 阶段 6.1：设置搜索过滤（对应 SearchModuleAlt::RenderUI）
void ModuleTreeModel::setFilter(const QString &v)
{
    if (m_filter == v) return;
    m_filter = v;
    emit filterChanged();
    rebuild();
}

void ModuleTreeModel::toggleExpanded(int row)
{
    if (row < 0 || row >= static_cast<int>(m_nodes.size())) return;
    if (!m_nodes[row].isFolder) return;
    m_nodes[row].expanded = !m_nodes[row].expanded;
    QModelIndex idx = index(row);
    emit dataChanged(idx, idx, {ExpandedRole});
    rebuild();
}

void ModuleTreeModel::expandAll()
{
    for (auto &n : m_nodes) if (n.isFolder) n.expanded = true;
    rebuild();
}

void ModuleTreeModel::collapseAll()
{
    for (auto &n : m_nodes) if (n.isFolder) n.expanded = false;
    rebuild();
}

void ModuleTreeModel::refresh()
{
    rebuild();
}

void ModuleTreeModel::rebuild()
{
    bool wasEmpty = m_nodes.empty();
    beginResetModel();
    m_nodes.clear();
    if (m_includeSpecial)
    {
        traverse(&IBB_ModuleAltDefault::GetSpecialModulesTree(), 0);
    }
    traverse(&IBB_ModuleAltDefault::GetAllModulesTree(), 0);
    // 阶段 6.1：应用搜索过滤，移除不匹配的模块节点和无可见子节点的目录
    if (!m_filter.isEmpty())
    {
        applyFilter();
    }
    endResetModel();
    if (wasEmpty != m_nodes.empty())
        emit isEmptyChanged();
}

// 阶段 6.1：过滤逻辑（对应 SearchModuleAlt::RenderUI 的搜索过滤）
// 保留匹配 filter 的模块节点 + 其祖先目录路径，移除其余节点
void ModuleTreeModel::applyFilter()
{
    if (m_nodes.empty()) return;

    std::vector<bool> keep(m_nodes.size(), false);

    // 第一遍：标记匹配 filter 的模块节点（对应 IBB_ModuleAltDefault::Search 的匹配逻辑）
    // 检查字段：name(DescShort/Name)、moduleKey(Name)、descLong(DescLong)
    // 对应 ImGui 版本 ConsiderDescName + ConsiderDesc + ConsiderRegName 三个开关全开
    for (size_t i = 0; i < m_nodes.size(); ++i)
    {
        if (!m_nodes[i].isFolder)
        {
            if (m_nodes[i].name.contains(m_filter, Qt::CaseInsensitive) ||
                m_nodes[i].moduleKey.contains(m_filter, Qt::CaseInsensitive) ||
                m_nodes[i].descLong.contains(m_filter, Qt::CaseInsensitive))
            {
                keep[i] = true;
            }
        }
    }

    // 第二遍：对每个保留的模块节点，向上标记祖先目录
    // 目录节点 depth = 模块 depth - 1（见 traverse: n.depth = depth - 1 for folders）
    for (size_t i = 0; i < m_nodes.size(); ++i)
    {
        if (!keep[i] || m_nodes[i].isFolder) continue;
        int targetDepth = m_nodes[i].depth - 1;
        for (int j = static_cast<int>(i) - 1; j >= 0 && targetDepth >= 0; --j)
        {
            if (m_nodes[j].isFolder && m_nodes[j].depth == targetDepth)
            {
                keep[j] = true;
                targetDepth--;
            }
        }
    }

    // 第三遍：移除未标记的节点
    std::vector<Node> filtered;
    filtered.reserve(m_nodes.size());
    for (size_t i = 0; i < m_nodes.size(); ++i)
    {
        if (keep[i])
        {
            filtered.push_back(std::move(m_nodes[i]));
        }
    }
    m_nodes = std::move(filtered);
}

void ModuleTreeModel::traverse(const void *treePtr, int depth)
{
    using ModuleTree = IBB_ModuleAltDefault::ModuleTree;
    const auto *tree = static_cast<const ModuleTree *>(treePtr);

    bool hasContent = !tree->Sub.empty() || !tree->Modules.empty();
    if (!hasContent) return;

    // 计算直接子节点数
    int childCount = static_cast<int>(tree->Sub.size() + tree->Modules.size());

    // 当前目录节点本身（非根）
    if (depth > 0)
    {
        Node n;
        n.name = QString::fromUtf8(tree->Name);
        n.depth = depth - 1;
        n.isFolder = true;
        n.expanded = true;
        n.childCount = childCount;
        m_nodes.push_back(std::move(n));
    }

    // 折叠则不遍历子节点
    bool expanded = (depth == 0) ? true : m_nodes.back().expanded;
    if (!expanded) return;

    // 先 Sub（子目录）
    for (const auto &sub : tree->Sub)
    {
        traverse(sub.get(), depth + 1);
    }
    // 后 ModuleOrder（模块）
    for (const auto &name : tree->ModuleOrder)
    {
        auto it = tree->Modules.find(name);
        if (it == tree->Modules.end() || !it->second) continue;
        const auto *mod = it->second;

        Node n;
        n.name = QString::fromUtf8(mod->DescShort.empty() ? mod->Name : mod->DescShort);
        n.descLong = QString::fromUtf8(mod->DescLong);
        n.moduleKey = QString::fromUtf8(mod->Name);
        n.depth = depth;
        n.isFolder = false;
        n.expanded = false;
        n.childCount = 0;
        n.modulePtr = const_cast<IBB_ModuleAlt*>(mod);
        m_nodes.push_back(std::move(n));
    }
}

QString ModuleTreeModel::search(const QString &text)
{
    // 简单实现：返回第一个匹配的 moduleKey（后续可扩展为过滤模型）
    if (text.isEmpty()) return {};
    auto target = text.toStdString();
    for (const auto &node : m_nodes)
    {
        if (node.isFolder) continue;
        if (node.name.contains(text, Qt::CaseInsensitive) ||
            node.moduleKey.contains(text, Qt::CaseInsensitive))
        {
            return node.moduleKey;
        }
    }
    return {};
}

void ModuleTreeModel::addModule(int row)
{
    if (row < 0 || row >= static_cast<int>(m_nodes.size())) return;
    const auto &node = m_nodes[row];
    if (node.isFolder || !node.modulePtr) return;

    // 对应 Tree_RenderUISidebar: mod->FullyLoad(); IBR_Inst_Project.AddModule(*mod, GenerateModuleTag());
    // 通过 SendToR 切到渲染线程执行（AddModule 标注 _PROJ_CMD_WRITE _PROJ_CMD_UPDATE）
    auto *mod = static_cast<IBB_ModuleAlt*>(node.modulePtr);
    IBRF_CoreBump.SendToR({ [mod]() {
        mod->FullyLoad();
        IBR_Inst_Project.AddModule(*mod, GenerateModuleTag());
    } });
}

void ModuleTreeModel::addModuleByKey(const QString &key)
{
    if (key.isEmpty()) return;
    for (int i = 0; i < static_cast<int>(m_nodes.size()); ++i)
    {
        if (!m_nodes[i].isFolder && m_nodes[i].moduleKey == key)
        {
            addModule(i);
            return;
        }
    }
}

// 阶段 13.2：SearchModuleAlt 搜索（对应 IBB_ModuleAltDefault::Search）
QVariantList ModuleTreeModel::searchWithConsider(const QString &text,
                                                   bool considerRegName,
                                                   bool considerDescName,
                                                   bool considerDesc) const
{
    QVariantList results;
    if (text.isEmpty()) return results;

    auto results_raw = IBB_ModuleAltDefault::Search(
        text.toStdString(), considerRegName, considerDescName, considerDesc);

    for (const auto *mod : results_raw)
    {
        if (!mod) continue;
        QVariantMap item;
        item["moduleKey"] = QString::fromUtf8(mod->Name);
        item["name"] = QString::fromUtf8(mod->DescShort.empty() ? mod->Name : mod->DescShort);
        item["descLong"] = QString::fromUtf8(mod->DescLong);
        results.append(item);
    }
    return results;
}
