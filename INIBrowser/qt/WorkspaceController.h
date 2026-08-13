// WorkspaceController.h
// Qt6 工作区控制器：桥接 IBR_WorkSpace / IBR_FullView 状态机和坐标系
// 阶段 5.1：暴露 ratio/eqCenter/section 数据给 QML，接收鼠标交互
#pragma once

#include <QObject>
#include <QPointF>
#include <QRectF>
#include <QSizeF>
#include <QVariantList>
#include <QColor>
#include <QElapsedTimer>
#include <QHash>
#include <QPointer>
#include <QtQmlIntegration/qqmlintegration.h>
#include "IBR_Project.h"  // ModuleID_t, ImVec2

class SectionLineModel;

class WorkspaceController : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(float ratio READ ratio NOTIFY ratioChanged)
    Q_PROPERTY(QPointF eqCenter READ eqCenter NOTIFY eqCenterChanged)
    Q_PROPERTY(QPointF viewCenter READ viewCenter NOTIFY viewCenterChanged)
    Q_PROPERTY(QRectF workspaceRect READ workspaceRect NOTIFY workspaceRectChanged)
    // 性能优化：视口在 Eq 坐标系的矩形（MiniMap 用），避免 QML 端手动转换
    Q_PROPERTY(QRectF viewportEqRect READ viewportEqRect NOTIFY workspaceRectChanged)
    Q_PROPERTY(bool isProjectOpen READ isProjectOpen NOTIFY projectOpenChanged)
    Q_PROPERTY(int inputState READ inputState NOTIFY inputStateChanged)
    Q_PROPERTY(QVariantList sections READ sections NOTIFY sectionsChanged)
    // D20：数据版本号，每次 sectionsChanged 时递增，供 QML 子模块绑定以实时刷新 getSectionData
    Q_PROPERTY(int sectionDataRevision READ sectionDataRevision NOTIFY sectionsChanged)
    Q_PROPERTY(QVariantList links READ links NOTIFY linksChanged)
    // 连线端点表：[{sessionId, x, y, isCollapsed}, ...]，供 LinkRenderer 查表
    // 对应 ImGui Session.LastCenter（每行 RadioButton 屏幕坐标）
    Q_PROPERTY(QVariantList linkEndpoints READ linkEndpoints NOTIFY linkEndpointsChanged)
    // D21：端点 map（key = "sessionId:destId"），供 LinkRenderer 按 key 查找 pb，避免索引依赖
    Q_PROPERTY(QVariantMap linkEndpointsMap READ linkEndpointsMap NOTIFY linkEndpointsChanged)
    Q_PROPERTY(QRectF selectionRect READ selectionRect NOTIFY selectionRectChanged)
    Q_PROPERTY(bool hasSelection READ hasSelection NOTIFY selectionRectChanged)
    // 当前编辑节点 ID（对应 IBR_EditFrame::CurSection.ID，供 LinkRenderer 高亮连线）
    Q_PROPERTY(qulonglong focusedSectionId READ focusedSectionId NOTIFY sectionsChanged)
    // 显示模式：true=寄存器名，false=显示名（对应 IBR_WorkSpace::ShowRegName）
    Q_PROPERTY(bool showRegName READ showRegName NOTIFY showRegNameChanged)
    // 节点拖拽偏移量（屏幕坐标）：拖拽中实时更新，QML 端用它计算拖拽节点的临时位置
    // 拖拽结束后清零并写回 EqPos
    Q_PROPERTY(QPointF dragOffset READ dragOffset NOTIFY dragOffsetChanged)
    // v3 批次 1.4：拖拽 LinkLimit=0 节点时 DropArea 接收状态（对应 ImGui IsDragDropPayloadBeingAccepted）
    Q_PROPERTY(bool dragInvalidLink READ dragInvalidLink NOTIFY dragInvalidLinkChanged)
    // 拖拽中的节点 ID（单节点拖拽用，0 表示无拖拽）
    // 修复：避免 beginMoveSection 中 refresh() 重建 QVariantList 导致 mouse grab 丢失
    Q_PROPERTY(qulonglong draggingSectionId READ draggingSectionId NOTIFY draggingSectionIdChanged)
    // 多节点拖拽标志（MassAfter → HoldingModules 时为 true）
    Q_PROPERTY(bool massDragging READ massDragging NOTIFY massDraggingChanged)
    // 选中版本号：每次 MassTarget 变化时递增，QML 据此重新查询 isSectionSelected
    // 避免单击选中/取消选中时全量 refresh() 重建 QVariantList 导致延迟
    Q_PROPERTY(int selectedRevision READ selectedRevision NOTIFY selectedRevisionChanged)

