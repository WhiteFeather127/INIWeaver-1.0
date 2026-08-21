// WorkspaceController.cpp
// 工作区控制器实现：桥接 IBR_WorkSpace / IBR_FullView / IBR_RealCenter
#include "WorkspaceController.h"
#include "WorkspaceSectionModel.h"
#include "EditPanelController.h"
#include "MenuController.h"
#include "Global.h"
#include "IBR_Project.h"
#include "IBR_Misc.h"
#include "IBB_RegType.h"
#include "IBB_PropStringPool.h"
#include "IBB_Ini.h"
#include "IBG_UndoTree.h"
#include "SectionLineModel.h"
#include "IBR_LinkNode.h"
#include "IBB_ModuleAlt.h"
#include "FromEngine/ImGuiDeps.h"
#include "FromEngine/global_tool_func.h"
#include <QGuiApplication>
#include <QFontMetrics>
#include <QTimer>
#include <QFile>
#include <QTextStream>
#include <QtDebug>
#include <QCursor>
#include <unordered_set>
#include <algorithm>
#include <functional>
#include <cmath>
#include <limits>

// 对齐 imgui：连线拖拽类型不符时生成带"连线类型 + 目标注册名"的预览文字（GUI_Preview_WrongType）
// 定义于 IBR_SectionData.cpp:465，IBR_Misc.cpp:54 也仅作本地前向声明，这里补充声明供调用
void Acceptor_RefusePreview(StrPoolID SourceReg, StrPoolID TargetReg, StrPoolID LinkType);

// 阶段 12.4：前向声明 IBR_SectionData.cpp 中的自由函数（全局作用域）
// 对应 IBR_SectionData.cpp:438-464
bool Acceptor_CheckLinkType(StrPoolID SourceReg, StrPoolID TargetReg, StrPoolID LinkType);
bool Acceptor_CheckSecType(StrPoolID SourceReg, StrPoolID SecType);

// 前向声明 IBR_WorkSpace.cpp 内 Qt 导出模块函数（该 namespace 无头文件）
namespace IBR_WorkSpace {
    void ExportSelectedModuleQt(const std::string &Utf8Name, const std::string &Utf8Desc, const std::string &Utf8Path);
}

WorkspaceController::WorkspaceController(QObject *parent)
    : QObject(parent)
{
    // 初始化 MassAfter 右键按下位置为"未按下"（对应 FLT_MAX）
    m_massAfterRightDownPos = QPointF(std::numeric_limits<qreal>::max(),
                                       std::numeric_limits<qreal>::max());
    // C11：Paste 节流计时器
    m_uptimeTimer.start();

    // 增量 Section 模型（QML Repeater 使用，整表替换会重建全部节点）
    m_sectionsModel = new WorkspaceSectionModel(this);
}

float WorkspaceController::ratio() const
{
    return IBR_FullView::Ratio;
}

QPointF WorkspaceController::eqCenter() const
{
    return QPointF(IBR_FullView::EqCenter.x, IBR_FullView::EqCenter.y);
}

QPointF WorkspaceController::viewCenter() const
{
    return QPointF(IBR_RealCenter::Center.x, IBR_RealCenter::Center.y);
}

QRectF WorkspaceController::workspaceRect() const
{
    return QRectF(
        IBR_RealCenter::WorkSpaceUL.x,
        IBR_RealCenter::WorkSpaceUL.y,
        IBR_RealCenter::WorkSpaceDR.x - IBR_RealCenter::WorkSpaceUL.x,
        IBR_RealCenter::WorkSpaceDR.y - IBR_RealCenter::WorkSpaceUL.y
    );
}

// 视口在 Eq 坐标系的矩形（MiniMap 用）
// RePosToEqPos: (RePos - Center) / Ratio + EqCenter
// 视口左上角 Eq = RePosToEqPos(WorkSpaceUL)，右下角 Eq = RePosToEqPos(WorkSpaceDR)
QRectF WorkspaceController::viewportEqRect() const
{
    ImVec2 ulEq = IBR_WorkSpace::RePosToEqPos(IBR_RealCenter::WorkSpaceUL);
    ImVec2 drEq = IBR_WorkSpace::RePosToEqPos(IBR_RealCenter::WorkSpaceDR);
    return QRectF(ulEq.x, ulEq.y, drEq.x - ulEq.x, drEq.y - ulEq.y);
}

bool WorkspaceController::isProjectOpen() const
{
    return IBR_ProjectManager::IsOpen();
}

qulonglong WorkspaceController::focusedSectionId() const
{
    return static_cast<qulonglong>(IBR_EditFrame::CurSection.ID);
}

QPointF WorkspaceController::eqToScreen(QPointF eqPos) const
{
    ImVec2 result = IBR_WorkSpace::EqPosToRePos(ImVec2(eqPos.x(), eqPos.y()));
    return QPointF(result.x, result.y);
}

QPointF WorkspaceController::screenToEq(QPointF screenPos) const
{
    ImVec2 result = IBR_WorkSpace::RePosToEqPos(ImVec2(screenPos.x(), screenPos.y()));
    return QPointF(result.x, result.y);
}

qulonglong WorkspaceController::hitTestSection(qreal screenX, qreal screenY) const
{
    // 鼠标屏幕坐标 → eq 坐标（与 updateSelection 框选命中同款换算）
    QPointF mouseEq = screenToEq(QPointF(screenX, screenY));
    // 从后向前遍历顶层 sections（后渲染的在上层，命中优先级高），对应 Repeater 绘制顺序
    // 注意：m_sections 的 eqX/eqY/eqW/eqH 是上次 refreshSections 的快照，reportSectionSize
    // 更新 EqSize 后不会重建列表 → 尺寸/位置会过期（高度偏大导致"模块下方一段距离也误判"）。
    // 因此仅用 m_sections 确定顶层 sectionId 集合与 hidden 状态，实际矩形读 IBR_SectionMap 实时值。
    for (int i = m_sections.size() - 1; i >= 0; --i) {
        const QVariantMap map = m_sections.at(i).toMap();
        if (map["hidden"].toBool()) continue;
        const qulonglong id = map["sectionId"].toULongLong();
        auto sec = IBR_Inst_Project.GetSectionFromID(static_cast<ModuleID_t>(id));
        auto data = sec.GetSectionData();
        if (!data) continue;
        const qreal x = data->EqPos.x;
        const qreal y = data->EqPos.y;
        const qreal w = data->EqSize.x;
        const qreal h = data->EqSize.y;
        if (w <= 0 || h <= 0) continue;  // 尺寸未回写时跳过
        if (mouseEq.x() >= x && mouseEq.x() <= x + w &&
            mouseEq.y() >= y && mouseEq.y() <= y + h)
            return id;
    }
    return 0;
}

void WorkspaceController::onMousePress(qreal x, qreal y, int button)
{
    // 帧率节流：按下前先应用待处理位置，避免状态切换读到过期坐标
    applyPendingDrag();
    // 阶段 12.1：完整状态机（对应 IBR_WorkSpace.cpp:822-830 状态迁移图）
    if (button == Qt::LeftButton) {
        if (m_inputState == 0) {
            // v3 批次 2.1：进入 BgDragging 前保存视口前态（对应 MainStage.h:122 UpdatePrev）
            IBR_WorkSpace::UpdatePrev();
            // 缩放防抖窗口内（zoomPending 未收尾）开始拖动画布：立即终止缩放换算叠加
            // 并重建端点表。否则平移中连线先按 zoomTransform 的 (C_b-C_c)*r_c 项随
            // eqCenter 移动一次，再叠加 canvasOffset 又移动一次 → 连线偏移为画布的两倍。
            // 收尾顺序与 finalizeZoom 一致：先 emit zoomFinalizeRequested（QML callLater
            // 回写缩放后行圆点，先入队），再 Queued 重建端点表（读到回写后的缩放后坐标）。
            if (m_zoomPending) {
                finishZoomTweenNow();
                m_zoomPending = false;
                emit zoomBaseChanged();
                emit zoomFinalizeRequested();
                QMetaObject::invokeMethod(this, [this]() {
                    // 平移中重建：与 finalizeZoom 的 Queued 不同，此处主动触发，
                    // 不因 inputState==1 拦截（缩放换算必须先于 canvasOffset 叠加终止）
                    m_linkEndpointsDirty = true;
                    if (!m_suppressLinkRebuild)
                        rebuildLinkEndpoints();
                }, Qt::QueuedConnection);
            }
            // Normal → BgDragging（空白处左键拖拽平移画布）
            updateInputState(1);  // BgDragging
            m_dragStartScreen = QPointF(x, y);
        } else if (m_inputState == 4) {
            // MassAfter 状态下左键按下空白处 → MassAfter → Normal
            // 对应 IBR_WorkSpace.cpp:1150-1155
            // 若命中节点，SectionNode MouseArea 会调用 beginMassDrag 而非走到这里
            clearSelection();
            updateInputState(0);
        } else if (m_inputState == 3) {
            // HoldingModules 中左键按下且非 Wait → 取消拖拽
            // 对应 IBR_WorkSpace.cpp:1034-1043
            if (!m_hasLeftDownToWait) {
                endMassDrag();
                // 修复：拖拽结束后若仍有选中模块，回到 MassAfter 而非 Normal
                restoreStateAfterDrag();
            }
        }
    } else if (button == Qt::RightButton) {
        if (m_inputState == 0) {
            // Normal → MassSelecting（右键按下开始框选）
            // 对应 IBR_WorkSpace.cpp:986-996
            startSelection(x, y);
            updateInputState(2);
        } else if (m_inputState == 4) {
            // MassAfter 中右键按下：记录位置，后续移动>5px 则重新框选
            // 对应 IBR_WorkSpace.cpp:1156-1168
            m_massAfterRightDownPos = QPointF(x, y);
        } else if (m_inputState == 3) {
            // HoldingModules 中右键按下：
            // - MoveAfterMass=true → 仅取消拖拽
            // - MoveAfterMass=false → 删除拖拽中节点
            // 对应 IBR_WorkSpace.cpp:1044-1072
            if (m_moveAfterMass) {
                endMassDrag();
                m_moveAfterMass = false;
                // 修复：拖拽结束后若仍有选中模块，回到 MassAfter 而非 Normal
                restoreStateAfterDrag();
            } else {
                // 删除所有拖拽中节点
                IBRF_CoreBump.SendToR({ []() {
                    std::vector<IBB_Section_Desc> Descs;
                    for (const auto &p : IBR_Inst_Project.IBR_SectionMap)
                        if (p.second.Dragging) Descs.push_back(p.second.Desc);
                    IBR_Inst_Project.DeleteSection(Descs);
                    IBF_Inst_Project.UpdateAll();
                } });
                m_massDragIds.clear();
                m_moveAfterMass = false;
                m_initHolding = false;
                // 修复：清除单节点拖拽的残留状态（与 endMassDrag 修复同理）
                m_dragSectionId = INVALID_MODULE_ID;
                m_draggingSectionId = INVALID_MODULE_ID;
                emit draggingSectionIdChanged();
                m_massDragging = false;
                emit massDraggingChanged();
                m_dragOffset = QPointF();
                emit dragOffsetChanged();
                updateInputState(0);
                refresh();
            }
        }
    }
}

void WorkspaceController::onMouseMove(qreal x, qreal y)
{
    if (m_inputState == 1) {
        // BgDragging：拖拽平移（对应 IBR_WorkSpace::ProcessBackgroundOpr 的 IsBgDragging 分支）
        // 帧率节流：只记录最新位置，由帧定时器 tick（applyPendingDrag）统一应用。
        // 若逐事件更新 EqCenter，SceneGraph 会跟随鼠标事件率渲染（拖动画布时模块帧率跑满本机），
        // 帧率设置对实际渲染帧率失效。延迟到帧定时器后，位置更新频率 = 帧率设置。
        m_pendingDragPos = QPointF(x, y);
        m_hasPendingDrag = true;
    } else if (m_inputState == 2) {
        // MassSelecting：更新框选矩形
        updateSelection(x, y);
    } else if (m_inputState == 4) {
        // MassAfter：检查右键是否按下且移动>5px → 重新框选
        // 对应 IBR_WorkSpace.cpp:1156-1168
        if (m_massAfterRightDownPos.x() != std::numeric_limits<qreal>::max()) {
            QPointF delta = QPointF(x, y) - m_massAfterRightDownPos;
            if (std::max(std::abs(delta.x()), std::abs(delta.y())) > 5.0) {
                // MassAfter → MassSelecting（重新框选）
                m_massAfterRightDownPos = QPointF(std::numeric_limits<qreal>::max(),
                                                  std::numeric_limits<qreal>::max());
                startSelection(x, y);
                updateInputState(2);
            }
        }
    } else if (m_inputState == 3) {
        // HoldingModules：多节点拖拽中，延迟到帧定时器应用（帧率节流）
        m_pendingDragPos = QPointF(x, y);
        m_hasPendingDrag = true;
    }
}

void WorkspaceController::onMouseRelease(qreal x, qreal y, int button)
{
    // 帧率节流：松手前应用最后一次待处理位置，确保最终位置准确提交
    applyPendingDrag();
    if (button == Qt::LeftButton) {
        if (m_inputState == 1) {
            // v3 批次 2.1：BgDragging 结束时推入 Undo 栈（对应 MainStage.h:218 UpdatePrevII）
            // UpdatePrevII 内部有阈值检查（EqCenter ≥1 / Ratio ≥0.01）和 merge 逻辑
            IBR_WorkSpace::UpdatePrevII();
            // v3 批次 3.1：平移后更新 EqMax，确保 MiniMap 可见范围正确（对应 IBR_FullView::UpdateCurrentEqMax）
            IBR_FullView::UpdateCurrentEqMax();
            // 对应 ImGui IBR_WorkSpace.cpp:1322-1329 BgDragging -> Normal 时 IBR_EditFrame::Clear()
            // 点击空白处（含单击和拖拽平移）取消选中，清空编辑侧边栏，切回 MODULES 菜单
            clearSelection();
            // 若缩放叠加残留（缩放后立刻拖动画布，finalizeZoom 已推迟收尾）：
            // 先强制完成补间（清 pending 后 Ratio 不得再变，否则连线与节点错位），再清 pending，
            // 否则 QML onInputStateChanged 的 callLater 回写会被 zoomPending
            // 拦截，Queued rebuild 读到缩放前旧坐标 → 连线错位。清完回写照常、重建读到新坐标。
            finishZoomTweenNow();
            if (m_zoomPending) m_zoomPending = false;
            // BgDragging → Normal
            updateInputState(0);
            // 画布平移结束：延迟到事件队列末尾重建端点表（对应之前"一帧回弹"的修复方法）。
            // QML 端 onInputStateChanged 已用 Qt.callLater 把平移后坐标回写排入事件队列（先入队），
            // 此 QueuedConnection 后入队 → 重建读到回写后的基准坐标，并清零 canvasDragOffset。
            // 若不延迟重建：拖模块时源行回写触发重建会读到"画布平移前"的目标行旧坐标
            // （目标行平移期间回写被跳过）→ pb 少画布平移量错位。
            QMetaObject::invokeMethod(this, [this]() {
                rebuildLinkEndpoints();
            }, Qt::QueuedConnection);
        } else if (m_inputState == 3) {
            // HoldingModules 左键释放 → 确认移动
            // 对应 IBR_WorkSpace.cpp:1073-1090
            m_hasLeftDownToWait = false;
            if (m_moveAfterMass) {
                endMassDrag();
                m_moveAfterMass = false;
                // 修复：拖拽结束后若仍有选中模块，回到 MassAfter 而非 Normal
                restoreStateAfterDrag();
            }
        }
    } else if (button == Qt::RightButton) {
        if (m_inputState == 2) {
            // MassSelecting 右键释放 → 判定移动距离
            // 对应 IBR_WorkSpace.cpp:1103-1123
            QPointF delta = QPointF(x, y) - m_dragStartScreen;
            if (std::max(std::abs(delta.x()), std::abs(delta.y())) > 2.0) {
                // 移动>2px → MassAfter
                endSelection();
                if (IBR_WorkSpace::MassTarget.empty()) {
                    updateInputState(0);  // 空选 → Normal
                } else {
                    updateInputState(4);  // MassAfter
                }
            } else {
                // 移动≤2px → 右键单击空白处：取消选中 + 空白右键菜单（对齐 ImGui OpenRightClick）
                // 单选模块后 inputState 为 Normal(0)，右键按下进入 MassSelecting；此处直接取消选中，
                // 避免 endSelection 保留选中导致 QML 侧 massTargetIds 非空误弹多选菜单
                clearSelection();
                emit contextMenuRequested(x, y);
                updateInputState(0);
            }
        } else if (m_inputState == 4) {
            // MassAfter 右键释放 → 弹出多选右键菜单
            // 对应 IBR_WorkSpace.cpp:1170-1290
            if (m_massAfterRightDownPos.x() != std::numeric_limits<qreal>::max()) {
                // 右键按下后未移动>5px，视为右键单击
                m_massAfterRightDownPos = QPointF(std::numeric_limits<qreal>::max(),
                                                  std::numeric_limits<qreal>::max());
                // 命中模块 → 多选操作菜单（保持选中）；空白处 → 取消选中 + 空白右键菜单
                // （背景 MouseArea 只能收到空白处右键，命中检测为防御性兜底）
                if (hitTestSection(x, y) != 0) {
                    showMassContextMenu(x, y);
                } else {
                    clearSelection();
                    emit contextMenuRequested(x, y);
                    updateInputState(0);
                }
            }
        }
    }
}

void WorkspaceController::onWheel(qreal x, qreal y, qreal delta)
{
    // 阶段 13.4：Ratio 离散化（对应 IBR_WorkSpace.cpp:913-918）
    // ImGui: NewRatio = floor(Ratio * exp(DeltaWheel/9) * 20) / 20
    //         if (DeltaWheel < 0) NewRatio += 0.05
    //         if (abs(NewRatio - Ratio) < 1e-6) NewRatio += DeltaWheel < 0 ? -0.05 : 0.05
    // Qt wheel delta 通常是 ±120，转为 DeltaWheel = delta / 120.0
    float deltaWheel = static_cast<float>(delta) / 120.0f;
    if (std::abs(deltaWheel) < 1e-6f) return;

    // 补间进行中：以补间目标为基准累加（连续滚动平滑趋近，而非按中间值重算目标）
    bool animRunning = m_zoomAnimating;
    float baseRatio = animRunning ? m_zoomTargetRatio : IBR_FullView::Ratio;
    float newRatio = std::floor(baseRatio * std::exp(deltaWheel / 9.0f) * 20.0f) / 20.0f;
    if (deltaWheel < 0) newRatio += 0.05f;
    if (std::abs(newRatio - baseRatio) < 1e-6f)
        newRatio += deltaWheel < 0 ? -0.05f : 0.05f;
    newRatio = std::min(newRatio, IBR_FullView::RatioMax / 100.0f);
    newRatio = std::max(newRatio, IBR_FullView::RatioMin / 100.0f);
    if (std::abs(newRatio - baseRatio) < 1e-6f) return;  // 已达边界且目标无变化

    // v3 批次 2.1：缩放前保存视口前态（对应 MainStage.h:122 UpdatePrev）
    IBR_WorkSpace::UpdatePrev();
    // 缩放叠加（仿画布平移的 canvasDragOffset）：缩放中端点表保持快照不重建，
    // 记录端点表对应的基准 ratio/eqCenter，LinkRenderer 据此把快照端点坐标换算到新 ratio 下，
    // 使连线与节点（QML 绑定即时缩放）同步移动，避免等 tick 全量重建导致的连线滞后。
    // 缩放停止（防抖 300ms）后重建端点表结束叠加。
    if (!m_zoomPending) {
        m_zoomBaseRatio = IBR_FullView::Ratio;
        m_zoomBaseCenter = QPointF(IBR_FullView::EqCenter.x, IBR_FullView::EqCenter.y);
        m_zoomPending = true;
        emit zoomBaseChanged();
    }
    if (m_dragSectionId != INVALID_MODULE_ID || m_massDragging) {
        // 拖拽中滚轮：先应用待处理的拖拽位置（帧率节流下 onPositionChanged 只写
        // m_pendingDragPos，由 applyPendingDrag 在 tick 统一应用），
        // 保证拖拽状态与当前鼠标同步后再改 Ratio。
        m_pendingDragPos = QPointF(x, y);
        m_hasPendingDrag = true;
        applyPendingDrag();
    }
    // 补间搬 QML 端驱动：C++ 只设定目标 Ratio 与锚点并 emit 请求信号，
    // QML NumberAnimation 每渲染帧回调 applyZoomRatio —— Ratio/EqCenter 变更、
    // 拖拽基准修正、dragOffset 重算、预览线重发在同一渲染帧内原子完成。
    // 动画由 QQuickWindow 动画驱动推进（与渲染帧同步），消除 C++ 定时器驱动的
    // QVariantAnimation 与渲染循环不同步造成的跳帧/低帧率；UpdatePrevII 收尾
    // 移至 QML 动画停止回调 zoomTweenFinished。
    m_zoomTargetRatio = newRatio;
    emit zoomTargetRatioChanged();
    m_zoomAnchorScreen = QPointF(x, y);
    m_zoomAnimating = true;
    // 关键：每次启动补间都刷新防抖 token，作废上一次缩放遗留的 finalize 定时器。
    // 否则"上次补间结束后 300ms 内再次滚滚轮"时，旧定时器会在本次补间进行中
    // 触发 finalizeZoom：清 zoomPending（换算失效）→ 连线以旧端点表无换算渲染错位。
    // finalizeZoom 内部的 m_zoomAnimating 推迟检查是第二道防线（见 finalizeZoom）。
    scheduleZoomFinalize();
    emit zoomTweenRequested();
}

void WorkspaceController::applyZoomRatio(float newRatio)
{
    if (std::abs(newRatio - IBR_FullView::Ratio) < 1e-6f) return;
    // 锚点由 onWheel 在滚轮事件时更新（连续滚轮取最新位置），动画期间保持不变
    const QPointF anchorScreen = m_zoomAnchorScreen;
    bool dragging = (m_dragSectionId != INVALID_MODULE_ID || m_massDragging);
    // 拖拽连线预览：缩放前把预览线起点（源节点上的固定点，Eq 不变）换算到方程空间，
    // 缩放后换算回新屏幕坐标重发 —— 预览 Canvas 由 draggingLinkChanged 驱动重绘，
    // 不重发则端点停留在旧屏幕坐标，缩放时线不随节点变形。
    bool hasDragLink = m_hasDraggingLink;
    QPointF dragLinkFromEq;
    if (hasDragLink)
        dragLinkFromEq = screenToEq(QPointF(m_dragLinkFromX, m_dragLinkFromY));
    // 拖拽中的最新鼠标位置：从"虚拟按下点 + 当前 dragOffset"自恢复
    // （不依赖 pending —— 收尾路径 pending 已被 applyPendingDrag 清空，此时锚点 ≠
    // 松手鼠标位置，直接用锚点重算 dragOffset 会把模块拉回滚轮处）
    QPointF dragMouse = anchorScreen;
    if (dragging)
        dragMouse = m_dragStartScreenPos + m_dragOffset;
    // 统一鼠标锚定缩放（差值法保持锚点处 Eq 内容不变），模块屏幕位置随画布缩放移动
    QPointF anchorEqBefore = screenToEq(anchorScreen);
    float oldRatio = IBR_FullView::Ratio;
    IBR_FullView::Ratio = newRatio;
    QPointF anchorEqAfter = screenToEq(anchorScreen);
    IBR_FullView::EqCenter.x += static_cast<float>(anchorEqBefore.x() - anchorEqAfter.x());
    IBR_FullView::EqCenter.y += static_cast<float>(anchorEqBefore.y() - anchorEqAfter.y());
    if (dragging) {
        // 缩放与拖拽同帧原子处理：把"虚拟按下点"围绕锚点同步缩放（k = new/old），
        // 再以最新鼠标重算 dragOffset —— 抓取点（模块身上按下的点）始终跟手，
        // 模块随画布同步缩放；松手写回 EqPos = 基准 + dragOffset / Ratio 在缩放
        // 前后数学等值（Eq 位移不变，模块 Eq 位置不因缩放跳变）。
        // 先例：updateMassDrag 边界推回同样按视口位移修正 m_dragStartScreenPos。
        float k = newRatio / oldRatio;
        m_dragStartScreenPos = anchorScreen
            + QPointF((m_dragStartScreenPos.x() - anchorScreen.x()) * k,
                      (m_dragStartScreenPos.y() - anchorScreen.y()) * k);
        m_dragOffset = QPointF(dragMouse.x() - m_dragStartScreenPos.x(),
                               dragMouse.y() - m_dragStartScreenPos.y());
        emit dragOffsetChanged();
    }
    if (hasDragLink) {
        // 终点 = 拖拽连线时跟随的最新鼠标，起点 = 源节点随缩放后的新屏幕位置
        QPointF newFrom = eqToScreen(dragLinkFromEq);
        setDraggingLink(newFrom.x(), newFrom.y(), dragMouse.x(), dragMouse.y());
    }
    // v3 批次 3.1：缩放后更新 EqMax，确保 MiniMap 可见范围正确（对应 IBR_FullView::UpdateCurrentEqMax）
    IBR_FullView::UpdateCurrentEqMax();
    // 性能优化：QML 根据 ratio 和 eqCenter 实时计算屏幕坐标，不需要全量 refresh
    emit ratioChanged();
    emit eqCenterChanged();
}

