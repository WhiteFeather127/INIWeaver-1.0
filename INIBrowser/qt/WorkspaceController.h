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
#include <QSet>
#include <QPointer>
#include <QMutex>
#include <QStringList>
#include <QVector>
#include <QtQmlIntegration/qqmlintegration.h>
#include "IBR_Project.h"  // ModuleID_t, ImVec2
#include "WorkspaceSectionModel.h"  // sectionsModel 完整类型（getter 返回 QObject* 需已知继承关系）

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
    // 增量 Section 模型（QAbstractListModel）：QML Repeater 用此属性，增删模块时
    // 只增删对应 delegate，避免整表替换重建全部 SectionNode（卡顿根因）。
    // 保留 sections（全量列表）供 LinkRenderer/MiniMap/length 判断等使用。
    Q_PROPERTY(QObject *sectionsModel READ sectionsModel CONSTANT)
    // D20：数据版本号，仅在 section 数据结构变化时递增（refreshSections/项目关闭），
    // 供 QML 子模块绑定以实时刷新 getSectionData。
    // 注意：NOTIFY 用独立的 sectionDataRevisionChanged，而非 sectionsChanged。
    // 否则 reportSectionSize 为刷新 MiniMap 快照 emit sectionsChanged 时，会连带触发
    // 子模块 getSectionData 重查 → buildSectionItem 重建 → SectionNode 高度变化 →
    // reportSectionSize → 再 emit → 无限重入循环（导入大项目死循环根因）。
    Q_PROPERTY(int sectionDataRevision READ sectionDataRevision NOTIFY sectionDataRevisionChanged)
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
    // 画布平移偏移（屏幕像素）：拖动画布时端点表保持快照不重建，
    // QML LinkRenderer 渲染时对所有端点叠加此偏移，与节点移动保持一致
    Q_PROPERTY(QPointF canvasDragOffset READ canvasDragOffset NOTIFY canvasDragOffsetChanged)
    // 视口偏移（仿 canvasDragOffset）：拖拽侧边栏调宽/窗口 resize 时视口中心
    // (width/2, height/2) 变化，所有模块随之整体平移，而端点表是"上次 rebuild 时"
    // 的快照坐标 → 连线端点落后模块。QML LinkRenderer 渲染时对所有端点叠加此偏移，
    // 与模块即时移动保持一致；下次端点表重建时归零（见 rebuildLinkEndpoints）
    Q_PROPERTY(qreal viewportOffsetX READ viewportOffsetX NOTIFY viewportOffsetChanged)
    Q_PROPERTY(qreal viewportOffsetY READ viewportOffsetY NOTIFY viewportOffsetChanged)
    // 缩放叠加（仿 canvasDragOffset）：缩放中端点表保持快照不重建，
    // QML LinkRenderer 按 zoomBaseRatio/zoomBaseCenter 基准换算端点坐标（等比+中心偏移），
    // 与节点即时缩放保持一致；缩放停止防抖后重建端点表结束叠加（降低延迟）
    Q_PROPERTY(bool zoomPending READ zoomPending NOTIFY zoomBaseChanged)
    Q_PROPERTY(float zoomBaseRatio READ zoomBaseRatio NOTIFY zoomBaseChanged)
    Q_PROPERTY(QPointF zoomBaseCenter READ zoomBaseCenter NOTIFY zoomBaseChanged)
    // 缩放补间目标（QML 端动画读取）：滚轮只设定目标 Ratio，QML NumberAnimation
    // 驱动每帧 applyZoomRatio 应用，动画在渲染线程与帧同步推进
    Q_PROPERTY(float zoomTargetRatio READ zoomTargetRatio NOTIFY zoomTargetRatioChanged)
    // v3 批次 1.4：拖拽 LinkLimit=0 节点时 DropArea 接收状态（对应 ImGui IsDragDropPayloadBeingAccepted）
    Q_PROPERTY(bool dragInvalidLink READ dragInvalidLink NOTIFY dragInvalidLinkChanged)
    // 拖拽目标命中：鼠标命中测试选中的目标节点（0=未命中），驱动目标节点预览框显示
    Q_PROPERTY(qulonglong dragTargetSectionId READ dragTargetSectionId NOTIFY dragTargetChanged)
    Q_PROPERTY(bool hasDragTarget READ hasDragTarget NOTIFY dragTargetChanged)
    Q_PROPERTY(QString dragTargetColor READ dragTargetColor NOTIFY dragTargetChanged)
    Q_PROPERTY(QString dragTargetText READ dragTargetText NOTIFY dragTargetChanged)
    // 连线拖拽的源预览标签（对应 ImGui BeginDragDropSource 里的源文本，IBR_LinkNode.cpp:570-573）
    Q_PROPERTY(QString dragSourceText READ dragSourceText NOTIFY dragTargetChanged)
    // 行为A：拖线悬停 acceptor 键行时的整行高亮（对应 ImGui IBR_Misc.cpp:100-178 Acceptor Logic）
    Q_PROPERTY(qulonglong dragAcceptorSectionId READ dragAcceptorSectionId NOTIFY dragAcceptorRowChanged)
    Q_PROPERTY(QString dragAcceptorRowKey READ dragAcceptorRowKey NOTIFY dragAcceptorRowChanged)
    Q_PROPERTY(QColor dragAcceptorColor READ dragAcceptorColor NOTIFY dragAcceptorRowChanged)
    // 是否正在拖拽连线（区分有无目标命中：无目标时仅显示源标签，对齐 imgui 拖拽图像）
    Q_PROPERTY(bool hasDraggingLink READ hasDraggingLink NOTIFY draggingActiveChanged)
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
    bool zoomPending() const { return m_zoomPending; }
    float zoomBaseRatio() const { return m_zoomBaseRatio; }
    QPointF zoomBaseCenter() const { return m_zoomBaseCenter; }
    float zoomTargetRatio() const { return m_zoomTargetRatio; }
    int inputState() const { return m_inputState; }
    QVariantList sections() const { return m_sections; }
    QObject *sectionsModel() const { return m_sectionsModel; }
    int sectionDataRevision() const { return m_sectionDataRevision; }
    QVariantList links() const { return m_links; }
    QVariantList linkEndpoints() const { return m_linkEndpoints; }
    QVariantMap linkEndpointsMap() const { return m_linkEndpointsMap; }
    QRectF selectionRect() const { return m_selectionRect; }
    bool hasSelection() const { return !m_selectionRect.isNull(); }
    qulonglong focusedSectionId() const;
    bool showRegName() const;
    QPointF dragOffset() const { return m_dragOffset; }
    QPointF canvasDragOffset() const { return m_canvasDragOffset; }
    qreal viewportOffsetX() const { return m_viewportOffsetX; }
    qreal viewportOffsetY() const { return m_viewportOffsetY; }
    // 拖拽中的节点 ID（0=无拖拽），QML 据此判断是否对节点应用 dragOffset
    qulonglong draggingSectionId() const { return m_draggingSectionId; }
    // 多节点拖拽标志，QML 据此判断是否对所有选中节点应用 dragOffset
    bool massDragging() const { return m_massDragging; }
    int selectedRevision() const { return m_selectedRevision; }

    // 坐标转换（对应 IBR_WorkSpace::EqPosToRePos / RePosToEqPos）
    Q_INVOKABLE QPointF eqToScreen(QPointF eqPos) const;
    Q_INVOKABLE QPointF screenToEq(QPointF screenPos) const;
    // 鼠标命中测试：返回鼠标屏幕坐标命中的顶层 sectionId（INVALID_MODULE_ID=未命中）；从后向前遍历（上层优先）
    // 复用 updateSelection 的 eq 矩形 contains 模式，基于 sections 模型自带 eqX/eqY/eqW/eqH
    Q_INVOKABLE qulonglong hitTestSection(qreal screenX, qreal screenY) const;
    // 命中测试的字符串版本：命中返回 QString::number(id)，未命中返回空串。供 QML 判断"是否命中"，
    // 避免 INVALID_MODULE_ID(UINT64_MAX) 传给 QML 时超出 double 精度而失真；id 取 Number(str) 转回。
    Q_INVOKABLE QString hitTestSectionStr(qreal screenX, qreal screenY) const;
    // 拖拽目标预览（拖拽源在 onPositionChanged 中调用；结束/离开时用 clearDragTarget() 清除）
    Q_INVOKABLE void setDragTarget(qulonglong targetId, const QString &color, const QString &text);
    // 清除拖拽目标预览（终止拖拽/离开模块悬停时调用）
    Q_INVOKABLE void clearDragTarget();
    // 设置连线拖拽的源预览标签（由 LinkNodePoint 拖动时调用，imgui BeginDragDropSource 源文本）
    Q_INVOKABLE void setDragSourceText(const QString &text);
    // 行为A：拖线悬停 acceptor 键行 → 通知 LineRow 画整行高亮框 + 记录端点锚点目标
    Q_INVOKABLE void setDragAcceptorRow(qulonglong sectionId, const QString &keyName, const QColor &color);
    Q_INVOKABLE void clearDragAcceptorRow();
    qulonglong dragAcceptorSectionId() const { return m_dragAcceptorSectionId; }
    QString dragAcceptorRowKey() const { return m_dragAcceptorRowKey; }
    QColor dragAcceptorColor() const { return m_dragAcceptorColor; }
    qulonglong dragTargetSectionId() const { return m_dragTargetSectionId; }
    bool hasDragTarget() const { return m_dragTargetSectionId != INVALID_MODULE_ID; }
    QString dragTargetColor() const { return m_dragTargetColor; }
    QString dragTargetText() const { return m_dragTargetText; }
    QString dragSourceText() const { return m_dragSourceText; }
    bool hasDraggingLink() const { return m_hasDraggingLink; }

    // 鼠标交互（对应 IBR_WorkSpace::ProcessBackgroundOpr 状态机）
    // button: Qt::LeftButton=1, Qt::RightButton=2, Qt::MiddleButton=4
    Q_INVOKABLE void onMousePress(qreal x, qreal y, int button);
    Q_INVOKABLE void onMouseMove(qreal x, qreal y);
    Q_INVOKABLE void onMouseRelease(qreal x, qreal y, int button);
    Q_INVOKABLE void onWheel(qreal x, qreal y, qreal delta);
    Q_INVOKABLE void onDrop(qreal x, qreal y, const QString &moduleKey);
    // 缩放补间逐帧应用（QML NumberAnimation 每帧调用，用当前成员锚点原子更新
    // Ratio/EqCenter/拖拽基准/dragOffset/预览线，与节点渲染同帧）
    Q_INVOKABLE void applyZoomRatio(float newRatio);
    // 缩放补间结束回调（QML 动画停止时调用：推 Undo 栈 + 更新 EqMax + 调度防抖重建端点表）
    Q_INVOKABLE void zoomTweenFinished();

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
    // isSectionSelected 的实现体（diag 构建下 isSectionSelected 外包计时探针，
    // 逻辑收在此处避免探针递归计时）
    bool isSectionSelectedImpl(qulonglong sectionId) const;
    // 选中集快照（批量查询支撑）：一次 C++ 往返取回整个选中集合（框选中返回预览集，
    // 语义与 isSectionSelected 完全一致），QML 侧建 JS Set 后 O(1) 查询。
    // 替代画布每帧对每条连线/每节点的逐项 isSectionSelected 跨语言调用
    //（1471 模块全选后每帧 4000+ 次 C++ 往返是拖拽/重绘卡顿热点）。
    Q_INVOKABLE QVariantList massTargetSnapshot() const;
    // 编组块成员全集（含 groupId 自身，BFS 所有层级子块）：拖编组时 QML 取一次建
    // JS Set，逐连线判断"是否随拖拽移动"不再逐条调 isInComposedOf 跨语言 BFS
    Q_INVOKABLE QVariantList composedMembersList(qulonglong groupId) const;
    // 行圆点回写的世界坐标镜像入口（公开：SectionLineModel::setLinkNodeCenter* 调用）。
    // 屏幕↔Eq 换算复用既有的 Q_INVOKABLE screenToEq/eqToScreen（QPointF 版，EqPosToRePos 同式）
    Q_INVOKABLE void noteSessionCenterEq(qulonglong sectionId, qulonglong sessionId, qreal sx, qreal sy); // SectionLineModel 行圆点回写入口（公开）
    // 节点当前 EqPos（世界坐标；SectionLineModel 写相对偏移用）
    Q_INVOKABLE QPointF sectionEqPos(qulonglong sectionId) const;
    // 壳渲染位置上报（updatePosition 每次调用）：连线端点对账的"节点实际渲染位置"。
    // 屏幕坐标与世界坐标（Eq）都存，EP-DIAG 同屏对比 painted pb / 世界投影 / 实际渲染。
    Q_INVOKABLE void noteShellRenderPos(qulonglong sectionId, qreal x, qreal y);
    QPointF shellRenderPos(qulonglong sectionId) const;   // 屏幕坐标（未上报过返回 QPointF()）
    QPointF shellRenderEq(qulonglong sectionId) const;    // 世界坐标
    // QML 回写级联完成后的强制端点表重建：世界缓存变更（非视口变化）无法用
    // canvasOff/zoomTransform 补偿，必须立即重建；绕过叠加期门控与防抖守卫
    //（调用时机已保证几何最终）。缩放补间中仍走 finishZoomTweenNow 推到最终值。
    Q_INVOKABLE void flushLinkEndpointsRebuild();
    // 顶层祖先的 EqPos：编组成员的 EqPos 是组内相对/过期基准，其世界锚定是
    // 顶层祖先（组被拖拽时顶层 EqPos 才会更新）。端点世界缓存的相对偏移以它为锚，
    // 否则"rel + 成员EqPos"仍然冻结（编组成员连线不跟随的根因）。
    // 顶层节点返回自身 EqPos；沿 IncludedByModule 上溯（带深度防护）。
    Q_INVOKABLE QPointF topAncestorEqPos(qulonglong sectionId) const;
    // 懒加载 culling 状态同步：shell（WorkspaceView delegate）的 Loader 激活/停用时调用。
    // 被 culled 的节点没有 delegate 回写，其屏幕坐标来源（m_sectionAcceptPoint /
    // Session LastCenter / 行级 acceptCenter）全部过期，端点表重建时必须跳过这些来源、
    // 落到 EqPosToRePos 兜底（按当前视口实时计算，永远新鲜），否则平移后视口外节点
    // 的连线端点停留在旧屏幕位置（大偏差根因）。
    // 激活（culled=false）后 delegate 创建 → 回写恢复精确端点。
    Q_INVOKABLE void setSectionCulled(qulonglong sectionId, bool culled);
    // 多选拖拽松手后过渡期查询：松手到收尾 rebuild 完成前，所有被拖节点需继续叠加
    // dragOffset 防连线弹回（单节点用 m_lastDraggedId，多选用此集合）。详见 buildSectionMap。
    Q_INVOKABLE bool isLastMassDragged(qulonglong sectionId) const;
    // 判断 sectionId 是否在 groupId（虚拟块）的编组内（含自身/任意层级嵌套子块）。
    // 拖动编组块时整组一起移动，内部子模块连线的 pa/pb 也用同一 dragOffset 跟随，
    // 否则子模块连线停在原地（子模块不在 sections 快照、dstSec 为 undefined 不叠加偏移）。
    Q_INVOKABLE bool isInComposedOf(qulonglong groupId, qulonglong sectionId) const;
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
    // 按 acceptor 键判定的类型校验（照抄 ImGui Acceptor_CheckLinkType(srcReg, KeyAcceptRegType, ...)，IBR_Misc.cpp:154）
    // destKey 为 acceptor 键名，命中其 AcceptType.AcceptRegType 作为目标寄存器类型
    Q_INVOKABLE int checkMergePreviewKey(qulonglong sourceId, qulonglong destId, const QString &destKey, const QString &linkKey);
    Q_INVOKABLE QString mergePreviewTextKey(qulonglong sourceId, qulonglong destId, const QString &destKey, const QString &linkKey);
    // 获取拖拽预览文本（对应 DragConditionText / DragConditionTextAlt）
    Q_INVOKABLE QString mergePreviewText(qulonglong sourceId, qulonglong destId, const QString &linkKey, bool isLinkDrag);
    // 获取连线拖拽的源预览标签（对应 ImGui BeginDragDropSource 源文本，IBR_LinkNode.cpp:570-573）
    // 返回 "Ini -> DisplayName : KeyName"（继承连为 GUI_InheritTo 格式）
    Q_INVOKABLE QString lineDragSourcePreview(qulonglong sectionId, const QString &keyName);
    // 标题栏节点拖拽源预览（对应 ImGui IBR_SectionData.cpp:767 "Ini -> DisplayName"，
    // 与 lineDragSourcePreview 不同：无 ": KeyName" 后缀，用于合并拖拽的源标签）
    Q_INVOKABLE QString headDragSourcePreview(qulonglong sectionId);

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

    // ===== 特殊块创建（补全：ImGui GUI_CreateCommentBlock/GUI_CreateSingleValBlock）=====
    // 在空白右键处创建注释块/单值块（对应 IBR_WorkSpace.cpp:663-674 SendToR -> CreateCommentBlock(EqMouse)）。
    // 参数为视图视口局部坐标 RePos（空白右键弹菜单处），由本方法换算成 Eq 后再创建。
    Q_INVOKABLE void createCommentBlock(qreal reX, qreal reY);
    Q_INVOKABLE void createSingleValBlock(qreal reX, qreal reY);

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
    // 复制指定节点到剪贴板（对应 IBR_SectionData::CopyToClipBoard，不改选择状态）
    Q_INVOKABLE void copySection(qulonglong sectionId);
    Q_INVOKABLE void paste();
    Q_INVOKABLE void duplicateSelected();
    Q_INVOKABLE void selectAll();
    // 反选：遍历所有 section，取当前未选中的作为新 MassTarget（对应 ImGui 列表 SelectInvert
    // 直接遍历改 Dynamic.Selected；Qt 版统一走 MassSelect + inputState + selectedRevision）
    Q_INVOKABLE void selectInvert();
    // 单行 toggle 勾选：列表 checkbox 点击时调用，toggle 该 id 的选中状态
    // 单选/多选语义由结果数量决定：0→Normal，1+→MassAfter（蓝框）
    Q_INVOKABLE void toggleSelectSection(qulonglong sectionId);
    Q_INVOKABLE void composeSelected();
    Q_INVOKABLE void freezeSelected(bool frozen);
    Q_INVOKABLE void hideSelected(bool hidden);
    Q_INVOKABLE void ignoreSelected(bool ignored);
    // 导出选中模块为自定义模块（对应 ImGui GUI_ExportModule → OutputSelected）
    // 由 QML 导出模块对话框填写名称/描述/路径后调用
    Q_INVOKABLE bool outputSelectedModule(const QString &name, const QString &desc, const QString &path);
    // 请求打开"导出模块"对话框（ContextMenu 调用 → emit outputModuleRequested → Main.qml 打开）
    Q_INVOKABLE void requestOutputModuleDialog();
    // 获取默认导出路径（对应 ImGui OutputSelected 的 IBB_ModuleAltDefault::GenerateModulePath）
    // 供导出模块对话框初始化路径输入框
    Q_INVOKABLE QString defaultModuleExportPath() const;
    // 获取鼠标全局（屏幕）坐标：供级联菜单"鼠标移出自动收起"轮询
    Q_INVOKABLE QPointF globalMousePos() const;
    // 是否按住鼠标左键：供提示框"点击任意处即收起"轮询（对齐 imgui 点击后 IsItemHovered 变 false）
    Q_INVOKABLE bool leftButtonDown() const;

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
    // 增量刷新链接数据（连线/取消连线后调用，按后端重建 LinkList/m_links，
    // 阻止 refreshFromTimer 因计数变化触发全量 refreshSections（行重建期间坐标污染））
    void refreshLinkIncremental();
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
    // 记录工作区视口在全局(屏幕)坐标系中的左上角位置（QML WorkspaceView 布局后上报）。
    // 用于侧边栏模块拖放：把鼠标全局坐标换算成工作区视口局部坐标再放置。
    Q_INVOKABLE void setViewportGlobal(qreal gx, qreal gy);
    // 判断某个全局(屏幕)坐标是否落在工作区视口内
    Q_INVOKABLE bool viewportContainsPoint(qreal gx, qreal gy) const;
    // 把全局(屏幕)坐标换算为工作区视口局部坐标（WorkSpaceUL 原点）
    Q_INVOKABLE QPointF globalToViewport(qreal gx, qreal gy) const;
    // 最近拖拽结束的节点 ID（LinkRenderer buildSectionMap 据此在端点表重建前继续叠加 dragOffset）
    // 哨兵值为 INVALID_MODULE_ID（非 0，避免与合法 sectionId 0 冲突），QML 侧用 hasLastDragged 守卫
    Q_INVOKABLE qulonglong lastDraggedId() const { return m_lastDraggedId; }
    Q_INVOKABLE bool hasLastDragged() const { return m_lastDraggedId != INVALID_MODULE_ID; }
    // 诊断模式开关：编译时由 INIWEAVER_DIAG 宏决定，QML 用它门控 console.log
    Q_INVOKABLE bool diagLogEnabled() const
    {
#ifdef INIWEAVER_DIAG
        return true;
#else
        // 关闭所有 QML 诊断（REFRESH/LINK/ONSHOW/IIF-DIAG 高频刷屏，淹没 C++ 写回诊断）
        return false;
#endif
    }

    // ===== 渲染性能探针（[PERF-DIAG]，仅 INIWEAVER_DIAG 诊断构建编译进实现；普通构建零开销）=====
    // QML 渲染函数（Canvas onPaint / 坐标回写等）用 perfBegin/perfEnd 包裹计时，
    // perfFrame 由 QML window.afterRendering 每渲染帧调用；每 120 帧输出一次聚合统计到日志，
    // 按总耗时排序，配合帧间隔统计定位"什么最影响帧率"。
    Q_INVOKABLE void perfBegin(const QString &tag);
    Q_INVOKABLE void perfEnd();
    Q_INVOKABLE void perfFrame();

