// WorkspaceController.cpp
// 工作区控制器实现：桥接 IBR_WorkSpace / IBR_FullView / IBR_RealCenter
#include "WorkspaceController.h"
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
#include "FromEngine/ImGuiDeps.h"
#include <QGuiApplication>
#include <QFontMetrics>
#include <QTimer>
#include <QFile>
#include <QTextStream>
#include <unordered_set>
#include <algorithm>
#include <functional>
#include <cmath>
#include <limits>

// 阶段 12.4：前向声明 IBR_SectionData.cpp 中的自由函数（全局作用域）
// 对应 IBR_SectionData.cpp:438-464
bool Acceptor_CheckLinkType(StrPoolID SourceReg, StrPoolID TargetReg, StrPoolID LinkType);
bool Acceptor_CheckSecType(StrPoolID SourceReg, StrPoolID SecType);

WorkspaceController::WorkspaceController(QObject *parent)
    : QObject(parent)
{
    // 初始化 MassAfter 右键按下位置为"未按下"（对应 FLT_MAX）
    m_massAfterRightDownPos = QPointF(std::numeric_limits<qreal>::max(),
                                       std::numeric_limits<qreal>::max());
    // C11：Paste 节流计时器
    m_uptimeTimer.start();
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

void WorkspaceController::onMousePress(qreal x, qreal y, int button)
{
    // 阶段 12.1：完整状态机（对应 IBR_WorkSpace.cpp:822-830 状态迁移图）
    if (button == Qt::LeftButton) {
        if (m_inputState == 0) {
            // v3 批次 2.1：进入 BgDragging 前保存视口前态（对应 MainStage.h:122 UpdatePrev）
            IBR_WorkSpace::UpdatePrev();
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
                m_dragSectionId = 0;
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
        // C7：加入边界推回 + EqPosFixRange 钳制（对应 IBR_WorkSpace.cpp:247-264 UpdateScroll）
        QPointF delta = QPointF(x, y) - m_dragStartScreen;
        IBR_FullView::EqCenter.x -= static_cast<float>(delta.x()) / IBR_FullView::Ratio;
        IBR_FullView::EqCenter.y -= static_cast<float>(delta.y()) / IBR_FullView::Ratio;
        m_dragStartScreen = QPointF(x, y);
        // C7：钳制到有效范围（对应 IBR_WorkSpace.cpp:261 EqPosFixRange）
        IBR_FullView::EqPosFixRange(IBR_FullView::EqCenter);
        // 性能优化：拖拽平移时只发信号，QML 根据新 EqCenter 重新计算屏幕坐标
        // 不调用 refresh()（全量重建 QVariantList 太慢，鼠标移动每秒触发几十次）
        emit eqCenterChanged();
        emit edgeFlagsChanged();
        m_linkEndpointsDirty = true;  // D10：EqCenter 变化导致节点位置变化，需重建端点表
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
        // HoldingModules：多节点拖拽中，更新位置
        updateMassDrag(x, y);
    }
}

void WorkspaceController::onMouseRelease(qreal x, qreal y, int button)
{
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
            // BgDragging → Normal
            updateInputState(0);
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
                // 移动≤2px → 右键单击，弹出右键菜单
                endSelection();
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
                showMassContextMenu(x, y);
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

    float newRatio = std::floor(IBR_FullView::Ratio * std::exp(deltaWheel / 9.0f) * 20.0f) / 20.0f;
    if (deltaWheel < 0) newRatio += 0.05f;
    if (std::abs(newRatio - IBR_FullView::Ratio) < 1e-6f)
        newRatio += deltaWheel < 0 ? -0.05f : 0.05f;
    newRatio = std::min(newRatio, IBR_FullView::RatioMax / 100.0f);
    newRatio = std::max(newRatio, IBR_FullView::RatioMin / 100.0f);

    // v3 批次 2.1：缩放前保存视口前态（对应 MainStage.h:122 UpdatePrev）
    IBR_WorkSpace::UpdatePrev();
    // 保持鼠标位置对应的 EqPos 不变（对应 UpdateNewRatio 的鼠标锚定）
    QPointF mouseEqBefore = screenToEq(QPointF(x, y));
    IBR_FullView::Ratio = newRatio;
    QPointF mouseEqAfter = screenToEq(QPointF(x, y));
    IBR_FullView::EqCenter.x += static_cast<float>(mouseEqBefore.x() - mouseEqAfter.x());
    IBR_FullView::EqCenter.y += static_cast<float>(mouseEqBefore.y() - mouseEqAfter.y());
    // v3 批次 2.1：缩放后推入 Undo 栈（对应 MainStage.h:218 UpdatePrevII）
    IBR_WorkSpace::UpdatePrevII();
    // v3 批次 3.1：缩放后更新 EqMax，确保 MiniMap 可见范围正确（对应 IBR_FullView::UpdateCurrentEqMax）
    IBR_FullView::UpdateCurrentEqMax();
    // 性能优化：QML 根据 ratio 和 eqCenter 实时计算屏幕坐标，不需要全量 refresh
    emit ratioChanged();
    emit eqCenterChanged();
    m_linkEndpointsDirty = true;  // D10：Ratio 变化导致节点位置变化，需重建端点表
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
    float clamped = std::clamp(newRatio,
        IBR_FullView::RatioMin / 100.0f,
        IBR_FullView::RatioMax / 100.0f);
    IBR_FullView::Ratio = clamped;
    // 性能优化：缩放只改变 Ratio，不改变 sections 数据
    // QML 根据 ratio 实时计算节点尺寸/坐标，不需要全量 refresh()（避免 91 个节点重建卡顿）
    emit ratioChanged();
    emit workspaceRectChanged();  // 视口 EqRect 随 Ratio 变化
    m_linkEndpointsDirty = true;  // Ratio 变化导致节点位置变化，需重建端点表
}

void WorkspaceController::centerView()
{
    moveToCenter();
}

void WorkspaceController::centerViewTo(qreal eqX, qreal eqY)
{
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
    // 点击空白处取消选中时清空编辑侧边栏，切回 MODULES 菜单
    IBR_EditFrame::CurSection.ID = UINT_MAX;
    if (m_editPanelController) {
        m_editPanelController->clear();
    }
    if (m_menuController) {
        m_menuController->setActiveMenu(1);  // MenuItemID_MODULES
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
    IBR_WorkSpace::DeleteSelected();
    refresh();
}

void WorkspaceController::copySelected()
{
    IBR_WorkSpace::CopySelected();
}

void WorkspaceController::cutSelected()
{
    IBR_WorkSpace::CutSelected();
    refresh();
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
    // 性能优化：全选只改变 MassTarget（选中集合），不改变 sections 数据
    // QML 的 isSelected 绑定到 selectedRevision，递增即可触发蓝框更新
    ++m_selectedRevision;
    emit selectedRevisionChanged();
}

void WorkspaceController::composeSelected()
{
    IBR_WorkSpace::ComposeSelected();
    refresh();
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
    refresh();
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
    // 对应 ImGui IBR_SectionData.cpp:916-918 CurOnRender_Clicked → ActivateAndEdit
    // 拖动单个模块结束后选中该模块（非追加模式）
    // 注意：在 m_dragSectionId 清零之前保存 ID
    ModuleID_t draggedId = m_dragSectionId;
    IBR_WorkSpace::MassSelect({ draggedId });
    updateInputState(4);  // MassAfter
    // 先发蓝框更新信号（视觉反馈优先，不阻塞）
    ++m_selectedRevision;
    emit selectedRevisionChanged();
    if (m_editPanelController) {
        m_editPanelController->setActive(changedId);
    }
    if (m_menuController) {
        m_menuController->setActiveMenu(4);
    }
    // 性能优化：直接更新 CurSection.ID，不调用 SetActive
    // SetActive 在 50ms tick 的 IBR_AutoProc 中同步执行于 UI 线程，包含：
    //   1. 遍历所有 INI 的所有 section 清除 Selected（ExtendMassSelect 已做，冗余）
    //   2. IBD_RInterruptF 获取锁（自旋等待，阻塞 UI 线程导致拖动卡顿）
    //   3. ResetEdit 重建 EditLines（Qt 版本 EditPanelController 不读 EditLines，冗余）
    // Qt 版本只需要 CurSection.ID 用于 focusedSectionId（连线高亮）
    IBR_EditFrame::CurSection.ID = draggedId;
    // 清零 dragOffset，QML 不再应用偏移
    m_dragOffset = QPointF();
    emit dragOffsetChanged();
    m_dragSectionId = 0;
    m_draggingSectionId = INVALID_MODULE_ID;
    emit draggingSectionIdChanged();
    // 性能优化：不调全量 refresh()，只通知 QML 更新被拖拽节点的位置
    emit sectionPositionChanged(changedId);
    // 连线端点表需要重建（节点位置变了）
    m_linkEndpointsDirty = true;
    // 修复：松手后延迟重建端点表，确保 QML LineRow onIsDraggingChanged 回写 LastCenter 后再用新值重建
    // 时序：dragOffsetChanged/draggingSectionIdChanged → QML isDragging 变 false → LineRow 回写新 LastCenter
    //       → QueuedConnection 回调重建端点表（用新 LastCenter）→ emit linkEndpointsChanged → LinkRenderer 重绘
    // 不加此延迟会导致一帧回弹：旧端点表 pa=原位置 + dragOffset=0 = 原位置
    QMetaObject::invokeMethod(this, [this]() {
        rebuildLinkEndpoints();
    }, Qt::QueuedConnection);
}

void WorkspaceController::updateDrag(qreal curX, qreal curY)
{
    // 阶段 12.1：统一拖拽更新入口，根据 m_moveAfterMass 分发
    if (m_inputState != 3) return;
    if (m_moveAfterMass) {
        updateMassDrag(curX, curY);
    } else {
        updateMoveSection(curX, curY);
    }
}

void WorkspaceController::endDrag()
{
    // 阶段 12.1：统一拖拽结束入口
    if (m_inputState != 3) return;
    if (m_moveAfterMass) {
        endMassDrag();
        m_moveAfterMass = false;
        // 修复：拖拽结束后若仍有选中模块，回到 MassAfter 而非 Normal
        restoreStateAfterDrag();
    } else {
        endMoveSection();
    }
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
        if (m_inputState == 4) updateInputState(0);
        // 对应 ImGui IBR_WorkSpace.cpp:1076-1079 Clear() + ChooseMenu(MenuItemID_MODULES)
        // 取消选中最后一个模块时清空编辑侧边栏，切回 MODULES 菜单
        IBR_EditFrame::CurSection.ID = UINT_MAX;
        if (m_editPanelController) {
            m_editPanelController->clear();
        }
        if (m_menuController) {
            m_menuController->setActiveMenu(1);  // MenuItemID_MODULES
        }
    } else {
        if (!additive && current.size() == 1) {
            // 非追加单击单选：保持 Normal(0) 状态，切换到 EditPanel
            // 对应 ImGui IBR_EditFrame::ActivateAndEdit → ChooseMenu(MenuItemID_EDIT)
            // 单选用边框高亮（非 MassAfter），多选才进入 MassAfter 用覆盖层
            if (m_inputState == 4) updateInputState(0);
            IBR_EditFrame::CurSection.ID = id;
            if (m_editPanelController) {
                m_editPanelController->setActive(sectionId);
            }
            if (m_menuController) {
                m_menuController->setActiveMenu(4);
            }
        } else {
            // 追加多选（additive=true）或 size>1：进入 MassAfter(4)
            // 对应 ImGui MassAfter 状态，用覆盖层变色，不显示编辑框
            updateInputState(4);
            IBR_EditFrame::CurSection.ID = UINT_MAX;
            if (m_editPanelController) {
                m_editPanelController->clear();
            }
            if (m_menuController) {
                m_menuController->setActiveMenu(1);  // MenuItemID_MODULES
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
    if (m_menuController) {
        m_menuController->setActiveMenu(4);
    }
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
}

// v3 批次 1.4：DropArea 接收 linkLimit=0 拖拽时通知源 LinkNode 显示"无效链接"提示
// 对应 ImGui IsDragDropPayloadBeingAccepted（IBR_SectionData.cpp:96-106 DrawDragPreviewIcon_LinkLim0）
void WorkspaceController::setDragInvalidLink(bool invalid)
{
    if (m_dragInvalidLink == invalid) return;
    m_dragInvalidLink = invalid;
    emit dragInvalidLinkChanged(invalid);
}

void WorkspaceController::refresh()
{
    // 全量刷新（由 Q_INVOKABLE 修改操作直接调用）
    m_dirty = true;
    m_linkEndpointsDirty = true;  // D10：全量刷新时连带重建端点表
    refreshFromTimer();
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

void WorkspaceController::setViewportSize(qreal width, qreal height)
{
    // 同步视口尺寸到 IBR_RealCenter（对应 ImGui 版本 IBR_Misc.cpp:484-493 IBR_RealCenter::Update）
    // QML WorkspaceView 的 (0,0) 就是工作区左上角（不含菜单栏/侧边栏）
    // 所以 WorkSpaceUL=(0,0), WorkSpaceDR=(width,height), Center=(width/2, height/2)
    IBR_RealCenter::WorkSpaceUL = dImVec2{ 0.0, 0.0 };
    IBR_RealCenter::WorkSpaceDR = dImVec2{ static_cast<double>(width), static_cast<double>(height) };
    IBR_RealCenter::Center = ImVec2(static_cast<float>(width) * 0.5f,
                                     static_cast<float>(height) * 0.5f);
    // 标记脏，下次 refreshFromTimer 时重建（screenX/screenY 依赖 Center）
    m_dirty = true;
}

void WorkspaceController::refreshFromTimer()
{
    bool isOpen = IBR_ProjectManager::IsOpen();
    size_t smapSize = IBR_Inst_Project.IBR_SectionMap.size();
    if (!isOpen) {
        if (!m_sections.isEmpty()) {
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
        if (m_lastIsOpen) {
            emit projectOpenChanged();
            m_lastIsOpen = false;
        }
        m_lastSectionCount = 0;
        m_lastLinkCount = 0;
        return;
    }

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

    if (!m_dirty &&
        m_lastIsOpen &&
        m_lastSectionCount == curSectionCount &&
        m_lastLinkCount == curLinkCount)
    {
        // 无变化，跳过全量重建
        // 但端点表可能因 QML LinkNode 布局回写而变脏（m_linkEndpointsDirty）
        // 此时只重建端点表，不重建 sections/links（对应 ImGui 每帧用新 LastCenter 重绘连线）
        if (m_linkEndpointsDirty) {
            rebuildLinkEndpoints();
            m_linkEndpointsDirty = false;
        }
        return;
    }

    // 保存项目打开前的状态，用于判断是否为"刚打开"
    bool wasOpen = m_lastIsOpen;
    m_dirty = false;
    m_lastIsOpen = true;
    m_lastSectionCount = curSectionCount;
    m_lastLinkCount = curLinkCount;

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

    { refreshSections(); }
    { refreshLinks(); }
    // 修复：rebuildLinkEndpoints 必须在 refreshSections/refreshLinks 之后执行
    // 因为 rebuildLinkEndpoints 依赖 m_lineModels（在 refreshSections 中创建）和 LinkList（在 refreshLinks→rebuildLinkList 中填充）
    if (m_linkEndpointsDirty) {
        rebuildLinkEndpoints();
        m_linkEndpointsDirty = false;
    }
    emit projectOpenChanged();
    emit ratioChanged();
    emit eqCenterChanged();
    emit viewCenterChanged();
    emit workspaceRectChanged();
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
    for (const auto &[id, sd] : IBR_Inst_Project.IBR_SectionMap)
    {
        if (sd.Hidden && !sd.First) continue;
        // 阶段 D：跳过 IsIncluded() 的子模块（由父虚拟块递归渲染）
        // 对应 ImGui IBR_WorkSpace.cpp 主循环：if (sd.IsIncluded()) continue;
        // 子模块数据通过 getSectionData(id) 按需查询，IncludingModules 列表下发到父虚拟块
        if (sd.IsIncluded()) continue;

        list.append(buildSectionItem(id, sd, selectedSet));
    }
    m_sections = std::move(list);
    m_sectionDataRevision++;  // D20：递增版本号
    emit sectionsChanged();
}

// 编辑侧边栏修改值后，只刷新指定 sectionId 的 SectionLineModel
// 避免全量 refreshSections 重建所有节点的 QVariantList 导致卡顿
// SectionLineModel::refresh 内部调 beginResetModel/endResetModel，
// QML 绑定到 lineModel 的 Repeater 会自动更新画布上该模块的键显示
void WorkspaceController::refreshSectionLines(qulonglong sectionId)
{
    if (sectionId == 0) return;
    ModuleID_t id = static_cast<ModuleID_t>(sectionId);
    auto it = m_lineModels.find(id);
    if (it != m_lineModels.end() && *it) {
        (*it)->refresh();
    }
    // 行值/OnShow 变化可能影响连线，标记端点脏
    m_linkEndpointsDirty = true;
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
    item["eqX"] = sd.EqPos.x;
    item["eqY"] = sd.EqPos.y;
    item["eqW"] = sd.EqSize.x;
    item["eqH"] = sd.EqSize.y;
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
            // 画布操作（toggleInputMode/modifyValue/createLink 等）→ 通知侧边栏刷新
            // 双向同步：EditPanel 改值 → refreshSectionLines 刷新画布；画布改值 → 通知 EditPanel
            connect(modelPtr, &SectionLineModel::sectionDataChanged,
                    this, [this](qulonglong sid) {
                if (m_editPanelController && sid == m_editPanelController->currentSectionId()) {
                    m_editPanelController->refreshLines();
                }
            });
            // LinkNode 位置回写 → 标记端点表脏，由 refreshFromTimer 统一重建
            // 对应 ImGui 每帧 UpdateLink → SetSessionStatus 后下帧用新 LastCenter 绘制连线
            connect(modelPtr, &SectionLineModel::linkNodeCenterChanged,
                    this, [this]() {
                        const_cast<WorkspaceController*>(this)->m_linkEndpointsDirty = true;
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
        QVariantMap item;
        // 修复：sourceId/sourceSessionId/destId 用字符串传递
        // 原因：sessionId 是 uint64（高达 1.6e19），超过 JS Number 精度上限（2^53≈9e15），
        //       QML 里作为 number 会精度丢失，导致 linkEndpointsMap 查找 key 不匹配
        // 修复 destId：用 GetSection(link.Dest).ID 获取实际 ID（link.Dest.ID 是旧失效 ID）
        auto dstRsec = IBR_Inst_Project.GetSection(link.Dest);
        ModuleID_t dstActualId = dstRsec.ID;
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

        // ===== 起点 pa（源端 LastCenter）=====
        qreal paX = 0.0, paY = 0.0;
        bool paValid = false;

        // 优先级 1：QML 回写的 LastCenter（非 0 才用，0 表示尚未回写）
        if (sv.LastCenter.x != 0.0f || sv.LastCenter.y != 0.0f)
        {
            paX = static_cast<qreal>(sv.LastCenter.x);
            paY = static_cast<qreal>(sv.LastCenter.y);
            paValid = true;
        }
        // 优先级 2：QML 回写的标题栏接受点（折叠态由 setHeadLineRN 回写）
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
        // 诊断：pa 来源和 LastCenter 原始值
        ep["paSource"] = paValid
            ? (sv.LastCenter.x != 0.0f || sv.LastCenter.y != 0.0f ? "LastCenter" : "AcceptPoint")
            : "EqPos";
        ep["lastCenterX"] = static_cast<qreal>(sv.LastCenter.x);
        ep["lastCenterY"] = static_cast<qreal>(sv.LastCenter.y);

        // ===== 终点 pb（目标接受点）=====
        qreal pbX = 0.0, pbY = 0.0;
        bool pbValid = false;

        // 优先级 1：行级接受点（非折叠态目标，对应 ImGui AcceptCenter[LineMult]）
        // 修复：用 GetSection(Desc) 按 Desc 查找，而非 GetSectionFromID(Dest.ID)
        // 因为 Dest.ID 是 Desc 里的旧 ID，项目加载后 ID 重新分配，旧 ID 失效
        // 对应 ImGui RenderUI_Links 行 1465: auto Rsec = IBR_Inst_Project.GetSection(Link.Dest);
        auto dstRsec = IBR_Inst_Project.GetSection(link.Dest);
        auto dstData = dstRsec.GetSectionData();
        auto dstBsec = dstRsec.GetBack_Unsafe();
        ModuleID_t dstActualId = dstRsec.ID;  // 实际 ID（用于查 m_lineModels/m_sectionAcceptPoint）

        if (!sv.Collapsed && link.DestKey != EmptyPoolStr && dstData)
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

        // D21：构建 map（key = "sessionId:destId"）
        // 字符串 key，与 QML 端 link.sourceSessionId + ":" + link.destId 拼接结果一致
        QString mapKey = QString::number(static_cast<qulonglong>(link.SourceID))
                         + QLatin1String(":")
                         + QString::number(static_cast<qulonglong>(dstActualId));
        endpointsMap.insert(mapKey, ep);
    }
    m_linkEndpoints = std::move(endpoints);
    m_linkEndpointsMap = std::move(endpointsMap);
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
    m_massDragIds.clear();
    m_initHolding = false;
    m_hasLeftDownToWait = false;
    // 修复：清除单节点拖拽的残留状态
    // endMassDrag 可能在单节点拖拽中被调用（onMousePress 取消拖拽），
    // 此时 m_massDragIds 为空，但 m_dragSectionId 非零，Dragging 未被清除
    // 导致该节点永远显示 isDragging 蓝框（#4fc3f7, width=1 较细）
    if (m_dragSectionId != 0) {
        auto dragIt = IBR_Inst_Project.IBR_SectionMap.find(m_dragSectionId);
        if (dragIt != IBR_Inst_Project.IBR_SectionMap.end()) {
            dragIt->second.Dragging = false;
        }
        m_dragSectionId = 0;
        m_draggingSectionId = INVALID_MODULE_ID;
        emit draggingSectionIdChanged();
    }
    // 清零 dragOffset，QML 不再应用偏移
    m_dragOffset = QPointF();
    emit dragOffsetChanged();
    // 清除 massDragging，QML 不再对选中节点应用 dragOffset
    m_massDragging = false;
    emit massDraggingChanged();
    // 性能优化：与 endMoveSection 行为一致
    // - 保持 MassTarget 选中状态（不清除），允许用户连续拖动同一组模块
    // - 回到 MassAfter 状态（而非 Normal），与 endMoveSection 统一
    // - 不调全量 refresh()，改为对每个拖拽节点 emit sectionPositionChanged
    //   避免重建 91 个节点的 QVariantList + SectionLineModel::refresh() 导致卡顿
    restoreStateAfterDrag();
    // 通知 QML 更新所有被拖拽节点的位置（轻量，只更新坐标）
    for (auto id : draggedIds) {
        emit sectionPositionChanged(static_cast<qulonglong>(id));
    }
    // 连线端点表需要重建（多节点位置变了）
    m_linkEndpointsDirty = true;
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
        it->second.EqSize = newEqSize;
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
    ModuleID_t id = static_cast<ModuleID_t>(sectionId);
    IBRF_CoreBump.SendToR({ [id, collapsed]() {
        auto it = IBR_Inst_Project.IBR_SectionMap.find(id);
        if (it != IBR_Inst_Project.IBR_SectionMap.end()) {
            it->second.CollapsedInComposed = collapsed;
        }
    } });
    refresh();
}

void WorkspaceController::foldComposed(qulonglong virtualBlockId)
{
    // 对应 IBR_SectionData.cpp:355-361 FoldComposed
    ModuleID_t id = static_cast<ModuleID_t>(virtualBlockId);
    IBRF_CoreBump.SendToR({ [id]() {
        auto it = IBR_Inst_Project.IBR_SectionMap.find(id);
        if (it == IBR_Inst_Project.IBR_SectionMap.end()) return;
        auto &sd = it->second;
        for (auto subId : sd.IncludingModules) {
            auto subIt = IBR_Inst_Project.IBR_SectionMap.find(subId);
            if (subIt != IBR_Inst_Project.IBR_SectionMap.end()) {
                subIt->second.CollapsedInComposed = true;
            }
        }
    } });
    refresh();
}

void WorkspaceController::unfoldComposed(qulonglong virtualBlockId)
{
    // 对应 IBR_SectionData.cpp:363-369 UnfoldComposed
    ModuleID_t id = static_cast<ModuleID_t>(virtualBlockId);
    IBRF_CoreBump.SendToR({ [id]() {
        auto it = IBR_Inst_Project.IBR_SectionMap.find(id);
        if (it == IBR_Inst_Project.IBR_SectionMap.end()) return;
        auto &sd = it->second;
        for (auto subId : sd.IncludingModules) {
            auto subIt = IBR_Inst_Project.IBR_SectionMap.find(subId);
            if (subIt != IBR_Inst_Project.IBR_SectionMap.end()) {
                subIt->second.CollapsedInComposed = false;
            }
        }
    } });
    refresh();
}

void WorkspaceController::decomposeSection(qulonglong virtualBlockId)
{
    // 对应 IBR_SectionData.cpp:322-339 Decompose
    ModuleID_t id = static_cast<ModuleID_t>(virtualBlockId);
    IBRF_CoreBump.SendToR({ [id]() {
        auto it = IBR_Inst_Project.IBR_SectionMap.find(id);
        if (it == IBR_Inst_Project.IBR_SectionMap.end()) return;
        auto &sd = it->second;
        if (!sd.Decomposable()) return;
        auto res = IBR_Inst_Project.DecomposeSection(id);
        if (res) {
            IBR_WorkSpace::MassSelect(*res);
        }
    } });
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

    IBRF_CoreBump.SendToR({ [srcId, dstId, srcKeyId, srcLineMult, finalDstKey, dstMult, dstData]() {
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
    } });

    refresh();
    return true;
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
            return QString::fromUtf8(u8"类型不匹配");
        case 2:
            return QString::fromUtf8(u8"无默认链接 key");
        case 3:
        default:
            return QString::fromUtf8(u8"无效链接");
    }
}

// ===== 阶段 12.5：节点右键菜单项补齐实现 =====

void WorkspaceController::toggleIgnore(qulonglong sectionId)
{
    // 对应 IBR_SectionData.cpp:629-644 Ignore/NoIgnore 智能互斥
    ModuleID_t id = static_cast<ModuleID_t>(sectionId);
    IBRF_CoreBump.SendToR({ [id]() {
        auto it = IBR_Inst_Project.IBR_SectionMap.find(id);
        if (it != IBR_Inst_Project.IBR_SectionMap.end()) {
            it->second.Ignore = !it->second.Ignore;
        }
    } });
    refresh();
}

void WorkspaceController::toggleFreeze(qulonglong sectionId)
{
    // 对应 IBR_SectionData.cpp:645-660 FreezeSec/UnfreezeSec 智能互斥
    ModuleID_t id = static_cast<ModuleID_t>(sectionId);
    IBRF_CoreBump.SendToR({ [id]() {
        auto it = IBR_Inst_Project.IBR_SectionMap.find(id);
        if (it != IBR_Inst_Project.IBR_SectionMap.end()) {
            it->second.Frozen = !it->second.Frozen;
        }
    } });
    refresh();
}

void WorkspaceController::toggleHide(qulonglong sectionId)
{
    // 对应 IBR_SectionData.cpp:661-676 HideSec/ShowSec 智能互斥
    ModuleID_t id = static_cast<ModuleID_t>(sectionId);
    IBRF_CoreBump.SendToR({ [id]() {
        auto it = IBR_Inst_Project.IBR_SectionMap.find(id);
        if (it != IBR_Inst_Project.IBR_SectionMap.end()) {
            it->second.Hidden = !it->second.Hidden;
        }
    } });
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