void WorkspaceController::zoomTweenFinished()
{
    // QML 端动画停止回调（原 QVariantAnimation finished 逻辑，补间搬 QML 后由
    // NumberAnimation onRunningChanged 调用）：
    // v3 批次 2.1：补间完成推入 Undo 栈（对应 MainStage.h:218 UpdatePrevII，内部有阈值/merge）
    m_zoomAnimating = false;
    IBR_WorkSpace::UpdatePrevII();
    // v3 批次 3.1：缩放后更新 EqMax（对应 IBR_FullView::UpdateCurrentEqMax）
    IBR_FullView::UpdateCurrentEqMax();
    // 缩放中不重建端点表（靠 LinkRenderer 换算叠加），缩放停止防抖后重建一次
    scheduleZoomFinalize();
}

void WorkspaceController::finishZoomTweenNow()
{
    // 收尾路径（拖拽结束/端点表重建/平移结束）清 zoomPending 前调用：
    // 先把 Ratio 直接推到目标（含拖拽基准修正，此刻写回 EqPos 用的即最终 Ratio），
    // 再通知 QML 立即停止动画。否则清 m_zoomPending → zoomTransform 失效后，
    // QML 动画仍在逐帧改 Ratio，连线（旧端点表 + 无换算）将与节点错位。
    if (!m_zoomAnimating) return;
    if (std::abs(m_zoomTargetRatio - IBR_FullView::Ratio) > 1e-6f)
        applyZoomRatio(m_zoomTargetRatio);
    emit zoomTweenAbortRequested();
    m_zoomAnimating = false;
}

void WorkspaceController::scheduleZoomFinalize()
{
    // 缩放结束防抖：每次缩放事件重置定时器，最后一次缩放后 300ms 再收尾。
    // 期间端点表保持快照，LinkRenderer 按 zoomPending 换算叠加（仿画布平移的 canvasDragOffset）。
    int token = ++m_zoomFinalizeToken;
    QTimer::singleShot(300, this, [this, token]() {
        if (token != m_zoomFinalizeToken) return;  // 期间又发生缩放，旧回调作废
        finalizeZoom();
    });
}

void WorkspaceController::finalizeZoom()
{
    // 平移（state=1）/拖拽（state=3）进行中：推迟缩放收尾。
    // 平移中行圆点回写被跳过，此刻重建端点表会读到旧坐标 → 连线变形；
    // 且换算+canvasOffset 组合在平移中数学上精确（缩放变换与平移独立），可继续叠加渲染。
    // 平移/拖拽收尾（onMouseRelease/endDrag）会清 zoomPending 并重建端点表。
    if (m_inputState == 1 || m_inputState == 3 || m_dragSectionId != INVALID_MODULE_ID
        || m_zoomAnimating) {
        // 补间进行中同样推迟：此刻收尾会清 zoomPending（换算失效）后 Ratio 仍在动画，
        // 连线以旧端点表无换算渲染 → 与节点错位；等补间 finished 刷新的防抖再收尾
        scheduleZoomFinalize();  // 稍后重试（token 防抖）
        return;
    }
    // 缩放叠加结束（对应平移的 onMouseRelease BgDragging→Normal 收尾）：
    // 1. 清 pending，但【不 emit zoomBaseChanged】：此刻端点表仍是旧快照，若触发 LinkRenderer
    //    重绘，会画出"换算失效 + 旧端点表"的回弹中间帧；等 Queued 重建完成后由
    //    rebuildLinkEndpoints 内的 linkEndpointsChanged 触发一次重绘，直接落到新端点表；
    // 2. emit zoomFinalizeRequested → QML 用 Qt.callLater 回写缩放后的行圆点坐标（先入队）；
    // 3. QueuedConnection 重建端点表（后入队）→ 读到回写后的基准坐标。
    // 顺序保证：回写先于重建（同平移 onInputStateChanged 的模式，否则 pb 读到缩放前旧坐标错位）。
    m_zoomPending = false;
    emit zoomFinalizeRequested();
    QMetaObject::invokeMethod(this, [this]() {
        // 入队期间若开始平移/拖拽（快速操作），放弃本次重建，由对应收尾路径重建
        if (m_inputState == 1 || m_inputState == 3 || m_dragSectionId != INVALID_MODULE_ID) return;
        m_linkEndpointsDirty = true;
        if (!m_suppressLinkRebuild) {
            rebuildLinkEndpoints();
            m_linkEndpointsDirty = false;
        }
    }, Qt::QueuedConnection);
}

void WorkspaceController::onDrop(qreal x, qreal y, const QString &moduleKey)
{
    // 模块树拖拽到工作区：坐标作为投放位置，模块由 ModuleTreeModel::addModule 解析
    // 这里仅更新 EqCenter 提示位置，实际添加由 QML 侧调用 moduleTreeModel.addModule(row)
    Q_UNUSED(x)
    Q_UNUSED(y)
    Q_UNUSED(moduleKey)
    emit hintMessage(QString::fromUtf8(u8"已投放模块: ") + moduleKey);
}

void WorkspaceController::zoomTo(float newRatio)
{
    // 补间进行中直接跳变缩放：先强制完成补间（Ratio 推目标 + 停 QML 动画），
    // 否则 QML 动画仍在逐帧 applyZoomRatio，会覆盖这里的跳变值
    finishZoomTweenNow();
    float clamped = std::clamp(newRatio,
        IBR_FullView::RatioMin / 100.0f,
        IBR_FullView::RatioMax / 100.0f);
    // 缩放叠加：记录端点表基准（同 onWheel），缩放中不重建端点表，LinkRenderer 换算叠加
    if (!m_zoomPending) {
        m_zoomBaseRatio = IBR_FullView::Ratio;
        m_zoomBaseCenter = QPointF(IBR_FullView::EqCenter.x, IBR_FullView::EqCenter.y);
        m_zoomPending = true;
        emit zoomBaseChanged();
    }
    IBR_FullView::Ratio = clamped;
    // 性能优化：缩放只改变 Ratio，不改变 sections 数据
    // QML 根据 ratio 实时计算节点尺寸/坐标，不需要全量 refresh()（避免 91 个节点重建卡顿）
    emit ratioChanged();
    emit workspaceRectChanged();  // 视口 EqRect 随 Ratio 变化
    // 缩放中不重建端点表（靠 LinkRenderer 换算叠加），缩放停止防抖后重建一次
    scheduleZoomFinalize();
}

void WorkspaceController::centerView()
{
    moveToCenter();
}

void WorkspaceController::centerViewTo(qreal eqX, qreal eqY)
{
#ifdef INIWEAVER_DIAG
    qDebug() << "[REFRESH-DIAG] centerViewTo eqX=" << eqX << "eqY=" << eqY
             << "curEqCenter=" << IBR_FullView::EqCenter.x << IBR_FullView::EqCenter.y
             << "eqMax=" << IBR_FullView::GetEqMax().x << IBR_FullView::GetEqMax().y;
#endif
    // 修复：钳制到安全范围，防止视图框拖到特别远导致崩溃。
    // 根因：EqCenter 巨大时 float 精度丢失（7 位有效数字），viewportEqRect 的
    // w/h 变 0 → MiniMap world bounds 只含巨大 x 不含宽度 → scale 极小 →
    // eqX 指数爆炸 → EqCenter 继续增大 → 恶性循环溢出崩溃。
    // 钳制到 ±1e9：float 在 1e9 时 ulp≈64，视口宽度（约 1256 Eq）仍可精确表示，
    // 不会精度丢失；同时允许侧边栏视图框拖到极远（模块范围通常几百，1e9 是百万倍）。
    // 视图窗口（includeViewportInWorld=false）的视图框由 MiniMap.locateTo 的 clamp
    // 限制在画布内，传入值本就在模块范围，不受此钳制影响。
    constexpr qreal kSafeEqLimit = 1e9;
    eqX = std::clamp(eqX, -kSafeEqLimit, kSafeEqLimit);
    eqY = std::clamp(eqY, -kSafeEqLimit, kSafeEqLimit);
    // v3 批次 2.1：视口跳转前保存前态（对应 MainStage.h:122 UpdatePrev）
    IBR_WorkSpace::UpdatePrev();
    // 对应 IBR_FullView::ChangeOffsetPos：将 EqCenter 移动到指定 Eq 坐标
    IBR_FullView::EqCenter = ImVec2(static_cast<float>(eqX), static_cast<float>(eqY));
    IBR_FullView::UpdateCurrentEqMax();
    // v3 批次 2.1：推入 Undo 栈（对应 MainStage.h:218 UpdatePrevII）
    IBR_WorkSpace::UpdatePrevII();
    // 性能优化：视口跳转只改变 EqCenter，不改变 sections 数据
    // QML 根据 eqCenter 实时计算屏幕坐标，不需要全量 refresh()（避免 91 个节点重建卡顿）
    emit eqCenterChanged();
    emit workspaceRectChanged();
    m_linkEndpointsDirty = true;  // EqCenter 变化导致节点位置变化，需重建端点表
}

void WorkspaceController::moveToCenter()
{
    // v3 批次 2.1：视口跳转前保存前态（对应 MainStage.h:122 UpdatePrev）
    IBR_WorkSpace::UpdatePrev();
    // Qt 版本改进：计算所有可见模块的包围盒中心，而非固定 (0,0)
    // ImGui 版本 MoveToCenter 设为 (0,0)，但项目文件中模块可能不在原点附近
    // （如用户拖拽后按 F5 保存），导致加载后节点在视口外
    bool hasVisible = false;
    float minX = std::numeric_limits<float>::max();
    float minY = std::numeric_limits<float>::max();
    float maxX = std::numeric_limits<float>::lowest();
    float maxY = std::numeric_limits<float>::lowest();
    for (const auto &[id, sd] : IBR_Inst_Project.IBR_SectionMap)
    {
        if (sd.Hidden && !sd.First) continue;
        if (sd.IsIncluded()) continue;
        hasVisible = true;
        minX = std::min(minX, sd.EqPos.x);
        minY = std::min(minY, sd.EqPos.y);
        maxX = std::max(maxX, sd.EqPos.x + sd.EqSize.x);
        maxY = std::max(maxY, sd.EqPos.y + sd.EqSize.y);
    }
    if (hasVisible) {
        IBR_FullView::EqCenter = ImVec2((minX + maxX) * 0.5f, (minY + maxY) * 0.5f);
    } else {
        IBR_FullView::EqCenter = ImVec2(0, 0);
    }
    IBR_FullView::UpdateCurrentEqMax();
    // v3 批次 2.1：推入 Undo 栈（对应 MainStage.h:218 UpdatePrevII）
    IBR_WorkSpace::UpdatePrevII();
    // 性能优化：居中只改变 EqCenter，不改变 sections 数据
    // QML 根据 eqCenter 实时计算屏幕坐标，不需要全量 refresh()
    emit eqCenterChanged();
    emit workspaceRectChanged();
    m_linkEndpointsDirty = true;
}

void WorkspaceController::startSelection(qreal x, qreal y)
{
    m_dragStartScreen = QPointF(x, y);
    m_dragStartEq = screenToEq(m_dragStartScreen);
    m_selectionRect = QRectF(m_dragStartEq, QSizeF(0, 0));
    m_massSelectPreview.clear();
    emit selectionRectChanged();
}

void WorkspaceController::updateSelection(qreal x, qreal y)
{
    QPointF curEq = screenToEq(QPointF(x, y));
    m_selectionRect = QRectF(
        std::min(m_dragStartEq.x(), curEq.x()),
        std::min(m_dragStartEq.y(), curEq.y()),
        std::abs(curEq.x() - m_dragStartEq.x()),
        std::abs(curEq.y() - m_dragStartEq.y())
    );
    emit selectionRectChanged();

    // 实时更新预览集合（对应 ImGui IBR_SelectMode::UpdateMassSelect 每帧调用）
    // 框选矩形完全包含的 Section 立即显示选中状态
    // 过滤条件与 ImGui 一致：仅检查 HasBack()（后端数据存在），不检查 Hidden
    m_massSelectPreview.clear();
    for (const auto &[id, sd] : IBR_Inst_Project.IBR_SectionMap)
    {
        if (!IBR_Inst_Project.GetSectionFromID(id).HasBack()) continue;
        QRectF secRect(sd.EqPos.x, sd.EqPos.y, sd.EqSize.x, sd.EqSize.y);
        if (m_selectionRect.contains(secRect))
        {
            m_massSelectPreview.insert(id);
        }
    }
    // 通知 QML 更新选中状态（实时反馈）
    ++m_selectedRevision;
    emit selectedRevisionChanged();
}

void WorkspaceController::endSelection()
{
    // 框选完成：将预览集合转为正式 MassTarget
    // 对应 ImGui IBR_WorkSpace.cpp:1109 MassTarget = IBR_SelectMode::GetMassSelected()
    if (!m_massSelectPreview.empty())
    {
        std::vector<ModuleID_t> hits(m_massSelectPreview.begin(), m_massSelectPreview.end());
        IBR_WorkSpace::MassSelect(hits);
    }
    m_massSelectPreview.clear();
    // 清空框选矩形（对应 ImGui IsMassSelecting=false 后 RenderUI_MassSelect 不再画框）
    m_selectionRect = QRectF();
    emit selectionRectChanged();
    // 通知 QML 更新选中状态（切换到 MassTarget 查询）
    ++m_selectedRevision;
    emit selectedRevisionChanged();
}

void WorkspaceController::clearSelection()
{
    m_selectionRect = QRectF();
    m_massSelectPreview.clear();
    IBR_WorkSpace::MassSelect({});
    emit selectionRectChanged();
    // 性能优化：不调全量 refresh()，仅递增选中版本号
    ++m_selectedRevision;
    emit selectedRevisionChanged();

    // 对应 ImGui IBR_WorkSpace.cpp:1076-1079 Clear() + ChooseMenu(MenuItemID_MODULES)
    // 点击空白处取消选中时清空编辑侧边栏；菜单恢复到选中前的侧边栏（restoreMenuAfterDeselect）
    IBR_EditFrame::CurSection.ID = UINT_MAX;
    if (m_editPanelController) {
        m_editPanelController->clear();
    }
    restoreMenuAfterDeselect();
}

void WorkspaceController::enterEditMenu()
{
    if (!m_menuController) return;
    // 记录进入编辑侧边栏前的菜单（供取消选中后恢复"选中前的侧边栏"）
    if (m_menuController->activeMenu() != 4) {
        m_prevMenuBeforeEdit = m_menuController->activeMenu();
        m_hasPrevMenuBeforeEdit = true;
    }
    m_menuController->setActiveMenu(4);
}

void WorkspaceController::restoreMenuAfterDeselect()
{
    if (!m_menuController) return;
    // 仅在当前菜单是编辑侧边栏时恢复；否则保持现状（用户可能在编辑态外手动切换过菜单）
    if (m_menuController->activeMenu() == 4) {
        // 回到选中前的侧边栏；无记录（异常场景）兜底回 MODULES
        m_menuController->setActiveMenu(
            m_hasPrevMenuBeforeEdit ? m_prevMenuBeforeEdit : 1);
    }
}

void WorkspaceController::setSelectedIDs(const QVariantList &ids)
{
    std::vector<ModuleID_t> hits;
    hits.reserve(ids.size());
    for (const auto &v : ids)
    {
        bool ok = false;
        quint64 id = v.toULongLong(&ok);
        if (ok) hits.push_back(static_cast<ModuleID_t>(id));
    }
    IBR_WorkSpace::MassSelect(hits);
}

void WorkspaceController::deleteSelected()
{
    // 对应 IBR_WorkSpace.cpp:454-460 DeleteSelected
    // DeleteSection 经 SendToR 异步执行（下个 tick 的 IBR_AutoProc 中）。
    // 修复：
    // 1) 不立即 refresh()——此刻数据未变，同步全量重建是白重建（删除卡顿主因，
    //    删除实际会触发两次全量重建）；置 m_dirty 让删除执行后仅重建一次。
    // 2) 置 RefreshLinkList=true——DeleteSection 不清理 LinkList，若不重建，
    //    已删除模块的连线残留引用导致连线偏移/残留一帧。
    IBR_WorkSpace::DeleteSelected();
    m_dirty = true;
    IBR_Inst_Project.RefreshLinkList = true;
}

void WorkspaceController::copySelected()
{
    IBR_WorkSpace::CopySelected();
}

void WorkspaceController::copySection(qulonglong sectionId)
{
    // 复制指定节点到剪贴板，不改选择状态
    // 对应 ImGui IBR_SectionData::CopyToClipBoard()（IBR_SectionData.cpp:282-289）
    // 注意：必须直接查 IBR_SectionMap（key=ModuleID_t/MaxID 递增整数）。
    // 不能走 IBR_Project::GetSection(ModuleID_t)：它会构造 IBB_SectionID{MaxID} 去查
    // IBR_Rev_SectionMapII（key 是 IBB_SectionID{Desc} 的 DescToID 哈希，另一套 ID），
    // 找不到后 ToDesc() 返回空 Desc → GetSection(空Desc) 会创建 Back_GunMu"滚木"占位模块！
    ModuleID_t id = static_cast<ModuleID_t>(sectionId);
    auto it = IBR_Inst_Project.IBR_SectionMap.find(id);
    if (it == IBR_Inst_Project.IBR_SectionMap.end()) return;
    it->second.CopyToClipBoard();
}

void WorkspaceController::cutSelected()
{
    // CutSelected 内部 CopySelected（同步生成剪贴板）+ DeleteSelected（SendToR 异步删除）
    // 同 deleteSelected：去同步白重建，置脏等删除执行后由 tick 一次有效重建
    IBR_WorkSpace::CutSelected();
    m_dirty = true;
    IBR_Inst_Project.RefreshLinkList = true;
}

void WorkspaceController::paste()
{
    // C11：120ms 节流（对应 IBR_WorkSpace.cpp:543 PasteNarrowTime）
    qint64 now = m_uptimeTimer.elapsed();
    if (now - m_lastPasteElapsed < 120) return;
    m_lastPasteElapsed = now;
    IBR_WorkSpace::Paste();
    refresh();
}

void WorkspaceController::duplicateSelected()
{
    IBR_WorkSpace::DuplicateSelected();
    refresh();
}

void WorkspaceController::selectAll()
{
    IBR_WorkSpace::SelectAll();
    // 修复：全选后切换到 MassAfter(4) 状态，与框选 endSelection + handleMouseButtonReleased
    // (line 311-316) 的逻辑对齐。后端 SelectAll 已设 IsMassAfter=true，但 QML 端 m_inputState
    // 未同步，导致 beginMassDrag 的 if(m_inputState!=4) return 拒绝多节点拖动，SectionNode 的
    // isMassSelectState(inputState===4) 也不显示多选蓝框。空项目 MassTarget 为空时回 Normal。
    if (IBR_WorkSpace::MassTarget.empty()) {
        updateInputState(0);  // Normal
    } else {
        updateInputState(4);  // MassAfter
    }
    // 性能优化：全选只改变 MassTarget（选中集合），不改变 sections 数据
    // QML 的 isSelected 绑定到 selectedRevision，递增即可触发蓝框更新
    ++m_selectedRevision;
    emit selectedRevisionChanged();
}

void WorkspaceController::selectInvert()
{
    // 反选：toggle 所有 section 的 Dynamic.Selected，收集新选中集合同步 MassTarget
    // 不调 MassSelect（它会设 IsBgDragging=true 污染画布状态机，对齐 ImGui 列表直接改字段）
    std::vector<ModuleID_t> newTarget;
    for (auto & [id, data] : IBR_Inst_Project.IBR_SectionMap)
    {
        auto sec = IBR_Inst_Project.GetSectionFromID(id);
        if (sec.HasBack())
        {
            auto *back = sec.GetBack_Unsafe();
            if (back)
            {
                back->Dynamic.Selected = !back->Dynamic.Selected;
                if (back->Dynamic.Selected) newTarget.push_back(id);
            }
        }
    }
    IBR_WorkSpace::MassTarget = newTarget;
    if (newTarget.empty()) {
        updateInputState(0);  // Normal
    } else {
        updateInputState(4);  // MassAfter
    }
    ++m_selectedRevision;
    emit selectedRevisionChanged();
}

void WorkspaceController::toggleSelectSection(qulonglong sectionId)
{
    // 列表单行 checkbox toggle：直接改 Dynamic.Selected（对齐 ImGui IBR_ListView.cpp:296
    // 直接写 sec.Dynamic.Selected），不调 MassSelect 避免 IsBgDragging=true 污染画布状态机
    ModuleID_t id = static_cast<ModuleID_t>(sectionId);
    auto sec = IBR_Inst_Project.GetSectionFromID(id);
    if (sec.HasBack())
    {
        auto *back = sec.GetBack_Unsafe();
        if (back) back->Dynamic.Selected = !back->Dynamic.Selected;
    }
    // 收集所有 Dynamic.Selected 同步 MassTarget（画布 isSelected 查 MassTarget 显示选中）
    std::vector<ModuleID_t> newTarget;
    for (auto & [sid, data] : IBR_Inst_Project.IBR_SectionMap)
    {
        auto s = IBR_Inst_Project.GetSectionFromID(sid);
        if (s.HasBack())
        {
            auto *b = s.GetBack_Unsafe();
            if (b && b->Dynamic.Selected) newTarget.push_back(sid);
        }
    }
    IBR_WorkSpace::MassTarget = newTarget;
    if (newTarget.empty()) {
        updateInputState(0);  // Normal
    } else {
        updateInputState(4);  // MassAfter
    }
    ++m_selectedRevision;
    emit selectedRevisionChanged();
}

void WorkspaceController::composeSelected()
{
    // 编组：ComposeSelected 原生实现（IsMassAfter 多选态 → ComposeSections）。
    // ComposeSections 计算父块 EqPos=质心 但不设置子模块 EqDelta，而 DecomposeSection 解组时
    // 用 pData->EqPos = 父EqPos + pData->EqDelta 还原位置。若 EpDelta 未设（0/旧残留），
    // 解组后子模块会挤到父块质心 → 位置出错/重叠。
    // 故编组前记录目标，成功后给子模块设 EqDelta = 子EqPos - 父块EqPos，使解组可精确还原原位。
    // 注：需在 ComposeSelected 前捕获 targets（Compose 后 MassTarget 被消费/清空）。
    std::vector<ModuleID_t> targets;
    const auto mass = massTargetIds();
    for (const auto &v : mass) targets.push_back(static_cast<ModuleID_t>(v.toULongLong()));

    IBR_WorkSpace::ComposeSelected();

    if (!targets.empty()) {
        for (auto &kv : IBR_Inst_Project.IBR_SectionMap) {
            auto &sd = kv.second;
            if (sd.IncludingModules.empty()) continue;
            bool allMatch = true;
            for (auto t : targets) {
                if (std::find(sd.IncludingModules.begin(), sd.IncludingModules.end(), t)
                    == sd.IncludingModules.end()) { allMatch = false; break; }
            }
            if (!allMatch) continue;
            // 找到包含全部目标的新虚拟块：给各子模块设 EqDelta = 子EqPos - 父块EqPos
            for (auto subId : sd.IncludingModules) {
                auto subIt = IBR_Inst_Project.IBR_SectionMap.find(subId);
                if (subIt != IBR_Inst_Project.IBR_SectionMap.end()) {
                    subIt->second.EqDelta.x = subIt->second.EqPos.x - sd.EqPos.x;
                    subIt->second.EqDelta.y = subIt->second.EqPos.y - sd.EqPos.y;
                }
            }
            break;
        }
    }
    refresh();
}

bool WorkspaceController::outputSelectedModule(const QString &name, const QString &desc, const QString &path)
{
    // 对应 ImGui GUI_ExportModule → OutputSelected → OutputSelectedImpl
    // QML 导出模块对话框确认后调用：把选中模块保存为自定义模块（.ini）并注册到模块树
    if (name.trimmed().isEmpty() || path.trimmed().isEmpty()) return false;
    IBR_WorkSpace::ExportSelectedModuleQt(
        name.toUtf8().constData(),
        desc.toUtf8().constData(),
        path.toUtf8().constData());
    // 模块树需重新加载新模块
    return true;
}

void WorkspaceController::requestOutputModuleDialog()
{
    emit outputModuleRequested();
}

QString WorkspaceController::defaultModuleExportPath() const
{
    // 对应 ImGui OutputSelected（IBR_WorkSpace.cpp:752）IBB_ModuleAltDefault::GenerateModulePath()
    // 生成默认导出路径（含文件名，如 模块目录/DefaultName.ini），供对话框初始化
    return QString::fromUtf8(UnicodetoUTF8(IBB_ModuleAltDefault::GenerateModulePath()).c_str());
}

QPointF WorkspaceController::globalMousePos() const
{
    // 全局屏幕坐标（多显示器可用），QML 侧用 Overlay.mapFromGlobal 折算
    return QCursor::pos();
}

void WorkspaceController::freezeSelected(bool frozen)
{
    // 对应 IBR_WorkSpace.cpp:404 FreezeSelected / :416 UnfreezeSelected
    if (frozen) {
        IBR_WorkSpace::FreezeSelected();
    } else {
        IBR_WorkSpace::UnfreezeSelected();
    }
    refresh();
}

void WorkspaceController::hideSelected(bool hidden)
{
    // 对应 IBR_WorkSpace.cpp:429 HideSelected / :441 ShowSelected
    if (hidden) {
        IBR_WorkSpace::HideSelected();
    } else {
        IBR_WorkSpace::ShowSelected();
    }
    refresh();
}

void WorkspaceController::ignoreSelected(bool ignored)
{
    // 对应 IBR_WorkSpace.cpp:379 IgnoreSelected / :391 NoIgnoreSelected
    if (ignored) {
        IBR_WorkSpace::IgnoreSelected();
    } else {
        IBR_WorkSpace::NoIgnoreSelected();
    }
    refresh();
}