public:
    explicit WorkspaceController(QObject *parent = nullptr);

    // 注入 EditPanelController 和 MenuController，用于选中模块时切换侧边栏
    // 对应 ImGui IBR_EditFrame::ActivateAndEdit → IBR_Inst_Menu.ChooseMenu(MenuItemID_EDIT)
    void setEditPanelController(class EditPanelController *p) { m_editPanelController = p; }
    void setMenuController(class MenuController *p) { m_menuController = p; }

public slots:
    // 编辑侧边栏修改值后，只刷新指定 sectionId 的 SectionLineModel，避免全量 refresh
    // 监听 EditPanelController::sectionDataChanged 信号
    void refreshSectionLines(qulonglong sectionId);

public:
    // 属性访问器
    float ratio() const;
    QPointF eqCenter() const;
    QPointF viewCenter() const;
    QRectF workspaceRect() const;
    QRectF viewportEqRect() const;
    bool isProjectOpen() const;
    int inputState() const { return m_inputState; }
    QVariantList sections() const { return m_sections; }
    int sectionDataRevision() const { return m_sectionDataRevision; }
    QVariantList links() const { return m_links; }
    QVariantList linkEndpoints() const { return m_linkEndpoints; }
    QVariantMap linkEndpointsMap() const { return m_linkEndpointsMap; }
    QRectF selectionRect() const { return m_selectionRect; }
    bool hasSelection() const { return !m_selectionRect.isNull(); }
    qulonglong focusedSectionId() const;
    bool showRegName() const;
    QPointF dragOffset() const { return m_dragOffset; }
    // 拖拽中的节点 ID（0=无拖拽），QML 据此判断是否对节点应用 dragOffset
    qulonglong draggingSectionId() const { return m_draggingSectionId; }
    // 多节点拖拽标志，QML 据此判断是否对所有选中节点应用 dragOffset
    bool massDragging() const { return m_massDragging; }
    int selectedRevision() const { return m_selectedRevision; }

    // 坐标转换（对应 IBR_WorkSpace::EqPosToRePos / RePosToEqPos）
    Q_INVOKABLE QPointF eqToScreen(QPointF eqPos) const;
    Q_INVOKABLE QPointF screenToEq(QPointF screenPos) const;

    // 鼠标交互（对应 IBR_WorkSpace::ProcessBackgroundOpr 状态机）
    // button: Qt::LeftButton=1, Qt::RightButton=2, Qt::MiddleButton=4
    Q_INVOKABLE void onMousePress(qreal x, qreal y, int button);
    Q_INVOKABLE void onMouseMove(qreal x, qreal y);
    Q_INVOKABLE void onMouseRelease(qreal x, qreal y, int button);
    Q_INVOKABLE void onWheel(qreal x, qreal y, qreal delta);
    Q_INVOKABLE void onDrop(qreal x, qreal y, const QString &moduleKey);

    // ===== 阶段 12.1：MassAfter 状态机相关 =====
    // MassAfter 状态下左键按下命中节点 → 进入 HoldingModules 多节点拖拽
    // 对应 IBR_WorkSpace.cpp:1291-1308 MassAfter → HoldingModules
    Q_INVOKABLE void beginMassDrag(qreal startX, qreal startY);
    // MassAfter 状态下右键释放 → 弹出多选右键菜单
    // 对应 IBR_WorkSpace.cpp:1170-1290
    Q_INVOKABLE void showMassContextMenu(qreal x, qreal y);
    // 获取 MassTarget 列表（供 QML 侧高亮选中节点）
    Q_INVOKABLE QVariantList massTargetIds() const;
    // 轻量级选中查询：QML 通过 selectedRevision 触发重新评估
    // MassSelecting 状态下查询 m_massSelectPreview（实时预览），否则查询 MassTarget
    Q_INVOKABLE bool isSectionSelected(qulonglong sectionId) const;
    // 多选状态查询（对应 ImGui SelectedAllIgnored/Frozen/Hidden）
    // 用于右键菜单智能互斥显示：全忽略时只显示"取消忽略"，否则只显示"忽略"
    Q_INVOKABLE bool selectedAllIgnored() const;
    Q_INVOKABLE bool selectedAllFrozen() const;
    Q_INVOKABLE bool selectedAllHidden() const;
    // QML SectionNode 渲染后回报实际屏幕尺寸，更新 sd.EqSize（对应 ImGui 实时更新 EqSize）
    // Qt 版本 SectionNode 尺寸由内容驱动（implicitWidth/Height），与 sd.EqSize 初始值可能不符
    // 框选命中检测依赖准确的 EqSize，需在渲染后同步
    Q_INVOKABLE void reportSectionSize(qulonglong sectionId, qreal screenW, qreal screenH);
    // MassAfter 状态下快捷键响应（Delete/Copy/Cut/Duplicate）
    Q_INVOKABLE void massAfterDelete();
    Q_INVOKABLE void massAfterCopy();
    Q_INVOKABLE void massAfterCut();
    Q_INVOKABLE void massAfterDuplicate();

    // ===== 阶段 12.3：虚拟块相关 =====
    // 切换子块在虚拟块中的折叠态（对应 IBR_SectionData.cpp:860-862 / 942-944）
    Q_INVOKABLE void toggleCollapseInComposed(qulonglong sectionId, bool collapsed);
    // 批量折叠/展开虚拟块的所有子块（对应 IBR_SectionData.cpp:355-369 FoldComposed/UnfoldComposed）
    Q_INVOKABLE void foldComposed(qulonglong virtualBlockId);
    Q_INVOKABLE void unfoldComposed(qulonglong virtualBlockId);
    // 解散虚拟块（对应 IBR_SectionData.cpp:322-339 Decompose）
    Q_INVOKABLE void decomposeSection(qulonglong virtualBlockId);
    // 显示虚拟块中所有隐藏的子块（对应 IBR_SectionData.cpp:925-934）
    Q_INVOKABLE void showAllIncludingBlocks(qulonglong virtualBlockId);

    // ===== 阶段 12.4：节点 Acceptor DropTarget 合并语义 =====
    // 节点拖拽到目标节点上：合并源到目标（对应 IBR_SectionData.cpp:486-527 RenderUI_Acceptor 的 IBR_SecDrag 路径）
    // 返回值：true=合并成功，false=类型不匹配或无默认链接 key
    Q_INVOKABLE bool mergeSectionToSection(qulonglong sourceId, qulonglong destId);
    // 连线拖拽到目标节点上：合并链接（对应 IBR_SectionData.cpp:528-560 IBR_LineDrag 路径）
    // sourceId=源 section, destId=目标 section, linkKey=链接 key 名
    Q_INVOKABLE bool mergeLinkToSection(qulonglong sourceId, qulonglong destId, const QString &linkKey);
    // 连线拖拽到目标节点上：在源行创建链接（对应 ImGui lin.pSession->NotifyValueToMerge 路径）
    // sourceId=源 section, sourceKeyName=源行 key 名, sourceLineMult=源行分量索引
    // destSectionId=目标 section, destKey=目标行 key 名（空则用目标 DLK）, destMult=目标行分量索引
    // 走 ModifyAndShow 路径：收集现有链接 + 追加新目标 + 遵守 LinkLimit
    Q_INVOKABLE bool createLinkFromDrag(qulonglong sourceId, const QString &sourceKeyName, int sourceLineMult,
                                        qulonglong destSectionId, const QString &destKey, int destMult);
    // 类型校验预览（对应 Acceptor_CheckLinkType）
    // 返回：0=允许(绿色对勾), 1=类型不匹配(红色禁止), 2=无默认链接key(红色禁止), 3=无效链接(红色叉)
    Q_INVOKABLE int checkMergePreview(qulonglong sourceId, qulonglong destId, const QString &linkKey, bool isLinkDrag);
    // 获取拖拽预览文本（对应 DragConditionText / DragConditionTextAlt）
    Q_INVOKABLE QString mergePreviewText(qulonglong sourceId, qulonglong destId, const QString &linkKey, bool isLinkDrag);

    // ===== 阶段 12.5：节点右键菜单项补齐 =====
    // 智能切换 Ignore/Freeze/Hide（对应 IBR_SectionData.cpp:629-676 的智能互斥逻辑）
    // QML 侧根据节点当前状态决定菜单项文本，C++ 侧执行切换
    Q_INVOKABLE void toggleIgnore(qulonglong sectionId);
    Q_INVOKABLE void toggleFreeze(qulonglong sectionId);
    Q_INVOKABLE void toggleHide(qulonglong sectionId);
    // 进入文本编辑模式（对应 IBR_SectionData.cpp:720-724 EditText）
    Q_INVOKABLE void enterEditTextMode(qulonglong sectionId);
    // 查询节点是否为 Comment 块（对应 IBR_SectionData.cpp:584-626 Comment 分支）
    Q_INVOKABLE bool isCommentBlock(qulonglong sectionId) const;

    // ===== 阶段 12.6：边界推回 + 触边提示 =====
    // 检查 EqCenter 是否在有效范围内（对应 EqPosInRange）
    Q_INVOKABLE bool isEqCenterInRange() const;
    // 获取触边方向（对应 EqXRange/EqYRange）：0=无, 1=左, 2=右, 4=上, 8=下
    Q_INVOKABLE int edgeFlags() const;

    // ===== 阶段 12.7：双击空白弹 SearchModuleAlt =====
    // 双击空白触发模块搜索（对应 IBR_WorkSpace.cpp:961-969）
    Q_INVOKABLE void onDoubleClickEmpty(qreal x, qreal y);

    // ===== 阶段 12.8：连线系统补齐 =====
    // 解除单个链接（已有 deleteLink，这里提供批量版本）
    Q_INVOKABLE void unlinkAllLinks(qulonglong sectionId);
    // 获取节点的链接限制（对应 IBR_LinkNode::LinkLimit）
    Q_INVOKABLE int getLinkLimit(qulonglong sectionId, const QString &linkKey) const;
    // 获取节点的所有链接（用于多链接 RadioButton 切换视图）
    Q_INVOKABLE QVariantList getSectionLinks(qulonglong sectionId) const;

    // 视图操作
    Q_INVOKABLE void zoomTo(float newRatio);
    Q_INVOKABLE void centerView();
    Q_INVOKABLE void centerViewTo(qreal eqX, qreal eqY);
    Q_INVOKABLE void moveToCenter();

    // 框选
    Q_INVOKABLE void startSelection(qreal x, qreal y);
    Q_INVOKABLE void updateSelection(qreal x, qreal y);
    Q_INVOKABLE void endSelection();
    Q_INVOKABLE void clearSelection();
    // 设置当前选中的 Section ID 列表（同步到 IBR_WorkSpace::MassTarget）
    Q_INVOKABLE void setSelectedIDs(const QVariantList &ids);

    // 批量操作（对应 IBR_WorkSpace 命名空间的函数）
    Q_INVOKABLE void deleteSelected();
    Q_INVOKABLE void copySelected();
    Q_INVOKABLE void cutSelected();
    Q_INVOKABLE void paste();
    Q_INVOKABLE void duplicateSelected();
    Q_INVOKABLE void selectAll();
    Q_INVOKABLE void composeSelected();
    Q_INVOKABLE void freezeSelected(bool frozen);
    Q_INVOKABLE void hideSelected(bool hidden);
    Q_INVOKABLE void ignoreSelected(bool ignored);

    // ===== 阶段 7 新增：缺失的快捷键对应方法 =====
    // 反选（对应 Ctrl+Shift+I）
    Q_INVOKABLE void invertSelection();
    // 删除所有模块（对应 DeleteAll 快捷键，IBR_WorkSpace.cpp:944-957）
    Q_INVOKABLE void deleteAllSections();
    // 切换显示模式：寄存器名/显示名（对应 SwitchDisplayMode，IBR_WorkSpace.cpp:958）
    Q_INVOKABLE void switchDisplayMode();
    // 重命名选中节点的显示名（对应 F2 RenameModule，IBR_WorkSpace.cpp:1874）
    Q_INVOKABLE void renameSelected();
    // 重命名选中节点的寄存器名（对应 F3 RenameRegister，IBR_WorkSpace.cpp:1875）
    Q_INVOKABLE void renameRegisterSelected();
    // 撤销/重做（对应 Ctrl+Z/Ctrl+Y，IBG_UndoStack）
    Q_INVOKABLE void undo();
    Q_INVOKABLE void redo();

    // 右键菜单
    Q_INVOKABLE void showContextMenu(qreal x, qreal y);

    // ===== 阶段 1 新增：节点交互 =====
    // 单节点拖拽移动（对应 IBR_WorkSpace.cpp:1004-1024 HoldingModules 状态机）
    Q_INVOKABLE void beginMoveSection(qulonglong sectionId, qreal startX, qreal startY);
    Q_INVOKABLE void updateMoveSection(qreal curX, qreal curY);
    Q_INVOKABLE void endMoveSection();
    // 阶段 12.1：统一拖拽更新/结束入口（根据 m_moveAfterMass 分发到单节点或多节点路径）
    // QML 侧 SectionNode 拖拽时统一调用此方法，无需关心当前是单拖还是多拖
    Q_INVOKABLE void updateDrag(qreal curX, qreal curY);
    Q_INVOKABLE void endDrag();
    // 单节点单击选中切换（对应 IBR_WorkSpace.cpp:1732 CurOnRender_Clicked）
    Q_INVOKABLE void toggleSelectSection(qulonglong sectionId, bool additive);
    // 双击进入编辑模式（对应 IBR_EditFrame::ActivateAndEdit）
    Q_INVOKABLE void activateAndEdit(qulonglong sectionId);
    // 切换节点折叠状态（持久化到 sd.UICollapsed）
    Q_INVOKABLE void toggleCollapseSection(qulonglong sectionId, bool collapsed);
    // C11：注释块文本编辑回写（对应 IBR_SectionData.cpp:813-833 InputTextMultiline）
    Q_INVOKABLE void updateCommentText(qulonglong sectionId, const QString &text);

    // ===== 阶段 1：折叠态连线锚点回写（修复 D1）=====
    // 折叠态下所有连线的源端点应汇聚到头部 RadioButton 位置
    // 对应 ImGui RenderUI_Collapsed（IBR_SectionData.cpp:835-868）：
    //   遍历所有 SubSec 的 NewLinkTo，调 PushLinkForDraw(HeadLineRN, ..., Collapsed=true)
    //   → SetSessionStatus(SessionID, HeadLineRN, true)
    // QML 端 SectionNode header 的 headLineRN Rectangle 在 isCollapsed||collapsedInComposed 时
    //   通过 onXChanged/onYChanged 回写屏幕坐标到此方法
    Q_INVOKABLE void setHeadLineRN(qulonglong sectionId, qreal x, qreal y);

    // ===== 阶段 3：终点 pb 精确化（修复 D3、D7）=====
    // 非折叠态下，目标 section 的"接受点"为其标题栏 RadioButton 位置
    // 对应 ImGui RSD->ReWindowUL + RSD->ReOffset（标题栏中央节点）
    // QML 端 SectionNode header 的 headLineRN Rectangle 在非折叠态时回写
    Q_INVOKABLE void setSectionAcceptPoint(qulonglong sectionId, qreal x, qreal y);

    // ===== 阶段 2 新增：连线系统 =====
    // 创建连线（对应 IBR_WorkSpace.cpp:1436-1459 拖拽创建）
    Q_INVOKABLE void createLink(qulonglong sourceId, qulonglong destId, const QString &destKey);
    // 删除连线（对应 IBR_SectionData::RenderUI ActiveLines 处理）
    Q_INVOKABLE void deleteLink(qulonglong sourceId, qulonglong destId, const QString &destKey);
    // 临时拖拽连线显示（鼠标拖动时显示临时 Bezier）
    Q_INVOKABLE void setDraggingLink(qreal fromX, qreal fromY, qreal toX, qreal toY);
    Q_INVOKABLE void clearDraggingLink();
    // v3 批次 1.4：DropArea 接收 linkLimit=0 拖拽时通知源 LinkNode 显示"无效链接"提示
    // 对应 ImGui IsDragDropPayloadBeingAccepted（IBR_SectionData.cpp:96-106 DrawDragPreviewIcon_LinkLim0）
    Q_INVOKABLE void setDragInvalidLink(bool invalid);
    bool dragInvalidLink() const { return m_dragInvalidLink; }

    // 刷新数据（从 IBR_Inst_Project 同步到 QML）
    Q_INVOKABLE void refresh();
    // 阶段 D2：根据 sectionId 查询单个 Section 的完整数据（供虚拟块递归渲染子模块用）
    // 返回与 refreshSections 中每个 item 相同字段的 QVariantMap，包括 lineModel
    Q_INVOKABLE QVariantMap getSectionData(qulonglong sectionId) const;
    // 性能优化：轻量查询，仅返回 EqPos（避免 buildSectionItem 的全量重建）
    Q_INVOKABLE QPointF getSectionEqPos(qulonglong sectionId) const;
    // QTimer 专用刷新：带脏标记检查，无变化时跳过全量重建
    void refreshFromTimer();
    // 设置视口尺寸（QML WorkspaceView 在 onWidthChanged/onHeightChanged 时调用）
    // 同步到 IBR_RealCenter::Center/WorkSpaceUL/WorkSpaceDR，供 EqPosToRePos 计算屏幕坐标
    // 对应 ImGui 版本 IBR_Misc.cpp:484-493 IBR_RealCenter::Update()
    Q_INVOKABLE void setViewportSize(qreal width, qreal height);

