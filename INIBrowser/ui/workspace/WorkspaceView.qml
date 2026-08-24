// WorkspaceView.qml
// 工作区主容器，对应 IBR_WorkSpace::RenderUI() 主流程
// 包含：背景 + Section 节点 Repeater + 连线渲染 + 框选 + 右键菜单
import QtQuick
import QtQuick.Controls
import INIWeaver
import "../components"

Item {
    id: workspaceView
    // 注：不要使用 anchors.fill: parent，本组件由 RowLayout 管理（Main.qml:91）
    // Layout 会自动设置 x/y/width/height，使用 anchors 会导致 undefined behavior
    // clip: true 防止模块节点溢出到侧边栏/菜单栏之上
    clip: true

    // 拖拽预览框（文件末尾 dragPreview）暴露为属性：
    // QML 中 id 不能通过 "." 从其他组件文件访问，子组件（LinkNodePoint/SectionNode）
    // 必须经此属性更新预览框位置（跟随鼠标）
    property alias dragPreviewItem: dragPreview

    // 缩放补间动画（QML 端驱动，渲染线程与帧同步）
    // 参考 GraphFlow（Qt/QML 节点图编辑器）：缩放动画放 QML 侧由 QQuickWindow
    // 动画驱动推进，避免 C++ QVariantAnimation 定时器 tick 与渲染循环不同步
    // 造成的跳帧/低帧率。C++ onWheel 只设定目标 Ratio 与锚点并 emit zoomTweenRequested，
    // 本动画每渲染帧更新 zoomAnimRatio 并调 applyZoomRatio 应用到 C++ 全局态。
    // zoomAnimRatio 故意非绑定：动画期间由 NumberAnimation 独占控制，不受 C++ 回写干扰
    property real zoomAnimRatio: 1.0
    // 动画每帧写入 zoomAnimRatio 时应用到 C++ 全局态（NumberAnimation 无 onValueChanged
    // 信号处理器，用属性变化处理器驱动：Ratio/EqCenter/拖拽基准修正/dragOffset/预览线
    // 在 C++ applyZoomRatio 内原子完成）
    onZoomAnimRatioChanged: workspaceController.applyZoomRatio(zoomAnimRatio)
    // restart/abort 的瞬停抑制：stop() 同步触发 onRunningChanged(false)，此时不应收尾
    property bool zoomSuppressFinish: false

    NumberAnimation {
        id: zoomTweenAnim
        target: workspaceView
        property: "zoomAnimRatio"
        duration: 120
        easing.type: Easing.OutCubic
        onRunningChanged: {
            // 正常结束才收尾（restart/abort 的瞬停被 zoomSuppressFinish 抑制）
            if (!running && !workspaceView.zoomSuppressFinish)
                workspaceController.zoomTweenFinished()
        }
    }

    // 同步视口尺寸到 C++ IBR_RealCenter，供 EqPosToRePos 计算屏幕坐标
    // 对应 ImGui 版本 IBR_Misc.cpp:484-493 IBR_RealCenter::Update()
    function reportViewportGlobal() {
        var g = mapToGlobal(0, 0)
        workspaceController.setViewportGlobal(g.x, g.y)
    }
    onWidthChanged: { workspaceController.setViewportSize(width, height); workspaceView.reportViewportGlobal() }
    onHeightChanged: { workspaceController.setViewportSize(width, height); workspaceView.reportViewportGlobal() }
    onXChanged: workspaceView.reportViewportGlobal()
    onYChanged: workspaceView.reportViewportGlobal()
    Component.onCompleted: {
        workspaceController.setViewportSize(width, height)
        workspaceView.reportViewportGlobal()
    }

    // 查找指定 workspaceView 坐标系坐标下的 SectionNode（供 LinkPoint 拖放检测）
    // 顶层 Repeater 只含顶层模块；编组内子模块由 SectionNode.hitTestChild 递归命中
    //（子模块实际坐标由 QML Column 布局决定，不能用 C++ EqPos 盒测试）。
    // 从后向前遍历（后渲染的在上层），返回命中的最深层 sectionData，未命中返回 null
    function findSectionAt(screenX, screenY) {
        for (var i = sectionRepeater.count - 1; i >= 0; --i) {
            var item = sectionRepeater.itemAt(i)
            if (item && item.hitTestChild) {
                var hit = item.hitTestChild(screenX, screenY)
                if (hit) return hit
            }
        }
        return null
    }

    // 命中 sectionId 的字符串版本：命中返回 sectionId（String），未命中返回 ""。
    // 用途与 C++ hitTestSectionStr 一致，但覆盖编组内子模块（按实际渲染矩形递归命中）。
    function findHitSectionIdStr(wsX, wsY) {
        var node = findSectionAt(wsX, wsY)
        return (node && node.sectionId !== undefined) ? String(node.sectionId) : ""
    }

    // 临时拖拽连线（对应 IBR_WorkSpace.cpp:1436-1459 拖动中的 Bezier）
    // D22：双层 Bezier 光晕效果 + ImGui 控制点公式
    Canvas {
        id: draggingLinkCanvas
        anchors.fill: parent
        visible: false
        z: 60

        property real fromX: 0
        property real fromY: 0
        property real toX: 0
        property real toY: 0
        // D22：拖拽源颜色（对应 ImGui LinkNodeContext::CurDragCol）
        property color dragCol: "#cccccc"
        property real ratio: workspaceController.ratio

        onRatioChanged: requestPaint()

        onPaint: {
            var ctx = getContext("2d")
            ctx.reset()
            // D22：ImGui 控制点公式（对应 IBR_WorkSpace.cpp:1448-1449）
            // cp1 = (pa.x + 4*pb.x)/5, pa.y; cp2 = (4*pa.x + pb.x)/5, pb.y
            var cp1x = (fromX + 4 * toX) / 5
            var cp1y = fromY
            var cp2x = (4 * fromX + toX) / 5
            var cp2y = toY
            var fontHeight = 13.0 * ratio
            var lineWidth = fontHeight / 5.0

            // D22：外层 FocusLineColor 光晕（宽度 +2）
            ctx.strokeStyle = "#4fc3f7"
            ctx.lineWidth = lineWidth + 2.0
            ctx.beginPath()
            ctx.moveTo(fromX, fromY)
            ctx.bezierCurveTo(cp1x, cp1y, cp2x, cp2y, toX, toY)
            ctx.stroke()

            // D22：内层拖拽源颜色
            ctx.strokeStyle = dragCol
            ctx.lineWidth = lineWidth
            ctx.beginPath()
            ctx.moveTo(fromX, fromY)
            ctx.bezierCurveTo(cp1x, cp1y, cp2x, cp2y, toX, toY)
            ctx.stroke()
        }
    }

    Connections {
        target: workspaceController
        function onDraggingLinkChanged(fx, fy, tx, ty) {
            draggingLinkCanvas.fromX = fx
            draggingLinkCanvas.fromY = fy
            draggingLinkCanvas.toX = tx
            draggingLinkCanvas.toY = ty
            draggingLinkCanvas.visible = true
            draggingLinkCanvas.requestPaint()
        }
        function onDraggingLinkCleared() {
            draggingLinkCanvas.visible = false
        }
        // 缩放补间启动/续接（C++ 滚轮事件触发）：从当前实际 Ratio 向新目标平滑过渡，
        // 连续滚轮续接不跳变。restart 的 stop() 瞬停由 zoomSuppressFinish 抑制收尾。
        function onZoomTweenRequested() {
            workspaceView.zoomSuppressFinish = true
            zoomTweenAnim.stop()
            zoomTweenAnim.from = workspaceController.ratio
            zoomTweenAnim.to = workspaceController.zoomTargetRatio
            zoomTweenAnim.start()
            workspaceView.zoomSuppressFinish = false
        }
        // 缩放补间强制中止（C++ 收尾路径清 zoomPending 前调用）：
        // Ratio 已由 C++ 直接推到目标，这里只停动画，不再逐帧改值
        function onZoomTweenAbortRequested() {
            workspaceView.zoomSuppressFinish = true
            zoomTweenAnim.stop()
            workspaceView.zoomSuppressFinish = false
        }
    }

    // 工作区背景
    Rectangle {
        anchors.fill: parent
        color: "#1e1e1e"

        // 网格背景（视觉参考）——网格固定在 Eq 世界坐标系上：
        // 网格线与模块保持相对位置，平移画布（EqCenter 变化）/缩放（ratio 变化）时
        // 网格随世界坐标同步移动（对应 ImGui 原版网格随视口移动的行为，而非固定屏幕网格）。
        // 参考：QML 无限画布方案（startX = floor(-translate/zoom/grid)*grid 只画可视区域）
        Canvas {
            id: gridCanvas
            anchors.fill: parent
            property real ratio: workspaceController.ratio
            property real ecX: workspaceController.eqCenter.x
            property real ecY: workspaceController.eqCenter.y
            onRatioChanged: requestPaint()
            onEcXChanged: requestPaint()
            onEcYChanged: requestPaint()

            onPaint: {
                var ctx = getContext("2d");
                ctx.reset();
                ctx.strokeStyle = "#252525";
                ctx.lineWidth = 1;
                var vx = width / 2, vy = height / 2;
                // 世界网格间距（Eq 单位，对应基准 40px @ ratio=1）
                var gridEq = 40;
                // 缩放时保持视觉密度：屏幕间距过小（<16px）时倍增世界间距，
                // 防止 ratio 很小（缩小）时网格线过密导致性能与视觉问题
                var screenGap = gridEq * ratio;
                while (screenGap < 16 && gridEq < 2560) {
                    gridEq *= 2;
                    screenGap = gridEq * ratio;
                }
                // 可视范围对应的 Eq 区间（只画可视区域，避免全屏过多线条）
                var leftEq = ecX - vx / ratio;
                var rightEq = ecX + vx / ratio;
                var topEq = ecY - vy / ratio;
                var bottomEq = ecY + vy / ratio;
                // 从可视区域内第一条网格线开始（floor 到 gridEq 整数倍，参考无限画布方案）
                var startEqX = Math.floor(leftEq / gridEq) * gridEq;
                var startEqY = Math.floor(topEq / gridEq) * gridEq;
                // 单 path 一次 stroke 画全部网格线（原逐线 beginPath+stroke 的状态切换
                // 在缩放补间每帧重绘时是明显开销，合并后大幅降低 Canvas 重绘成本）
                // 屏幕坐标 = (eqX - eqCenter) * ratio + 视口中心（与 SectionNode 同公式）
                ctx.beginPath();
                for (var ex = startEqX; ex <= rightEq; ex += gridEq) {
                    var sx = (ex - ecX) * ratio + vx;
                    ctx.moveTo(sx, 0);
                    ctx.lineTo(sx, height);
                }
                for (var ey = startEqY; ey <= bottomEq; ey += gridEq) {
                    var sy = (ey - ecY) * ratio + vy;
                    ctx.moveTo(0, sy);
                    ctx.lineTo(width, sy);
                }
                ctx.stroke();
            }
        }
    }

    // 连线层（在节点下方）
    LinkRenderer {
        id: linkRenderer
        anchors.fill: parent
        links: workspaceController.links
        sections: workspaceController.sections
        linkEndpoints: workspaceController.linkEndpoints
        linkEndpointsMap: workspaceController.linkEndpointsMap
        focusedSectionId: workspaceController.focusedSectionId
        ratio: workspaceController.ratio
        // 阶段 2：传入 dragOffset，拖拽中实时更新连线端点
        dragOffset: workspaceController.dragOffset
        // 画布平移偏移：拖动画布时端点表保持快照，渲染叠加此偏移与节点移动保持一致
        canvasOffset: workspaceController.canvasDragOffset
    }

    // 鼠标交互区（空白处）
    // 修复：移除 hoverEnabled，避免背景 MouseArea 窃取 SectionNode 的鼠标移动事件
    // 背景仅在 pressed/released/wheel 时处理，不主动跟踪 hover
    MouseArea {
        id: mouseArea
        anchors.fill: parent
        acceptedButtons: Qt.LeftButton | Qt.RightButton
        hoverEnabled: false
        z: 0  // 确保在 SectionNode（z>=1）之下

        onPressed: {
            workspaceController.onMousePress(mouseX, mouseY, mouse.button)
        }

        onPositionChanged: {
            if (pressed) {
                workspaceController.onMouseMove(mouseX, mouseY)
            }
        }

        onReleased: {
            workspaceController.onMouseRelease(mouseX, mouseY, mouse.button)
        }

        // 阶段 8.3：双击事件转发到 ProjectController，供 DebugPanel UIState 显示
        // 对应 ImGui IsMouseDoubleClicked(ImGuiMouseButton_Left)
        // 阶段 12.7：双击空白触发模块搜索（对应 IBR_WorkSpace.cpp:961-969 SearchModuleAlt::RenderUI）
        onDoubleClicked: {
            if (mouse.button === Qt.LeftButton) {
                projectController.markDblClickLeft()
                workspaceController.onDoubleClickEmpty(mouseX, mouseY)
            }
        }

        onWheel: {
            workspaceController.onWheel(wheel.x, wheel.y, wheel.angleDelta.y)
        }
    }

    // 模块拖放接收区已废弃：Qt Drag/DropArea 在本窗口/Layout 环境下不可靠（拖拽源在
    // 列表 delegate 内被裁剪，自动拖拽不投递到 DropArea）。改为侧边栏手动拖拽
    // （ModulesPanel onReleased 换算全局坐标 → 视口坐标 → placeModuleByKey 放置）。

    // Section 节点层（z-order 高于背景 MouseArea，确保节点可接收鼠标事件）
    // 性能优化：拖拽平移时不全量 refresh，而是根据 eqCenter 实时计算屏幕坐标
    // 对应 IBR_Misc.h:136 EqPosToRePos: (EqPos - EqCenter) * Ratio + Center
    // Center = 视口中心 = (width/2, height/2)
    // 节点拖拽中：根据 dragOffset（屏幕坐标偏移）实时更新位置，避免每帧 refresh
    Repeater {
        id: sectionRepeater
        // 增量模型（QAbstractListModel）：新建/删除模块时只增删变化的 delegate，
        // 避免整表替换重建全部 SectionNode（卡顿根因）
        model: workspaceController.sectionsModel

        SectionNode {
            id: sectionDelegate
            // QAbstractItemModel 的 delegate 中 modelData 不生成（Qt6 实测 undefined），
            // 改用 role 访问：roleNames 定义了 "sectionData" role
            sectionData: model.sectionData
            // z 高于背景 MouseArea（z=0），确保节点 MouseArea 优先接收事件
            z: 1
            // 性能优化：本地缓存的 EqPos，拖拽结束时通过 sectionPositionChanged 信号更新
            // 避免全量 refresh() 重建 Repeater 导致连续拖拽卡顿
            property real localEqX: sectionData.eqX
            property real localEqY: sectionData.eqY
            // 根据 EqPos 和当前 EqCenter 实时计算屏幕坐标
            // 拖拽中实时应用 dragOffset，节点跟随鼠标移动
            // 注意：用 isSelected（动态绑定）而非 sectionData.selected（快照值）
            //   选中状态变化后 sections 列表未重建，sectionData.selected 仍是旧值，
            //   会导致多节点拖拽时不应用 dragOffset（节点不跟随鼠标）
            x: (localEqX - workspaceController.eqCenter.x) * workspaceController.ratio + workspaceView.width / 2
              + ((sectionData.sectionId === workspaceController.draggingSectionId
                  || (workspaceController.massDragging && isSelected))
                 ? workspaceController.dragOffset.x : 0)
            y: (localEqY - workspaceController.eqCenter.y) * workspaceController.ratio + workspaceView.height / 2
              + ((sectionData.sectionId === workspaceController.draggingSectionId
                  || (workspaceController.massDragging && isSelected))
                 ? workspaceController.dragOffset.y : 0)
            // 尺寸完全由 Qt 内容驱动（不依赖 ImGui 的 EqW/EqH）
            // 宽度：max(WidthFix, widthBase) * ratio（widthBase 已用真实 FontHeight 计算）
            // 高度：header + 行数 * 行高 + margin，随 ratio 等比缩放，内容显示全
            width: implicitWidth
            height: implicitHeight
            // 渲染后回报实际屏幕尺寸到 C++，同步 sd.EqSize（对应 ImGui 实时更新 EqSize）
            // 框选命中检测依赖准确的 EqSize，否则框选位置和命中模块不一致
            // 缩放方案：width/height 为逻辑尺寸（内部已 scale=_r），回报需乘 _r 得视觉尺寸，
            // 否则 EqSize = 逻辑/ratio 会随缩放漂移。仅在逻辑尺寸变化时触发（缩放不变）→ 天然节流
            onWidthChanged: {
                if (workspaceController.diagLogEnabled()) console.log("[LINK-DIAG] SectionNode onWidthChanged sid=" + sectionData.sectionId + " w=" + width + " h=" + height)
                workspaceController.reportSectionSize(sectionData.sectionId, width * workspaceController.ratio, height * workspaceController.ratio)
            }
            onHeightChanged: {
                if (workspaceController.diagLogEnabled()) console.log("[LINK-DIAG] SectionNode onHeightChanged sid=" + sectionData.sectionId + " w=" + width + " h=" + height)
                workspaceController.reportSectionSize(sectionData.sectionId, width * workspaceController.ratio, height * workspaceController.ratio)
            }
            Component.onCompleted: workspaceController.reportSectionSize(sectionData.sectionId, width * workspaceController.ratio, height * workspaceController.ratio)
            isSelected: {
                // 性能优化：通过 selectedRevision 触发重新评估，不依赖全量 refresh
                workspaceController.selectedRevision
                return workspaceController.isSectionSelected(sectionData.sectionId)
            }
            // 性能优化：拖拽结束时只更新被拖拽节点的位置，不重建整个 Repeater
            // 注意：不调用 getSectionData（会触发 buildSectionItem → SectionLineModel::refresh，
            //        重建行模型导致卡顿）。直接读 IBR_SectionMap 的 EqPos 通过专用轻量接口。
            Connections {
                target: workspaceController
                function onSectionPositionChanged(sid) {
                    if (sid === sectionData.sectionId) {
                        var pos = workspaceController.getSectionEqPos(sectionData.sectionId)
                        sectionDelegate.localEqX = pos.x
                        sectionDelegate.localEqY = pos.y
                    }
                }
            }
        }
    }

    // 框选矩形
    // selectionRect 存储的是 Eq 坐标，需转换为屏幕坐标（与 SectionNode 同公式）
    // screenX = (eqX - eqCenter.x) * ratio + width/2
    // screenW = eqW * ratio
    SelectionBox {
        id: selectionBox
        x: (workspaceController.selectionRect.x - workspaceController.eqCenter.x) * workspaceController.ratio + workspaceView.width / 2
        y: (workspaceController.selectionRect.y - workspaceController.eqCenter.y) * workspaceController.ratio + workspaceView.height / 2
        width: workspaceController.selectionRect.width * workspaceController.ratio
        height: workspaceController.selectionRect.height * workspaceController.ratio
    }

    // ===== 右键菜单（单例统一，host 定义于 Main.qml 根级，此处经动态作用域引用）=====
    // 所有右键入口统一经 contextMenuHost.show() 提交内容（对齐 ImGui IBR_PopupManager 单例）

    // 多选态菜单内容（对应 ImGui MassAfter 右键，IBR_WorkSpace.cpp:1196-1311）
    // 顺序：复制 → 剪切 → 粘贴 → 忽略/取消忽略 → 冻结/解冻 → 隐藏/显示 → 缩合 → 导出模块 → 克隆 → 删除
    function massAfterDescs() {
        var descs = []
        if (workspaceController.massTargetIds().length === 0) return descs
        descs.push({ type: "item", text: (i18n.rev, i18n.tr("GUI_Copy")), action: "copy" })
        descs.push({ type: "item", text: (i18n.rev, i18n.tr("GUI_Cut")), action: "cut" })
        descs.push({ type: "item", text: (i18n.rev, i18n.tr("GUI_Paste")), action: "paste" })
        // 智能互斥：全忽略只显示"取消忽略"，否则只显示"忽略"；冻结/隐藏同理
        if (workspaceController.selectedAllIgnored())
            descs.push({ type: "item", text: (i18n.rev, i18n.tr("GUI_NoIgnore")), action: "unignore" })
        else
            descs.push({ type: "item", text: (i18n.rev, i18n.tr("GUI_Ignore")), action: "ignore" })
        if (workspaceController.selectedAllFrozen())
            descs.push({ type: "item", text: (i18n.rev, i18n.tr("GUI_UnfreezeSec")), action: "unfreeze" })
        else
            descs.push({ type: "item", text: (i18n.rev, i18n.tr("GUI_FreezeSec")), action: "freeze" })
        if (workspaceController.selectedAllHidden())
            descs.push({ type: "item", text: (i18n.rev, i18n.tr("GUI_ShowSec")), action: "show" })
        else
            descs.push({ type: "item", text: (i18n.rev, i18n.tr("GUI_HideSec")), action: "hide" })
        descs.push({ type: "item", text: (i18n.rev, i18n.tr("GUI_Compose")), action: "compose" })
        descs.push({ type: "item", text: (i18n.rev, i18n.tr("GUI_ExportModule")), action: "outputModule" })
        descs.push({ type: "item", text: (i18n.rev, i18n.tr("GUI_Duplicate")), action: "duplicate" })
        descs.push({ type: "item", text: (i18n.rev, i18n.tr("GUI_Delete")), action: "delete" })
        return descs
    }

    // 多选态动作分发
    function dispatchMassAction(action) {
        switch (action) {
        case "copy":        workspaceController.copySelected(); break
        case "cut":         workspaceController.cutSelected(); break
        case "paste":       workspaceController.paste(); break
        case "unignore":    workspaceController.ignoreSelected(false); break
        case "ignore":      workspaceController.ignoreSelected(true); break
        case "unfreeze":    workspaceController.freezeSelected(false); break
        case "freeze":      workspaceController.freezeSelected(true); break
        case "show":        workspaceController.hideSelected(false); break
        case "hide":        workspaceController.hideSelected(true); break
        case "compose":     workspaceController.composeSelected(); break
        case "outputModule": workspaceController.requestOutputModuleDialog(); break
        case "duplicate":   workspaceController.duplicateSelected(); break
        case "delete":      workspaceController.deleteSelected(); break
        }
    }

    // 空白右键动作分发（模块树项在 host 内部处理，这里只处理操作项）
    function dispatchEmptyAction(action) {
        switch (action) {
        case "selectAll":      workspaceController.selectAll(); break
        case "paste":          workspaceController.paste(); break
        case "refreshRegName": projectController.refreshAllRegName(); break
        }
    }

    // 连接右键菜单信号
    Connections {
        target: workspaceController
        function onContextMenuRequested(x, y) {
            var g = workspaceView.mapToGlobal(x, y)
            // 有选中模块 → 多选操作菜单；无选中（空白右键）→ 模块树菜单
            if (workspaceController.massTargetIds().length > 0) {
                contextMenuHost.placePos = null
                contextMenuHost.show(workspaceView.massAfterDescs(), g.x, g.y, (a) => workspaceView.dispatchMassAction(a))
            } else {
                // 记录右键的工作区视口坐标，模块菜单选中项时放到该位置
                contextMenuHost.placePos = { x: x, y: y }
                contextMenuHost.show(contextMenuHost.moduleTreeDescs(), g.x, g.y, (a) => workspaceView.dispatchEmptyAction(a))
            }
        }
    }

    // 空状态提示
    Text {
        anchors.centerIn: parent
        visible: workspaceController.sections.length === 0 && workspaceController.isProjectOpen
        color: "#5a5a5a"
        font.pixelSize: 14
        text: (i18n.rev, i18n.tr("GUI_WorkspaceEmpty"))
    }

    // 拖拽目标预览（对应 ImGui DrawDragPreviewIcon，IBR_SectionData.cpp:108-129）
    // ImGui 原版 BeginDragDropSource 内的内容作为拖拽图像跟随鼠标显示（对勾/叉 + "链接到: ..."文本）。
    // 放在 workspaceView 最上层（z=200 > 节点最高 z=100），确保不被任何块遮挡；
    // 位置由拖拽源（键行圆点/标题栏圆点）onPositionChanged 更新（workspaceView 坐标）。
    // 有效（蓝）→ 对勾；无效（红/橙）→ 叉；边框随状态色，使其明显。
    Item {
        id: dragPreview
        x: -1000
        y: -1000
        z: 200
        // 连线拖拽中即显示源标签（不对齐模块时仅源标签跟随鼠标，对齐 imgui 拖拽图像；
        // 命中模块后 dragTargetSectionId 非 0，则下方目标行一并显示）
        visible: workspaceController.hasDraggingLink || workspaceController.hasDragTarget
        width: dragPreviewCol.implicitWidth + 6
        height: dragPreviewCol.implicitHeight + 4

        Rectangle {
            anchors.fill: parent
            color: "#e6262626"
            radius: 3
            // 未命中模块时（仅显示源标签）用白框；命中模块后按目标状态色（绿/红）
            border.color: workspaceController.hasDragTarget
                         ? (workspaceController.dragTargetColor === "#4fc3f7" ? "#4fc3f7" : "#ff5050")
                         : "#ffffff"
            border.width: 1
        }
        Column {
            id: dragPreviewCol
            anchors.left: parent.left
            anchors.leftMargin: 3
            anchors.verticalCenter: parent.verticalCenter
            spacing: 2
            // 连线源预览（对应 imgui BeginDragDropSource 源文本，IBR_LinkNode.cpp:570-573）
            Text {
                visible: workspaceController.dragSourceText.length > 0
                text: workspaceController.dragSourceText
                color: "#d4d4d4"
                // 还原：拖拽预览框非悬停提示，字体随工作区缩放
                font.pixelSize: Math.max(1, Math.round(10 * workspaceController.ratio))
            }
            // 目标命中行（对勾/叉 + 目标文字），仅悬停在模块上时显示
            Row {
                visible: workspaceController.hasDragTarget
                spacing: 3
                // 对勾 / 叉（对应 DrawCheckmark / DrawCross）
                Text {
                    text: workspaceController.dragTargetColor === "#4fc3f7" ? "✓" : "✕"
                    color: workspaceController.dragTargetColor === "#4fc3f7" ? "#4fc3f7" : "#ff5050"
                    font.pixelSize: Math.max(1, Math.round(10 * workspaceController.ratio))
                }
                // 预览文本（"链接到: Xxx -> Yyy" / "无效链接" / "类型不匹配" 等）
                Text {
                    text: workspaceController.dragTargetText
                    color: workspaceController.dragTargetColor === "#4fc3f7" ? "#4fc3f7" : "#ff5050"
                    font.pixelSize: Math.max(1, Math.round(10 * workspaceController.ratio))
                }
            }
        }
    }
}
