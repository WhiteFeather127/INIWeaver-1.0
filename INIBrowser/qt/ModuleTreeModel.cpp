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
    // 持久化展开状态，rebuild 后仍能恢复（否则 traverse 会重置为展开，收起失效）
    if (!m_nodes[row].path.isEmpty())
        m_expandedState[m_nodes[row].path] = m_nodes[row].expanded;
    rebuild();
}

void ModuleTreeModel::expandAll()
{
    for (auto &n : m_nodes)
        if (n.isFolder) { n.expanded = true; if (!n.path.isEmpty()) m_expandedState[n.path] = true; }
    rebuild();
}

void ModuleTreeModel::collapseAll()
{
    for (auto &n : m_nodes)
        if (n.isFolder) { n.expanded = false; if (!n.path.isEmpty()) m_expandedState[n.path] = false; }
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
        traverse(&IBB_ModuleAltDefault::GetSpecialModulesTree(), 0, QStringLiteral("S:"));
    }
    traverse(&IBB_ModuleAltDefault::GetAllModulesTree(), 0, QStringLiteral("A:"));
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

void ModuleTreeModel::traverse(const void *treePtr, int depth, const QString &prefix)
{
    using ModuleTree = IBB_ModuleAltDefault::ModuleTree;
    const auto *tree = static_cast<const ModuleTree *>(treePtr);

    bool hasContent = !tree->Sub.empty() || !tree->Modules.empty();
    if (!hasContent) return;

    // 计算直接子节点数
    int childCount = static_cast<int>(tree->Sub.size() + tree->Modules.size());

    // 当前目录节点本身（非根）
    QString myPath;
    if (depth > 0)
    {
        myPath = prefix.endsWith(u':') ? prefix + QString::fromUtf8(tree->Name)
                                       : prefix + u'/' + QString::fromUtf8(tree->Name);
        Node n;
        n.name = QString::fromUtf8(tree->Name);
        n.path = myPath;
        n.depth = depth - 1;
        n.isFolder = true;
        // 从持久化状态恢复展开/收起（默认折叠，需求：侧边栏文件夹默认全部折叠）。
        // 未记录过的文件夹默认折叠；用户手动展开过的（toggleExpanded 写入 m_expandedState）保持展开
        n.expanded = m_expandedState.value(myPath, false);
        n.childCount = childCount;
        m_nodes.push_back(std::move(n));
    }

    // 折叠则不遍历子节点
    bool expanded = (depth == 0) ? true : m_nodes.back().expanded;
    if (!expanded) return;

    // 先 Sub（子目录），路径前缀：当前节点无路径时用传入前缀，否则用当前节点路径
    const QString subPrefix = myPath.isEmpty() ? prefix : myPath;
    for (const auto &sub : tree->Sub)
    {
        traverse(sub.get(), depth + 1, subPrefix);
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

namespace {
// 递归在完整模块树中按 Name 查找模块。
// 右键菜单/拖放用 placeModuleByKey 走完整树：m_nodes 只含"当前可见且展开"的扁平节点，
// 嵌套在收起文件夹里的模块不在其中，故 m_nodes 无命中时需回到完整树（All/Special）递归查找。
IBB_ModuleAlt* FindModuleInTree(const IBB_ModuleAltDefault::ModuleTree& tree, const std::string& key)
{
    for (const auto& name : tree.ModuleOrder)
    {
        auto it = tree.Modules.find(name);
        if (it != tree.Modules.end() && it->second && it->second->Name == key)
            return it->second;
    }
    for (const auto& sub : tree.Sub)
    {
        if (auto* r = FindModuleInTree(*sub, key)) return r;
    }
    return nullptr;
}

// 把一条"添加"投递到渲染线程：useCenter=true 时先设 ImGui 鼠标到视图坐标，
// 使 AddModule(UseMouseCenter=true) 把模块中心落在 (viewX,viewY) 对应的 EqPos。
// Qt 端未同步 ImGui 鼠标，直接依靠 AddModule 的鼠标定位会得到未初始化的 (0,0)，
// 故必须在渲染线程内、调用 AddModule 前显式写 ImGui::GetIO().MousePos。
void PostAddModule(IBB_ModuleAlt* mod, qreal viewX, qreal viewY, bool useCenter)
{
    if (!mod) return;
    qreal x = viewX, y = viewY;
    IBRF_CoreBump.SendToR({ [mod, useCenter, x, y]() {
        if (useCenter)
        {
            ImGui::GetIO().MousePos.x = static_cast<float>(x);
            ImGui::GetIO().MousePos.y = static_cast<float>(y);
        }
        mod->FullyLoad();
        IBR_Inst_Project.AddModule(*mod, GenerateModuleTag(), useCenter);
    } });
}

// 在完整模块树（All + Special）中按 moduleKey 解析模块指针。
// 全树本就包含所有可见/嵌套模块，无需依赖 m_nodes 平铺缓存，避免私有 Node 类型的依赖。
IBB_ModuleAlt* ResolveModule(const QString& key)
{
    const auto k = key.toStdString();
    IBB_ModuleAlt* mod = FindModuleInTree(IBB_ModuleAltDefault::GetAllModulesTree(), k);
    if (!mod) mod = FindModuleInTree(IBB_ModuleAltDefault::GetSpecialModulesTree(), k);
    return mod;
}
}

void ModuleTreeModel::addModuleByKey(const QString &key)
{
    if (key.isEmpty()) return;
    auto* mod = ResolveModule(key);
    if (mod) PostAddModule(mod, 0.0, 0.0, false);
}

void ModuleTreeModel::placeModuleByKey(const QString &key, qreal viewX, qreal viewY)
{
    if (key.isEmpty()) return;
    // 在 (viewX, viewY) 视口坐标处放置（右键菜单/侧边栏拖放）
    auto* mod = ResolveModule(key);
    if (mod) PostAddModule(mod, viewX, viewY, true);
}

// 阶段 13.2：SearchModuleAlt 搜索（对应 IBB_ModuleAltDefault::Search）
QVariantList ModuleTreeModel::searchWithConsider(const QString &text,
                                                   bool considerRegName,
                                                   bool considerDescName,
                                                   bool considerDesc) const
{
    QVariantList results;
    // 空串不短路：底层 Search("") 的 find 恒匹配（IBB_ModuleAlt.cpp:833），返回所有模块；
    // 对应 ImGui SearchModuleAlt 空白时显示全部

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

// ===== 右键菜单级联树：按路径取某文件夹的直接子项 =====
namespace
{
    using ModuleTreeT = IBB_ModuleAltDefault::ModuleTree;

    // 收集 tree 的直接子项（子目录 + 模块）到 out；parentPath 为父文件夹完整路径（如 "S:" / "S:车辆"）
    void collectLevelItems(const ModuleTreeT &tree, const QString &parentPath, QVariantList &out)
    {
        for (const auto &sub : tree.Sub)
        {
            const auto nm = QString::fromUtf8(sub->Name);
            QVariantMap m;
            m["path"] = parentPath + "/" + nm;
            m["name"] = nm;
            m["isFolder"] = true;
            m["hasChildren"] = !sub->Sub.empty() || !sub->Modules.empty();
            m["moduleKey"] = QString();
            m["descLong"] = QString();
            out.append(m);
        }
        for (const auto &modName : tree.ModuleOrder)
        {
            auto it = tree.Modules.find(modName);
            if (it == tree.Modules.end() || !it->second) continue;
            const auto *mod = it->second;
            QVariantMap m;
            m["path"] = parentPath + "/" + QString::fromUtf8(mod->Name);
            m["name"] = QString::fromUtf8(mod->DescShort.empty() ? mod->Name : mod->DescShort);
            m["isFolder"] = false;
            m["hasChildren"] = false;
            m["moduleKey"] = QString::fromUtf8(mod->Name);
            m["descLong"] = QString::fromUtf8(mod->DescLong);
            out.append(m);
        }
    }

    // 从树根沿路径下钻（relPath 不含 "S:"/"A:" 前缀，如 "车辆/发动机"）
    const ModuleTreeT *drillTree(const ModuleTreeT &root, const QString &relPath)
    {
        if (relPath.isEmpty()) return &root;
        const auto parts = relPath.split('/', Qt::SkipEmptyParts);
        const ModuleTreeT *cur = &root;
        for (const auto &seg : parts)
        {
            const ModuleTreeT *next = nullptr;
            for (const auto &sub : cur->Sub)
                if (QString::fromUtf8(sub->Name) == seg) { next = sub.get(); break; }
            if (!next) return nullptr;
            cur = next;
        }
        return cur;
    }
}

QVariantList ModuleTreeModel::levelChildren(const QString &folderPath) const
{
    QVariantList out;
    if (folderPath.isEmpty())
    {
        // 根层：Special 树（S: 前缀）+ All 树（A: 前缀），对应 ImGui SpecialTree_RenderUI + Tree_RenderUI
        if (m_includeSpecial)
        {
            const auto &sp = IBB_ModuleAltDefault::GetSpecialModulesTree();
            collectLevelItems(sp, "S:", out);
        }
        const auto &all = IBB_ModuleAltDefault::GetAllModulesTree();
        collectLevelItems(all, "A:", out);
        return out;
    }

    const bool isSpecial = folderPath.startsWith("S:");
    const auto &tree = isSpecial ? IBB_ModuleAltDefault::GetSpecialModulesTree()
                                 : IBB_ModuleAltDefault::GetAllModulesTree();
    const auto *target = drillTree(tree, folderPath.mid(2));
    if (!target) return out;
    collectLevelItems(*target, folderPath, out);
    return out;
}
