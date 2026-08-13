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

    // QML layout 后回写 RadioButton 屏幕坐标到 IBR_NodeSession::SessionValue.LastCenter
    // 对应 ImGui 立即模式下每帧重算 DefaultCenter
    Q_INVOKABLE void setLinkNodeCenter(int row, qreal x, qreal y);
    // 行级接受点回写（对应 ImGui ActiveLines[key].AcceptCenter[mult]）
    // 该坐标作为连线终点 pb 的行精确值，由 LineRow.updateLinkNodeCenter 同步回写
    Q_INVOKABLE void setAcceptCenter(int row, qreal x, qreal y);
    // 阶段 3：按 keyName + lineMult 查询行接受点（供 WorkspaceController::rebuildLinkEndpoints 构建 pb）
    // 返回 QPointF() 表示无记录（调用方需回退到标题栏接受点）
    Q_INVOKABLE QPointF acceptCenterByKey(const QString &keyName, int lineMult) const;

    // 类型校验（对应 IBR_SectionData.cpp:438-465 Acceptor_CheckLinkType/CheckSecType）
    Q_INVOKABLE bool checkLinkType(int row, qulonglong destSectionId) const;
    Q_INVOKABLE bool checkSecType(qulonglong destSectionId) const;

    // 创建/删除链接（对应 ImGui DragDropSource + AcceptDragDropPayload 流程）
    Q_INVOKABLE bool createLink(int row, qulonglong destSectionId, const QString &destKey);
    Q_INVOKABLE bool deleteLink(int row, int linkIdx);
    Q_INVOKABLE void deleteAllLinks(int row);

    // 修改行值（对应 Input 态 ModifyAndShow）
    Q_INVOKABLE bool modifyValue(int row, const QString &newText);
    // D17：批量应用链接启用状态（对应 ImGui 多链菜单 RadioButton 切换 UseLink）
    // keepLinkIdxs 为保留的 linkIdx 列表，不在其中的将被删除
    Q_INVOKABLE bool applyLinkStates(int row, QVariantList keepLinkIdxs);
    // D14：切换 Input/Link 态（对应 ImGui 双击翻转 IICStatus.InputMethod）
    // 业务层 Data 为 Source of Truth，此处直接读写 Data 的 Status_Workspace/ComponentStatus
    Q_INVOKABLE void toggleInputMode(int row);

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

    // 行级增行按钮（对应 IBR_Misc.cpp:373-385 "+" 按钮）
    // addLine：对 Multiple 行追加新分量（bsec->MergeLine(Key, Index_AlwaysNew, ...))
    Q_INVOKABLE void addLine(int row);

signals:
    void sectionIdChanged();
    void showRegNameChanged();
    // 画布上修改数据后通知外部（WorkspaceController 监听后通知 EditPanel 刷新侧边栏）
    // 保证画布操作（toggleInputMode/modifyValue/createLink/deleteLink 等）与侧边栏双向同步
    void sectionDataChanged(qulonglong sectionId);
    // LinkNode 位置回写后通知 WorkspaceController 重建连线端点表
    // 对应 ImGui 每帧 UpdateLink → SetSessionStatus → 下帧 RenderUI_Links 用新 LastCenter
    void linkNodeCenterChanged();

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
        int lineIdx{ 0 };
        int lineMult{ 0 };
        int compIdx{ 0 };
        uint64_t sessionId{ 0 };
        QVariantList links;
        bool isCollapsed{ false };
        QString exportValue;
        bool isInputMode{ false };  // D14: IICStatus 持久化（true=Input 态，false=Link 态）
        bool isMultiple{ false };   // InputType.Multiple（可增行）
    };

    qulonglong m_sectionId{ 0 };
    bool m_showRegName{ false };
    std::vector<LineEntry> m_entries;

    // 阶段 3：行级接受点缓存（按 row 索引）
    // 对应 ImGui ActiveLines[key].AcceptCenter[mult]，供 WorkspaceController 查询 pb
    QHash<int, QPointF> m_acceptCenters;

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
    // D13：构建继承描述文本（对应 ImGui IBR_SectionData.cpp:396-414 InheritStr lambda）
    // 按 ShowRegName 决定显示注册名或解析后的显示名，最终格式化为本地化字符串
    QString buildInheritStr(const struct IBB_Section *bsec, const struct IBR_Section &rsec,
                            bool showReg, bool showInherit) const;
};
