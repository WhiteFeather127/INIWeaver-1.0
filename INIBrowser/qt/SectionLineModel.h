// SectionLineModel.h
// 单 Section 的行列表模型，对应 ImGui IBR_SectionData::RenderUI_Lines
// 遍历 Bsec->SubSecOrder → Bsec->LineOrder，按 SubSec 分组扁平化
// 每行携带 ImGui 渲染所需的全部字段（OnShow/LinkNode/Empty/Links/SessionID 等）
// QML 端通过 sectionData.lineModel 访问，delegate 用 LineRow.qml
#pragma once

#include <QAbstractListModel>
#include <QColor>
#include <QHash>
#include <QPointF>
#include <QVariantList>
#include <QtQmlIntegration/qqmlintegration.h>
#include "IBR_Project.h"  // ModuleID_t, StrPoolID

class WorkspaceController;

class SectionLineModel : public QAbstractListModel
{
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(qulonglong sectionId READ sectionId WRITE setSectionId NOTIFY sectionIdChanged)
    Q_PROPERTY(bool showRegName READ showRegName WRITE setShowRegName NOTIFY showRegNameChanged)

public:
    // 角色定义（对应 ImGui RenderUI_Line / RenderUI_Node 所需的全部字段）
    enum Roles {
        LineKeyRole = Qt::UserRole + 1,  // QString: key name（PoolStr）
        OnShowRole,                      // QString: 显示文本（ShowRegName ? key : OnShow 描述）
        DescLongRole,                    // QString: 长描述（PoolDesc(DescLong)）
        SubSecNameRole,                  // QString: SubSec 名称
        SubSecTypeRole,                  // int: 0=Default, 1=Inherit, 2=Import
        HasLinkNodeRole,                 // bool: HasRegType(LinkNode.LinkType)
        LinkColRole,                     // QColor: AdjustNodeCol 后的节点颜色
        LinkLimitRole,                   // int: LinkNode.LinkLimit
        LinkTypeRole,                    // QString: LinkType name
        IsEmptyRole,                     // bool: 无连线（GetLink begin==end）
        IsInheritRole,                   // bool: SubSec.Type == Inherit
        IsImportRole,                    // bool: SubSec.Type == Import
        LineIdxRole,                     // int: 行在 LineOrder 中的索引
        LineMultRole,                    // int: 分量索引
        CompIdxRole,                     // int: 组件索引
        SessionIdRole,                   // qulonglong: SessionID
        LinksRole,                       // QVariantList: 当前连线列表
        IsCollapsedRole,                     // bool: 行是否折叠到边缘（IsOnShow）
        ExportValueRole,                     // QString: FinalExportString（Input 态用）
        IsInputModeRole,                     // bool: IICStatus 持久化（true=Input 态，false=Link 态）
        IsMultipleRole,                      // bool: InputType.Multiple（可增行，对应 ImGui "+" 按钮）
        SpecialAcceptRole,                   // bool: SpecialAccept 临时态（行右键菜单切换）
        InputOnShowRole,                     // bool: InputOnShow 编辑态（行右键菜单 EditDesc 切换）
        KeyTypeRole,                         // int: 键输入类型（0=String, 1=Bool, 2=IIF）
        BoolCheckedRole,                 // bool: Bool 键当前布尔值（仅 keyType==1 有意义）
        IsAcceptorRole,                      // bool: 键是否为 accept 目标（InputType 带 AcceptType），左侧方形接收点
        AcceptorColorRole,               // QColor: acceptor 节点颜色（AcceptType.NodeColor）
    };
    Q_ENUM(Roles)

    explicit SectionLineModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    qulonglong sectionId() const { return m_sectionId; }
    void setSectionId(qulonglong id);

    bool showRegName() const { return m_showRegName; }
    void setShowRegName(bool v);

    // 重新遍历后端，重建 m_entries（对应 ImGui 每帧 RenderUI_Lines）
    Q_INVOKABLE void refresh();