signals:
    void ratioChanged();
    void eqCenterChanged();
    void viewCenterChanged();
    void workspaceRectChanged();
    void projectOpenChanged();
    void inputStateChanged();
    void sectionsChanged();
    void linksChanged();
    void linkEndpointsChanged();
    void selectionRectChanged();
    void contextMenuRequested(qreal x, qreal y);
    void hintMessage(const QString &msg);
    void draggingLinkChanged(qreal fromX, qreal fromY, qreal toX, qreal toY);
    void draggingLinkCleared();
    void showRegNameChanged();
    void edgeFlagsChanged();
    void dragOffsetChanged();
    // 阶段 12.7：双击空白触发模块搜索
    void moduleSearchRequested(qreal x, qreal y);
    // v3 批次 1.4：拖拽 LinkLimit=0 节点时 DropArea 接收状态通知
    // 对应 ImGui IsDragDropPayloadBeingAccepted（IBR_SectionData.cpp:96-106）
    void dragInvalidLinkChanged(bool invalid);
    // 拖拽节点 ID 变化通知（QML 据此决定是否应用 dragOffset）
    void draggingSectionIdChanged();
    // 多节点拖拽状态变化通知
    void massDraggingChanged();
    // 选中状态变化通知（轻量级，不触发 sectionsChanged）
    void selectedRevisionChanged();
    // 单节点位置变化通知（拖拽结束时不调全量 refresh，只通知 QML 更新对应节点）
    void sectionPositionChanged(qulonglong sectionId);