// ===== 阶段 7 新增：缺失的快捷键对应方法 =====

bool WorkspaceController::showRegName() const
{
    return IBR_WorkSpace::ShowRegName;
}

void WorkspaceController::invertSelection()
{
    // 反选：遍历所有 Section，将不在 MassTarget 中的加入，在其中的移除
    std::vector<ModuleID_t> newTarget;
    for (const auto &[id, sd] : IBR_Inst_Project.IBR_SectionMap)
    {
        if (sd.Hidden && !sd.First) continue;
        if (sd.IsIncluded()) continue;
        bool inMass = std::find(IBR_WorkSpace::MassTarget.begin(),
                                IBR_WorkSpace::MassTarget.end(), id) == IBR_WorkSpace::MassTarget.end();
        if (inMass) {
            newTarget.push_back(id);
        }
    }
    IBRF_CoreBump.SendToR({ [newTarget = std::move(newTarget)]() {
        IBR_WorkSpace::MassTarget = newTarget;
    } });
    // 性能优化：反选只改变 MassTarget，不改变 sections 数据
    // QML 的 isSelected 绑定到 selectedRevision
    ++m_selectedRevision;
    emit selectedRevisionChanged();
}

void WorkspaceController::deleteAllSections()
{
    // 对应 IBR_WorkSpace.cpp:944-957 DeleteAll
    IBRF_CoreBump.SendToR({ []() {
        IBG_Undo.SomethingShouldBeHere();
        std::vector<IBB_Section_Desc> all;
        all.reserve(IBR_Inst_Project.IBR_SectionMap.size());
        for (const auto &[k, v] : IBR_Inst_Project.IBR_SectionMap)
        {
            if (v.Desc.Ini.empty() || v.Desc.Sec.empty()) continue;
            all.push_back(v.Desc);
        }
        IBR_Inst_Project.DeleteSection(all);
    } });
    // 同 deleteSelected：去同步白重建 + 置 RefreshLinkList（删除后重建连线）
    m_dirty = true;
    IBR_Inst_Project.RefreshLinkList = true;
}

void WorkspaceController::switchDisplayMode()
{
    // 对应 IBR_WorkSpace.cpp:958 ShowRegName ^= 1
    IBR_WorkSpace::ShowRegName = !IBR_WorkSpace::ShowRegName;
    emit showRegNameChanged();
    refresh();
}

void WorkspaceController::renameSelected()
{
    // 对应 IBR_WorkSpace.cpp:1874 sd.RenameDisplay()
    ModuleID_t curId = IBR_EditFrame::CurSection.ID;
    auto it = IBR_Inst_Project.IBR_SectionMap.find(curId);
    if (it != IBR_Inst_Project.IBR_SectionMap.end())
    {
        IBRF_CoreBump.SendToR({ [curId]() {
            auto it2 = IBR_Inst_Project.IBR_SectionMap.find(curId);
            if (it2 != IBR_Inst_Project.IBR_SectionMap.end()) {
                it2->second.RenameDisplay();
            }
        } });
    }
}

void WorkspaceController::renameRegisterSelected()
{
    // 对应 IBR_WorkSpace.cpp:1875 sd.RenameRegister()
    ModuleID_t curId = IBR_EditFrame::CurSection.ID;
    auto it = IBR_Inst_Project.IBR_SectionMap.find(curId);
    if (it != IBR_Inst_Project.IBR_SectionMap.end())
    {
        IBRF_CoreBump.SendToR({ [curId]() {
            auto it2 = IBR_Inst_Project.IBR_SectionMap.find(curId);
            if (it2 != IBR_Inst_Project.IBR_SectionMap.end()) {
                it2->second.RenameRegister();
            }
        } });
    }
}

void WorkspaceController::undo()
{
    // 对应 IBG_UndoStack::Undo()
    // 修复：SendToR 异步执行 Undo，在 R 线程执行完后通过 QMetaObject::invokeMethod 通知主线程 refresh
    // 移除冗余的 150ms 延迟 refresh（与回调里的 refresh 重复，会导致两次全量重建）
    IBRF_CoreBump.SendToR({ [this]() {
        IBG_Undo.Undo();
        QMetaObject::invokeMethod(this, [this]() {
            m_dirty = true;
            m_linkEndpointsDirty = true;
            emit eqCenterChanged();
            emit ratioChanged();
            refresh();
        }, Qt::QueuedConnection);
    } });
}

void WorkspaceController::redo()
{
    // 对应 IBG_UndoStack::Redo()
    IBRF_CoreBump.SendToR({ [this]() {
        IBG_Undo.Redo();
        QMetaObject::invokeMethod(this, [this]() {
            m_dirty = true;
            m_linkEndpointsDirty = true;
            emit eqCenterChanged();
            emit ratioChanged();
            refresh();
        }, Qt::QueuedConnection);
    } });
}

void WorkspaceController::showContextMenu(qreal x, qreal y)
{
    emit contextMenuRequested(x, y);
}

// ===== 阶段 1 新增：节点交互 =====

void WorkspaceController::beginMoveSection(qulonglong sectionId, qreal startX, qreal startY)
{
#ifdef INIWEAVER_DIAG
    qDebug() << "[DRAG-DIAG] beginMoveSection id=" << sectionId
             << "prevState=" << m_inputState
             << "lastDraggedId=" << m_lastDraggedId
             << "dragOffset=" << m_dragOffset
             << "dragSectionId(before)=" << m_dragSectionId;
#endif
    m_suppressLinkRebuild = false;  // 新拖拽开始：恢复端点表重建（防上次松手残留抑制）
    m_lastMassDragIds.clear();      // 清多选过渡集合（单节点拖动不用它，防上次多选残留）
    m_dragSectionId = static_cast<ModuleID_t>(sectionId);
    m_dragStartScreenPos = QPointF(startX, startY);
    auto it = IBR_Inst_Project.IBR_SectionMap.find(m_dragSectionId);
    if (it != IBR_Inst_Project.IBR_SectionMap.end())
    {
        m_dragStartEqPos = it->second.EqPos;
        // 同步设置 Dragging（拖拽结束时 refresh 会反映到 QML）
        it->second.Dragging = true;
    }
    // 修复：MassAfter 状态下点击未选中节点 → 替换选中为该节点（对应 ImGui ActivateAndEdit）
    // 避免拖动时丢失之前的选中状态，也避免拖动后 MassTarget 不一致
    if (m_inputState == 4) {
        std::vector<ModuleID_t> newTarget = { m_dragSectionId };
        IBR_WorkSpace::MassSelect(newTarget);
        ++m_selectedRevision;
        emit selectedRevisionChanged();
    }
    updateInputState(3);  // HoldingModules
    // 修复：不调用 refresh()，避免重建 QVariantList 导致 Repeater 销毁当前 delegate 丢失 mouse grab
    // 改用 draggingSectionId 属性通知 QML 应用 dragOffset
    m_draggingSectionId = sectionId;
    emit draggingSectionIdChanged();
}

void WorkspaceController::updateMoveSection(qreal curX, qreal curY)
{
    if (m_inputState != 3) return;
    // 修复：QML 传入 WorkspaceView 屏幕坐标（mapToItem(workspaceView, mouseX, mouseY)）
    // 该坐标不受节点位置变化影响（节点 x 移动 D → mouseX 反向减 D → mapToItem 结果不变）
    // dragOffset = 当前屏幕坐标 - 按下时屏幕坐标（与 updateMassDrag 逻辑统一）
    // 鼠标向右拖 → curX 增大 → dragOffset.x > 0 → 节点 x 增加 → 节点向右跟随
    m_dragOffset = QPointF(curX - m_dragStartScreenPos.x(),
                           curY - m_dragStartScreenPos.y());
    emit dragOffsetChanged();
}

void WorkspaceController::endMoveSection()
{
    if (m_inputState != 3) return;
    // 拖拽收尾前强制完成缩放补间：写回 EqPos = 基准 + dragOffset/Ratio 要求
    // dragOffset 与 Ratio 为同一时刻配套值（applyZoomStep 同步修正拖拽基准后重算），
    // 若松手时鼠标已离开滚轮锚点，用补间中间值写回会引入 (鼠标−锚点)·(1/r_mid−1/r_final) 偏差。
    // 此处 m_dragSectionId 尚未清零，applyZoomStep 的拖拽修正分支仍生效。
    finishZoomTweenNow();
    // 拖拽结束：把 dragOffset 转换为 EqPos 偏移并写回后端
    ImVec2 newEqPos(
        m_dragStartEqPos.x + static_cast<float>(m_dragOffset.x()) / IBR_FullView::Ratio,
        m_dragStartEqPos.y + static_cast<float>(m_dragOffset.y()) / IBR_FullView::Ratio
    );
    auto it = IBR_Inst_Project.IBR_SectionMap.find(m_dragSectionId);
    if (it != IBR_Inst_Project.IBR_SectionMap.end()) {
        it->second.EqPos = newEqPos;
        it->second.Dragging = false;
    }
    qulonglong changedId = static_cast<qulonglong>(m_dragSectionId);
    // 同步 MiniMap：快照 eqX/eqY 写回最新 EqPos，emit sectionsChanged 触发迷你地图重绘
    syncSectionSnapshotPos(m_dragSectionId);
    emit sectionsChanged();
    // 对应 ImGui IBR_SectionData.cpp:916-918 CurOnRender_Clicked → ActivateAndEdit
    // 拖动单个模块结束后选中该模块（非追加模式）
    // 注意：在 m_dragSectionId 清零之前保存 ID
    ModuleID_t draggedId = m_dragSectionId;
    // 注释块拖动结束后不选中（对应 ImGui：注释块不可单选，返回编辑态而非选中态）。
    // 普通模块拖动结束（含从编组内拖出等特殊路径）仍选中。
    bool isCommentDragged = IBR_Inst_Project.GetSectionFromID(draggedId).GetSectionData()
                            && IBR_Inst_Project.GetSectionFromID(draggedId).GetSectionData()->IsComment;
    if (!isCommentDragged) {
        IBR_WorkSpace::MassSelect({ draggedId });
    }
    // 用户要求：拖单个模块后回到普通态（不进入 MassAfter 多选操作态），
    // 模块保持选中（蓝框），右键弹单模块菜单；点空白仍可取消选中（BgDragging → clearSelection）
    updateInputState(0);  // Normal（原为 4/MassAfter）
    // 先发蓝框更新信号（视觉反馈优先，不阻塞）
    ++m_selectedRevision;
    emit selectedRevisionChanged();
    // 注释块拖动结束不激活编辑侧边栏（选中与编辑侧边栏强绑定；注释块不可单选 → 不进编辑态）。
    // 否则注释块拖动后侧边栏切到编辑、显示"编辑"文本，与"注释块不进编辑侧边栏"冲突。
    if (!isCommentDragged) {
        if (m_editPanelController) {
            m_editPanelController->setActive(changedId);
        }
        enterEditMenu();  // 记录进入编辑前的菜单 + 切到编辑侧边栏
    }
    // 性能优化：直接更新 CurSection.ID，不调用 SetActive
    // SetActive 在 50ms tick 的 IBR_AutoProc 中同步执行于 UI 线程，包含：
    //   1. 遍历所有 INI 的所有 section 清除 Selected（ExtendMassSelect 已做，冗余）
    //   2. IBD_RInterruptF 获取锁（自旋等待，阻塞 UI 线程导致拖动卡顿）
    //   3. ResetEdit 重建 EditLines（Qt 版本 EditPanelController 不读 EditLines，冗余）
    // Qt 版本只需要 CurSection.ID 用于 focusedSectionId（连线高亮）
    IBR_EditFrame::CurSection.ID = isCommentDragged ? INVALID_MODULE_ID : draggedId;
    m_dragSectionId = INVALID_MODULE_ID;
    m_lastDraggedId = changedId;  // LinkRenderer 据此在端点表重建前继续叠加 dragOffset（防松手连线弹）
#ifdef INIWEAVER_DIAG
    qDebug() << "[DRAG-DIAG] endMoveSection set lastDraggedId=" << m_lastDraggedId
             << "dragOffset=" << m_dragOffset;
#endif
    // 修复"松手连线弹"（根治）：拖拽中 LinkRenderer 用"旧端点表 + dragOffset"叠加显示连线。
    // 松手后若立即清 dragOffset/停止叠加，而端点表仍是旧值（拖拽前原位置），连线会弹回起点。
    // 方案：节点与连线解耦 ——
    //   节点：立即置 draggingSectionId=INVALID + sectionPositionChanged（localEqX=终点，不再叠加）；
    //   连线：LinkRenderer 的 buildSectionMap 以"lastDraggedId + dragOffset 非零"独立继续叠加
    //         dragOffset（见 LinkRenderer.qml），使连线在端点表重建完成前持续跟随鼠标终点；
    //   随后事件队列末尾：rebuildLinkEndpoints（用 LineRow 已回写的终点坐标）→ 清 dragOffset
    //         （叠加条件失效，端点表已终点 → 连线无任何中间态跳变）。
    m_draggingSectionId = INVALID_MODULE_ID;  // 节点立即恢复（不再叠加 dragOffset）
    // 拖拽结束：终止缩放换算（不 emit zoomBaseChanged 避免中间帧；补间已在函数开头强制完成）。
    // 否则残留 zoomPending 会拦截下面 isDragging→false 触发的 LineRow 回写，
    // 收尾 Queued rebuild 读到旧坐标 → 连线错位；换算失效由收尾重建后的 linkEndpointsChanged 统一切换。
    m_zoomPending = false;
    // 抑制端点表提前重建：必须在下面两个 emit 之前置位。emit draggingSectionIdChanged 会同步触发
    // LineRow.onIsDraggingChanged → linkNodeCenterChanged；若此时 suppress 仍为 false，该处理器会排队
    // Queued 重建 R1（R1 lambda 不检查 suppress），R1 在 dragOffset 未清零时用 LineRow 回写的终点坐标重建
    // 端点表，而 buildSectionMap 仍按"lastDraggedId + dragOffset 非零"叠加 dragOffset → 连线终点 = 终点 +
    // dragOffset，比节点多偏移一个 dragOffset → 连线一帧偏移。suppress 前置后，回写仅标记 dirty 不排队 R1，
    // 端点表保持旧值 + dragOffset 叠加 = 终点（与节点一致），收尾重建统一切换，无中间态。
    m_suppressLinkRebuild = true;
    emit sectionPositionChanged(changedId);   // localEqX → 终点（节点位置正确）
    emit draggingSectionIdChanged();          // isDragging → false（LineRow 回写终点坐标）
    // 连线端点表需要重建（节点位置变了）
    m_linkEndpointsDirty = true;
    QMetaObject::invokeMethod(this, [this, changedId]() {
        emit sectionPositionChanged(changedId);  // 最终一致性兜底（先让节点位置同步）
        QMetaObject::invokeMethod(this, [this]() {
            m_suppressLinkRebuild = false;       // 恢复 tick 重建
#ifdef INIWEAVER_DIAG
            qDebug() << "[DRAG-DIAG] cleanup check: dragSectionId=" << m_dragSectionId
                     << "inputState=" << m_inputState
                     << "cond(dragSec==0 && state!=3)=" << (m_dragSectionId == INVALID_MODULE_ID && m_inputState != 3);
#endif
            if (m_dragSectionId == INVALID_MODULE_ID && m_inputState != 3) {
                m_dragOffset = QPointF();        // 叠加条件（lastDraggedId+dragOffset）失效
                emit dragOffsetChanged();
                rebuildLinkEndpoints();          // 端点表 → 终点（LineRow 已回写），同回调渲染合并
                // 过渡期结束：清 lastDraggedId，防止下次拖动其他模块时（dragOffset 非零）
                // LinkRenderer 的 lastDraggedId+hasOffset 条件仍命中旧模块，把它的连线一起拖走
                m_lastDraggedId = INVALID_MODULE_ID;
#ifdef INIWEAVER_DIAG
                qDebug() << "[DRAG-DIAG] cleanup PASSED, cleared lastDraggedId + dragOffset";
#endif
            } else {
#ifdef INIWEAVER_DIAG
                qDebug() << "[DRAG-DIAG] cleanup SKIPPED, lastDraggedId stays=" << m_lastDraggedId;
#endif
            }
        }, Qt::QueuedConnection);
    }, Qt::QueuedConnection);
}

void WorkspaceController::updateDrag(qreal curX, qreal curY)
{
    // 阶段 12.1：统一拖拽更新入口，根据 m_moveAfterMass 分发
    if (m_inputState != 3) return;
    // 帧率节流：只记录最新位置，由帧定时器 tick（applyPendingDrag）统一应用
    // 使拖拽模块时的实际渲染帧率与帧率设置一致
    m_pendingDragPos = QPointF(curX, curY);
    m_hasPendingDrag = true;
}

void WorkspaceController::applyPendingDrag()
{
    // 帧率节流：把累积的拖拽/平移位置应用到后端并通知 QML
    // 调用时机：帧定时器 tick（refreshFromTimer）或拖拽结束前（onMouseRelease/endDrag/onMousePress）
    // 只应用最新一次位置（跳过中间事件），位置不丢失：增量从最后一次应用的屏幕位置累计
    if (!m_hasPendingDrag) return;
    m_hasPendingDrag = false;
    QPointF pos = m_pendingDragPos;
    if (m_inputState == 1) {
        // BgDragging：拖拽平移（对应 IBR_WorkSpace.cpp:247-264 UpdateScroll）
        // C7：累积增量从最后一次应用的屏幕位置计算，等价于原逐事件累加
        QPointF delta = pos - m_dragStartScreen;
        float oldCenterX = IBR_FullView::EqCenter.x;
        float oldCenterY = IBR_FullView::EqCenter.y;
        IBR_FullView::EqCenter.x -= static_cast<float>(delta.x()) / IBR_FullView::Ratio;
        IBR_FullView::EqCenter.y -= static_cast<float>(delta.y()) / IBR_FullView::Ratio;
        m_dragStartScreen = pos;
        // C7：钳制到有效范围（对应 IBR_WorkSpace.cpp:261 EqPosFixRange）
        IBR_FullView::EqPosFixRange(IBR_FullView::EqCenter);
        // 画布平移偏移叠加（对应拖模块的 dragOffset 机制）：
        // 拖动画布时端点表保持快照不重建、行圆点不回写（QML 端 inputState==1 跳过），
        // LinkRenderer 渲染时对所有端点叠加此屏幕偏移，与节点移动保持一致。
        // 省去每帧全量端点表重建（N 条链接 QVariantMap 重建 + LinkRenderer 重绘）→
        // 拖动画布帧率与拖模块一致（否则拖动画布被重建开销拖垮）。
        // 偏移用实际钳制后的 EqCenter 变化计算，触边时偏移与节点位移严格一致。
        QPointF centerDelta(IBR_FullView::EqCenter.x - oldCenterX,
                            IBR_FullView::EqCenter.y - oldCenterY);
        m_canvasDragOffset -= QPointF(centerDelta.x() * IBR_FullView::Ratio,
                                      centerDelta.y() * IBR_FullView::Ratio);
        emit canvasDragOffsetChanged();
        // 性能优化：只发信号，QML 根据新 EqCenter 重新计算屏幕坐标
        // 不调用 refresh()（全量重建 QVariantList 太慢）
        emit eqCenterChanged();
        emit edgeFlagsChanged();
        // 注意：不再设置 m_linkEndpointsDirty —— 拖动画布中端点表不重建，靠偏移叠加。
        // 下次端点表重建（松手后缩放/拖模块/项目操作触发）时自动清零偏移。
    } else if (m_inputState == 3) {
        // HoldingModules：按拖拽来源分发（对应 updateDrag 原逻辑）
        if (m_moveAfterMass) {
            updateMassDrag(pos.x(), pos.y());
        } else {
            updateMoveSection(pos.x(), pos.y());
        }
    }
}

void WorkspaceController::endDrag()
{
    // 阶段 12.1：统一拖拽结束入口
    if (m_inputState != 3) return;
    // 帧率节流：松手前应用最后一次待处理位置，确保 EqPos 写回准确
    applyPendingDrag();
    if (m_moveAfterMass) {
        endMassDrag();
        m_moveAfterMass = false;
        // 修复：拖拽结束后若仍有选中模块，回到 MassAfter 而非 Normal
        restoreStateAfterDrag();
    } else {
        endMoveSection();
    }
}

void WorkspaceController::clearEditSelection()
{
    // 清空选中/编辑状态（对应 ImGui IBR_WorkSpace.cpp:1076-1079 Clear() + ChooseMenu(MenuItemID_MODULES)）
    // 用户要求：取消选中/删除模块后，若仍选中单个有效模块 → 显示该模块的编辑侧边栏；
    // 无选中 → 清空编辑面板并恢复到选中前的侧边栏（restoreMenuAfterDeselect）
    if (m_inputState == 4) updateInputState(0);
    IBR_EditFrame::CurSection.ID = UINT_MAX;
    // 判断当前是否仍选中单个有效模块（如多选取消/删除后剩余一个）→ 显示编辑侧边栏
    if (IBR_WorkSpace::MassTarget.size() == 1) {
        ModuleID_t remainId = IBR_WorkSpace::MassTarget.front();
        if (IBR_Inst_Project.IBR_SectionMap.find(remainId)
            != IBR_Inst_Project.IBR_SectionMap.end()) {
            IBR_EditFrame::CurSection.ID = remainId;
            if (m_editPanelController) {
                m_editPanelController->setActive(static_cast<qulonglong>(remainId));
            }
            enterEditMenu();  // 记录进入编辑前的菜单 + 切到编辑侧边栏
            return;
        }
    }
    if (m_editPanelController) {
        m_editPanelController->clear();
    }
    restoreMenuAfterDeselect();  // 恢复到选中前的侧边栏
}

void WorkspaceController::toggleSelectSection(qulonglong sectionId, bool additive)
{
    // 对应 ImGui 的点击模块行为（IBR_SectionData.cpp:916-918 ActivateAndEdit）
    // - 非追加模式（单击）：替换选中为该模块，切到 EditPanel（对应 ActivateAndEdit）
    // - 追加模式（Ctrl+点击）：切换该模块的选中状态，不切到 EditPanel
    ModuleID_t id = static_cast<ModuleID_t>(sectionId);
    std::vector<ModuleID_t> current(IBR_WorkSpace::MassTarget.begin(), IBR_WorkSpace::MassTarget.end());

    if (additive) {
        // Ctrl+点击：切换选中状态（对应 ImGui 的多选追加）
        auto it = std::find(current.begin(), current.end(), id);
        if (it != current.end()) {
            current.erase(it);  // 已选中则取消
        } else {
            current.push_back(id);  // 未选中则追加
        }
    } else {
        // 单击：替换选中为该模块（对应 ImGui ActivateAndEdit → SetActive 清除其他选中）
        current.clear();
        current.push_back(id);
    }

    IBR_WorkSpace::MassSelect(current);

    // 先发蓝框更新信号（视觉反馈优先，不阻塞）
    ++m_selectedRevision;
    emit selectedRevisionChanged();

    // 同步 Qt 状态机
    if (current.empty()) {
        clearEditSelection();
    } else {
        if (current.size() == 1) {
            // 单选（单击替换，或 Ctrl 追加/取消后剩一个）：保持 Normal(0) 状态，切换到 EditPanel
            // 对应 ImGui IBR_EditFrame::ActivateAndEdit → ChooseMenu(MenuItemID_EDIT)
            // 用户要求：只要选中单个模块就显示编辑侧边栏（含多选逐个取消剩一个的场景）
            // 单选用边框高亮（非 MassAfter），多选才进入 MassAfter 用覆盖层
            if (m_inputState == 4) updateInputState(0);
            ModuleID_t remainId = current[0];  // Ctrl 取消场景下剩余的是未点击的那个模块
            IBR_EditFrame::CurSection.ID = remainId;
            if (m_editPanelController) {
                m_editPanelController->setActive(static_cast<qulonglong>(remainId));
            }
            enterEditMenu();  // 记录进入编辑前的菜单 + 切到编辑侧边栏
        } else {
            // 追加多选（additive=true）或 size>1：进入 MassAfter(4)
            // 对应 ImGui MassAfter 状态，用覆盖层变色，不显示编辑框
            // 用户要求：Ctrl 多选时不跳侧边栏（原 setActiveMenu(1) 会切到模块列表），
            // 保持当前面板；编辑面板清空显示空态
            updateInputState(4);
            IBR_EditFrame::CurSection.ID = UINT_MAX;
            if (m_editPanelController) {
                m_editPanelController->clear();
            }
        }
    }
}

void WorkspaceController::activateAndEdit(qulonglong sectionId)
{
    ModuleID_t id = static_cast<ModuleID_t>(sectionId);
    // 性能优化：直接更新 CurSection.ID + 选中 + 切换菜单，不调用 SetActive（同 endMoveSection）
    IBR_EditFrame::CurSection.ID = id;
    IBR_WorkSpace::MassSelect({ id });
    ++m_selectedRevision;
    emit selectedRevisionChanged();
    if (m_editPanelController) {
        m_editPanelController->setActive(sectionId);
    }
    enterEditMenu();  // 记录进入编辑前的菜单 + 切到编辑侧边栏
}

void WorkspaceController::toggleCollapseSection(qulonglong sectionId, bool collapsed)
{
    ModuleID_t id = static_cast<ModuleID_t>(sectionId);
    IBRF_CoreBump.SendToR({ [id, collapsed]() {
        auto it = IBR_Inst_Project.IBR_SectionMap.find(id);
        if (it != IBR_Inst_Project.IBR_SectionMap.end()) {
            it->second.UICollapsed = collapsed;
        }
    } });
}