    // 注入所属 WorkspaceController（链接修改后同步触发工作区全量刷新，消除队列延迟）
    void setWorkspaceController(WorkspaceController *wc) { m_workspace = wc; }

    // QML layout 后回写 RadioButton 屏幕坐标到 IBR_NodeSession::SessionValue.LastCenter
    // 对应 ImGui 立即模式下每帧重算 DefaultCenter
    Q_INVOKABLE void setLinkNodeCenter(int row, qreal x, qreal y);
    // IIF 分量节点坐标回写：compIdx 定位分量，sessionId 按 compIdx 重算（对应 UpdateAll 用 Comp=cidx）
    // 写回后连线端点表经 priority 2（sv.LastCenter）解析到正确分量圆点
    Q_INVOKABLE void setLinkNodeCenterAt(int row, int compIdx, qreal x, qreal y);
    // IIF 分量节点坐标回写（按 keyName 稳定定位）：row 索引在 rebuildEntries 重建后可能错位，
    // 用 keyName 查 lineIdx（稳定）避免 sessionId 算错导致连线端点漂移
    Q_INVOKABLE void setLinkNodeCenterAtKey(const QString &keyName, int lineMult, int compIdx, qreal x, qreal y);
    // 行级接受点回写（对应 ImGui ActiveLines[key].AcceptCenter[mult]）
    // 该坐标作为连线终点 pb 的行精确值，由 LineRow.updateLinkNodeCenter 同步回写
    Q_INVOKABLE void setAcceptCenter(int row, qreal x, qreal y);
    // 行级接受点回写（按 keyName+mult 直接定位，免疫 m_entries 重建/行顺序变化导致的 row 错位）
    Q_INVOKABLE void setAcceptCenterByKey(const QString &keyName, int lineMult, qreal x, qreal y);
    // 阶段 3：按 keyName + lineMult 查询行接受点（供 WorkspaceController::rebuildLinkEndpoints 构建 pb）
    // 返回 QPointF() 表示无记录（调用方需回退到标题栏接受点）
    Q_INVOKABLE QPointF acceptCenterByKey(const QString &keyName, int lineMult) const;
    // 行接受点的世界坐标（Eq 空间）镜像查询：懒加载 cull 节点的端点再投影用。
    // 与 acceptCenterByKey 同键同时写入；返回 QPointF() 表示无记录。
    QPointF acceptCenterEqByKey(const QString &keyName, int lineMult) const;
    // 按 keyName+lineMult 定位行序号（m_entries 显示序）；未找到返回 -1。
    // 懒加载兜底的行级 Y 估算用（从未渲染的节点无回写，按行序均匀估算行中心）。
    int rowIndexOf(const QString &keyName, int lineMult) const;
    // accept 目标键（Collector/Armor）左侧方形接收点回写（对应 ImGui AcceptCenter = Cursor.x - LH*0.7）
    Q_INVOKABLE void setAcceptorCenter(const QString &keyName, int lineMult, qreal x, qreal y);
    // accept 目标键方形接收点查询（连线终点连到方形）
    // 屏幕值：仅 INIWEAVER_DIAG 诊断对照/回退用；正式路径读 acceptorCenterEqByKey（世界坐标）
    Q_INVOKABLE QPointF acceptorCenterByKey(const QString &keyName, int lineMult) const;
    // accept 目标键方形接收点的世界坐标（Eq 空间）镜像查询，与 acceptorCenterByKey 同键同时写入。
    // 返回 QPointF() 表示无记录。端点表改存世界坐标后，rebuildLinkEndpoints 只读这一个。
    QPointF acceptorCenterEqByKey(const QString &keyName, int lineMult) const;
    // 诊断：返回已回写接受点的全部 keyName@mult 复合键（排查 FromKey 查询不匹配）
    QStringList acceptCenterKeys() const;

    // 类型校验（对应 IBR_SectionData.cpp:438-465 Acceptor_CheckLinkType/CheckSecType）
    Q_INVOKABLE bool checkLinkType(int row, qulonglong destSectionId) const;
    Q_INVOKABLE bool checkSecType(qulonglong destSectionId) const;