signals:
    void ratioChanged();
    void eqCenterChanged();
    void viewCenterChanged();
    void workspaceRectChanged();
    void projectOpenChanged();
    void inputStateChanged();
    void sectionsChanged();
    // D20：section 数据结构版本变化（refreshSections/项目关闭），独立于 sectionsChanged，
    // 避免 reportSectionSize 的几何刷新连带触发子模块数据重查导致重入死循环
    void sectionDataRevisionChanged();
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
    // 画布平移偏移变化通知（拖动画布时驱动 LinkRenderer 叠加渲染，端点表不重建）
    void canvasDragOffsetChanged();
    // 视口偏移变化通知（调宽/resize 时驱动 LinkRenderer 叠加渲染，端点表不重建）
    void viewportOffsetChanged();
    // 缩放叠加基准/状态变化通知（缩放时驱动 LinkRenderer 换算端点坐标，端点表不重建）
    void zoomBaseChanged();
    // 缩放叠加结束通知（QML SectionNode 据此用 Qt.callLater 回写缩放后的行圆点坐标）
    void zoomFinalizeRequested();
    // 缩放补间启动/续接请求（QML NumberAnimation 据此从当前 Ratio 平滑过渡到 zoomTargetRatio；
    // 动画在 QML 渲染线程驱动，与帧同步，避免 C++ 定时器动画与渲染循环不同步跳帧）
    void zoomTweenRequested();
    // 缩放补间目标变化通知（QML 动画在 zoomTweenRequested 时读取新目标）
    void zoomTargetRatioChanged();
    // 缩放补间强制中止请求（收尾路径清 zoomPending 前调用：QML 立即停止动画，
    // Ratio 已由 C++ 直接推到目标，动画不再逐帧改值）
    void zoomTweenAbortRequested();
    // 阶段 12.7：双击空白触发模块搜索
    void moduleSearchRequested(qreal x, qreal y);
    // v3 批次 1.4：拖拽 LinkLimit=0 节点时 DropArea 接收状态通知
    // 对应 ImGui IsDragDropPayloadBeingAccepted（IBR_SectionData.cpp:96-106）
    void dragInvalidLinkChanged(bool invalid);
    // 拖拽目标命中变化通知（QML SectionNode 据此绑定预览框显示）
    void dragTargetChanged();
    // 行为A：acceptor 键行拖拽高亮变化通知（QML LineRow 据此画/清整行高亮框）
    void dragAcceptorRowChanged();
    // 连线拖拽开始/结束（驱动预览框是否有源标签显示）
    void draggingActiveChanged();
    // 拖拽节点 ID 变化通知（QML 据此决定是否应用 dragOffset）
    void draggingSectionIdChanged();
    // 多节点拖拽状态变化通知
    void massDraggingChanged();
    // 选中状态变化通知（轻量级，不触发 sectionsChanged）
    void selectedRevisionChanged();
    // 单节点位置变化通知（拖拽结束时不调全量 refresh，只通知 QML 更新对应节点）
    void sectionPositionChanged(qulonglong sectionId);
    // 请求打开"导出模块"对话框（对应 ImGui GUI_ExportModule 弹窗）
    void outputModuleRequested();