void WorkspaceController::updateCommentText(qulonglong sectionId, const QString &text)
{
    // D8 修复：注释块回写应写到 Bsec->Comment（对应 IBR_SectionData.cpp:827）
    // ImGui: Bsec->Comment = CommentEdit.get() + IBG_Undo.SomethingShouldBeHere()
    // 原错误：写到 DisplayName（显示名字段，非注释正文）
    ModuleID_t id = static_cast<ModuleID_t>(sectionId);
    auto stdText = text.toStdString();
    // v3 批次 1.5：计算注释文本像素宽度（对应 ImGui CalcTextSize）
    // 用于更新 WidthFix（IBR_SectionData.cpp:818-831）
    QFontMetrics fm(QGuiApplication::font());
    qreal textWidth = fm.horizontalAdvance(text);

    IBRF_CoreBump.SendToR({ [id, stdText, textWidth]() {
        auto it = IBR_Inst_Project.IBR_SectionMap.find(id);
        if (it == IBR_Inst_Project.IBR_SectionMap.end()) return;
        auto rsec = IBR_Inst_Project.GetSectionFromID(id);
        auto bsec = rsec.GetBack_Unsafe();
        if (bsec) {
            bsec->Comment = stdText;  // 回写到后端注释字段
            IBG_Undo.SomethingShouldBeHere();

            // v3 批次 1.5：同步更新 WidthFix（对应 IBR_SectionData.cpp:818-831）
            // TSize.x = max(textWidth + FontHeight*1.2, FontHeight*(wbase - 0.2))
            // wbase = 17 * WidthRatio（IBB_Section::GetWidthBase）
            // 若 TSize.x > FontHeight * 14.6 → WidthFix = TSize.x
            constexpr float FontHeight = 13.0f;
            float wbase = 17.0f * bsec->WidthRatio;
            float tSizeX = std::max(static_cast<float>(textWidth) + FontHeight * 1.2f,
                                    FontHeight * (wbase - 0.2f));
            if (tSizeX > FontHeight * 14.6f) {
                it->second.WidthFix = tSizeX;
            }
        }
        // 同步 CommentEdit 缓冲（供 ImGui 模式或下次渲染读取）
        if (it->second.CommentEdit) {
            // CommentEdit 是 shared_ptr<BufString>，BufString = char[MAX_STRING_LENGTH]
            // shared_ptr<数组类型> 的 operator* 在 MSVC 下不可用，用 .get() 取原始指针
            char *buf = reinterpret_cast<char*>(it->second.CommentEdit.get());
            std::size_t len = stdText.copy(buf, MAX_STRING_LENGTH - 1);
            buf[len] = '\0';
        }
    } });
    refresh();
}

// ===== 阶段 1：折叠态连线锚点回写（修复 D1）=====
void WorkspaceController::setHeadLineRN(qulonglong sectionId, qreal x, qreal y)
{
    // 对应 ImGui RenderUI_Collapsed（IBR_SectionData.cpp:835-868）：
    //   折叠态下所有连线的源端点汇聚到头部 RadioButton 位置
    //   遍历所有 SubSec 的 NewLinkTo，调 SetSessionStatus(SessionID, HeadLineRN, true)
    // 这里通过遍历 LinkList 找出该 section 所有的 SessionID，统一回写 LastCenter 并标记 Collapsed=true
    ModuleID_t id = static_cast<ModuleID_t>(sectionId);
    ImVec2 center(static_cast<float>(x), static_cast<float>(y));
    for (const auto &link : IBR_Inst_Project.LinkList)
    {
        if (link.SrcModuleID != id) continue;
        IBR_NodeSession::SetSessionStatus(link.SourceID, center, true);
    }
    // 折叠态也回写 m_sectionAcceptPoint（绝对头部中心）。否则折叠目标子模块收 pb 时，
    // 优先级 2 找不到新鲜接受点，会落到基于 EqPos 的兜底，而子模块 EqPos 是组内相对坐标
    // → pb 错位（编组内子模块折叠时连线位置错误的根因）。setSectionAcceptPoint 只在非折叠态回写。
    m_sectionAcceptPoint[id] = QPointF(x, y);
    m_linkEndpointsDirty = true;  // D10：标记端点表脏，下次 refreshFromTimer 重建
}

// ===== 阶段 3：终点 pb 精确化（修复 D3、D7）=====
void WorkspaceController::setSectionAcceptPoint(qulonglong sectionId, qreal x, qreal y)
{
    // 非折叠态下，目标 section 的"接受点"为其标题栏 RadioButton 位置
    // 对应 ImGui RSD->ReWindowUL + RSD->ReOffset（标题栏中央节点）
    // 存入 m_sectionAcceptPoint，供 rebuildLinkEndpoints 构建 pb
    m_sectionAcceptPoint[sectionId] = QPointF(x, y);
    m_linkEndpointsDirty = true;  // D10：标记端点表脏，下次 refreshFromTimer 重建
}

// ===== 阶段 2 新增：连线系统 =====

void WorkspaceController::createLink(qulonglong sourceId, qulonglong destId, const QString &destKey)
{
    ModuleID_t srcId = static_cast<ModuleID_t>(sourceId);
    ModuleID_t dstId = static_cast<ModuleID_t>(destId);

    // 查询目标 Section 的 DefaultLinkKey（对应 IBR_SectionData.cpp:501-511 GetDLK）
    StrPoolID keyId{ 0 };
    if (destKey.isEmpty() || destKey == QLatin1String("default"))
    {
        // 从目标 Section 的 Back 查询 DLK
        auto rsec = IBR_Inst_Project.GetSectionFromID(dstId);
        auto bsec = rsec.GetBack_Unsafe();
        if (bsec)
        {
            keyId = bsec->GetDLK(bsec->Register);
        }
        // 若无 DLK，回退到 "default" 字符串（保持向后兼容）
        if (keyId == 0)
        {
            keyId = NewPoolStr("default");
        }
    }
    else
    {
        auto keyUtf8 = destKey.toUtf8();
        keyId = NewPoolStr(keyUtf8.constData());
    }

    // 对应 IBR_Project::LinkList 添加（在渲染线程执行）
    IBRF_CoreBump.SendToR({ [srcId, dstId, keyId]() {
        IBR_Project::_Plink link{};
        link.SourceID = srcId;
        link.Dest = IBB_SectionID{ dstId };
        link.DestKey = keyId;
        link.Color = 0xFFCCCCCC;  // 默认颜色 ABGR
        link.FromImport = false;
        link.IsSelfLinked = (srcId == dstId);
        link.IsSrcDragging = false;
        IBR_Inst_Project.LinkList.push_back(link);
        IBR_Inst_Project.RefreshLinkList = true;
    } });
    refresh();
}

void WorkspaceController::deleteLink(qulonglong sourceId, qulonglong destId, const QString &destKey)
{
    ModuleID_t srcId = static_cast<ModuleID_t>(sourceId);
    ModuleID_t dstId = static_cast<ModuleID_t>(destId);
    auto keyUtf8 = destKey.toUtf8();
    StrPoolID keyId = NewPoolStr(keyUtf8.constData());

    IBRF_CoreBump.SendToR({ [srcId, dstId, keyId]() {
        auto &links = IBR_Inst_Project.LinkList;
        links.erase(
            std::remove_if(links.begin(), links.end(),
                [srcId, dstId, keyId](const IBR_Project::_Plink &l) {
                    return l.SourceID == srcId
                        && l.Dest.ID == dstId
                        && l.DestKey == keyId;
                }),
            links.end());
        IBR_Inst_Project.RefreshLinkList = true;
    } });
    refresh();
}

void WorkspaceController::setDraggingLink(qreal fromX, qreal fromY, qreal toX, qreal toY)
{
    m_hasDraggingLink = true;
    m_dragLinkFromX = fromX; m_dragLinkFromY = fromY;
    m_dragLinkToX = toX; m_dragLinkToY = toY;
    emit draggingLinkChanged(fromX, fromY, toX, toY);
}

void WorkspaceController::clearDraggingLink()
{
    m_hasDraggingLink = false;
    emit draggingLinkCleared();
    // v3 批次 1.4：清除拖拽无效链接状态
    if (m_dragInvalidLink) {
        m_dragInvalidLink = false;
        emit dragInvalidLinkChanged(false);
    }
    // 拖拽结束同时清拖拽目标预览（拖拽源 onReleased 已执行建链/合并，这里兜底清理）
    if (m_dragTargetSectionId != 0 || !m_dragTargetColor.isEmpty() || !m_dragTargetText.isEmpty() || !m_dragSourceText.isEmpty()) {
        m_dragTargetSectionId = 0;
        m_dragTargetColor.clear();
        m_dragTargetText.clear();
        m_dragSourceText.clear();
        emit dragTargetChanged();
    }
}

// v3 批次 1.4：DropArea 接收 linkLimit=0 拖拽时通知源 LinkNode 显示"无效链接"提示
// 对应 ImGui IsDragDropPayloadBeingAccepted（IBR_SectionData.cpp:96-106 DrawDragPreviewIcon_LinkLim0）
void WorkspaceController::setDragInvalidLink(bool invalid)
{
    if (m_dragInvalidLink == invalid) return;
    m_dragInvalidLink = invalid;
    emit dragInvalidLinkChanged(invalid);
}

void WorkspaceController::setDragTarget(qulonglong targetId, const QString &color, const QString &text)
{
    if (m_dragTargetSectionId == targetId && m_dragTargetColor == color && m_dragTargetText == text)
        return;
    m_dragTargetSectionId = targetId;
    m_dragTargetColor = color;
    m_dragTargetText = text;
    emit dragTargetChanged();
}

void WorkspaceController::setDragSourceText(const QString &text)
{
    if (m_dragSourceText == text) return;
    m_dragSourceText = text;
    emit dragTargetChanged();
}

void WorkspaceController::refresh()
{
    // 全量刷新（由 Q_INVOKABLE 修改操作直接调用）
    m_dirty = true;
    m_linkEndpointsDirty = true;  // D10：全量刷新时连带重建端点表
    refreshFromTimer();
}

void WorkspaceController::refreshLinkIncremental()
{
    // 增量刷新链接数据（连线/取消连线后调用）：按后端重建 LinkList/m_links，
    // 并阻止 refreshFromTimer 因计数变化触发全量 refreshSections
    //（行模型 resetModel 重建期间坐标未就绪，全量 rebuild 会读到污染值导致连线全跑模块第一个节点）。
    // 行模型重建完成后正确回写会再次置 dirty，届时全量 rebuild 读到稳定坐标。
    if (IBR_Inst_Project.RefreshLinkList) {
        rebuildLinkList();
        refreshLinks();  // 重建 m_links（LinkRenderer 遍历的链接列表），新增/删除链接才能反映到渲染
        m_linkEndpointsDirty = false;
        m_lastLinkCount = IBR_Inst_Project.LinkList.size();
    }
}

void WorkspaceController::restoreStateAfterDrag()
{
    // 修复：拖拽结束后根据 MassTarget 是否为空决定回到 MassAfter(4) 或 Normal(0)
    // 若拖拽前有选中模块（MassTarget 非空），应回到 MassAfter，
    // 否则点击空白处进入 BgDragging 分支而非 clearSelection 分支，导致选中无法清除
    if (IBR_WorkSpace::MassTarget.empty()) {
        updateInputState(0);  // Normal
    } else {
        updateInputState(4);  // MassAfter
    }
}

void WorkspaceController::syncSectionSnapshotPos(ModuleID_t id)
{
    // 画布节点位置由 QML 实时绑定（localEqX + width/2）驱动，拖拽结束后只 emit
    // sectionPositionChanged 更新 localEqX/localEqY，不重建 m_sections。而 MiniMap
    // 的 sections 绑定读取的正是 m_sections 快照 → 快照 eqX/eqY 过期则迷你地图模块
    // 位置不刷新。这里把 IBR_SectionMap 的最新 EqPos 同步回快照，由调用方 emit
    // sectionsChanged 触发 MiniMap 重绘。
    for (auto &item : m_sections) {
        const QVariantMap map = item.toMap();
        if (map.value(QStringLiteral("sectionId")).toULongLong() != static_cast<qulonglong>(id))
            continue;
        auto it = IBR_Inst_Project.IBR_SectionMap.find(id);
        if (it == IBR_Inst_Project.IBR_SectionMap.end())
            return;
        QVariantMap updated = map;
        updated["eqX"] = static_cast<double>(it->second.EqPos.x);
        updated["eqY"] = static_cast<double>(it->second.EqPos.y);
        item = updated;
        return;
    }
}

void WorkspaceController::setViewportSize(qreal width, qreal height)
{
    // 同步视口尺寸到 IBR_RealCenter（对应 ImGui 版本 IBR_Misc.cpp:484-493 IBR_RealCenter::Update）
    // QML WorkspaceView 的 (0,0) 就是工作区左上角（不含菜单栏/侧边栏）
    // 所以 WorkSpaceUL=(0,0), WorkSpaceDR=(width,height), Center=(width/2, height/2)
    IBR_RealCenter::WorkSpaceUL = dImVec2{ 0.0, 0.0 };
    IBR_RealCenter::WorkSpaceDR = dImVec2{ static_cast<double>(width), static_cast<double>(height) };
    IBR_RealCenter::Center = ImVec2(static_cast<float>(width) * 0.5f,
                                     static_cast<float>(height) * 0.5f);
    m_viewportW = width;
    m_viewportH = height;
    // 注意：不能置 m_dirty = true！拖拽侧边栏调整宽度时 QML 连续调用本函数，
    // 每帧触发 FULL REBUILD（90+ 节点 + 连线全量重建）导致画布卡顿/闪帧。
    // 视口变化只需 QML 侧实时生效：
    //   - SectionNode 位置绑定 (eqX-eqCenter)*ratio + width/2 自动跟随视口宽度
    //   - 连线精确端点由 QML 圆点回写（onXChanged → linkNodeCenterChanged）实时更新
    //   - m_sections 里的 screenX/screenY 仅作 LinkRenderer 兜底 pb，精确 pb 始终优先
    // 因此这里只发轻量信号（MiniMap/视口矩形）+ 标记端点表重建；拖拽松手后
    // QML 调 refresh() 做一次全量同步（补齐兜底坐标）。
    emit workspaceRectChanged();
    emit viewCenterChanged();
    m_linkEndpointsDirty = true;

    // 视口偏移补偿：端点表是"上次 rebuild 时"的快照坐标，而模块已按 (ΔW/2, ΔH/2)
    // 整体平移（位置公式含 width/2、height/2）。QML LinkRenderer 渲染时叠加本偏移
    // 使连线端点立即与模块对齐（QTimer tick 的 rebuild 总落后渲染一帧，见 QtMain 帧率节流）。
    // 端点表重建（rebuildLinkEndpoints）时用当前视口更新基准并归零。
    m_viewportOffsetX = (width - m_endpointBaseW) * 0.5;
    m_viewportOffsetY = (height - m_endpointBaseH) * 0.5;
    emit viewportOffsetChanged();
}

void WorkspaceController::setViewportGlobal(qreal gx, qreal gy)
{
    m_viewportGX = gx;
    m_viewportGY = gy;
}

bool WorkspaceController::viewportContainsPoint(qreal gx, qreal gy) const
{
    return gx >= m_viewportGX && gx <= m_viewportGX + m_viewportW &&
           gy >= m_viewportGY && gy <= m_viewportGY + m_viewportH;
}

QPointF WorkspaceController::globalToViewport(qreal gx, qreal gy) const
{
    return QPointF(gx - m_viewportGX, gy - m_viewportGY);
}

void WorkspaceController::refreshFromTimer()
{
    bool isOpen = IBR_ProjectManager::IsOpen();
    size_t smapSize = IBR_Inst_Project.IBR_SectionMap.size();
#ifdef INIWEAVER_DIAG
    qDebug() << "[REFRESH-DIAG] refreshFromTimer isOpen=" << isOpen
             << "smapSize=" << smapSize
             << "m_lastIsOpen=" << m_lastIsOpen
             << "m_lastSectionCount=" << m_lastSectionCount
             << "m_lastLinkCount=" << m_lastLinkCount
             << "m_dirty=" << m_dirty
             << "RefreshLinkList=" << IBR_Inst_Project.RefreshLinkList;
#endif
    if (!isOpen) {
        if (!m_sections.isEmpty()) {
            m_sectionsModel->updateFrom(QVariantList{});  // 同步清空增量模型
            m_sections.clear();
            m_sectionDataRevision++;  // D20：递增版本号
            emit sectionsChanged();
        }
        if (!m_links.isEmpty()) {
            m_links.clear();
            emit linksChanged();
        }
        if (!m_linkEndpoints.isEmpty()) {
            m_linkEndpoints.clear();
            emit linkEndpointsChanged();
        }
        // 修复：关闭项目时清理编辑侧边栏 + CurSection，避免残留旧编辑状态。
        // 根因：关闭项目后 IBR_SectionMap 清空，但 IBR_EditFrame::CurSection 残留旧 ID/Back 指针，
        // EditPanelController.m_isEmpty 仍为 false → EditPanel 仍显示旧模块键行，
        // 点 OnShow 开关 → toggleOnShow → CurSection.GetBack() 返回空/悬空指针 → return → 开关失效。
        if (m_editPanelController) {
            m_editPanelController->clear();
        }
        IBR_EditFrame::CurSection.ID = UINT_MAX;
        if (m_lastIsOpen) {
            emit projectOpenChanged();
            m_lastIsOpen = false;
        }
        m_lastSectionCount = 0;
        m_lastLinkCount = 0;
        return;
    }

    // 帧率节流：每帧 tick 应用一次待处理的拖拽/平移位置
    //（拖动画布/模块时位置更新频率 = 帧率设置，SceneGraph 实际渲染帧率随之受帧率设置限制）
    applyPendingDrag();

    // D10：连线端点表脏检查
    // 仅在以下情况重建：LinkList 结构变化 / RefreshLinkList 标志 / 端点表为空 / 全量刷新已标记脏
    // 静止状态下跳过，避免 50ms 无谓重建（LastCenter 由 QML onXChanged 回写后 m_linkEndpointsDirty 置位）
    // 注意：rebuildLinkEndpoints 依赖 m_lineModels（行级接受点），必须在 refreshSections 之后执行
    //       此处只标记脏，实际重建移到 refreshSections/refreshLinks 之后
    {
        size_t curLinkCount = IBR_Inst_Project.LinkList.size();
        bool needRebuild = m_linkEndpointsDirty
                           || m_linkEndpoints.size() != static_cast<int>(curLinkCount)
                           || IBR_Inst_Project.RefreshLinkList
                           || m_linkEndpoints.isEmpty();
        if (needRebuild) {
            m_linkEndpointsDirty = true;  // 保留脏标记，等 refreshSections 后再重建
        }
    }

    // 性能优化：检查关键状态是否变化，无变化时跳过全量重建
    // 拖拽时的 EqPos 变化由 onMouseMove/updateMassDrag 中的直接 refresh 调用处理
    // QTimer 的 refreshFromTimer 只负责"项目状态变化"后的数据同步（加载/增删/导入等异步操作）
    // 注意：不检查 IBR_EditFrame::CurSection.ID，因为选中模块只影响蓝框（由 selectedRevision 驱动），
    //       不需要重建 sections 列表。否则每次切换模块都会触发全量 refreshSections（91个节点重建）
    size_t curSectionCount = IBR_Inst_Project.IBR_SectionMap.size();
    size_t curLinkCount = IBR_Inst_Project.LinkList.size();

    // 修复：RefreshLinkList=true 时禁止早退。
    // 行值/链接修改（createLink/deleteAllLinks/拖拽建链等）在 SendToR lambda 中只设此标志，
    // LinkList 需由 refreshLinks 重建（行值可能已变但 LinkList.size() 未变，计数检查发现不了）。
    // 若此处早退，refreshLinks 永不执行 → 连线仍从过期 LinkList 重建（取消链接后连线还在）。
    if (!m_dirty &&
        m_lastIsOpen &&
        !IBR_Inst_Project.RefreshLinkList &&
        m_lastSectionCount == curSectionCount &&
        m_lastLinkCount == curLinkCount)
    {
        // 无变化，跳过全量重建
        // 但端点表可能因 QML LinkNode 布局回写而变脏（m_linkEndpointsDirty）
        // 此时只重建端点表，不重建 sections/links（对应 ImGui 每帧用新 LastCenter 重绘连线）
        // 松手过渡期（m_suppressLinkRebuild）跳过：避免叠加残留 dragOffset 导致偏移
        if (m_linkEndpointsDirty && !m_suppressLinkRebuild && !m_pendingRebuild) {
            rebuildLinkEndpoints();
            m_linkEndpointsDirty = false;
        }
        return;
    }

    // 保存项目打开前的状态，用于判断是否为"刚打开"
    bool wasOpen = m_lastIsOpen;
    m_dirty = false;
    m_lastIsOpen = true;

    // 修复：节点数量变化（新建/删除模块）时强制重建 LinkList。
    // AddModule/DeleteSection 等业务层操作只 UpdateAll、不置 RefreshLinkList，
    // 若不重建，增删模块后连线列表仍是旧数据 → 连线偏移/残留（DeleteSection 不清理
    // LinkList，AddModule 不把新模块连线加入 LinkList）
    if (m_lastSectionCount != curSectionCount)
        IBR_Inst_Project.RefreshLinkList = true;
    // 注意：m_lastSectionCount/m_lastLinkCount 不能在刷新前赋值！
    // LinkList 由 refreshLinks->rebuildLinkList 重建（0->58），刷新前快照是旧值，
    // 会导致下一次 QTimer 误判计数变化而重复全量刷新，须在全量末尾重新记录（见下）

    // 项目刚打开时自动居中到模块包围盒中心
    // 项目文件保存的 EqCenter 可能为 (0,0)，但模块可能不在原点附近
    // 导致加载后节点在视口外，用户看到空白工作区
    if (!wasOpen) {
        bool hasVisible = false;
        float minX = std::numeric_limits<float>::max();
        float minY = std::numeric_limits<float>::max();
        float maxX = std::numeric_limits<float>::lowest();
        float maxY = std::numeric_limits<float>::lowest();
        for (const auto &[id, sd] : IBR_Inst_Project.IBR_SectionMap)
        {
            if (sd.Hidden && !sd.First) continue;
            if (sd.IsIncluded()) continue;
            hasVisible = true;
            minX = std::min(minX, sd.EqPos.x);
            minY = std::min(minY, sd.EqPos.y);
            maxX = std::max(maxX, sd.EqPos.x + sd.EqSize.x);
            maxY = std::max(maxY, sd.EqPos.y + sd.EqSize.y);
        }
        if (hasVisible) {
            IBR_FullView::EqCenter = ImVec2((minX + maxX) * 0.5f, (minY + maxY) * 0.5f);
        }
    }

    // 模块被删除时关闭编辑侧边栏：
    // 若当前编辑模块已从 SectionMap 移除（删除/剪切/全删/撤销等），
    // 与"取消选中"表现一致（清空编辑面板 + 菜单切回 MODULES + 复位状态机）
    // 全量重建分支仅在项目数据变化时进入（增删必变计数），检查成本一次 map 查找
    if (m_editPanelController) {
        qulonglong curId = m_editPanelController->currentSectionId();
        if (curId != 0
            && IBR_Inst_Project.IBR_SectionMap.find(static_cast<ModuleID_t>(curId))
                   == IBR_Inst_Project.IBR_SectionMap.end()) {
            clearEditSelection();
        }
    }

    { refreshSections(); }
    { refreshLinks(); }
    // 修复：rebuildLinkEndpoints 必须在 refreshSections/refreshLinks 之后执行
    // 因为 rebuildLinkEndpoints 依赖 m_lineModels（在 refreshSections 中创建）和 LinkList（在 refreshLinks→rebuildLinkList 中填充）
    // 松手过渡期（m_suppressLinkRebuild）跳过：避免叠加残留 dragOffset 导致偏移
    if (m_linkEndpointsDirty && !m_suppressLinkRebuild && !m_pendingRebuild) {
        rebuildLinkEndpoints();
        m_linkEndpointsDirty = false;
    }
    emit projectOpenChanged();
    emit ratioChanged();
    emit eqCenterChanged();
    emit viewCenterChanged();
    emit workspaceRectChanged();

    // 全量刷新结束后记录最终计数（LinkList 已由 refreshLinks 重建）
    m_lastSectionCount = static_cast<size_t>(IBR_Inst_Project.IBR_SectionMap.size());
    m_lastLinkCount = static_cast<size_t>(IBR_Inst_Project.LinkList.size());
}