private:
    // 关联控制器（由 QtMain.cpp 注入，用于选中模块时切换侧边栏）
    class EditPanelController *m_editPanelController{nullptr};
    class MenuController *m_menuController{nullptr};

    // 状态机：0=Normal, 1=BgDragging, 2=MassSelecting, 3=HoldingModules, 4=MassAfter
    // 对应 IBR_WorkSpace.cpp:822-830 状态机注释
    int m_inputState{0};
    QVariantList m_sections;
    // D20：数据版本号，每次 sectionsChanged 递增
    int m_sectionDataRevision{0};
    QVariantList m_links;
    // 连线端点表：每行 RadioButton 屏幕坐标，由 SectionLineModel::setLinkNodeCenter 回写
    QVariantList m_linkEndpoints;
    // D21：端点 map（key = "sessionId:destId"），与 m_linkEndpoints 同步构建
    QVariantMap m_linkEndpointsMap;
    QRectF m_selectionRect;
    QPointF m_dragStartScreen;
    QPointF m_dragStartEq;

    // 节点拖拽状态（单节点拖拽，对应 HoldingModules 的简化版）
    ModuleID_t m_dragSectionId{0};
    ImVec2 m_dragStartEqPos{0, 0};
    QPointF m_dragStartScreenPos;
    // 性能优化：拖拽偏移量（屏幕坐标），拖拽中实时更新，QML 端用它计算临时位置
    // 避免每帧 refresh() 全量重建 QVariantList
    QPointF m_dragOffset;
    // v3 批次 1.4：拖拽 LinkLimit=0 节点时 DropArea 接收状态
    bool m_dragInvalidLink{false};
    // 拖拽中的节点 ID（单节点拖拽，INVALID_MODULE_ID=无拖拽）
    // 修复：不能用 0 作为"无拖拽"哨兵值，因为 0 是合法的 sectionId（第一个新建模块 ID=0）
    // 否则第一个新建模块会因 sectionId(0) === draggingSectionId(0) 永远显示 isDragging 蓝框
    qulonglong m_draggingSectionId{INVALID_MODULE_ID};
    // 多节点拖拽标志（MassAfter → HoldingModules 时为 true）
    bool m_massDragging{false};
    // 选中版本号：每次 MassTarget 变化时递增，避免全量 refresh
    int m_selectedRevision{0};

    // ===== 阶段 12.1：MassAfter / 多节点拖拽状态 =====
    // 对应 IBR_WorkSpace.cpp:186-211 的状态变量
    QPointF m_massAfterRightDownPos;   // MassAfter 右键按下位置（FLT_MAX 表示未按下）
    bool m_moveAfterMass{false};       // 当前 HoldingModules 来源于 MassAfter/Duplicate
    bool m_hasLeftDownToWait{false};   // 左键等待（防止 MassAfter→HoldingModules 时本帧误触发取消）
    bool m_initHolding{false};         // HoldingModules 是否已初始化质心
    QPointF m_holdingStartEqMouse;     // 拖拽起点鼠标 Eq 坐标
    QPointF m_holdingStartEqDelta;     // 鼠标→质心的偏移
    std::vector<ModuleID_t> m_massDragIds;  // 多节点拖拽中的节点列表

    // 框选拖拽中的实时预览集合（对应 ImGui IBR_SelectMode::MassSelectWindows）
    // 拖拽过程中实时更新，框内模块立即显示选中状态
    // 释放右键时转为正式 MassTarget
    std::unordered_set<ModuleID_t> m_massSelectPreview;

    // C11：Paste 120ms 节流（对应 IBR_WorkSpace.cpp:543 PasteNarrowTime=120ms）
    QElapsedTimer m_uptimeTimer;
    qint64 m_lastPasteElapsed{0};

    // 连线拖拽状态
    bool m_hasDraggingLink{false};
    qreal m_dragLinkFromX{0}, m_dragLinkFromY{0}, m_dragLinkToX{0}, m_dragLinkToY{0};

    void refreshSections();
    void refreshLinks();
    // 修复：Qt 版本无 ImGui 每帧渲染流程，LinkList 不会被 PushLinkForDraw 填充
    // 需在 refreshLinks 前遍历所有 Section 的 SubSecs.NewLinkTo 重建 LinkList
    // 对应 ImGui RenderUI_Collapsed/RenderUI_Lines 中遍历 Bsec->SubSecs 调 PushLinkForDraw
    void rebuildLinkList();
    void updateInputState(int newState);
    // 修复：拖拽结束后根据 MassTarget 是否为空决定回到 MassAfter(4) 或 Normal(0)
    // 若拖拽前有选中模块，拖拽结束应回到 MassAfter，否则点空白无法取消选中
    void restoreStateAfterDrag();

    // 阶段 D2：构建单个 Section 的 QVariantMap（refreshSections 与 getSectionData 共用）
    // id=Section ID, sd=SectionData 引用, selectedSet=MassTarget 选中集合
    QVariantMap buildSectionItem(ModuleID_t id, const IBR_SectionData &sd,
                                 const std::unordered_set<ModuleID_t> &selectedSet) const;

    // 阶段 12.2：多节点拖拽辅助
    void initMassDrag();  // 计算质心 + 各节点 EqDelta（对应 IBR_WorkSpace.cpp:1004-1024）
    void updateMassDrag(qreal curX, qreal curY);  // 整组平移（对应 UpdateScrollAlt）
    void endMassDrag();   // 清除 Dragging 标志

    // 性能优化：脏标记，避免 QTimer 每 50ms 全量重建 QVariantList
    // 只在关键状态变化时才全量刷新（Section 数量/Link 数量/项目开关/编辑节点）
    bool m_dirty{true};               // 初始为 true，确保首次刷新
    size_t m_lastSectionCount{0};     // 上次刷新时的 Section 数量
    size_t m_lastLinkCount{0};        // 上次刷新时的 Link 数量
    bool m_lastIsOpen{false};         // 上次刷新时的项目打开状态
    // D11：状态哈希（Frozen/Hidden/Ignore/UICollapsed/CollapsedInComposed/ShowRegName）
    // 用于检测节点状态变化（如 Ignore 切换），补充 sectionCount/linkCount/editId 的不足
    size_t m_lastStateHash{0};

    // D10：连线端点表脏标记
    // 由坐标回写方法（setHeadLineRN/setSectionAcceptPoint）和视口变化（onMouseMove/onWheel）设置
    // refreshFromTimer 检测到此标志才调 rebuildLinkEndpoints，避免静止时无谓重建
    bool m_linkEndpointsDirty{true};
    // 防止多次 linkNodeCenterChanged 重复排队重建（缩放/平移时大量 LineRow 回写）
    bool m_pendingRebuild{false};

    // 行模型缓存：每个 Section 对应一个 SectionLineModel
    // 在 refreshSections 时创建/更新，QML 通过 sectionData.lineModel 访问
    // 标记 mutable：buildSectionItem 是 const 方法（getSectionData 复用），但需要懒创建缓存
    mutable QHash<ModuleID_t, QPointer<SectionLineModel>> m_lineModels;

    // 重建连线端点表（从 IBR_NodeSession 全局表同步 LastCenter）
    void rebuildLinkEndpoints();

    // D11：计算当前所有 Section 的状态哈希（Frozen/Hidden/Ignore/UICollapsed/CollapsedInComposed + ShowRegName）
    // 用于 refreshFromTimer 脏检查，检测节点状态变化
    size_t computeStateHash() const;

    // 阶段 3：每个 section 的标题栏 RadioButton 屏幕坐标（非折叠态接受点）
    // 由 QML SectionNode header headLineRN 回写，供 rebuildLinkEndpoints 构建 pb
    QHash<qulonglong, QPointF> m_sectionAcceptPoint;
};