    // 创建/删除链接（对应 ImGui DragDropSource + AcceptDragDropPayload 流程）
    Q_INVOKABLE bool createLink(int row, qulonglong destSectionId, const QString &destKey);
    Q_INVOKABLE bool deleteLink(int row, int linkIdx);
    Q_INVOKABLE void deleteAllLinks(int row);
    // 阶段 4：IIF 分量节点建链（compIdx 定位分量，写入该分量 Value 后重算格式化串，落盘）
    Q_INVOKABLE bool createLinkAt(int row, int compIdx, qulonglong destSectionId, const QString &destKey);
    // IIF 分量节点清空链接（解除该分量链接：写入空值 + 重算格式化串）
    Q_INVOKABLE void deleteAllLinksAt(int row, int compIdx);

    // 修改行值（对应 Input 态 ModifyAndShow）
    Q_INVOKABLE bool modifyValue(int row, const QString &newText);
    // D17：批量应用链接启用状态（对应 ImGui 多链菜单 RadioButton 切换 UseLink）
    // keepLinkIdxs 为保留的 linkIdx 列表，不在其中的将被删除
    Q_INVOKABLE bool applyLinkStates(int row, QVariantList keepLinkIdxs);
    // D14：切换 Input/Link 态（对应 ImGui 双击翻转 IICStatus.InputMethod）
    // 业务层 Data 为 Source of Truth，此处直接读写 Data 的 Status_Workspace/ComponentStatus
    Q_INVOKABLE void toggleInputMode(int row);
    // D14b：双击键名 → 切换整键输入框/节点（IIF 翻全部分量）
    Q_INVOKABLE void toggleKeyInputMode(int row);
    // D14c：双击某分量 → 只切换该分量输入框/节点（按 compIdx）
    Q_INVOKABLE void toggleComponentInputMode(int row, int compIdx);

    // 行级右键菜单（对应 IBR_Misc.cpp:201-252 WorkSpaceLine 右键菜单）
    // toggleSpecialAccept：翻转 SpecialAccept 临时态（画布会话级，不持久化到 INI）
    Q_INVOKABLE void toggleSpecialAccept(int row);
    // removeLine：删除整行（对应 pbk->RemoveLine(Key)，通过 SendToR 异步执行）
    Q_INVOKABLE void removeLine(int row);
    // toggleInputOnShow：翻转 InputOnShow 编辑态（对应 ImGui CC->InputOnShow = !CC->InputOnShow）
    // 仅切换显示/隐藏编辑框，不直接写入业务层
    Q_INVOKABLE void toggleInputOnShow(int row);
    // editDesc：提交 OnShow 描述编辑结果（对应 ImGui InputText EnterReturnsTrue 后写 bsec->OnShow[key]）
    // 由 LineRow 编辑框 onAccepted/onEditingFinished 调用
    Q_INVOKABLE void editDesc(int row, const QString& text);
    // hasActiveInputOnShow：本模块是否有行处于 InputOnShow 描述编辑态
    // 供 nodeMouseArea 门控：编辑期间不抢左键点击，避免点击编辑框自身触发选中→刷新→编辑框销毁失焦
    Q_INVOKABLE bool hasActiveInputOnShow() const;

    // 行级增行按钮（对应 IBR_Misc.cpp:373-385 "+" 按钮）
    // addLine：对 Multiple 行追加新分量（bsec->MergeLine(Key, Index_AlwaysNew, ...))
    Q_INVOKABLE void addLine(int row);

    // 键类型：Bool（勾选框）翻转值（对应 ImGui IIC_Bool 对话框切换）
    // 读取 Data_Bool 当前 bool，翻转后按 StrBoolType 格式写回
    Q_INVOKABLE bool toggleBoolValue(int row);

    // IIF 多分量导出（对齐 imgui IBG_InputForm::RenderUI）：返回该行各分量描述(QVariantMap)
    // 字段：idx/type(细粒度)、label(Short)、tooltip(Long||Short)、value、isLink、readOnly、disabled、
    //       link 分量附节点元数据(hasLinkNode/linkLimit/linkCol/isEmpty/links/linkType/sessionId/lineMult)
    Q_INVOKABLE QVariantList iifComponents(int row) const;