void WorkspaceController::refreshSections()
{
    // v3 批次 3.1 修复：Sections 重建时同步更新 CurrentEqMax
    // 项目打开/增删模块后 EqMax 需重新计算，否则 EqPosFixRange 会用过小的旧值钳制 EqCenter
    // 导致拖动画布时模块跑出视口（对应 ImGui 每帧 UpdateCurrentEqMax）
    IBR_FullView::UpdateCurrentEqMax();

    // 构建 MassTarget 集合用于 O(1) 查询选中状态
    std::unordered_set<ModuleID_t> selectedSet(
        IBR_WorkSpace::MassTarget.begin(),
        IBR_WorkSpace::MassTarget.end());

    QVariantList list;
    for (auto &[id, sd] : IBR_Inst_Project.IBR_SectionMap)
    {
        if (sd.Hidden && !sd.First) continue;
        // 阶段 D：跳过 IsIncluded() 的子模块（由父虚拟块递归渲染）
        // 对应 ImGui IBR_WorkSpace.cpp 主循环：if (sd.IsIncluded()) continue;
        // 子模块数据通过 getSectionData(id) 按需查询，IncludingModules 列表下发到父虚拟块
        if (sd.IsIncluded()) continue;

        list.append(buildSectionItem(id, sd, selectedSet));
        // 对齐 ImGui RenderUI 首次渲染后清除 First（IBR_SectionData.cpp:887）：
        // Qt 版不跑 ImGui RenderUI，First 恒为 true，导致上方"Hidden && !First 豁免"
        // 分支永不跳过 → 隐藏模块始终显示。首次构建后清除，恢复隐藏语义。
        sd.First = false;
    }
    // 增量更新 Repeater 模型：只对变化的项发信号（新增/删除只增删对应 delegate），
    // 避免整表替换重建全部 SectionNode（新建/删除模块卡顿根因）
    // 修复：必须先赋值 m_sections 再 updateFrom。updateFrom 会同步触发 QML Repeater
    // 创建 delegate → SectionNode 渲染 → reportSectionSize 回调，若此时 m_sections 还是
    // 旧值（空），reportSectionSize 里遍历 m_sections 找不到匹配项 → foundInSnapshot=false
    // → MiniMap 收到 eqW=0 的模块（不可见）。关闭项目→新建项目→加模块不显示即此根因。
    m_sections = std::move(list);
    m_sectionsModel->updateFrom(m_sections);
    m_sectionDataRevision++;  // D20：递增版本号
#ifdef INIWEAVER_DIAG
    qDebug() << "[REFRESH-DIAG] refreshSections emit sectionsChanged count=" << m_sections.size()
             << "smapSize=" << IBR_Inst_Project.IBR_SectionMap.size();
#endif
    emit sectionsChanged();
}

// 编辑侧边栏修改值后，只刷新指定 sectionId 的 SectionLineModel
// 避免全量 refreshSections 重建所有节点的 QVariantList 导致卡顿
// SectionLineModel::refresh 内部调 beginResetModel/endResetModel，
// QML 绑定到 lineModel 的 Repeater 会自动更新画布上该模块的键显示
void WorkspaceController::refreshSectionLines(qulonglong sectionId)
{
    // 修复：不能用 sectionId==0 判断无效。模块 ID=0 是合法的（第一个模块），
    // m_currentSectionId 初始化/clear 时用 0 作"无激活"标记，但 0 同时是合法 ID，
    // 导致激活模块 ID=0 时 OnShow 开关失效（emit sectionDataChanged(0) → 此处 return →
    // 画布 SectionLineModel 不刷新 → 键行不显示/隐藏 → "点了没反应"）。
    // 改用 IBR_SectionMap.find 判断：存在则刷新，不存在（含无激活的 0）才跳过。
    ModuleID_t id = static_cast<ModuleID_t>(sectionId);
    auto mapIt = IBR_Inst_Project.IBR_SectionMap.find(id);
    if (mapIt == IBR_Inst_Project.IBR_SectionMap.end()) return;
#ifdef INIWEAVER_DIAG
    qDebug() << "[ONSHOW-DIAG] refreshSectionLines sectionId=" << sectionId;
#endif
    auto it = m_lineModels.find(id);
    if (it != m_lineModels.end() && *it) {
        (*it)->refresh();
    }
    // 行值/OnShow 变化可能影响连线，标记端点脏
    m_linkEndpointsDirty = true;
    // 根治偶发一帧偏移：refresh() 触发 QML Repeater 重建 delegate（beginResetModel），
    // 新 delegate layoutDone=false，行重建期间 onYChanged 被门控拦截不回写 acceptCenter，
    // 只有 Component.onCompleted 的 Qt.callLater 回调回写一次。
    // 问题：50ms tick 可能在 Qt.callLater 回写之前的窗口期跑，此时 m_linkEndpointsDirty=true，
    // tick rebuild 读到旧 acceptCenter → 偏一帧（Qt.callLater 与 tick 时序非确定 → 偶发）。
    // 修复：refresh() 同步触发 delegate 创建 → onCompleted → Qt.callLater post（=QueuedConnection），
    // 此处 post 的 rebuild 排在所有 callLater 之后（同 posted event 队列，按 post 顺序处理），
    // 能读到全部新 acceptCenter。m_pendingRebuild 抑制 tick 在窗口期 rebuild（tick 检查此标志跳过）。
    // 拖拽/缩放/平移/松手过渡中跳过（由对应收尾路径重建）。
    if (!m_suppressLinkRebuild && m_inputState != 1 && m_dragSectionId == INVALID_MODULE_ID
        && !m_massDragging && !m_zoomPending) {
        if (!m_pendingRebuild) {
            m_pendingRebuild = true;
#ifdef INIWEAVER_DIAG
            qDebug() << "[ONSHOW-DIAG] refreshSectionLines QUEUED rebuild sectionId=" << sectionId;
#endif
            QMetaObject::invokeMethod(this, [this]() {
                m_pendingRebuild = false;
                rebuildLinkEndpoints();
                m_linkEndpointsDirty = false;
            }, Qt::QueuedConnection);
        }
    }
}

// 阶段 D2：构建单个 Section 的 QVariantMap（refreshSections 与 getSectionData 共用）
// 字段与 ImGui RenderUI 所需数据对齐：位置/状态/虚拟块/寄存器颜色/行模型/连线锚点
QVariantMap WorkspaceController::buildSectionItem(ModuleID_t id, const IBR_SectionData &sd,
                                                   const std::unordered_set<ModuleID_t> &selectedSet) const
{
    QVariantMap item;
    item["sectionId"] = static_cast<qulonglong>(id);
    item["displayName"] = QString::fromUtf8(sd.DisplayName);
    // 阶段 12.9：寄存器名（对应 ShowRegName 切换显示）
    item["registerName"] = QString::fromUtf8(sd.Desc.Sec);
    // 注释块正文（对应 ImGui CommentEdit / Bsec->Comment；输入框应显示它而非 DisplayName）
    item["comment"] = sd.CommentEdit
        ? QString::fromUtf8(reinterpret_cast<const char*>(sd.CommentEdit.get()))
        : QString();
    item["eqX"] = sd.EqPos.x;
    item["eqY"] = sd.EqPos.y;
    item["eqW"] = sd.EqSize.x;
    item["eqH"] = sd.EqSize.y;
#ifdef INIWEAVER_DIAG
    qDebug() << "[REFRESH-DIAG] buildSectionItem id=" << id
             << "EqPos=" << sd.EqPos.x << sd.EqPos.y
             << "EqSize=" << sd.EqSize.x << sd.EqSize.y;
#endif
    item["frozen"] = sd.Frozen;
    item["hidden"] = sd.Hidden;
    item["ignored"] = sd.Ignore;
    item["isComment"] = sd.IsComment;
    item["dragging"] = sd.Dragging;
    item["selected"] = selectedSet.contains(id);
    // 当前编辑节点高亮（对应 IBR_WorkSpace.cpp:1860-1868 FocusWindowColor）
    item["isEditing"] = (IBR_EditFrame::CurSection.ID == id);
    // 折叠状态持久化（对应 ActiveLines[k].Collapsed 的概念）
    item["collapsed"] = sd.UICollapsed;

    // 阶段 12.3：虚拟块相关字段
    bool isVirtualBlock = sd.IsVirtualBlock();
    bool isIncluded = sd.IsIncluded();
    item["isVirtualBlock"] = isVirtualBlock;
    item["isIncluded"] = isIncluded;
    item["collapsedInComposed"] = sd.CollapsedInComposed;
    item["includedByModule"] = static_cast<qulonglong>(sd.IncludedByModule);

    // 虚拟块：下发 IncludingModules 列表 + 隐藏子块计数
    if (isVirtualBlock)
    {
        QVariantList includingModules;
        int hiddenCount = 0;
        for (auto subId : sd.IncludingModules)
        {
            includingModules.append(static_cast<qulonglong>(subId));
            auto subData = IBR_Inst_Project.GetSectionFromID(subId).GetSectionData();
            if (subData && subData->Hidden) ++hiddenCount;
        }
        item["includingModules"] = includingModules;
        item["hiddenCount"] = hiddenCount;
        item["isComposedAllFold"] = sd.IsComposedAllFold();
    }

    // 寄存器类型颜色
    auto rsec = IBR_Inst_Project.GetSectionFromID(id);
    ImColor col = rsec.GetRegTypeColor();
    QColor regColor(
        static_cast<int>(col.Value.x * 255),
        static_cast<int>(col.Value.y * 255),
        static_cast<int>(col.Value.z * 255),
        static_cast<int>(col.Value.w * 255)
    );
    item["registerColor"] = regColor;

    // v3 批次 1.5：节点宽度自适应（对应 ImGui IBR_WorkSpace.cpp:1835-1842）
    // wbase = FontHeight * 17 * WidthRatio（IBB_Section::GetWidthBase = 17 * WidthRatio）
    // 节点宽度 = max(WidthFix, wbase)，WidthFix 在注释块编辑时更新（IBR_SectionData.cpp:831）
    // 注意：FontHeight 实际值为 24（Global.cpp:70），不是 13
    auto bsec = rsec.GetBack_Unsafe();
    if (bsec) {
        item["widthBase"] = static_cast<double>(FontHeight) * 17.0 * bsec->WidthRatio;
        item["widthFix"] = sd.WidthFix;
        // ImportCount > 0 表示该块是被导入的（对应 ImGui NotAsImported=false 分支）
        // ImGui 中导入块的 RadioButton 居中显示（IBR_SectionData.cpp:745-754）
        item["isImport"] = bsec->Dynamic.ImportCount > 0;
    } else {
        item["widthBase"] = static_cast<double>(FontHeight) * 17.0 * 1.0;  // 默认 WidthRatio=1
        item["widthFix"] = 0.0;
        item["isImport"] = false;
    }

    // 屏幕坐标
    ImVec2 rePos = IBR_WorkSpace::EqPosToRePos(sd.EqPos);
    item["screenX"] = rePos.x;
    item["screenY"] = rePos.y;
    item["screenW"] = sd.EqSize.x * IBR_FullView::Ratio;
    item["screenH"] = sd.EqSize.y * IBR_FullView::Ratio;

    // 行模型：每 Section 一个 SectionLineModel 实例（对应 ImGui RenderUI_Lines）
    // 被包含且折叠态的子模块不下发行模型（对应 RenderUI_Collapsed 不渲染行）
    if (!sd.IsComment && rsec.HasBack() && !(isIncluded && sd.CollapsedInComposed))
    {
        auto &modelPtr = m_lineModels[id];
        if (!modelPtr) {
            modelPtr = new SectionLineModel(const_cast<WorkspaceController*>(this));
            modelPtr->setSectionId(static_cast<qulonglong>(id));
            // 注入控制器：链接修改后同步触发工作区全量刷新（消除 SendToR 队列延迟）
            modelPtr->setWorkspaceController(const_cast<WorkspaceController*>(this));
            // 画布操作（toggleInputMode/modifyValue/createLink 等）→ 通知侧边栏刷新
            // 双向同步：EditPanel 改值 → refreshSectionLines 刷新画布；画布改值 → 通知 EditPanel
            connect(modelPtr, &SectionLineModel::sectionDataChanged,
                    this, [this](qulonglong sid) {
                if (m_editPanelController && sid == m_editPanelController->currentSectionId()) {
                    m_editPanelController->refreshLines();
                }
            });
            // LinkNode 位置回写 → 延迟重建端点表（根治 OnShow 切换/拖动偶发一帧偏移）。
            // 关键时序：LineRow.onYChanged 在 QML Column/ColumnLayout 的 polish event 中同步触发，
            // emit 本信号时 polish event 正在处理，QueuedConnection 入队的 rebuild 排在 polish 之后，
            // 此时所有子项 onYChanged 都已回写新 LastCenter → rebuild 读到新值。
            // 把 rebuild 时机从"父节点 height 变化"(reportSectionSize)挪到"子项 y 回写后"，
            // 时序确定，不再依赖 QML 布局同步/异步的非确定性。
            // 拖拽/缩放/平移/松手过渡中跳过 rebuild（由对应收尾路径重建），只标脏。
            // m_pendingRebuild 防止多行 onYChanged 连续 emit 时重复排队。
            connect(modelPtr, &SectionLineModel::linkNodeCenterChanged,
                    this, [this]() {
                auto *self = const_cast<WorkspaceController*>(this);
#ifdef INIWEAVER_DIAG
                qDebug() << "[ONSHOW-DIAG] linkNodeCenterChanged suppress=" << self->m_suppressLinkRebuild
                         << "state=" << self->m_inputState << "dragSec=" << self->m_dragSectionId
                         << "massDrag=" << self->m_massDragging << "zoomPending=" << self->m_zoomPending
                         << "pendingRebuild=" << self->m_pendingRebuild;
#endif
                if (self->m_suppressLinkRebuild || self->m_inputState == 1
                    || self->m_dragSectionId != INVALID_MODULE_ID || self->m_massDragging
                    || self->m_zoomPending) {
                    self->m_linkEndpointsDirty = true;
                    return;
                }
                if (!self->m_pendingRebuild) {
                    self->m_pendingRebuild = true;
                    QMetaObject::invokeMethod(self, [self]() {
                        self->m_pendingRebuild = false;
                        self->rebuildLinkEndpoints();
                        self->m_linkEndpointsDirty = false;
                    }, Qt::QueuedConnection);
                }
            });
        }
        modelPtr->setShowRegName(IBR_WorkSpace::ShowRegName);
        modelPtr->refresh();
        item["lineModel"] = QVariant::fromValue<QObject *>(modelPtr);
    }
    else
    {
        item["lineModel"] = QVariant();
    }

    // 折叠态连线锚点（标题栏右端点，QML layout 后回写）
    // 对应 ImGui HeadLineRN（IBR_SectionData.cpp:1063）
    item["collapsedCenterX"] = 0.0;
    item["collapsedCenterY"] = 0.0;
    item["headLineRNX"] = 0.0;
    item["headLineRNY"] = 0.0;

    return item;
}

// 阶段 D2：根据 sectionId 查询单个 Section 的完整数据（供虚拟块递归渲染子模块用）
// 对应 ImGui RenderUI_Virtual 中 GetSectionFromID(id).GetSectionData() 的数据获取
QVariantMap WorkspaceController::getSectionData(qulonglong sectionId) const
{
    ModuleID_t id = static_cast<ModuleID_t>(sectionId);
    auto rsec = IBR_Inst_Project.GetSectionFromID(id);
    auto sd = rsec.GetSectionData();
    if (!sd) return QVariantMap();

    // 选中状态查询（与 refreshSections 一致，从 MassTarget 构建）
    std::unordered_set<ModuleID_t> selectedSet(
        IBR_WorkSpace::MassTarget.begin(),
        IBR_WorkSpace::MassTarget.end());

    return buildSectionItem(id, *sd, selectedSet);
}

// 性能优化：轻量查询，仅返回 EqPos（避免 buildSectionItem 的全量重建）
QPointF WorkspaceController::getSectionEqPos(qulonglong sectionId) const
{
    ModuleID_t id = static_cast<ModuleID_t>(sectionId);
    auto it = IBR_Inst_Project.IBR_SectionMap.find(id);
    if (it == IBR_Inst_Project.IBR_SectionMap.end()) return QPointF();
    return QPointF(it->second.EqPos.x, it->second.EqPos.y);
}

// 修复：Qt 版本无 ImGui 每帧渲染流程，LinkList 不会被 PushLinkForDraw 自动填充
// 此函数遍历所有 Section 的 SubSecs.NewLinkTo，重建 LinkList
// 对应 ImGui RenderUI_Collapsed（IBR_SectionData.cpp:835-856）+ RenderUI_Lines 中 PushLinkForDraw 调用
void WorkspaceController::rebuildLinkList()
{
    // 仅在 RefreshLinkList=true 时重建（对应 ImGui 每帧开始时检查 RefreshLinkList）
    if (!IBR_Inst_Project.RefreshLinkList) return;

    IBR_Inst_Project.LinkList.clear();

    // 辅助：推送一个 section 的所有 SubSecs.NewLinkTo 到 LinkList
    // 虚拟块递归推送 IncludingModules 子模块，对应 ImGui RenderUI_Virtual
    // fold_left 调 Data->RenderUI() 递归渲染子模块，子模块 RenderUI_Collapsed/
    // RenderUI_Lines 内部调 PushLinkForDraw 推送自身连线
    std::function<void(ModuleID_t, const IBR_SectionData&)> pushLinks =
        [&](ModuleID_t id, const IBR_SectionData& sd)
    {
        auto rsec = IBR_Inst_Project.GetSectionFromID(id);
        auto bsec = rsec.GetBack_Unsafe();
        if (!bsec) return;

        // 折叠态：源端点汇聚到头部 RadioButton（LastCenter 由 QML headLineRN 回写）
        // 非折叠态：源端点为行级 RadioButton（LastCenter 由 LinkNodePoint 回写）
        bool collapsed = sd.UICollapsed || sd.CollapsedInComposed;

        // 遍历所有 SubSec 的 NewLinkTo（对应 RenderUI_Collapsed 行 840-855）
        for (auto i : bsec->SubSecOrder)
        {
            if (i >= bsec->SubSecs.size()) continue;
            const auto &sub = bsec->SubSecs[i];
            for (const auto &lt : sub.NewLinkTo)
            {
                if (lt.Empty()) continue;

                bool FromImport = (sub.Default->Type == IBB_SubSec_Default::Import);
                bool IsSelfLinked = (lt.FromLoc.Sec == lt.ToLoc.Sec);

                // 构建 _Plink 并 push（对应 PushLinkForDraw）
                IBR_Project::_Plink plink;
                plink.Dest = lt.ToLoc.Sec;
                plink.DestKey = lt.ToLoc.Key;
                plink.FromKey = lt.FromLoc.Key;
                plink.SrcMult = lt.FromLoc.Mult;
                plink.LineMult = lt.ToLoc.Mult;
                plink.SourceID = lt.SessionID;
                plink.SrcModuleID = id;
                plink.Color = lt.DefaultColor;
                plink.FromImport = FromImport;
                plink.IsSelfLinked = IsSelfLinked;
                plink.IsSrcDragging = false;
                IBR_Inst_Project.LinkList.push_back(plink);

                // 更新 Session 状态（对应 PushLinkForDraw 调 SetSessionStatus）
                // 折叠态标记 Collapsed=true，非折叠态由 LinkNodePoint 回写
                if (collapsed)
                {
                    IBR_NodeSession::SetSessionStatus(lt.SessionID, ImVec2(0, 0), true);
                }
            }
        }

        // 虚拟块：递归推送 IncludingModules 子模块的连线
        // 对应 ImGui RenderUI_Virtual 无条件调 Data->RenderUI() 渲染所有子模块
        if (sd.IsVirtualBlock())
        {
            for (auto subId : sd.IncludingModules)
            {
                auto subData = IBR_Inst_Project.GetSectionFromID(subId).GetSectionData();
                if (!subData) continue;
                if (subData->Hidden && !subData->First) continue;
                pushLinks(subId, *subData);
            }
        }
    };

    // 遍历所有 Section（对应 ImGui 主循环遍历 IBR_SectionMap）
    for (const auto &[id, sd] : IBR_Inst_Project.IBR_SectionMap)
    {
        if (sd.Hidden && !sd.First) continue;
        if (sd.IsIncluded()) continue;  // 子模块由父虚拟块递归处理，避免重复推送

        pushLinks(id, sd);
    }

    IBR_Inst_Project.RefreshLinkList = false;
    m_linkEndpointsDirty = true;  // LinkList 变化，端点表需重建
}

void WorkspaceController::refreshLinks()
{
    // 修复：Qt 版本无 ImGui 每帧渲染流程，LinkList 不会被 PushLinkForDraw 自动填充
    // 需在构建 m_links 前遍历所有 Section 的 SubSecs.NewLinkTo 重建 LinkList
    // 对应 ImGui RenderUI_Collapsed/RenderUI_Lines 中遍历 Bsec->SubSecs 调 PushLinkForDraw
    rebuildLinkList();

    QVariantList list;
    for (const auto &link : IBR_Inst_Project.LinkList)
    {
        // 对齐 ImGui 连线渲染跳过 Hidden（IBR_WorkSpace.cpp:1491 RSD->Hidden continue）：
        // 源/目标模块任一隐藏则不渲染该连线（隐藏后模块消失，连线不应残留）
        auto srcRsec = IBR_Inst_Project.GetSectionFromID(link.SrcModuleID);
        auto srcSD = srcRsec.GetSectionData();
        if (srcSD && srcSD->Hidden) continue;
        // 目标端：不能用 GetSection(link.Dest)！反向索引（IBR_Rev_SectionMapII）缺失时
        // 它会调用 GetSection(ToDesc()) 创建 Back_GunMu 滚木占位模块（IBR_Project.cpp:310），
        // 占位 Hidden=false → 隐藏过滤失效且每次刷新污染 IBR_SectionMap。
        // 改用只读的 GetSectionID(Desc→MaxID)，查不到视为残留连线直接跳过。
        ModuleID_t dstActualId = 0;
        auto dstIdOpt = IBR_Inst_Project.GetSectionID(link.Dest.ToDesc());
        if (dstIdOpt)
        {
            auto dstRsec = IBR_Inst_Project.GetSectionFromID(*dstIdOpt);
            auto dstSD = dstRsec.GetSectionData();
            if (dstSD && dstSD->Hidden) continue;
            dstActualId = *dstIdOpt;
        }
        else
        {
            continue;  // 目标模块不存在（残留连线）
        }

        QVariantMap item;
        // 源信息（字符串，避免 QML 大整数精度丢失）
        item["sourceId"] = QString::number(static_cast<qulonglong>(link.SrcModuleID));
        item["sourceSessionId"] = QString::number(static_cast<qulonglong>(link.SourceID));
        // 目标信息（字符串）
        item["destId"] = QString::number(static_cast<qulonglong>(dstActualId));
        item["destKey"] = QString::fromUtf8(PoolStr(link.DestKey));
        item["lineMult"] = static_cast<int>(link.LineMult);
        // 颜色（ImU32 ABGR → QColor）
        item["color"] = QColor(
            (link.Color >> 0) & 0xFF,
            (link.Color >> 8) & 0xFF,
            (link.Color >> 16) & 0xFF,
            (link.Color >> 24) & 0xFF
        );
        item["fromImport"] = link.FromImport;
        item["isSelfLinked"] = link.IsSelfLinked;
        item["isSrcDragging"] = link.IsSrcDragging;
        // 折叠态（从 Session 查询）
        const auto &sv = IBR_NodeSession::GetSessionValue(link.SourceID);
        item["isCollapsed"] = sv.Collapsed;
        list.append(item);
    }
    m_links = std::move(list);
    emit linksChanged();
}

