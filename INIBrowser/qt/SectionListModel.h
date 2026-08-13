// SectionListModel.h
// Qt6 Section 列表模型：暴露 IBR_SectionMap 的渲染数据
// 阶段 3.3：复用 IBR_Inst_Project.IBR_SectionMap 业务层数据
// 提供 displayName/selected/frozen/hidden/ignored/registerType/registerColor 角色
#pragma once

#include <QAbstractListModel>
#include <QString>
#include <QColor>
#include <QtQmlIntegration/qqmlintegration.h>
#include <vector>

class SectionListModel : public QAbstractListModel
{
    Q_OBJECT
    QML_ELEMENT
    // 阶段 4 新增：排序属性（对应 IBR_ListView.cpp:161-178 SortBy）
    Q_PROPERTY(int sortKey READ sortKey WRITE setSortKey NOTIFY sortKeyChanged)
    Q_PROPERTY(bool sortReverse READ sortReverse WRITE setSortReverse NOTIFY sortReverseChanged)
    // 阶段 4 新增：筛选属性（对应 IBR_ListView.cpp:180-194）
    Q_PROPERTY(QString filterText READ filterText WRITE setFilterText NOTIFY filterTextChanged)
    Q_PROPERTY(bool filterByRegistry READ filterByRegistry WRITE setFilterByRegistry NOTIFY filterByRegistryChanged)
    // 阶段 9.1 新增：筛选模式属性（对应 IBR_ListView.cpp:188, 191, 193 三个 Checkbox）
    Q_PROPERTY(bool filterFull READ filterFull WRITE setFilterFull NOTIFY filterFullChanged)
    Q_PROPERTY(bool filterCaseSensitive READ filterCaseSensitive WRITE setFilterCaseSensitive NOTIFY filterCaseSensitiveChanged)
    Q_PROPERTY(bool filterRegex READ filterRegex WRITE setFilterRegex NOTIFY filterRegexChanged)
    // 阶段 9.1 新增：显示模式切换（对应 IBR_ListView.cpp:294 读取 IBR_WorkSpace::ShowRegName）
    Q_PROPERTY(bool showRegName READ showRegName WRITE setShowRegName NOTIFY showRegNameChanged)
    // 阶段 9.1 新增：选中项统计量（对应 IBR_ListView.cpp:234-235, 272-273 SelAndFrozenN/SelAndHiddenN）
    Q_PROPERTY(int selAndFrozenN READ selAndFrozenN NOTIFY selAndFrozenNChanged)
    Q_PROPERTY(int selAndHiddenN READ selAndHiddenN NOTIFY selAndHiddenNChanged)
    // 选中数属性化（让 QML binding 能追踪变化，ListPanel 标题栏 N/M 显示）
    Q_PROPERTY(int selectedCount READ selectedCount NOTIFY selectedCountChanged)

public:
    enum Roles {
        DisplayNameRole = Qt::UserRole + 1,
        SectionIdRole,           // ModuleID_t
        IniNameRole,             // 所属 INI 名
        SecNameRole,             // Section 名
        FrozenRole,
        HiddenRole,
        IgnoredRole,
        SelectedRole,
        RegisterTypeRole,        // 寄存器类型名
        RegisterColorRole,       // 寄存器类型颜色
        IsCommentRole,
        EqXRole,                 // 阶段 4 新增：工作区坐标（供双击跳转）
        EqYRole,
        EqWRole,
        EqHRole
    };
    Q_ENUM(Roles)

    explicit SectionListModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    bool setData(const QModelIndex &index, const QVariant &value, int role) override;
    QHash<int, QByteArray> roleNames() const override;

    // 排序/筛选属性访问器
    int sortKey() const { return m_sortKey; }
    void setSortKey(int key);
    bool sortReverse() const { return m_sortReverse; }
    void setSortReverse(bool rev);
    QString filterText() const { return m_filterText; }
    void setFilterText(const QString &text);
    bool filterByRegistry() const { return m_filterByRegistry; }
    void setFilterByRegistry(bool byReg);
    // 阶段 9.1 新增：筛选模式访问器
    bool filterFull() const { return m_filterFull; }
    void setFilterFull(bool v);
    bool filterCaseSensitive() const { return m_filterCaseSensitive; }
    void setFilterCaseSensitive(bool v);
    bool filterRegex() const { return m_filterRegex; }
    void setFilterRegex(bool v);
    bool showRegName() const { return m_showRegName; }
    void setShowRegName(bool v);
    int selAndFrozenN() const { return m_selAndFrozenN; }
    int selAndHiddenN() const { return m_selAndHiddenN; }