    // 通知 QML 重读本行 IIF 分量（驱动 LineRow.iifRevision 递增）。供侧边栏改值后
    // 的稳定入口（refreshSectionLines）调用，不在 refresh() 内部触发。
    void notifyIifDataChanged();

    // IIF 分量值写回（阶段一核心：input/int 分量文本编辑）
    // 修改该分量 state 后按格式化串重建整行并写回业务层
    Q_INVOKABLE void setIifComponentValue(int row, int compIdx, const QString &rawText);

    // IIF 分量值写回（bool 分量：勾选框点击翻转）
    // 用 IIS_Bool 状态 + IIC_Bool::FmtType 写回，格式化后落盘
    Q_INVOKABLE void setIifComponentValueBool(int row, int compIdx, bool val);

signals:
    void sectionIdChanged();
    void showRegNameChanged();
    // 画布上修改数据后通知外部（WorkspaceController 监听后通知 EditPanel 刷新侧边栏）
    // 保证画布操作（toggleInputMode/modifyValue/createLink/deleteLink 等）与侧边栏双向同步
    void sectionDataChanged(qulonglong sectionId);
    // LinkNode 位置回写后通知 WorkspaceController 重建连线端点表
    // 对应 ImGui 每帧 UpdateLink → SetSessionStatus → 下帧 RenderUI_Links 用新 LastCenter
    void linkNodeCenterChanged();
    // IIF 分量值写回后通知 QML 刷新该行 IIF 显示
    // 因 IIF 值不走 role、且 refresh() 有内容快照 SKIP 优化，写回后需显式通知，
    // 否则共享同一 ValueID 的多个分量不会同步更新显示
    void iifDataChanged(int row);

private:
    // 行条目（对应 ImGui 一行渲染所需的全部数据）
    struct LineEntry
    {
        StrPoolID keyId{ 0 };
        QString keyName;
        QString onShow;
        QString descLong;
        QString subSecName;
        int subSecType{ 0 };       // 0=Default, 1=Inherit, 2=Import
        bool hasLinkNode{ false };
        QColor linkCol;
        int linkLimit{ 0 };
        QString linkType;
        bool isEmpty{ true };
        bool isInherit{ false };
        bool isImport{ false };
        // 键是否为 accept 目标（InputType 带 AcceptType / AcceptorSetting），左侧方形接收点
        bool isAcceptor{ false };
        QColor acceptorColor;
        int lineIdx{ 0 };
        int lineMult{ 0 };
        int compIdx{ 0 };
        uint64_t sessionId{ 0 };
        QVariantList links;
        bool isCollapsed{ false };
        QString exportValue;
        bool isInputMode{ false };  // D14: IICStatus 持久化（true=Input 态，false=Link 态）
        bool isMultiple{ false };   // InputType.Multiple（可增行）
        int keyType{ 0 };           // 键输入类型（0=String, 1=Bool, 2=IIF）
        bool boolChecked{ false };  // Bool 键当前布尔值

        // 内容比较：refresh() 用快照判断行数据是否变化。
        // 删除/新建模块等全量刷新时，无关节点的行数据未变，跳过
        // dataChanged/resetModel 可避免 QML 行绑定重求值（卡顿主因之一）
        bool operator==(const LineEntry &o) const
        {
            return keyId == o.keyId
                && keyName == o.keyName
                && onShow == o.onShow
                && descLong == o.descLong
                && subSecName == o.subSecName
                && subSecType == o.subSecType
                && hasLinkNode == o.hasLinkNode
                && linkCol == o.linkCol
                && linkLimit == o.linkLimit
                && linkType == o.linkType
                && isEmpty == o.isEmpty
                && isInherit == o.isInherit
                && isImport == o.isImport
                && isAcceptor == o.isAcceptor
                && acceptorColor == o.acceptorColor
                && lineIdx == o.lineIdx
                && lineMult == o.lineMult
                && compIdx == o.compIdx
                && sessionId == o.sessionId
                && links == o.links
                && isCollapsed == o.isCollapsed
                && exportValue == o.exportValue
                && isInputMode == o.isInputMode
                && isMultiple == o.isMultiple
                && keyType == o.keyType
                && boolChecked == o.boolChecked;
        }
    };