void WorkspaceController::rebuildLinkEndpoints()
{
    // 缩放叠加结束：先强制完成补间（重建后 Ratio 不得再变，否则连线与节点错位），
    // 再终止换算叠加（LinkRenderer 直接用新端点表）
    finishZoomTweenNow();
    if (m_zoomPending) {
        m_zoomPending = false;
        emit zoomBaseChanged();
    }
    // 修复时序竞态：QML 节点异步布局，首帧回写尚未到达时 m_sectionAcceptPoint/m_lineModels 均空，
    // 导致所有 pa=(0,0)、pbValid=false，连线画在屏幕原点或不显示。
    // 解决方案：用 IBR_FullView::EqPosToRePos(EqPos) 直接计算兜底坐标（对应 ImGui ReWindowUL），
    //          QML 回写的精确坐标作为高优先级覆盖。
    //
    // ImGui 标题栏 RadioButton 中心 = ReWindowUL + ReOffset
    //   ReWindowUL = ImGui::GetCursorScreenPos()（节点左上角屏幕坐标，= EqPosToRePos(EqPos)）
    //   ReOffset = { FontHeight*0.7, HalfLine }（普通块）或 { W*0.5 - FontHeight*0.5, HalfLine }（导入块）
    //   HalfLine = FontHeight * 0.5（标题栏垂直中线）
    //
    // 折叠态源端点（pa）= 头部 RadioButton 中心（对应 RenderUI_Collapsed HeadLineRN）
    // 非折叠态源端点（pa）= 行级 LinkNode 圆点中心（对应 RenderUI_Lines UpdateLink）
    //
    // 兜底策略：
    //   pa 优先级：QML 回写 LastCenter(非0) > QML 回写 m_sectionAcceptPoint > EqPos 兜底
    //   pb 优先级：QML 回写行级 acceptCenter > QML 回写 m_sectionAcceptPoint > EqPos 兜底
    //   这样首帧即可用 EqPos 兜底显示连线，QML 布局完成后回写覆盖为精确值

    constexpr float FontHeight = 13.0f;  // 对应 buildSectionItem 中 FontHeight 基准
    const float ratio = IBR_FullView::Ratio;
    const float fontHeightScaled = FontHeight * ratio;
    const float halfLine = fontHeightScaled * 0.5f;

    QVariantList endpoints;
    QVariantMap endpointsMap;
    for (const auto &link : IBR_Inst_Project.LinkList)
    {
        const auto &sv = IBR_NodeSession::GetSessionValue(link.SourceID);
        QVariantMap ep;
        // 字符串传递，避免 QML 大整数精度丢失
        ep["sessionId"] = QString::number(static_cast<qulonglong>(link.SourceID));

        // ===== 起点 pa（源端圆点）=====
        qreal paX = 0.0, paY = 0.0;
        bool paValid = false;

        // 源行可见性：OnShow=false 的行不在 m_entries 中，其 acceptCenter/LastCenter
        // 是隐藏前的残留坐标，直接使用会让连线起点指向空白处。
        // 隐藏行（非折叠态）回退到标题栏最右端；折叠态（sv.Collapsed=true）仍走优先级 3
        // （头部 RadioButton，对应 setHeadLineRN 语义）。
        auto srcRsec = IBR_Inst_Project.GetSectionFromID(link.SrcModuleID);
        auto srcBsec = srcRsec.GetBack_Unsafe();
        bool srcLineVisible = (link.FromKey == EmptyPoolStr) || !srcBsec
                              || srcBsec->IsOnShow(link.FromKey);

        // 源模块自身折叠（编组内收起 / UICollapsed）：连线源点收敛到模块「最右端」锚点，
        // 对应 ImGui RenderUI_Collapsed 的 HeadLineRN（IBR_SectionData.cpp:835-853，
        // HeadLineRN = GetLineEndPos - {FontHeight*1.5, HalfLine}，即标题栏最右端垂直居中），
        // 与隐藏行起点一致。直接计算并跳过后面的优先级。
        auto srcDataFull = IBR_Inst_Project.GetSectionFromID(link.SrcModuleID).GetSectionData();
        bool srcCollapsed = srcDataFull && (srcDataFull->CollapsedInComposed || srcDataFull->UICollapsed);
        if (srcCollapsed && srcDataFull)
        {
            // 折叠子模块在 QML 里被堆叠在父虚拟块内（同 x 列、不同 y），其实际可见位置是
            // QML 回写的 m_sectionAcceptPoint（headLineRN/RadioButton 中心）。不能用各子模块
            // 独立全局 EqPosToRePos(EqPos)——那套坐标与 QML 堆叠渲染脱节，导致连线错位成乱。
            // pa 为源端「最右端」。注意 acceptPt.x 是 RadioButton 中心而非模块左边缘：
            //   普通块 RadioButton 距模块左边缘 = leftMargin(8) + 半径(5) = 13 逻辑（×ratio 视觉）
            //   import 块 RadioButton 水平居中 → 距左边缘 = 半宽
            // 故 pa.x = 模块右端 = acceptPt.x - radioLeftOffset + EqSize.x*ratio
            ImVec2 rePosC = IBR_WorkSpace::EqPosToRePos(srcDataFull->EqPos);
            auto itA = m_sectionAcceptPoint.find(static_cast<qulonglong>(link.SrcModuleID));
            float baseX = (itA != m_sectionAcceptPoint.end() && !itA->isNull())
                ? static_cast<float>(itA->x()) : rePosC.x;
            bool srcIsImport = srcBsec && srcBsec->Dynamic.ImportCount > 0;
            float radioLeftOffset = srcIsImport
                ? (srcDataFull->EqSize.x * ratio * 0.5f)   // import 居中：中心=半宽
                : (13.0f * ratio);                          // 普通块：左margin8 + 圆心距5
            paX = static_cast<qreal>(baseX - radioLeftOffset + srcDataFull->EqSize.x * ratio);
            paY = (itA != m_sectionAcceptPoint.end() && !itA->isNull())
                ? itA->y() : static_cast<qreal>(rePosC.y + halfLine);
            paValid = true;
#ifdef INIWEAVER_DIAG
            qDebug() << "[FOLD-LINK] srcCollapsed src=" << static_cast<qulonglong>(link.SrcModuleID)
                     << "EqPos=(" << srcDataFull->EqPos.x << "," << srcDataFull->EqPos.y << ")"
                     << "EqSize=(" << srcDataFull->EqSize.x << "," << srcDataFull->EqSize.y << ")"
                     << "ratio=" << ratio
                     << "rePos=(" << rePosC.x << "," << rePosC.y << ")"
                     << "acceptPt=" << (itA != m_sectionAcceptPoint.end() ? QString("(%1,%2)").arg(itA->x()).arg(itA->y()) : QStringLiteral("none"))
                     << "pa=(" << paX << "," << paY << ")";
#endif
        }

        // 优先级 1：源行圆点（按 FromKey 查行级接受点，与 QML 回写同源）。
        // 修复：LastCenter 按 SessionID 索引，IIF 行链接的 SessionID 用 Comp=cidx（UpdateAll），
        // 而 QML 圆点回写用 Comp=0（rebuildEntries），两者不匹配 → LastCenter=0 → pa 回退标题栏（起点错）。
        // 按源行 key 查询直接取该行圆点坐标，免疫 SessionID 不匹配。
        if (srcLineVisible && !srcCollapsed && link.FromKey != EmptyPoolStr)
        {
            auto itSrcModel = m_lineModels.find(static_cast<qulonglong>(link.SrcModuleID));
            if (itSrcModel != m_lineModels.end() && *itSrcModel)
            {
                QPointF srcAc = (*itSrcModel)->acceptCenterByKey(
                    QString::fromUtf8(PoolStr(link.FromKey)), static_cast<int>(link.SrcMult));
                if (!srcAc.isNull())
                {
                    paX = srcAc.x();
                    paY = srcAc.y();
                    paValid = true;
                }
            }
        }
        // 优先级 2：QML 回写的 LastCenter（非 0 才用，0 表示尚未回写；源行隐藏时跳过）
        if (!paValid && srcLineVisible && (sv.LastCenter.x != 0.0f || sv.LastCenter.y != 0.0f))
        {
            paX = static_cast<qreal>(sv.LastCenter.x);
            paY = static_cast<qreal>(sv.LastCenter.y);
            paValid = true;
        }
        // 隐藏行专用：源行隐藏（非折叠态）时连线起点落到标题栏最右端。
        // EqSize 由 QML reportSectionSize 回写，EqSize.x*ratio = 节点实际屏幕宽度，
        // 故 rePos.x + EqSize.x*ratio = 标题栏右边缘。
        // y 与头节点 RadioButton 水平对齐：优先用 m_sectionAcceptPoint 回写的头节点实际中心 y
        // （非折叠态由 updateRNCenter→setSectionAcceptPoint 回写），兜底 halfLine。
        // 折叠态（sv.Collapsed=true / srcCollapsed）由上方 srcCollapsed 分支处理，不进此分支。
        if (!paValid && !srcCollapsed && !srcLineVisible && !sv.Collapsed)
        {
            auto srcDataH = IBR_Inst_Project.GetSectionFromID(link.SrcModuleID).GetSectionData();
            if (srcDataH)
            {
                ImVec2 rePos = IBR_WorkSpace::EqPosToRePos(srcDataH->EqPos);
                paX = static_cast<qreal>(rePos.x + srcDataH->EqSize.x * ratio);
                auto itH = m_sectionAcceptPoint.find(static_cast<qulonglong>(link.SrcModuleID));
                paY = (itH != m_sectionAcceptPoint.end() && !itH->isNull())
                    ? itH->y()
                    : static_cast<qreal>(rePos.y + halfLine);
                paValid = true;
            }
        }
        // 优先级 3：QML 回写的标题栏接受点（折叠态由 setHeadLineRN 回写）
        if (!paValid)
        {
            auto itSrcAccept = m_sectionAcceptPoint.find(static_cast<qulonglong>(link.SrcModuleID));
            if (itSrcAccept != m_sectionAcceptPoint.end() && !itSrcAccept->isNull())
            {
                paX = itSrcAccept->x();
                paY = itSrcAccept->y();
                paValid = true;
            }
        }
        // 兜底：用源 section 的 EqPos 直接计算标题栏 RadioButton 中心
        // 对应 ImGui ReWindowUL + ReOffset = EqPosToRePos(EqPos) + {FontHeight*0.7, HalfLine}
        if (!paValid)
        {
            auto srcData = IBR_Inst_Project.GetSectionFromID(link.SrcModuleID).GetSectionData();
            if (srcData)
            {
                ImVec2 rePos = IBR_WorkSpace::EqPosToRePos(srcData->EqPos);
                // 判断源是否导入块（ReOffset 不同）
                auto srcRsec = IBR_Inst_Project.GetSectionFromID(link.SrcModuleID);
                auto srcBsec = srcRsec.GetBack_Unsafe();
                bool isImport = srcBsec && srcBsec->Dynamic.ImportCount > 0;
                float reOffsetX = isImport
                    ? (srcData->EqSize.x * ratio * 0.5f - fontHeightScaled * 0.5f)
                    : (fontHeightScaled * 0.7f);
                paX = static_cast<qreal>(rePos.x + reOffsetX);
                paY = static_cast<qreal>(rePos.y + halfLine);
                paValid = true;
            }
        }
        ep["x"] = paX;
        ep["y"] = paY;
        ep["isCollapsed"] = sv.Collapsed;

        // ===== 终点 pb（目标接受点）=====
        qreal pbX = 0.0, pbY = 0.0;
        bool pbValid = false;

        // 优先级 1：行级接受点（非折叠态目标，对应 ImGui AcceptCenter[LineMult]）
        // 目标端安全查找：不能用 GetSection(link.Dest)！反向索引（IBR_Rev_SectionMapII）
        // 缺失时它会调用 GetSection(ToDesc()) 创建 Back_GunMu 滚木占位（IBR_Project.cpp:310），
        // pb 兜底会用到滚木的坐标（EqPos 默认 (0,0)）→ 连线终点错位到视口中心附近（轻微偏移），
        // 且每次重建都 ++MaxID 污染 IBR_SectionMap，滚木随后还会被 refreshSections 渲染到画布。
        // 改用只读的 GetSectionID(Desc→MaxID)，查不到视为残留连线直接跳过。
        ModuleID_t dstActualId = 0;
        auto dstIdOpt = IBR_Inst_Project.GetSectionID(link.Dest.ToDesc());
        if (!dstIdOpt) continue;  // 目标模块不存在（残留连线），跳过该端点
        dstActualId = *dstIdOpt;
        auto dstRsec = IBR_Inst_Project.GetSectionFromID(dstActualId);
        auto dstData = dstRsec.GetSectionData();
        auto dstBsec = dstRsec.GetBack_Unsafe();

        // 目标行可见性：OnShow=false 的行其 acceptCenter 是隐藏前残留坐标，
        // 直接使用会让连线终点指向空白处。隐藏行（非折叠态）回退到标题栏最右端；
        // 折叠态（sv.Collapsed=true）仍走优先级 2（标题栏 RadioButton）。
        bool dstLineVisible = (link.DestKey == EmptyPoolStr) || !dstBsec
                              || dstBsec->IsOnShow(link.DestKey);
        // 目标模块自身折叠（编组收起 / UICollapsed）：目标端是「块端」，连线应收敛到
        // 目标模块标题栏 RadioButton（左端，ReWindowUL+ReOffset），对应 ImGui
        // RSD->ReWindowUL + RSD->ReOffset（非折叠 pb 默认锚点，IBR_SectionData.cpp:749-753）。
        // 行已隐藏、坐标残留故跳过行级坐标，直接落左端标题栏。
        bool dstCollapsed = dstData && (dstData->CollapsedInComposed || dstData->UICollapsed);
        if (dstCollapsed && dstData)
        {
            // 目标端「块端」落在折叠子模块标题栏 RadioButton（左端）。折叠子模块在 QML 里
            // 堆叠于父虚拟块内，实际可见 RadioButton 位置由 QML 回写 m_sectionAcceptPoint，
            // 不能再用各子模块独立全局 EqPos。pb = acceptPt 直接（x 为 RadioButton 中心）。
            ImVec2 rePosD = IBR_WorkSpace::EqPosToRePos(dstData->EqPos);
            auto itD = m_sectionAcceptPoint.find(static_cast<qulonglong>(dstActualId));
            if (itD != m_sectionAcceptPoint.end() && !itD->isNull())
            {
                pbX = itD->x();
                pbY = itD->y();
            }
            else
            {
                float reOffsetX = (dstBsec && dstBsec->Dynamic.ImportCount > 0)
                    ? (dstData->EqSize.x * ratio * 0.5f - fontHeightScaled * 0.5f)
                    : (fontHeightScaled * 0.7f);
                pbX = static_cast<qreal>(rePosD.x + reOffsetX);
                pbY = static_cast<qreal>(rePosD.y + halfLine);
            }
            pbValid = true;
#ifdef INIWEAVER_DIAG
            qDebug() << "[FOLD-LINK] dstCollapsed dst=" << static_cast<qulonglong>(dstActualId)
                     << "EqPos=(" << dstData->EqPos.x << "," << dstData->EqPos.y << ")"
                     << "EqSize=(" << dstData->EqSize.x << "," << dstData->EqSize.y << ")"
                     << "ratio=" << ratio
                     << "rePos=(" << rePosD.x << "," << rePosD.y << ")"
                     << "acceptPt=" << (itD != m_sectionAcceptPoint.end() ? QString("(%1,%2)").arg(itD->x()).arg(itD->y()) : QStringLiteral("none"))
                     << "pb=(" << pbX << "," << pbY << ")";
#endif
        }

        if (!sv.Collapsed && !dstCollapsed && dstLineVisible && link.DestKey != EmptyPoolStr && dstData)
        {
            auto itModel = m_lineModels.find(dstActualId);
            if (itModel != m_lineModels.end() && *itModel)
            {
                QString keyName = QString::fromUtf8(PoolStr(link.DestKey));
                QPointF ac = (*itModel)->acceptCenterByKey(keyName, static_cast<int>(link.LineMult));
                if (!ac.isNull())
                {
                    pbX = ac.x();
                    pbY = ac.y();
                    pbValid = true;
                }
            }
        }
        // 隐藏行专用：目标行隐藏（非折叠态）时连线终点落到标题栏最右端。
        // y 与头节点 RadioButton 水平对齐：优先用 m_sectionAcceptPoint 回写的头节点实际中心 y，
        // 兜底 halfLine。折叠态（sv.Collapsed=true）不进此分支，仍由优先级 2 走标题栏 RadioButton。
        if (!pbValid && !dstLineVisible && !sv.Collapsed)
        {
            if (dstData)
            {
                ImVec2 rePos = IBR_WorkSpace::EqPosToRePos(dstData->EqPos);
                pbX = static_cast<qreal>(rePos.x + dstData->EqSize.x * ratio);
                auto itH = m_sectionAcceptPoint.find(static_cast<qulonglong>(dstActualId));
                pbY = (itH != m_sectionAcceptPoint.end() && !itH->isNull())
                    ? itH->y()
                    : static_cast<qreal>(rePos.y + halfLine);
                pbValid = true;
            }
        }
        // 优先级 2：QML 回写的标题栏接受点（对应 ImGui RSD->ReWindowUL + RSD->ReOffset）
        if (!pbValid)
        {
            auto itAccept = m_sectionAcceptPoint.find(static_cast<qulonglong>(dstActualId));
            if (itAccept != m_sectionAcceptPoint.end() && !itAccept->isNull())
            {
                pbX = itAccept->x();
                pbY = itAccept->y();
                pbValid = true;
            }
        }
        // 兜底：用目标 section 的 EqPos 直接计算标题栏 RadioButton 中心
        // 对应 ImGui ReWindowUL + ReOffset
        if (!pbValid && dstData)
        {
            ImVec2 rePos = IBR_WorkSpace::EqPosToRePos(dstData->EqPos);
            bool isImport = dstBsec && dstBsec->Dynamic.ImportCount > 0;
            float reOffsetX = isImport
                ? (dstData->EqSize.x * ratio * 0.5f - fontHeightScaled * 0.5f)
                : (fontHeightScaled * 0.7f);
            pbX = static_cast<qreal>(rePos.x + reOffsetX);
            pbY = static_cast<qreal>(rePos.y + halfLine);
            pbValid = true;
        }

        ep["pbX"] = pbX;
        ep["pbY"] = pbY;
        ep["pbValid"] = pbValid;
        // destId 用实际 ID（GetSection 返回的 ID），字符串传递
        ep["destId"] = QString::number(static_cast<qulonglong>(dstActualId));
        endpoints.append(ep);
#ifdef INIWEAVER_DIAG
        qDebug() << "[LINK-DIAG] rebuild src=" << static_cast<qulonglong>(link.SrcModuleID)
                 << "dest=" << static_cast<qulonglong>(dstActualId)
                 << "paValid=" << paValid << "pa=(" << paX << "," << paY << ")"
                 << "pbValid=" << pbValid << "pb=(" << pbX << "," << pbY << ")"
                 << "srcCollapsed=" << srcCollapsed
                 << "dstCollapsed=" << dstCollapsed
                 << "isCollapsed(src)=" << sv.Collapsed;
#endif

        // D21：构建 map（key = "sessionId:destId"）
        // 字符串 key，与 QML 端 link.sourceSessionId + ":" + link.destId 拼接结果一致
        QString mapKey = QString::number(static_cast<qulonglong>(link.SourceID))
                         + QLatin1String(":")
                         + QString::number(static_cast<qulonglong>(dstActualId));
        endpointsMap.insert(mapKey, ep);
    }
    m_linkEndpoints = std::move(endpoints);
    m_linkEndpointsMap = std::move(endpointsMap);
    // 端点表已按最新行圆点坐标重建，清空画布平移偏移（QML 不再叠加）
    if (!m_canvasDragOffset.isNull()) {
        m_canvasDragOffset = QPointF();
        emit canvasDragOffsetChanged();
    }
    // 端点表已重建到当前视口：更新基准尺寸并归零视口偏移（QML 不再叠加）
    m_endpointBaseW = static_cast<qreal>(IBR_RealCenter::WorkSpaceDR.x);
    m_endpointBaseH = static_cast<qreal>(IBR_RealCenter::WorkSpaceDR.y);
    if (m_viewportOffsetX != 0.0 || m_viewportOffsetY != 0.0) {
        m_viewportOffsetX = 0.0;
        m_viewportOffsetY = 0.0;
        emit viewportOffsetChanged();
    }
    emit linkEndpointsChanged();
}

void WorkspaceController::updateInputState(int newState)
{
    if (m_inputState != newState) {
        m_inputState = newState;
        emit inputStateChanged();
    }
}

size_t WorkspaceController::computeStateHash() const
{
    // D11：计算所有 Section 的状态哈希（Frozen/Hidden/Ignore/UICollapsed/CollapsedInComposed + ShowRegName）
    // 简单累加哈希，足以检测任意节点的状态变化（如 Ignore 切换）
    size_t h = showRegName() ? 1u : 0u;
    for (const auto &[id, sd] : IBR_Inst_Project.IBR_SectionMap) {
        h += static_cast<size_t>(id) * 31u
             + (sd.Frozen ? 1u : 0u)
             + (sd.Hidden ? 2u : 0u)
             + (sd.Ignore ? 4u : 0u)
             + (sd.UICollapsed ? 8u : 0u)
             + (sd.CollapsedInComposed ? 16u : 0u);
    }
    return h;
}

// ===== 阶段 12.1/12.2：MassAfter 状态机 + 多节点拖拽实现 =====

void WorkspaceController::beginMassDrag(qreal startX, qreal startY)
{
    // 阶段 12.2：MassAfter → HoldingModules（多节点拖拽入口）
    // 对应 IBR_WorkSpace.cpp:1291-1308
    if (m_inputState != 4) return;
    if (IBR_WorkSpace::MassTarget.empty()) return;

    m_suppressLinkRebuild = false;  // 新拖拽开始：恢复端点表重建（防上次松手残留抑制）
    m_lastMassDragIds.clear();      // 清上次多选松手过渡集合（防残留误叠加）
    m_massDragIds = IBR_WorkSpace::MassTarget;
    m_dragStartScreenPos = QPointF(startX, startY);
    m_moveAfterMass = true;
    m_hasLeftDownToWait = true;  // 防止本帧立刻触发取消分支
    m_initHolding = false;       // 待 initMassDrag 计算

    // 同步标记所有 MassTarget 为 Dragging（让 refreshSections 立即反映到 QML）
    for (auto id : m_massDragIds) {
        auto it = IBR_Inst_Project.IBR_SectionMap.find(id);
        if (it != IBR_Inst_Project.IBR_SectionMap.end()) {
            it->second.Dragging = true;
        }
    }

    updateInputState(3);  // HoldingModules
    // 修复：不调用 refresh()，避免重建 QVariantList 导致 Repeater 销毁当前 delegate 丢失 mouse grab
    // 改用 massDragging 属性通知 QML 对所有选中节点应用 dragOffset
    m_massDragging = true;
    emit massDraggingChanged();
}

void WorkspaceController::initMassDrag()
{
    // 计算面积加权质心 + 各节点 EqDelta
    // 对应 IBR_WorkSpace.cpp:1004-1024
    ImVec2 tSum{0, 0};
    double sum = 0;
    for (auto id : m_massDragIds) {
        auto it = IBR_Inst_Project.IBR_SectionMap.find(id);
        if (it == IBR_Inst_Project.IBR_SectionMap.end()) continue;
        const auto &data = it->second;
        if (!data.Dragging) continue;
        float area = data.EqSize.x * data.EqSize.y;
        tSum.x += data.EqPos.x * area;
        tSum.y += data.EqPos.y * area;
        sum += area;
    }
    if (sum > 0) {
        tSum.x /= static_cast<float>(sum);
        tSum.y /= static_cast<float>(sum);
    }
    // InitialMassCenter = 质心
    m_holdingStartEqMouse = screenToEq(m_dragStartScreenPos);
    // HoldingStartEqDelta = InitialMassCenter - HoldingStartEqMouse
    m_holdingStartEqDelta = QPointF(tSum.x - m_holdingStartEqMouse.x(),
                                     tSum.y - m_holdingStartEqMouse.y());
    // 各节点 EqDelta = EqPos - 质心
    IBRF_CoreBump.SendToR({ [ids = m_massDragIds, cx = tSum.x, cy = tSum.y]() {
        for (auto id : ids) {
            auto it = IBR_Inst_Project.IBR_SectionMap.find(id);
            if (it == IBR_Inst_Project.IBR_SectionMap.end()) continue;
            if (!it->second.Dragging) continue;
            it->second.EqDelta.x = it->second.EqPos.x - cx;
            it->second.EqDelta.y = it->second.EqPos.y - cy;
        }
    } });
    m_initHolding = true;
}

void WorkspaceController::updateMassDrag(qreal curX, qreal curY)
{
    // 性能优化：多节点拖拽也用 dragOffset（屏幕坐标偏移量）
    // QML 端根据 dragOffset 计算拖拽中节点的临时位置
    // EqPos 在 endMassDrag 时才写回后端
    if (!m_initHolding) {
        initMassDrag();
    }

    // 阶段 4：边界推回（对应 ImGui UpdateScrollAlt + UpdateScrollGeneral）
    // 鼠标接近视口边缘时，画布自动滚动，拖拽节点跟随鼠标
    // 推回量 ET（屏幕坐标），补偿 dragStartScreen 保持拖拽节点相对鼠标位置不变
    float fontHeight = 13.0f * IBR_FullView::Ratio;
    float scrollRate = 1.0f;
    float ulx = IBR_RealCenter::WorkSpaceUL.x;
    float uly = IBR_RealCenter::WorkSpaceUL.y;
    float drx = IBR_RealCenter::WorkSpaceDR.x;
    float dry = IBR_RealCenter::WorkSpaceDR.y;
    float mxF = static_cast<float>(curX);
    float myF = static_cast<float>(curY);

    float etX = 0.0f, etY = 0.0f;
    etX += scrollRate * std::min(std::max(fontHeight - mxF + ulx, 0.0f), fontHeight);
    etX -= scrollRate * std::min(std::max(fontHeight + mxF - drx, 0.0f), fontHeight);
    etY += scrollRate * std::min(std::max(fontHeight - myF + uly, 0.0f), fontHeight);
    etY -= scrollRate * std::min(std::max(fontHeight + myF - dry, 0.0f), fontHeight);

    if (std::abs(etX) > 0.01f || std::abs(etY) > 0.01f) {
        IBR_FullView::EqCenter.x -= etX / IBR_FullView::Ratio;
        IBR_FullView::EqCenter.y -= etY / IBR_FullView::Ratio;
        IBR_FullView::EqPosFixRange(IBR_FullView::EqCenter);
        // 补偿 dragStartScreenPos：dragOffset = curScreen - dragStartScreenPos
        // 画布推回 etX 后，拖拽节点视觉位置 += etX（因 EqCenter 变化）
        // 为保持节点相对鼠标不变，dragOffset 需 -= etX，故 dragStartScreenPos += etX
        m_dragStartScreenPos.setX(m_dragStartScreenPos.x() + etX);
        m_dragStartScreenPos.setY(m_dragStartScreenPos.y() + etY);
        emit eqCenterChanged();
        emit edgeFlagsChanged();
        m_linkEndpointsDirty = true;  // D10：边界推回导致 EqCenter 变化，需重建端点表
    }

    m_dragOffset = QPointF(curX - m_dragStartScreenPos.x(),
                            curY - m_dragStartScreenPos.y());
    emit dragOffsetChanged();
}