private:
    // 关联控制器（由 QtMain.cpp 注入，用于选中模块时切换侧边栏）
    class EditPanelController *m_editPanelController{nullptr};
    class MenuController *m_menuController{nullptr};

    // 状态机：0=Normal, 1=BgDragging, 2=MassSelecting, 3=HoldingModules, 4=MassAfter
    // 对应 IBR_WorkSpace.cpp:822-830 状态机注释
    int m_inputState{0};
    QVariantList m_sections;
    // 增量 Section 模型（供 QML Repeater，避免整表替换重建全部节点）
    WorkspaceSectionModel *m_sectionsModel{nullptr};
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
    // 哨兵值用 INVALID_MODULE_ID（非 0，避免与合法 sectionId 0 冲突，与 m_draggingSectionId 一致）
    ModuleID_t m_dragSectionId{INVALID_MODULE_ID};
    ImVec2 m_dragStartEqPos{0, 0};
    QPointF m_dragStartScreenPos;
    // 性能优化：拖拽偏移量（屏幕坐标），拖拽中实时更新，QML 端用它计算临时位置
    // 避免每帧 refresh() 全量重建 QVariantList
    QPointF m_dragOffset;
    // 帧率节流：待应用的拖拽/平移位置
    // onMouseMove/updateDrag 只记录最新位置并置位，帧定时器 tick（refreshFromTimer）统一应用
    QPointF m_pendingDragPos;
    bool m_hasPendingDrag{false};
    // 画布平移偏移（屏幕像素）：拖动画布时端点表保持快照，LinkRenderer 叠加此偏移渲染
    // 避免每帧全量端点表重建 + 行圆点回写（拖动画布帧率被拖垮），下次端点表重建时清零
    QPointF m_canvasDragOffset;
    // 视口偏移（屏幕像素）：调宽/resize 时端点表快照落后模块，LinkRenderer 叠加此偏移
    // 实时对齐；端点表重建（rebuildLinkEndpoints）时用当前视口更新基准并归零
    qreal m_viewportOffsetX{0};
    qreal m_viewportOffsetY{0};
    // 端点表重建时的视口基准尺寸（计算 viewportOffset = (当前 - 基准)/2）
    qreal m_endpointBaseW{0};
    qreal m_endpointBaseH{0};
    // 工作区视口几何（用于侧边栏模块拖放换算：全局坐标 → 视口局部坐标）
    qreal m_viewportGX{0}, m_viewportGY{0}, m_viewportW{0}, m_viewportH{0};
    // 缩放叠加基准与状态（仿 m_canvasDragOffset）：缩放中端点表保持快照不重建，
    // LinkRenderer 按 m_zoomBaseRatio/m_zoomBaseCenter 基准换算端点坐标（等比+中心偏移）；
    // 缩放停止后由 scheduleZoomFinalize 防抖重建端点表结束叠加
    float m_zoomBaseRatio{0};
    QPointF m_zoomBaseCenter;
    bool m_zoomPending{false};
    int m_zoomFinalizeToken{0};  // 防抖 token：新缩放事件使旧定时回调失效
    // 滚轮缩放补间（QML 端驱动）：滚轮只设定目标 Ratio 与锚点并 emit zoomTweenRequested，
    // QML NumberAnimation 每渲染帧回调 applyZoomRatio 原子应用 ——
    // 缩放与拖拽基准修正/dragOffset 重算/预览线重发在同一回调（同一渲染帧）内完成，
    // 消除"缩放与拖拽分帧更新"的中间态偏移；连续滚轮更新目标续接，不重启跳变。
    // 补间搬 QML 后动画由 QQuickWindow 动画驱动推进（与渲染帧同步），
    // 避免 C++ QVariantAnimation 定时器 tick 与渲染循环不同步导致的跳帧/低帧率。
    bool m_zoomAnimating{false};  // 补间运行中（QML 动画期间为 true，替代 QVariantAnimation 状态查询）
    float m_zoomTargetRatio{0};
    QPointF m_zoomAnchorScreen;  // 补间锚点（滚轮时鼠标位置，连续滚轮取最新）
    // 收尾路径清 zoomPending 前必须 finishZoomTweenNow 强制完成补间（Ratio 推目标 + 通知 QML 停动画）
    void finishZoomTweenNow();
    // 调度缩放结束后的端点表重建（防抖：最后一次缩放事件后 delayMs 再重建）
    void scheduleZoomFinalize();
    // 缩放叠加收尾：清 pending → 通知 QML 回写缩放后坐标 → Queued 重建端点表（先回写后重建）
    void finalizeZoom();
    // v3 批次 1.4：拖拽 LinkLimit=0 节点时 DropArea 接收状态
    bool m_dragInvalidLink{false};
    // 拖拽目标命中状态（命中测试驱动，拖拽源 onPositionChanged 更新，clearDraggingLink 清零）
    qulonglong m_dragTargetSectionId{INVALID_MODULE_ID};
    QString m_dragTargetColor;
    QString m_dragTargetText;
    QString m_dragSourceText;
    // 行为A：acceptor 键行拖拽高亮（QML LinkNodePoint 命中 acceptor 行时设置，clearDraggingLink 清零）
    qulonglong m_dragAcceptorSectionId{INVALID_MODULE_ID};
    QString m_dragAcceptorRowKey;
    QColor m_dragAcceptorColor;
    // 拖拽中的节点 ID（单节点拖拽，INVALID_MODULE_ID=无拖拽）
    // 修复：不能用 0 作为"无拖拽"哨兵值，因为 0 是合法的 sectionId（第一个新建模块 ID=0）
    // 否则第一个新建模块会因 sectionId(0) === draggingSectionId(0) 永远显示 isDragging 蓝框
    qulonglong m_draggingSectionId{INVALID_MODULE_ID};
    // 多节点拖拽标志（MassAfter → HoldingModules 时为 true）
    bool m_massDragging{false};
    // 选中版本号：每次 MassTarget 变化时递增，避免全量 refresh
    int m_selectedRevision{0};
    // 进入编辑侧边栏（EDIT=4）前记录的菜单 ID；取消选中后 restoreMenuAfterDeselect 据此恢复
    // 用户要求：取消选中回到"选中前的侧边栏"，而非固定 MODULES
    int m_prevMenuBeforeEdit{0};
    bool m_hasPrevMenuBeforeEdit{false};

    // 最近一次拖拽结束的节点 ID（LinkRenderer 据此继续叠加 dragOffset，防松手连线弹）
    // 哨兵值用 INVALID_MODULE_ID（非 0）：0 是合法 sectionId，会与"无最近拖拽"语义冲突，
    // 导致块 0 被误判为"刚拖过"，其连线端点跟着其他模块的 dragOffset 串动
    qulonglong m_lastDraggedId{INVALID_MODULE_ID};

    // 多选拖拽松手后的被拖节点集合（过渡期防弹回）
    // 单节点拖动用 m_lastDraggedId（单值）即可；多选拖动有 N 个节点，松手后节点立即跳终点
    //（sectionPositionChanged 同步 emit），但端点表 rebuild 是 Queued 异步，中间几帧需靠
    // buildSectionMap 对所有被拖节点叠加 dragOffset 让端点=旧基准+dragOffset=终点与节点一致。
    // 若只记 front（m_lastDraggedId），非 front 节点端点不叠加 → 用旧基准，节点已终点 → 偏移。
    // cleanup（收尾 rebuild 完成清 dragOffset）时一并清空。
    QHash<qulonglong, bool> m_lastMassDragIds;

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
    // 拖拽结束后同步 m_sections 快照中指定模块的 eqX/eqY（画布节点位置由 QML 实时绑定
    // 驱动，不重建 m_sections；但 MiniMap 读的正是 m_sections 快照 → 不更新则迷你地图
    // 模块位置不刷新）。更新后由调用方 emit sectionsChanged 触发 MiniMap 重绘。
    void syncSectionSnapshotPos(ModuleID_t id);
    // 清空选中/编辑状态（对应 ImGui IBR_WorkSpace.cpp:1076-1079 Clear() + ChooseMenu(MenuItemID_MODULES)）
    // 若仍选中单个有效模块（多选取消/删除后剩余一个）→ 显示该模块的编辑侧边栏；
    // 无选中 → 清空编辑面板、菜单恢复到选中前的侧边栏（restoreMenuAfterDeselect）
    // 调用点：toggleSelectSection 取消选中最后一个模块、refreshFromTimer 检测到当前编辑模块被删除
    void clearEditSelection();
    // 切换到编辑侧边栏（EDIT=4）：记录切换前的菜单（供取消选中后恢复），再 setActiveMenu(4)
    void enterEditMenu();
    // 取消选中后恢复侧边栏：当前菜单是 EDIT 时切回进入编辑前记录的菜单（无记录兜底 MODULES）
    void restoreMenuAfterDeselect();

    // 阶段 D2：构建单个 Section 的 QVariantMap（refreshSections 与 getSectionData 共用）
    // id=Section ID, sd=SectionData 引用, selectedSet=MassTarget 选中集合
    QVariantMap buildSectionItem(ModuleID_t id, const IBR_SectionData &sd,
                                 const std::unordered_set<ModuleID_t> &selectedSet) const;

    // 阶段 12.2：多节点拖拽辅助
    void initMassDrag();  // 计算质心 + 各节点 EqDelta（对应 IBR_WorkSpace.cpp:1004-1024）
    void updateMassDrag(qreal curX, qreal curY);  // 整组平移（对应 UpdateScrollAlt）
    void endMassDrag();   // 清除 Dragging 标志
    // 帧率节流：将待处理的拖拽/平移位置延迟到帧定时器（QTimer @ FrameRateLimit）统一应用
    // 使拖动画布/模块时的实际渲染帧率与帧率设置一致（否则 SceneGraph 跟随鼠标事件率渲染跑满本机帧率）
    void applyPendingDrag();

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
    bool m_pendingSizeRebuild{false};  // reportSectionSize 延迟重建已排队标志，防 onWidth/onHeight 都触发时重复排队
    // sectionId → m_sections 行索引：reportSectionSize / syncSectionSnapshotPos 用它 O(1) 定位
    // 快照项，替代此前每次回调 O(n) 全表扫描（导入 1471 模块时 8460 次回调 × O(n) 扫描 +
    // 每次 emit 触发 MiniMap 全量刷新 = 577s，部署日志 [PERF-DIAG] 实测的导入卡顿根因）。
    // refreshSections 重建 m_sections 后同步重建；项目关闭路径清空。
    QHash<qulonglong, int> m_sectionRowIndex;
    // reportSectionSize 待合并通知集合（sectionId → 行索引）：同一事件循环内的多次尺寸回报
    // 只在循环末尾 emit 一次 sectionsChanged（MiniMap 的 sections 绑定是全量列表，每 emit
    // 都重转 JS + 全量重绘；delegate 创建期高度分多步增长，合并后 emit 次数从 ~7000 → 每
    // 事件循环 1 次）。flushPendingSectionSizes 统一发出。
    QHash<qulonglong, int> m_pendingSizeRows;
    bool m_pendingSizeFlushScheduled{false};  // 防同一事件循环内重复排队 flush
    // 松手过渡期抑制 tick 的端点表重建：
    // 松手后 dragOffset 尚未清零时，若 tick 提前重建端点表，LinkRenderer 会叠加残留
    // dragOffset 显示连线（偏移一帧）。此标志抑制 tick 重建，仅允许松手收尾的最终重建。
    bool m_suppressLinkRebuild{false};
    // 防止多次 linkNodeCenterChanged 重复排队重建（缩放/平移时大量 LineRow 回写）
    bool m_pendingRebuild{false};

    // 行模型缓存：每个 Section 对应一个 SectionLineModel
    // 在 refreshSections 时创建/更新，QML 通过 sectionData.lineModel 访问
    // 标记 mutable：buildSectionItem 是 const 方法（getSectionData 复用），但需要懒创建缓存
    mutable QHash<ModuleID_t, QPointer<SectionLineModel>> m_lineModels;

    // 重建连线端点表（从 IBR_NodeSession 全局表同步 LastCenter）
    void rebuildLinkEndpoints();

    // 发出被合并的 reportSectionSize 尺寸通知：本事件循环内所有尺寸回写完成后
    // 只 emit 一次 sectionsChanged（QtMain QTimer tick 或 QML 事件处理结束后排队触发）
    void flushPendingSectionSizes();

    // 增量刷新单条链接端点：改变连线状态（拖拽建链等）后只更新该链接 pa/pb，
    // 避免全量 rebuild 读到行模型 resetModel 重建期间的污染坐标（连线全跑模块第一节点）
    void refreshLinkEndpoint(qulonglong srcId, const QString &fromKey, int mult, qulonglong dstId);

    // D11：计算当前所有 Section 的状态哈希（Frozen/Hidden/Ignore/UICollapsed/CollapsedInComposed + ShowRegName）
    // 用于 refreshFromTimer 脏检查，检测节点状态变化
    size_t computeStateHash() const;

    // 阶段 3：每个 section 的标题栏 RadioButton 屏幕坐标（非折叠态接受点）
    // 由 QML SectionNode header headLineRN 回写，供 rebuildLinkEndpoints 构建 pb
    QHash<qulonglong, QPointF> m_sectionAcceptPoint;
    // 懒加载 culling 集合（见 setSectionCulled 注释）：这些节点的 QML 回写坐标已过期，
    // rebuildLinkEndpoints 对其跳过 LastCenter/行级接受点来源，落 EqPosToRePos 兜底
    QSet<qulonglong> m_culledSections;
    // ===== 懒加载端点世界坐标镜像 =====
    // 被 cull 的节点没有 delegate 回写，屏幕坐标回写（上两族缓存 + Session LastCenter）
    // 会随平移/缩放过期。三处回写在写入屏幕坐标的同时换算存储**世界坐标（Eq 空间）**，
    // rebuildLinkEndpoints 对 cull 节点按当前视口再投影 → cull 期间平移/缩放后端点仍然
    // 精确（此前兜底锚到标题栏，行离标题栏越远偏差越大；编组成员 EqPos 为组内相对坐标
    // 时偏差更大）。只清于项目关闭（session id 全局递增，跨项目无碰撞）。
    // 两个缓存存的是【相对节点 EqPos 的偏移】（写入时 = 世界坐标 − 当时 EqPos）：
    // 节点被 cull 后 EqPos 仍会变（批量拖拽/撤销等），绝对世界坐标会永久滞后；
    // 相对偏移 + 当前 EqPos 再投影，节点怎么动缓存都自动跟随
    QHash<qulonglong, QPointF> m_sessionCenterEq;       // sessionId → 行圆点相对偏移
    QHash<qulonglong, QPointF> m_sectionAcceptPointEq;  // sectionId → 头部接受点相对偏移
    // 壳渲染位置上报（noteShellRenderPos）：EP-DIAG 对账用
    QHash<qulonglong, QPointF> m_shellRenderPos;        // sectionId → 实际渲染屏幕坐标
    QHash<qulonglong, QPointF> m_shellRenderEq;         // sectionId → 实际渲染世界坐标
    QPointF sessionCenterEq(qulonglong sessionId) const;
    QPointF sectionAcceptPointEq(qulonglong sectionId) const;

