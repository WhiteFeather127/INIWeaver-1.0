// ModuleTreeModel.h
// Qt6 模块树模型：扁平化列表 + 可展开/折叠
// 阶段 3.2：复用 IBB_ModuleAltDefault 业务层数据，提供 QML 访问接口
// 遍历规则：先 Sub（子目录），后 ModuleOrder（模块），按 DescShort 中文排序
#pragma once

#include <QAbstractListModel>
#include <QString>
#include <QHash>
#include <QVariant>
#include <QtQmlIntegration/qqmlintegration.h>
#include <vector>
#include <memory>

class ModuleTreeModel : public QAbstractListModel
{
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(bool includeSpecial READ includeSpecial WRITE setIncludeSpecial NOTIFY includeSpecialChanged)
    Q_PROPERTY(bool isEmpty READ isEmpty NOTIFY isEmptyChanged)
    // 阶段 6.1：搜索过滤（对应 SearchModuleAlt::RenderUI）
    Q_PROPERTY(QString filter READ filter WRITE setFilter NOTIFY filterChanged)

public:
    enum Roles {
        NameRole = Qt::UserRole + 1,       // 节点显示名（目录名或模块 DescShort）
        DescLongRole,                       // 模块长描述（目录为空）
        DepthRole,                          // 树深度（根=0）
        IsFolderRole,                       // 是否目录节点
        ExpandedRole,                       // 是否展开（仅目录有效）
        HasChildrenRole,                    // 是否有子节点
        ModuleKeyRole,                      // 模块唯一标识（目录为空）
        ChildCountRole                      // 直接子节点数
    };
    Q_ENUM(Roles)

    explicit ModuleTreeModel(QObject *parent = nullptr);

    // QAbstractListModel
    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    // 属性
    bool includeSpecial() const { return m_includeSpecial; }
    void setIncludeSpecial(bool v);
    bool isEmpty() const { return m_nodes.empty(); }
    // 阶段 6.1：搜索过滤
    QString filter() const { return m_filter; }
    void setFilter(const QString &v);

    // QML 调用
    Q_INVOKABLE void toggleExpanded(int row);
    Q_INVOKABLE void expandAll();
    Q_INVOKABLE void collapseAll();
    Q_INVOKABLE void refresh();
    Q_INVOKABLE QString search(const QString &text);
    // 添加模块到工作区（对应 Tree_RenderUISidebar 的 IsItemClicked 逻辑）
    // 异步执行：通过 IBRF_CoreBump.SendToR 在渲染线程调用 AddModule
    Q_INVOKABLE void addModule(int row);
    // 通过 moduleKey 添加模块（供 DropArea.onDropped 调用）
    Q_INVOKABLE void addModuleByKey(const QString &key);

    // 阶段 13.2：SearchModuleAlt 搜索（对应 IBB_ModuleAltDefault::Search）
    // 返回匹配模块列表（每项含 moduleKey/name/descLong）
    Q_INVOKABLE QVariantList searchWithConsider(const QString &text,
                                                 bool considerRegName,
                                                 bool considerDescName,
                                                 bool considerDesc) const;

    // 右键菜单级联树：按路径取某文件夹的直接子项（对应 ImGui RenderModuleTreeUI 逐层渲染）
    // 直接遍历 IBB_ModuleAltDefault 原始树，不依赖 m_nodes/展开状态，不与左侧面板互相干扰
    // folderPath 编码：""=根层；"S:"前缀=Special 树；"A:"前缀=All 树；嵌套用 "/" 连接
    // 返回项字段：{path, name, isFolder, hasChildren, moduleKey, descLong}
    Q_INVOKABLE QVariantList levelChildren(const QString &folderPath) const;

signals:
    void includeSpecialChanged();
    void isEmptyChanged();
    void filterChanged();

private:
    struct Node {
        QString name;          // 显示名
        QString descLong;      // 长描述
        QString moduleKey;     // 模块标识（目录为空）
        QString path;          // 文件夹唯一路径（"S:车辆/发动机" / "A:..."），模块节点为空
        int depth{0};
        bool isFolder{false};
        bool expanded{false};   // 默认折叠（需求：侧边栏文件夹默认全部折叠；用户可手动展开）
        int childCount{0};
        void *modulePtr{nullptr};  // IBB_ModuleAlt* 原始指针（非拥有，仅模块节点有效）
    };

    std::vector<Node> m_nodes;     // 扁平化列表（仅可见节点）
    bool m_includeSpecial{true};
    QString m_filter;              // 阶段 6.1：搜索过滤文本
    // 文件夹展开状态（path -> expanded），rebuild 时据此恢复，避免每次重建都重置为展开
    QHash<QString, bool> m_expandedState;

    void rebuild();
    // 通过 void* 避免头文件依赖 IBB_ModuleAlt.h（实现在 .cpp 中转换）
    // prefix 为树路径前缀（"S:" / "A:"），用于生成文件夹唯一路径
    void traverse(const void *treePtr, int depth, const QString &prefix);
    // 阶段 6.1：应用搜索过滤（保留匹配模块 + 祖先目录）
    void applyFilter();
};