void WorkspaceController::endMassDrag()
{
    // 拖拽收尾前强制完成缩放补间（同 endMoveSection：写回 dragOffset/Ratio 要求配套值，
    // 此处 m_massDragging 尚未清零，applyZoomStep 的拖拽修正分支仍生效）
    finishZoomTweenNow();
    // 拖拽结束：把 dragOffset 转换为 EqPos 偏移并写回所有拖拽节点
    float eqDeltaX = static_cast<float>(m_dragOffset.x()) / IBR_FullView::Ratio;
    float eqDeltaY = static_cast<float>(m_dragOffset.y()) / IBR_FullView::Ratio;
    // 修复：同步写回 EqPos，不走 SendToR 异步队列（与 endMoveSection 同理）
    for (auto id : m_massDragIds) {
        auto it = IBR_Inst_Project.IBR_SectionMap.find(id);
        if (it != IBR_Inst_Project.IBR_SectionMap.end()) {
            // 阶段 4 修复 D5：方向应为 +=（与 endMoveSection 一致）
            // 鼠标向右拖 dragOffset.x>0 → EqPos 增大 → 节点屏幕坐标右移
            it->second.EqPos.x += eqDeltaX;
            it->second.EqPos.y += eqDeltaY;
            it->second.Dragging = false;
        }
    }
    // 性能优化：在 clear 前保存 IDs，供后面 emit sectionPositionChanged
    auto draggedIds = m_massDragIds;
    // 同步 MiniMap：所有拖拽节点快照 eqX/eqY 写回最新 EqPos，emit sectionsChanged 触发迷你地图重绘
    for (auto id : draggedIds) {
        syncSectionSnapshotPos(id);
    }
    emit sectionsChanged();
    m_massDragIds.clear();
    m_initHolding = false;
    m_hasLeftDownToWait = false;
    // 修复：清除单节点拖拽的残留状态
    // endMassDrag 可能在单节点拖拽中被调用（onMousePress 取消拖拽），
    // 此时 m_massDragIds 为空，但 m_dragSectionId 非零，Dragging 未被清除
    // 导致该节点永远显示 isDragging 蓝框（#4fc3f7, width=1 较细）
    if (m_dragSectionId != INVALID_MODULE_ID) {
        auto dragIt = IBR_Inst_Project.IBR_SectionMap.find(m_dragSectionId);
        if (dragIt != IBR_Inst_Project.IBR_SectionMap.end()) {
            dragIt->second.Dragging = false;
        }
        m_dragSectionId = INVALID_MODULE_ID;
        m_draggingSectionId = INVALID_MODULE_ID;
        emit draggingSectionIdChanged();
    }
    // 静默置 massDragging=false（isDragging → false 触发 LineRow 回写新 LastCenter），
    // 再 emit sectionPositionChanged（各节点 localEqX=终点）。dragOffset 保持旧值，
    // 等端点表重建完成后再清（同 endMoveSection：避免 LinkRenderer 用旧端点表+0 弹回起点）。
    // 修复（同 endMoveSection line 1134）：suppress 必须前置到下面两个 emit 之前。
    // emit massDraggingChanged 会同步触发 SectionNode.onIsDraggingChanged → updateAllCenters
    // → LineRow.updateLinkNodeCenter → linkNodeCenterChanged；若此时 suppress 仍为 false，
    // 该处理器会排队 Queued 重建 R1（R1 lambda 不检查 suppress），R1 在 dragOffset 未清零时
    // 用 LineRow 回写的终点坐标重建端点表，而 buildSectionMap 仍按"lastDraggedId + dragOffset
    // 非零"叠加 dragOffset → 连线终点 = 终点 + dragOffset，比节点多偏移一个 dragOffset → 一帧偏移。
    // suppress 前置后，回写仅标记 dirty 不排队 R1，端点表保持旧值 + dragOffset 叠加 = 终点
    //（与节点一致），收尾重建统一切换，无中间态。
    m_suppressLinkRebuild = true;
    m_massDragging = false;
    // 拖拽结束：终止缩放换算（补间已在函数开头强制完成；防止残留 zoomPending 拦截回写）
    m_zoomPending = false;
    if (!draggedIds.empty())
        m_lastDraggedId = static_cast<qulonglong>(draggedIds.front());  // LinkRenderer 据此在端点表重建前继续叠加 dragOffset
    // 多选拖拽：记录所有被拖节点，供 buildSectionMap 过渡期对所有被拖节点叠加 dragOffset
    //（单值 m_lastDraggedId 只覆盖 front，非 front 节点端点会偏移一个 dragOffset）
    m_lastMassDragIds.clear();
    for (auto id : draggedIds) {
        m_lastMassDragIds.insert(static_cast<qulonglong>(id), true);
    }
    for (auto id : draggedIds) {
        emit sectionPositionChanged(static_cast<qulonglong>(id));
    }
    emit massDraggingChanged();  // isDragging → false（LineRow 回写新 LastCenter）
    // 性能优化：与 endMoveSection 行为一致
    // - 保持 MassTarget 选中状态（不清除），允许用户连续拖动同一组模块
    // - 回到 MassAfter 状态（而非 Normal），与 endMoveSection 统一
    // - 不调全量 refresh()，改为对每个拖拽节点 emit sectionPositionChanged
    //   避免重建 91 个节点的 QVariantList + SectionLineModel::refresh() 导致卡顿
    restoreStateAfterDrag();
    // 连线端点表需要重建（多节点位置变了）
    m_linkEndpointsDirty = true;
    // suppress 已在 emit massDraggingChanged 前置位（见上方），此处不再重复设置
    auto finalIds = draggedIds;
    QMetaObject::invokeMethod(this, [this, finalIds]() {
        for (auto id : finalIds) {
            emit sectionPositionChanged(static_cast<qulonglong>(id));
        }
        QMetaObject::invokeMethod(this, [this]() {
            m_suppressLinkRebuild = false;       // 恢复 tick 重建
            if (m_dragSectionId == INVALID_MODULE_ID && m_inputState != 3) {
                m_dragOffset = QPointF();
                emit dragOffsetChanged();
                rebuildLinkEndpoints();
                // 过渡期结束：清 lastDraggedId（同 endMoveSection，防旧模块连线被下次拖拽拖走）
                m_lastDraggedId = INVALID_MODULE_ID;
                m_lastMassDragIds.clear();  // 多选过渡期集合一并清空
            }
        }, Qt::QueuedConnection);
    }, Qt::QueuedConnection);
}

void WorkspaceController::showMassContextMenu(qreal x, qreal y)
{
    // MassAfter 状态下右键释放 → 弹出多选右键菜单
    // 对应 IBR_WorkSpace.cpp:1170-1290（含 Copy/Cut/Paste/Ignore/Freeze/Hide/Compose/Export/Duplicate/Delete）
    // 复用现有的 contextMenuRequested 信号，QML 侧根据 inputState==4 显示不同菜单
    emit contextMenuRequested(x, y);
}

QVariantList WorkspaceController::massTargetIds() const
{
    QVariantList list;
    for (auto id : IBR_WorkSpace::MassTarget) {
        list.append(static_cast<qulonglong>(id));
    }
    return list;
}

bool WorkspaceController::isSectionSelected(qulonglong sectionId) const
{
    ModuleID_t id = static_cast<ModuleID_t>(sectionId);
    // MassSelecting 状态下查询实时预览集合（对应 ImGui IsWindowMassSelected 检查 MassSelectWindows）
    // 预览为空时（endSelection 已转交 MassTarget）fallback 到 MassTarget
    if (m_inputState == 2 && !m_massSelectPreview.empty()) {
        return m_massSelectPreview.count(id) != 0;
    }
    return std::find(IBR_WorkSpace::MassTarget.begin(),
                     IBR_WorkSpace::MassTarget.end(), id) != IBR_WorkSpace::MassTarget.end();
}

bool WorkspaceController::isLastMassDragged(qulonglong sectionId) const
{
    return m_lastMassDragIds.contains(sectionId);
}

// 判断 sectionId 是否位于 groupId（虚拟块）的编组内（含 groupId 自身，可跨任意层级嵌套）。
// 拖动编组块时整组一起移动，QML LinkRenderer 据此对子模块连线的 pa/pb 叠加同一 dragOffset。
bool WorkspaceController::isInComposedOf(qulonglong groupId, qulonglong sectionId) const
{
    ModuleID_t root = static_cast<ModuleID_t>(groupId);
    ModuleID_t target = static_cast<ModuleID_t>(sectionId);
    // BFS 遍历 root 的所有层级 IncludngModules；命中 target 即返回 true
    std::vector<ModuleID_t> queue{ root };
    std::unordered_set<ModuleID_t> visited{ root };
    while (!queue.empty()) {
        auto cur = queue.back();
        queue.pop_back();
        if (cur == target) return true;
        auto it = IBR_Inst_Project.IBR_SectionMap.find(cur);
        if (it == IBR_Inst_Project.IBR_SectionMap.end()) continue;
        for (auto sub : it->second.IncludingModules) {
            if (visited.insert(sub).second) queue.push_back(sub);
        }
    }
    return false;
}

bool WorkspaceController::selectedAllIgnored() const
{
    return IBR_WorkSpace::SelectedAllIgnored();
}

bool WorkspaceController::selectedAllFrozen() const
{
    return IBR_WorkSpace::SelectedAllFrozen();
}

bool WorkspaceController::selectedAllHidden() const
{
    return IBR_WorkSpace::SelectedAllHidden();
}

void WorkspaceController::reportSectionSize(qulonglong sectionId, qreal screenW, qreal screenH)
{
    // QML SectionNode 渲染后回报实际屏幕尺寸，反算 EqSize 并同步到 IBR_SectionMap
    // 对应 ImGui IBR_WorkSpace.cpp:1801-1802 sd.EqSize = NP（实时更新）
    // Qt 版本 SectionNode 尺寸由内容驱动，必须同步否则框选命中检测不准
    ModuleID_t id = static_cast<ModuleID_t>(sectionId);
    auto it = IBR_Inst_Project.IBR_SectionMap.find(id);
    if (it == IBR_Inst_Project.IBR_SectionMap.end()) return;
    if (IBR_FullView::Ratio == 0.0f) return;
    ImVec2 newEqSize(static_cast<float>(screenW / IBR_FullView::Ratio),
                     static_cast<float>(screenH / IBR_FullView::Ratio));
    // 仅在尺寸变化时更新（避免每帧刷新）
    if (std::abs(it->second.EqSize.x - newEqSize.x) > 0.5f ||
        std::abs(it->second.EqSize.y - newEqSize.y) > 0.5f) {
#ifdef INIWEAVER_DIAG
        qDebug() << "[ONSHOW-DIAG] reportSectionSize sectionId=" << sectionId
                 << "oldEqSize=" << it->second.EqSize.x << it->second.EqSize.y
                 << "newEqSize=" << newEqSize.x << newEqSize.y;
#endif
        it->second.EqSize = newEqSize;
        // 同步更新 m_sections 快照（MiniMap 依赖 eqW/eqH 绘制模块矩形）。
        // 新模块添加后 refreshSections 构建 item 时 EqSize 可能还是 0（AddModule 异步
        // 初始化延迟），QML SectionNode 渲染后 reportSectionSize 回写真实尺寸，
        // 若不更新快照，MiniMap 一直显示宽度 0 的模块（不可见）。
        bool foundInSnapshot = false;
        for (auto &item : m_sections) {
            if (item.toMap().value(QStringLiteral("sectionId")).toULongLong() == sectionId) {
                QVariantMap m = item.toMap();
                m[QStringLiteral("eqW")] = static_cast<double>(newEqSize.x);
                m[QStringLiteral("eqH")] = static_cast<double>(newEqSize.y);
                item = m;
                foundInSnapshot = true;
                break;
            }
        }
#ifdef INIWEAVER_DIAG
        qDebug() << "[REFRESH-DIAG] reportSectionSize emit sectionsChanged sid=" << sectionId
                 << "m_sections.size=" << m_sections.size()
                 << "foundInSnapshot=" << foundInSnapshot
                 << "newEqSize=" << newEqSize.x << newEqSize.y;
#endif
        emit sectionsChanged();  // 更新 MiniMap 的 sections 属性并重绘
        // 节点尺寸变化（OnShow 切换导致高度变/隐藏宽行导致宽度变）后端点表需重建。
        // 关键时序：onHeightChanged 在 QML 父节点高度变化时同步触发，此时子项 LineRow 的
        // onYChanged（行往上移）尚未回写新 acceptCenter——QML Column/ColumnLayout 的子项重布局
        // 是异步 polish event。若此处同步/QueuedConnection rebuild，rebuild 会排在 polish 之前
        // （posted event 按 post 顺序处理），读到旧 LastCenter → 端点偏一帧（偶发：QML 布局
        // 时序非确定，子项 y 重布局有时同步有时异步 polish）。
        // 改为只标脏：由 LineRow.onYChanged 回写后 emit linkNodeCenterChanged 触发的延迟 rebuild
        // 来重建（该 rebuild 排在 polish event 之后，读到所有行新 LastCenter）。无行 y 变化时
        // 由 refreshFromTimer(50ms) 兜底重建。拖拽/缩放/平移/松手过渡中同样只标脏，由收尾路径重建。
#ifdef INIWEAVER_DIAG
        qDebug() << "[ONSHOW-DIAG] reportSectionSize MARK dirty sectionId=" << sectionId
                 << "suppress=" << m_suppressLinkRebuild << "state=" << m_inputState
                 << "dragSec=" << m_dragSectionId << "massDrag=" << m_massDragging
                 << "zoomPending=" << m_zoomPending;
#endif
        m_linkEndpointsDirty = true;
    }
}

void WorkspaceController::massAfterDelete()
{
    // 对应 IBR_WorkSpace.cpp:1131-1136
    if (m_inputState != 4) return;
    deleteSelected();
    updateInputState(0);
}

void WorkspaceController::massAfterCopy()
{
    // 对应 IBR_WorkSpace.cpp:1137-1140（不退出 MassAfter）
    if (m_inputState != 4) return;
    copySelected();
}

void WorkspaceController::massAfterCut()
{
    // 对应 IBR_WorkSpace.cpp:1145-1148
    if (m_inputState != 4) return;
    cutSelected();
    updateInputState(0);
}

void WorkspaceController::massAfterDuplicate()
{
    // 对应 IBR_WorkSpace.cpp:1141-1144（重置 MassAfter）
    if (m_inputState != 4) return;
    duplicateSelected();
    // DuplicateSelected 内部已调用 MassSelect，保持 MassAfter 状态
    refresh();
}

// ===== 阶段 12.3：虚拟块相关实现 =====

void WorkspaceController::toggleCollapseInComposed(qulonglong sectionId, bool collapsed)
{
    // 对应 IBR_SectionData.cpp:862 CollapsedInComposed = false / :944 CollapsedInComposed = true
    // 同步修改（见 toggleIgnore 注释）：SendToR 延迟到下一 tick 执行，而 refresh() 立即重建会读到旧值，
    // 且下一 tick 因状态哈希早退不再重建 → 展开/收起永不生效（编组子模块无法展开、锚点不切换）。
    ModuleID_t id = static_cast<ModuleID_t>(sectionId);
    auto it = IBR_Inst_Project.IBR_SectionMap.find(id);
    if (it != IBR_Inst_Project.IBR_SectionMap.end()) {
        it->second.CollapsedInComposed = collapsed;
#ifdef INIWEAVER_DIAG
        qDebug() << "[FOLD-DIAG] toggleCollapseInComposed id=" << sectionId
                 << "setCollapsed=" << collapsed
                 << "valueAfter=" << it->second.CollapsedInComposed;
#endif
    } else {
#ifdef INIWEAVER_DIAG
        qDebug() << "[FOLD-DIAG] toggleCollapseInComposed id=" << sectionId
                 << "NOT FOUND in IBR_SectionMap!";
#endif
    }
    refresh();
}

void WorkspaceController::foldComposed(qulonglong virtualBlockId)
{
    // 对应 IBR_SectionData.cpp:355-361 FoldComposed（同步修改，见 toggleCollapseInComposed 注释）
    ModuleID_t id = static_cast<ModuleID_t>(virtualBlockId);
    auto it = IBR_Inst_Project.IBR_SectionMap.find(id);
    if (it == IBR_Inst_Project.IBR_SectionMap.end()) return;
    auto &sd = it->second;
    for (auto subId : sd.IncludingModules) {
        auto subIt = IBR_Inst_Project.IBR_SectionMap.find(subId);
        if (subIt != IBR_Inst_Project.IBR_SectionMap.end()) {
            subIt->second.CollapsedInComposed = true;
#ifdef INIWEAVER_DIAG
            qDebug() << "[FOLD-DIAG] foldComposed subId=" << static_cast<qulonglong>(subId);
#endif
        }
    }
    refresh();
}

void WorkspaceController::unfoldComposed(qulonglong virtualBlockId)
{
    // 对应 IBR_SectionData.cpp:363-369 UnfoldComposed（同步修改，见 toggleCollapseInComposed 注释）
    ModuleID_t id = static_cast<ModuleID_t>(virtualBlockId);
    auto it = IBR_Inst_Project.IBR_SectionMap.find(id);
    if (it == IBR_Inst_Project.IBR_SectionMap.end()) return;
    auto &sd = it->second;
    for (auto subId : sd.IncludingModules) {
        auto subIt = IBR_Inst_Project.IBR_SectionMap.find(subId);
        if (subIt != IBR_Inst_Project.IBR_SectionMap.end()) {
            subIt->second.CollapsedInComposed = false;
#ifdef INIWEAVER_DIAG
            qDebug() << "[FOLD-DIAG] unfoldComposed subId=" << static_cast<qulonglong>(subId);
#endif
        }
    }
    refresh();
}

void WorkspaceController::decomposeSection(qulonglong virtualBlockId)
{
    // 对应 IBR_SectionData.cpp:322-339 Decompose
    ModuleID_t id = static_cast<ModuleID_t>(virtualBlockId);
    // 同步执行（见 toggleIgnore/toggleCollapseInComposed 注释）：SendToR 延迟到下一 tick，
    // 而 refresh() 立即重建会读到旧 IncludingModules/IncludedByModule → 虚拟块已清空 IncludedModules、
    // 子模块 IsIncluded 未刷新 → 子模块不再被父块 Include 也不独立渲染，模块消失在画布。
    // DecomposeSection 同步改 EqPos(IncludedByModule=INVALID + 父EqPos+EqDelta) 与 IncludingModules，
    // 随之 refresh() 读到正确的新状态，子模块作为独立 section 重新渲染。
    auto it = IBR_Inst_Project.IBR_SectionMap.find(id);
    if (it == IBR_Inst_Project.IBR_SectionMap.end()) return;
    auto &sd = it->second;
    if (!sd.Decomposable()) return;
    auto res = IBR_Inst_Project.DecomposeSection(id);
    if (res) {
        IBR_WorkSpace::MassSelect(*res);
    }
    refresh();
}

void WorkspaceController::showAllIncludingBlocks(qulonglong virtualBlockId)
{
    // 对应 IBR_SectionData.cpp:925-934
    ModuleID_t id = static_cast<ModuleID_t>(virtualBlockId);
    IBRF_CoreBump.SendToR({ [id]() {
        auto it = IBR_Inst_Project.IBR_SectionMap.find(id);
        if (it == IBR_Inst_Project.IBR_SectionMap.end()) return;
        auto &sd = it->second;
        for (auto subId : sd.IncludingModules) {
            auto subIt = IBR_Inst_Project.IBR_SectionMap.find(subId);
            if (subIt != IBR_Inst_Project.IBR_SectionMap.end()) {
                subIt->second.Hidden = false;
            }
        }
    } });
    refresh();
}

// ===== 阶段 12.4：节点 Acceptor DropTarget 合并语义实现 =====

bool WorkspaceController::mergeSectionToSection(qulonglong sourceId, qulonglong destId)
{
    // 对应 IBR_SectionData.cpp:505-510 RenderUI_Acceptor 的 IBR_SecDrag Delivery
    ModuleID_t srcId = static_cast<ModuleID_t>(sourceId);
    ModuleID_t dstId = static_cast<ModuleID_t>(destId);
    auto srcSec = IBR_Inst_Project.GetSectionFromID(srcId);
    auto dstSec = IBR_Inst_Project.GetSectionFromID(dstId);
    auto srcBack = srcSec.GetBack_Unsafe();
    auto dstBack = dstSec.GetBack_Unsafe();
    if (!srcBack || !dstBack) return false;

    // 获取默认链接 key（对应 srcback->GetDLK(back->Register)）
    StrPoolID dlk = srcBack->GetDLK(dstBack->Register);
    if (dlk == EmptyPoolStr) return false;  // 无默认链接 key

    // 执行合并（对应 back->MergeLine(DLK, Index_AlwaysNew, desc.Sec, IBB_IniMergeMode::Merge)）
    auto srcDesc = srcSec.GetSectionData();
    if (!srcDesc) return false;
    std::string secName = srcDesc->Desc.Sec;
    IBRF_CoreBump.SendToR({ [dstId, dlk, secName]() {
        auto dstSec2 = IBR_Inst_Project.GetSectionFromID(dstId);
        auto dstBack2 = dstSec2.GetBack_Unsafe();
        if (dstBack2) {
            IBG_Undo.SomethingShouldBeHere();
            dstBack2->MergeLine(dlk, Index_AlwaysNew, secName, IBB_IniMergeMode::Merge);
            dstBack2->SetOnShow(dlk);
        }
    } });
    refresh();
    return true;
}

bool WorkspaceController::mergeLinkToSection(qulonglong sourceId, qulonglong destId, const QString &linkKey)
{
    // 对应 IBR_SectionData.cpp:544-549 IBR_LineDrag Delivery
    // 实际合并在源 LinkNode 端通过 Session 异步执行
    ModuleID_t srcId = static_cast<ModuleID_t>(sourceId);
    ModuleID_t dstId = static_cast<ModuleID_t>(destId);
    auto dstSec = IBR_Inst_Project.GetSectionFromID(dstId);
    auto dstData = dstSec.GetSectionData();
    if (!dstData) return false;

    // 类型校验（对应 Acceptor_CheckLinkType）
    auto srcSec = IBR_Inst_Project.GetSectionFromID(srcId);
    auto srcBack = srcSec.GetBack_Unsafe();
    auto dstBack = dstSec.GetBack_Unsafe();
    if (!srcBack || !dstBack) return false;

    // linkKey 为空时使用源 section 的默认链接 key
    StrPoolID typeAlt{0};
    if (!linkKey.isEmpty() && linkKey != QLatin1String("default")) {
        typeAlt = NewPoolStr(linkKey.toUtf8().constData());
    }

    bool check = Acceptor_CheckLinkType(srcBack->Register, dstBack->Register, typeAlt);
    if (!check) return false;

    // 设置 ValueToMerge（对应 lin.pSession->ValueToMerge = Desc.Sec）
    // 注意：Qt 侧没有 LinkNode Session 机制，这里直接调用 MergeLine
    std::string secName = dstData->Desc.Sec;
    StrPoolID dlk = srcBack->GetDLK(dstBack->Register);
    if (dlk == EmptyPoolStr) return false;

    IBRF_CoreBump.SendToR({ [srcId, dlk, secName]() {
        auto srcSec2 = IBR_Inst_Project.GetSectionFromID(srcId);
        auto srcBack2 = srcSec2.GetBack_Unsafe();
        if (srcBack2) {
            IBG_Undo.SomethingShouldBeHere();
            srcBack2->MergeLine(dlk, Index_AlwaysNew, secName, IBB_IniMergeMode::Merge);
            srcBack2->SetOnShow(dlk);
        }
    } });
    refresh();
    return true;
}

bool WorkspaceController::createLinkFromDrag(qulonglong sourceId, const QString &sourceKeyName, int sourceLineMult,
                                             qulonglong destSectionId, const QString &destKey, int destMult)
{
    // 对应 ImGui lin.pSession->NotifyValueToMerge 路径（IBR_LinkNode.cpp:596-631）
    // 在源行上走 ModifyAndShow：收集现有链接 + 追加新目标 + 遵守 LinkLimit
    // 与 SectionLineModel::createLink 逻辑一致，但通过 sourceKeyName 在 C++ 侧定位源行
    ModuleID_t srcId = static_cast<ModuleID_t>(sourceId);
    ModuleID_t dstId = static_cast<ModuleID_t>(destSectionId);
    auto srcSec = IBR_Inst_Project.GetSectionFromID(srcId);
    auto dstSec = IBR_Inst_Project.GetSectionFromID(dstId);
    auto srcBack = srcSec.GetBack_Unsafe();
    auto dstBack = dstSec.GetBack_Unsafe();
    if (!srcBack || !dstBack) return false;

    auto dstData = dstSec.GetSectionData();
    if (!dstData) return false;

    StrPoolID srcKeyId = NewPoolStr(sourceKeyName.toUtf8().constData());
    auto *line = srcBack->GetLineFromSubSecs(srcKeyId);
    if (!line || !line->Default) return false;

    // 类型校验（对应 Acceptor_CheckLinkType）
    StrPoolID linkType = line->Default->LinkNode.LinkType;
    bool check = Acceptor_CheckLinkType(srcBack->Register, dstBack->Register, linkType);
    if (!check) return false;

    // 目标 key：若未指定，用目标的 DLK（DefaultLinkKey）
    StrPoolID finalDstKey = 0;
    if (!destKey.isEmpty() && destKey != QLatin1String("default")) {
        finalDstKey = NewPoolStr(destKey.toUtf8().constData());
    }
    if (finalDstKey == 0) {
        finalDstKey = dstBack->GetDLK(dstBack->Register);
        if (finalDstKey == 0) finalDstKey = NewPoolStr("default");
    }

    size_t srcLineMult = static_cast<size_t>(sourceLineMult);
    size_t dstMult = static_cast<size_t>(destMult);

    IBRF_CoreBump.SendToR({ [srcId, dstId, srcKeyId, srcLineMult, finalDstKey, dstMult, dstData, sourceKeyName, this]() {
        auto srcRsec = IBR_Inst_Project.GetSectionFromID(srcId);
        auto dstRsec = IBR_Inst_Project.GetSectionFromID(dstId);
        auto srcBsec = srcRsec.GetBack_Unsafe();
        auto dstBsec = dstRsec.GetBack_Unsafe();
        if (!srcBsec || !dstBsec) return;

        auto *ln = srcBsec->GetLineFromSubSecs(srcKeyId);
        if (!ln || !ln->Default) return;

        // 构建 newTarget（对应 TargetValueStr(dstData->Desc.Sec, finalDstKey, dstMult)）
        std::string newTarget = TargetValueStr(dstData->Desc.Sec, finalDstKey, dstMult);

        // 找到包含该 key 的 SubSec 及其 lineIdx（对应 Lines_ByName 中的位置索引）
        IBB_SubSec *foundSub = nullptr;
        size_t srcLineIdx = 0;
        for (auto subIdx : srcBsec->SubSecOrder) {
            auto &sub = srcBsec->SubSecs[subIdx];
            if (!sub.CanOwnKey(srcKeyId)) continue;
            foundSub = &sub;
            for (size_t i = 0; i < sub.Lines_ByName.size(); ++i) {
                if (sub.Lines_ByName[i] == srcKeyId) {
                    srcLineIdx = i;
                    break;
                }
            }
            break;
        }

        // 收集现有链接的 TargetValue 列表
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
        int limit = ln->Default->GetLinkLimit();
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

        ln->Merge(srcLineMult, str, IBB_IniMergeMode::Replace);
        srcBsec->UpdateAll();
        IBR_Inst_Project.RefreshLinkList = true;
        // 增量刷新（按后端）：Merge/UpdateAll 已更新后端键值，此处从后端重建 LinkList/m_links，
        // 只更新本条链接端点。不触发全量 refresh()（行模型 resetModel 重建期间坐标未就绪，
        // 全量 rebuildLinkEndpoints 会读到污染值导致连线全跑模块第一个节点）。
        refreshLinkIncremental();
        refreshLinkEndpoint(srcId, sourceKeyName, static_cast<int>(srcLineMult), dstId);
        auto it = m_lineModels.find(srcId);
        if (it != m_lineModels.end() && *it) {
            (*it)->refresh();
            emit (*it)->sectionDataChanged(static_cast<qulonglong>(srcId));
        }
    } });

    return true;
}