    qulonglong m_sectionId{ 0 };
    bool m_showRegName{ false };
    WorkspaceController *m_workspace{ nullptr };  // 所属控制器（链接修改后同步全量刷新）
    std::vector<LineEntry> m_entries;
    // 上次已发出信号的内容快照（refresh() 内容比较用，未变则跳过信号）
    std::vector<LineEntry> m_entriesSnapshot;

    // 行级接受点（keyName@mult → 坐标），对应 ImGui ActiveLines[key].AcceptCenter[mult]
    // 修复1：原按 row 索引存储，rebuildEntries 重建 m_entries 后旧 row 坐标残留导致串线
    //（增删行/重排后 acceptCenterByKey 取到错误行的坐标）。改按 keyName 存储，天然免疫行重排。
    // 修复2：多分量键同名 key 的每个分量有独立圆点，仅按 keyName 会把多分量合并到最后一个坐标
    //（起点错位），复合 keyName@mult 精确区分各分量。
    QHash<QString, QPointF> m_acceptCentersByKey;
    // 行接受点世界坐标镜像（Eq 空间，与 m_acceptCentersByKey 同键同时写入）：
    // 懒加载 cull 节点无 delegate 回写，屏幕坐标会随平移/缩放过期；
    // rebuildLinkEndpoints 对 cull 节点用世界坐标按当前视口再投影，端点保持精确
    QHash<QString, QPointF> m_acceptCentersEqByKey;
    // accept 目标键左侧方形接收点（keyName@mult -> 画布/屏幕坐标）
    // 端点表世界化后仅作 INIWEAVER_DIAG 诊断对照与回退手段，rebuild 不再读它
    QHash<QString, QPointF> m_acceptorCentersByKey;
    // accept 目标键方形接收点的世界坐标镜像（Eq 空间，与 m_acceptorCentersByKey 同键同时写入）：
    // 与 m_acceptCentersEqByKey 同语义——存【相对顶层祖先 EqPos 的偏移】，
    // rebuildLinkEndpoints 按当前视口再投影，视口怎么变端点都精确
    QHash<QString, QPointF> m_acceptorCentersEqByKey;

    // SpecialAccept 临时态缓存（按 keyId 索引，跨 rebuild 保留）
    // 对应 ImGui WorkSpaceLine::SpecialAccept（画布会话级，不持久化到 INI）
    QHash<StrPoolID, bool> m_specialAccept;
    // InputOnShow 编辑态缓存（按 keyId 索引，跨 rebuild 保留）
    // 对应 ImGui WorkSpaceLine::InputOnShow（画布会话级，控制 OnShow 描述编辑框显示）
    QHash<StrPoolID, bool> m_inputOnShow;

    void rebuildEntries();
    QVariantList collectLinks(const struct IBB_SubSec &sub,
                              size_t lineIdx, size_t lineMult, size_t compIdx) const;
    QColor computeNodeColor(uint32_t linkColRaw, bool isEmpty, bool isInherit,
                            bool hasLinkNode, bool sectionIgnored) const;
    // IIF 分量节点坐标 writing 需要 sessionId 与 UpdateAll 使用的 Comp=cidx 一致
    // 供 setLinkNodeCenterAt 查询（compIdx==0 直接用 e.sessionId 快速路径）
    qulonglong sessionIdFor(int row, int compIdx) const;
    // D13：构建继承描述文本（对应 ImGui IBR_SectionData.cpp:396-414 InheritStr lambda）
    // 按 ShowRegName 决定显示注册名或解析后的显示名，最终格式化为本地化字符串
    QString buildInheritStr(const struct IBB_Section *bsec, const struct IBR_Section &rsec,
                            bool showReg, bool showInherit) const;
};