    // QML 调用
    Q_INVOKABLE void refresh();
    // QTimer 专用刷新：带脏标记检查，无变化时跳过 rebuild
    void refreshFromTimer();
    Q_INVOKABLE void select(int row, bool single = true);
    Q_INVOKABLE void freeze(int row, bool frozen);
    Q_INVOKABLE void hide(int row, bool hidden);
    Q_INVOKABLE void ignore(int row, bool ignored);
    Q_INVOKABLE void deleteSection(int row);
    Q_INVOKABLE void deleteSelected();
    Q_INVOKABLE void clearSelection();
    int selectedCount() const;
    // 阶段 4 新增：全选/反选（对应 IBR_ListView.cpp:127-132）
    Q_INVOKABLE void selectAll();
    Q_INVOKABLE void selectInvert();
    // 阶段 4 新增：获取选中项的 sectionId 列表（供跳转）
    Q_INVOKABLE QVariantList selectedSectionIds() const;
    // 阶段 4 新增：跳转到工作区位置（对应 IBR_ListView.cpp:303-307）
    Q_INVOKABLE void jumpToSection(int row);
    // 阶段 9.1 新增：批量操作（对应 IBR_ListView.cpp:136-137, 144-150）
    Q_INVOKABLE void duplicate();           // 调用 IBR_WorkSpace::DuplicateSelected
    Q_INVOKABLE void freezeAll(bool frozen); // 智能批量冻结/解冻
    Q_INVOKABLE void hideAll(bool hidden);   // 智能批量隐藏/显示

signals:
    void sortKeyChanged();
    void sortReverseChanged();
    void filterTextChanged();
    void filterByRegistryChanged();
    // 阶段 9.1 新增信号
    void filterFullChanged();
    void filterCaseSensitiveChanged();
    void filterRegexChanged();
    void showRegNameChanged();
    void selAndFrozenNChanged();
    void selAndHiddenNChanged();
    void selectedCountChanged();
    // 阶段 4 新增：请求工作区跳转
    void jumpRequested(qreal eqX, qreal eqY, qulonglong sectionId);

private:
    struct Entry {
        uint64_t sectionId{0};
        QString displayName;
        QString iniName;
        QString secName;
        bool frozen{false};
        bool hidden{false};
        bool ignored{false};
        bool selected{false};
        QString registerType;
        QColor registerColor;
        bool isComment{false};
        // 阶段 4 新增：工作区坐标
        float eqX{0}, eqY{0}, eqW{0}, eqH{0};
    };

    std::vector<Entry> m_entries;
    std::vector<size_t> m_selectedRows;

    // 排序/筛选状态
    int m_sortKey{0};        // 0=Default, 1=RegName, 2=DisplayName, 3=RegType
    bool m_sortReverse{false};
    QString m_filterText;
    bool m_filterByRegistry{false};
    // 阶段 9.1 新增：筛选模式状态
    bool m_filterFull{false};        // 全字匹配
    bool m_filterCaseSensitive{false}; // 大小写敏感
    bool m_filterRegex{false};       // 正则匹配
    bool m_showRegName{false};       // 显示寄存器名而非显示名
    // 阶段 9.1 新增：选中项统计量
    int m_selAndFrozenN{0};
    int m_selAndHiddenN{0};
    int m_lastSelectedCount{0};  // 用于 selectedCountChanged 信号去重

    void rebuild();
    void applySortAndFilter();
    // 阶段 9.1 新增：刷新统计量并发信号
    void refreshSelectionStats();
    // 阶段 9.1 新增：判断单条 Entry 是否匹配当前筛选条件（Full × CaseSensitive × Regex 共 8 种组合）
    bool matchFilter(const QString &field) const;

    // 性能优化：脏标记，避免 QTimer 每 50ms 全量 rebuild
    bool m_dirty{true};
    size_t m_lastSectionCount{0};
};