void WorkspaceController::refreshLinkEndpoint(qulonglong srcId, const QString &fromKey, int mult,
                                              qulonglong dstId)
{
    // 增量刷新单条链接端点：改变连线状态后只更新该链接的 pa/pb。
    // 不触发全量 rebuildLinkEndpoints（行模型 resetModel 重建期间坐标未就绪，
    // 全量重建会读到污染坐标导致连线全跑模块第一个节点）。
    // 复用 rebuildLinkEndpoints 的 pa/pb 优先级逻辑，仅更新匹配链接的条目。
    constexpr float FontHeight = 13.0f;
    const float ratio = IBR_FullView::Ratio;
    const float fontHeightScaled = FontHeight * ratio;
    const float halfLine = fontHeightScaled * 0.5f;

    bool updated = false;
    for (auto &link : IBR_Inst_Project.LinkList)
    {
        if (link.SrcModuleID != static_cast<ModuleID_t>(srcId)) continue;
        // 修复：用字符串比较（NewPoolStr 重新生成的 ID 可能与 link.FromKey 池 ID 不一致导致匹配失败）
        if (!fromKey.isEmpty() && QString::fromUtf8(PoolStr(link.FromKey)) != fromKey) continue;
        if (static_cast<int>(link.SrcMult) != mult) continue;
        // 目标端安全查找：不能用 GetSection(link.Dest)（反向索引缺失时创建滚木占位，见 rebuildLinkEndpoints）
        auto dstIdOpt = IBR_Inst_Project.GetSectionID(link.Dest.ToDesc());
        if (!dstIdOpt) continue;
        if (*dstIdOpt != static_cast<ModuleID_t>(dstId)) continue;
        auto dstRsec = IBR_Inst_Project.GetSectionFromID(*dstIdOpt);

        const auto &sv = IBR_NodeSession::GetSessionValue(link.SourceID);

        // ===== 起点 pa =====
        qreal paX = 0.0, paY = 0.0;
        bool paValid = false;
        auto srcRsec = IBR_Inst_Project.GetSectionFromID(link.SrcModuleID);
        auto srcBsec = srcRsec.GetBack_Unsafe();
        bool srcLineVisible = (link.FromKey == EmptyPoolStr) || !srcBsec
                              || srcBsec->IsOnShow(link.FromKey);
        // 源模块自身折叠（编组内收起 / UICollapsed）：行坐标残留，pa 收敛到模块「最右端」
        //（同 rebuildLinkEndpoints，对齐 ImGui HeadLineRN 右端锚点）
        auto srcDataFull = srcRsec.GetSectionData();
        bool srcCollapsed = srcDataFull && (srcDataFull->CollapsedInComposed || srcDataFull->UICollapsed);
        if (srcCollapsed && srcDataFull)
        {
            // 折叠子模块堆叠在父虚拟块内（同 x 列、不同 y），实际位置用 QML 回写的
            // m_sectionAcceptPoint，不能用各自全局 EqPos。pa 源端「最右端」= 模块右端，
            // 需扣除 acceptPt 相对模块左边缘的偏移（普通块左margin8+圆心距5=13；import 居中=半宽）。
            ImVec2 rePosC = IBR_WorkSpace::EqPosToRePos(srcDataFull->EqPos);
            auto itA = m_sectionAcceptPoint.find(srcId);
            float baseX = (itA != m_sectionAcceptPoint.end() && !itA->isNull())
                ? static_cast<float>(itA->x()) : rePosC.x;
            bool srcIsImport = srcBsec && srcBsec->Dynamic.ImportCount > 0;
            float radioLeftOffset = srcIsImport
                ? (srcDataFull->EqSize.x * ratio * 0.5f)
                : (13.0f * ratio);
            paX = static_cast<qreal>(baseX - radioLeftOffset + srcDataFull->EqSize.x * ratio);
            paY = (itA != m_sectionAcceptPoint.end() && !itA->isNull())
                ? itA->y() : static_cast<qreal>(rePosC.y + halfLine);
            paValid = true;
        }
        if (srcLineVisible && !srcCollapsed && link.FromKey != EmptyPoolStr)
        {
            auto itSrcModel = m_lineModels.find(srcId);
            if (itSrcModel != m_lineModels.end() && *itSrcModel)
            {
                QPointF srcAc = (*itSrcModel)->acceptCenterByKey(
                    QString::fromUtf8(PoolStr(link.FromKey)), static_cast<int>(link.SrcMult));
                if (!srcAc.isNull()) { paX = srcAc.x(); paY = srcAc.y(); paValid = true; }
            }
        }
        if (!paValid && (srcCollapsed || srcLineVisible) && (sv.LastCenter.x != 0.0f || sv.LastCenter.y != 0.0f))
        {
            paX = static_cast<qreal>(sv.LastCenter.x);
            paY = static_cast<qreal>(sv.LastCenter.y);
            paValid = true;
        }
        // 隐藏行专用：源行隐藏（非折叠态）时连线起点落到标题栏最右端（同 rebuildLinkEndpoints）
        // y 与头节点水平对齐：优先 m_sectionAcceptPoint.y()，兜底 halfLine
        if (!paValid && !srcCollapsed && !srcLineVisible && !sv.Collapsed)
        {
            auto srcDataH = srcRsec.GetSectionData();
            if (srcDataH)
            {
                ImVec2 rePos = IBR_WorkSpace::EqPosToRePos(srcDataH->EqPos);
                paX = static_cast<qreal>(rePos.x + srcDataH->EqSize.x * ratio);
                auto itH = m_sectionAcceptPoint.find(srcId);
                paY = (itH != m_sectionAcceptPoint.end() && !itH->isNull())
                    ? itH->y()
                    : static_cast<qreal>(rePos.y + halfLine);
                paValid = true;
            }
        }
        if (!paValid)
        {
            auto itSrcAccept = m_sectionAcceptPoint.find(srcId);
            if (itSrcAccept != m_sectionAcceptPoint.end() && !itSrcAccept->isNull())
            {
                paX = itSrcAccept->x(); paY = itSrcAccept->y(); paValid = true;
            }
        }
        if (!paValid)
        {
            auto srcData = IBR_Inst_Project.GetSectionFromID(link.SrcModuleID).GetSectionData();
            if (srcData)
            {
                ImVec2 rePos = IBR_WorkSpace::EqPosToRePos(srcData->EqPos);
                paX = static_cast<qreal>(rePos.x + fontHeightScaled * 0.7f);
                paY = static_cast<qreal>(rePos.y + halfLine);
                paValid = true;
            }
        }

        // ===== 终点 pb =====
        qreal pbX = 0.0, pbY = 0.0;
        bool pbValid = false;
        auto dstData = dstRsec.GetSectionData();
        auto dstBsec = dstRsec.GetBack_Unsafe();
        bool dstLineVisible = (link.DestKey == EmptyPoolStr) || !dstBsec
                              || dstBsec->IsOnShow(link.DestKey);
        // 目标模块自身折叠（编组收起 / UICollapsed）：目标端是「块端」，连线应收敛到
        // 目标模块标题栏 RadioButton（左端，ReWindowUL+ReOffset），同 rebuildLinkEndpoints。
        bool dstCollapsed = dstData && (dstData->CollapsedInComposed || dstData->UICollapsed);
        if (dstCollapsed && dstData)
        {
            // 目标端「块端」落在折叠子模块标题栏 RadioButton（左端）。折叠子模块堆叠在父
            // 虚拟块内，实际位置用 QML 回写 m_sectionAcceptPoint，不能用各自全局 EqPos。
            ImVec2 rePosD = IBR_WorkSpace::EqPosToRePos(dstData->EqPos);
            auto itD = m_sectionAcceptPoint.find(static_cast<qulonglong>(dstRsec.ID));
            if (itD != m_sectionAcceptPoint.end() && !itD->isNull())
            {
                pbX = itD->x();
                pbY = itD->y();
            }
            else
            {
                float reOffsetX = (dstBsec && dstBsec->Dynamic.ImportCount > 0)
                    ? (dstData->EqSize.x * ratio * 0.5f - fontHeightScaled * 0.5f)
                    : (fontHeightScaled * 0.7f);
                pbX = static_cast<qreal>(rePosD.x + reOffsetX);
                pbY = static_cast<qreal>(rePosD.y + halfLine);
            }
            pbValid = true;
        }
        if (!sv.Collapsed && !dstCollapsed && dstLineVisible && link.DestKey != EmptyPoolStr && dstData)
        {
            auto itModel = m_lineModels.find(static_cast<qulonglong>(dstRsec.ID));
            if (itModel != m_lineModels.end() && *itModel)
            {
                QPointF ac = (*itModel)->acceptCenterByKey(
                    QString::fromUtf8(PoolStr(link.DestKey)), static_cast<int>(link.LineMult));
                if (!ac.isNull()) { pbX = ac.x(); pbY = ac.y(); pbValid = true; }
            }
        }
        // 隐藏行专用：目标行隐藏（非折叠态）时连线终点落到标题栏最右端（同 rebuildLinkEndpoints）
        // y 与头节点水平对齐：优先 m_sectionAcceptPoint.y()，兜底 halfLine
        if (!pbValid && !dstLineVisible && !sv.Collapsed && !dstCollapsed)
        {
            if (dstData)
            {
                ImVec2 rePos = IBR_WorkSpace::EqPosToRePos(dstData->EqPos);
                pbX = static_cast<qreal>(rePos.x + dstData->EqSize.x * ratio);
                auto itH = m_sectionAcceptPoint.find(static_cast<qulonglong>(dstRsec.ID));
                pbY = (itH != m_sectionAcceptPoint.end() && !itH->isNull())
                    ? itH->y()
                    : static_cast<qreal>(rePos.y + halfLine);
                pbValid = true;
            }
        }
        if (!pbValid)
        {
            auto itAccept = m_sectionAcceptPoint.find(static_cast<qulonglong>(dstRsec.ID));
            if (itAccept != m_sectionAcceptPoint.end() && !itAccept->isNull())
            {
                pbX = itAccept->x(); pbY = itAccept->y(); pbValid = true;
            }
        }
        if (!pbValid && dstData)
        {
            ImVec2 rePos = IBR_WorkSpace::EqPosToRePos(dstData->EqPos);
            pbX = static_cast<qreal>(rePos.x + fontHeightScaled * 0.7f);
            pbY = static_cast<qreal>(rePos.y + halfLine);
            pbValid = true;
        }

        // ===== 更新端点表 =====
        QString mapKey = QString::number(static_cast<qulonglong>(link.SourceID))
                         + QLatin1String(":")
                         + QString::number(static_cast<qulonglong>(dstRsec.ID));
        QVariantMap ep;
        const bool wasPresent = m_linkEndpointsMap.contains(mapKey);
        if (wasPresent)
            ep = m_linkEndpointsMap.value(mapKey).toMap();
        ep["sessionId"] = QString::number(static_cast<qulonglong>(link.SourceID));
        ep["destId"] = QString::number(static_cast<qulonglong>(dstRsec.ID));
        ep["x"] = paX; ep["y"] = paY;
        ep["pbX"] = pbX; ep["pbY"] = pbY; ep["pbValid"] = pbValid;
        ep["isCollapsed"] = sv.Collapsed;
        // 修复：map 与 list 必须同步更新（新链接 append 到 list 时也必须 insert map，
        // 否则 LinkRenderer 查 map 找不到该条导致连线渲染错位/丢失）
        m_linkEndpointsMap.insert(mapKey, ep);
        if (!wasPresent)
            m_linkEndpoints.append(ep);
        for (int i = 0; i < m_linkEndpoints.size(); ++i)
        {
            QVariantMap e2 = m_linkEndpoints[i].toMap();
            if (e2.value("sessionId").toString() == ep["sessionId"].toString()
                && e2.value("destId").toString() == ep["destId"].toString())
            {
                e2["x"] = paX; e2["y"] = paY;
                e2["pbX"] = pbX; e2["pbY"] = pbY; e2["pbValid"] = pbValid;
                m_linkEndpoints[i] = e2;
                break;
            }
        }
        updated = true;
    }

    if (updated)
    {
        emit linkEndpointsChanged();
    }
}

int WorkspaceController::checkMergePreview(qulonglong sourceId, qulonglong destId,
                                            const QString &linkKey, bool isLinkDrag)
{
    // 对应 DrawDragPreviewIcon 的三种状态
    ModuleID_t srcId = static_cast<ModuleID_t>(sourceId);
    ModuleID_t dstId = static_cast<ModuleID_t>(destId);
    auto srcSec = IBR_Inst_Project.GetSectionFromID(srcId);
    auto dstSec = IBR_Inst_Project.GetSectionFromID(dstId);
    auto srcBack = srcSec.GetBack_Unsafe();
    auto dstBack = dstSec.GetBack_Unsafe();
    if (!srcBack || !dstBack) return 3;  // 无效链接（红色叉）

    if (isLinkDrag) {
        // IBR_LineDrag 路径：类型校验
        StrPoolID typeAlt{0};
        if (!linkKey.isEmpty() && linkKey != QLatin1String("default")) {
            typeAlt = NewPoolStr(linkKey.toUtf8().constData());
        }
        bool check = Acceptor_CheckLinkType(srcBack->Register, dstBack->Register, typeAlt);
        if (!check) {
            // 对齐 imgui：类型不符 → 生成带"连线类型 + 目标注册名"的预览文字（GUI_Preview_WrongType）
            // 供 mergePreviewText 返回（对应 IBR_SectionData.cpp exam Acceptor_RefusePreview）
            Acceptor_RefusePreview(srcBack->Register, dstBack->Register, typeAlt);
        }
        return check ? 0 : 1;  // 0=允许, 1=类型不匹配
    } else {
        // IBR_SecDrag 路径：检查是否有默认链接 key
        StrPoolID dlk = srcBack->GetDLK(dstBack->Register);
        return (dlk != EmptyPoolStr) ? 0 : 2;  // 0=允许, 2=无默认链接key
    }
}

QString WorkspaceController::mergePreviewText(qulonglong sourceId, qulonglong destId,
                                               const QString &linkKey, bool isLinkDrag)
{
    // 对应 IBR_Inst_Project.DragConditionText / DragConditionTextAlt
    ModuleID_t srcId = static_cast<ModuleID_t>(sourceId);
    ModuleID_t dstId = static_cast<ModuleID_t>(destId);
    auto srcSec = IBR_Inst_Project.GetSectionFromID(srcId);
    auto dstSec = IBR_Inst_Project.GetSectionFromID(dstId);
    auto srcData = srcSec.GetSectionData();
    auto dstData = dstSec.GetSectionData();
    if (!srcData || !dstData) return QString::fromUtf8(u8"无效链接");

    int status = checkMergePreview(sourceId, destId, linkKey, isLinkDrag);
    switch (status) {
        case 0: {
            // 绿色对勾："Ini -> DisplayName : DLK" 或 "Ini -> DisplayName"
            QString srcText = QString::fromUtf8(srcData->Desc.Ini) + u8" → " +
                              QString::fromUtf8(dstData->DisplayName);
            if (!isLinkDrag) {
                auto srcBack = srcSec.GetBack_Unsafe();
                auto dstBack = dstSec.GetBack_Unsafe();
                if (srcBack && dstBack) {
                    StrPoolID dlk = srcBack->GetDLK(dstBack->Register);
                    if (dlk != EmptyPoolStr) {
                        srcText += u8" : " + QString::fromUtf8(PoolStr(dlk));
                    }
                }
            }
            return srcText;
        }
        case 1:
            // 对齐 imgui：连线拖拽类型不符时返回带"连线类型 + 目标注册名"的文字（GUI_Preview_WrongType）
            // 由 checkMergePreview 内 Acceptor_RefusePreview 生成，临时存于 DragConditionTextAlt
            if (isLinkDrag && !IBR_Inst_Project.DragConditionTextAlt.empty())
                return QString::fromUtf8(IBR_Inst_Project.DragConditionTextAlt.c_str());
            return QString::fromUtf8(u8"类型不匹配");
        case 2:
            return QString::fromUtf8(u8"无默认链接 key");
        case 3:
        default:
            return QString::fromUtf8(u8"无效链接");
    }
}

QString WorkspaceController::lineDragSourcePreview(qulonglong sectionId, const QString &keyName)
{
    // 对应 ImGui IBR_LinkNode.cpp:570-573 BeginDragDropSource 的源文本：
    //    普通连 "Ini -> DisplayName : KeyName"
    ModuleID_t id = static_cast<ModuleID_t>(sectionId);
    auto sec = IBR_Inst_Project.GetSectionFromID(id);
    auto data = sec.GetSectionData();
    if (!data) return QString();
    std::string first = std::string(data->Desc.Ini) + " -> " + std::string(data->DisplayName);
    StrPoolID keyId = NewPoolStr(keyName.toUtf8().constData());
    std::string full = first + " : " + PoolStr(keyId);
    return QString::fromUtf8(full.c_str());
}

// ===== 阶段 12.5：节点右键菜单项补齐实现 =====

void WorkspaceController::toggleIgnore(qulonglong sectionId)
{
    // 对应 IBR_SectionData.cpp:629-644 Ignore/NoIgnore 智能互斥
    // 同步修改：SendToR 延迟到下一 tick 执行，而 refresh() 立即重建会读到旧值
    // （画布永远不更新）。与 FreezeSelected/SectionListModel::freeze 同风格，GUI 线程直接改。
    ModuleID_t id = static_cast<ModuleID_t>(sectionId);
    auto it = IBR_Inst_Project.IBR_SectionMap.find(id);
    if (it != IBR_Inst_Project.IBR_SectionMap.end()) {
        it->second.Ignore = !it->second.Ignore;
    }
    refresh();
}

void WorkspaceController::toggleFreeze(qulonglong sectionId)
{
    // 对应 IBR_SectionData.cpp:645-660 FreezeSec/UnfreezeSec 智能互斥（同步修改，见 toggleIgnore 注释）
    ModuleID_t id = static_cast<ModuleID_t>(sectionId);
    auto it = IBR_Inst_Project.IBR_SectionMap.find(id);
    if (it != IBR_Inst_Project.IBR_SectionMap.end()) {
        it->second.Frozen = !it->second.Frozen;
    }
    refresh();
}

void WorkspaceController::toggleHide(qulonglong sectionId)
{
    // 对应 IBR_SectionData.cpp:661-676 HideSec/ShowSec 智能互斥（同步修改，见 toggleIgnore 注释）
    ModuleID_t id = static_cast<ModuleID_t>(sectionId);
    auto it = IBR_Inst_Project.IBR_SectionMap.find(id);
    if (it != IBR_Inst_Project.IBR_SectionMap.end()) {
        it->second.Hidden = !it->second.Hidden;
    }
    refresh();
}

void WorkspaceController::enterEditTextMode(qulonglong sectionId)
{
    // 对应 IBR_SectionData.cpp:720-724 EditText
    // 进入文本编辑模式：激活节点并切换到编辑菜单
    ModuleID_t id = static_cast<ModuleID_t>(sectionId);
    IBRF_CoreBump.SendToR({ [id]() {
        IBR_EditFrame::ActivateAndEdit(id, false);
        if (IBR_Inst_Menu.GetMenuItem() != MenuItemID_EDIT) {
            IBR_Inst_Menu.ChooseMenu(MenuItemID_EDIT);
        }
    } });
    refresh();
}

bool WorkspaceController::isCommentBlock(qulonglong sectionId) const
{
    // 对应 IBR_SectionData.cpp:584-626 Comment 分支
    ModuleID_t id = static_cast<ModuleID_t>(sectionId);
    auto it = IBR_Inst_Project.IBR_SectionMap.find(id);
    if (it == IBR_Inst_Project.IBR_SectionMap.end()) return false;
    return it->second.IsComment;
}

// ===== 阶段 12.6：边界推回 + 触边提示实现 =====

bool WorkspaceController::isEqCenterInRange() const
{
    // 对应 EqPosInRange：检查 EqCenter 是否在 GetEqMin()/GetEqMax() 范围内
    auto eqMin = IBR_FullView::GetEqMin();
    auto eqMax = IBR_FullView::GetEqMax();
    return IBR_FullView::EqCenter.x >= eqMin.x &&
           IBR_FullView::EqCenter.x <= eqMax.x &&
           IBR_FullView::EqCenter.y >= eqMin.y &&
           IBR_FullView::EqCenter.y <= eqMax.y;
}

int WorkspaceController::edgeFlags() const
{
    // 对应 EqXRange/EqYRange：返回触边方向位标志
    // 1=左, 2=右, 4=上, 8=下
    auto eqMin = IBR_FullView::GetEqMin();
    auto eqMax = IBR_FullView::GetEqMax();
    int flags = 0;
    if (IBR_FullView::EqCenter.x <= eqMin.x) flags |= 1;
    if (IBR_FullView::EqCenter.x >= eqMax.x) flags |= 2;
    if (IBR_FullView::EqCenter.y <= eqMin.y) flags |= 4;
    if (IBR_FullView::EqCenter.y >= eqMax.y) flags |= 8;
    return flags;
}

// ===== 阶段 12.7：双击空白弹 SearchModuleAlt 实现 =====

void WorkspaceController::onDoubleClickEmpty(qreal x, qreal y)
{
    // 对应 IBR_WorkSpace.cpp:961-969：双击空白触发模块搜索
    emit moduleSearchRequested(x, y);
}

// ===== 阶段 12.8：连线系统补齐实现 =====

void WorkspaceController::unlinkAllLinks(qulonglong sectionId)
{
    // 对应 IBR_LinkNode.cpp:487-495 Unlink / 510-538 UnlinkAll
    ModuleID_t id = static_cast<ModuleID_t>(sectionId);
    IBRF_CoreBump.SendToR({ [id]() {
        auto &links = IBR_Inst_Project.LinkList;
        links.erase(
            std::remove_if(links.begin(), links.end(),
                [id](const IBR_Project::_Plink &l) {
                    return l.SourceID == id || l.Dest.ID == id;
                }),
            links.end());
        IBR_Inst_Project.RefreshLinkList = true;
    } });
    refresh();
}

int WorkspaceController::getLinkLimit(qulonglong sectionId, const QString &linkKey) const
{
    // 对应 IBR_LinkNode::LinkLimit
    ModuleID_t id = static_cast<ModuleID_t>(sectionId);
    auto rsec = IBR_Inst_Project.GetSectionFromID(id);
    auto bsec = rsec.GetBack_Unsafe();
    if (!bsec) return 0;

    StrPoolID keyId{0};
    if (linkKey.isEmpty() || linkKey == QLatin1String("default")) {
        keyId = bsec->GetDLK(bsec->Register);
    } else {
        keyId = NewPoolStr(linkKey.toUtf8().constData());
    }

    // 从 DefaultTypeList 查询 LinkLimit
    auto def = IBF_Inst_DefaultTypeList.List.KeyBelongToLine(keyId, bsec->Register);
    if (def) return def->GetLinkLimit();
    return 0;
}

QVariantList WorkspaceController::getSectionLinks(qulonglong sectionId) const
{
    // 获取与该节点相关的所有连线（用于多链接 RadioButton 切换视图）
    ModuleID_t id = static_cast<ModuleID_t>(sectionId);
    QVariantList list;
    for (const auto &link : IBR_Inst_Project.LinkList)
    {
        if (link.SourceID == id || link.Dest.ID == id)
        {
            QVariantMap item;
            item["sourceId"] = static_cast<qulonglong>(link.SourceID);
            item["destId"] = static_cast<qulonglong>(link.Dest.ID);
            item["color"] = QColor(
                (link.Color >> 0) & 0xFF,
                (link.Color >> 8) & 0xFF,
                (link.Color >> 16) & 0xFF,
                (link.Color >> 24) & 0xFF
            );
            item["fromImport"] = link.FromImport;
            item["isSelfLinked"] = link.IsSelfLinked;
            item["isSource"] = (link.SourceID == id);
            list.append(item);
        }
    }
    return list;
}