#ifdef INIWEAVER_DIAG
    // ===== 渲染性能探针状态（[PERF-DIAG]，仅诊断构建编译）=====
    // mutable：isSectionSelected 等只读热点也挂探针（const 方法内收集统计），
    // 统计写入不改变对象的逻辑状态
    struct PerfStat { quint64 count = 0; qint64 totalNs = 0; qint64 maxNs = 0; };
    QElapsedTimer m_perfClock;            // 全局纳秒时钟（构造即启动，探针取差值）
    mutable QMutex m_perfMutex;           // 保护探针状态：Canvas onPaint 在渲染线程、坐标回写在 GUI 线程并发访问
    mutable QHash<QString, PerfStat> m_perfStats; // tag → 聚合统计
    QStringList m_perfTagStack;           // 嵌套探针标签栈（perfBegin 压入，perfEnd 弹出）
    QVector<qint64> m_perfStartStack;     // 嵌套探针开始时间戳栈（nsecs）
    quint64 m_perfFrames{0};              // 当前窗口已渲染帧数
    qint64 m_lastFrameNs{-1};             // 上一帧 afterRendering 时刻（-1=首帧，无间隔）
    qint64 m_frameTotalNs{0};             // 窗口内帧间隔累计（ns）
    qint64 m_frameMaxNs{0};               // 窗口内最差帧间隔（ns）
    void dumpPerfStats();                 // 输出 [PERF-DIAG] 汇总（按总耗时排序）并重置窗口
#endif
};
