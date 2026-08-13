// WorkspaceView.qml
// 工作区主容器，对应 IBR_WorkSpace::RenderUI() 主流程
// 包含：背景 + Section 节点 Repeater + 连线渲染 + 框选 + 右键菜单
import QtQuick
import QtQuick.Controls
import INIWeaver

Item {
    id: workspaceView
    // 注：不要使用 anchors.fill: parent，本组件由 RowLayout 管理（Main.qml:91）
    // Layout 会自动设置 x/y/width/height，使用 anchors 会导致 undefined behavior
    // clip: true 防止模块节点溢出到侧边栏/菜单栏之上
    clip: true

    // 同步视口尺寸到 C++ IBR_RealCenter，供 EqPosToRePos 计算屏幕坐标
    // 对应 ImGui 版本 IBR_Misc.cpp:484-493 IBR_RealCenter::Update()
    onWidthChanged: workspaceController.setViewportSize(width, height)
    onHeightChanged: workspaceController.setViewportSize(width, height)
    Component.onCompleted: workspaceController.setViewportSize(width, height)

    // 查找指定屏幕坐标下的 SectionNode（供 LinkPoint 拖放检测）
    function findSectionAt(screenX, screenY) {
        for (var i = 0; i < sectionRepeater.count; ++i) {
            var item = sectionRepeater.itemAt(i)
            if (item && item.contains(Qt.point(screenX - item.x, screenY - item.y))) {
                return item.sectionData
            }
        }
        return null
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
    }

    // 工作区背景
    Rectangle {
        anchors.fill: parent
        color: "#1e1e1e"

        // 网格背景（视觉参考）
        // D23：网格随 ratio 缩放，保持视觉密度一致
        Canvas {
            anchors.fill: parent
            property real ratio: workspaceController.ratio
            onRatioChanged: requestPaint()

            onPaint: {
                var ctx = getContext("2d");
                ctx.reset();
                ctx.strokeStyle = "#252525";
                ctx.lineWidth = 1;
                // D23：网格尺寸随 ratio 缩放（对应 ImGui Style.ScaleAllSizes(Ratio)）
                var gridSize = 40 * ratio;
                if (gridSize < 8) gridSize = 8;  // 防止过度缩小
                for (var x = 0; x < width; x += gridSize) {
                    ctx.beginPath();
                    ctx.moveTo(x, 0);
                    ctx.lineTo(x, height);
                    ctx.stroke();
                }
                for (var y = 0; y < height; y += gridSize) {
                    ctx.beginPath();
                    ctx.moveTo(0, y);
                    ctx.lineTo(width, y);
                    ctx.stroke();
                }
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

    // 模块拖放接收区（对应 IBR_WorkSpace::ProcessBackgroundOpr 的模块放置）
    DropArea {
        anchors.fill: parent
        onEntered: (drag) => {
            drag.accepted = true
        }
        onDropped: (drop) => {
            var x = drop.x
            var y = drop.y
            var moduleKey = drop.getDataAsString("text/plain")
            if (moduleKey.length > 0) {
                // 通过 moduleTreeModel 在渲染线程执行 AddModule
                // workspaceController.onDrop 仅更新投放坐标提示
                moduleTreeModel.addModuleByKey(moduleKey)
                workspaceController.onDrop(x, y, moduleKey)
            }
        }
    }

    // Section 节点层（z-order 高于背景 MouseArea，确保节点可接收鼠标事件）
    // 性能优化：拖拽平移时不全量 refresh，而是根据 eqCenter 实时计算屏幕坐标
    // 对应 IBR_Misc.h:136 EqPosToRePos: (EqPos - EqCenter) * Ratio + Center
    // Center = 视口中心 = (width/2, height/2)
    // 节点拖拽中：根据 dragOffset（屏幕坐标偏移）实时更新位置，避免每帧 refresh
    Repeater {
        id: sectionRepeater
        model: workspaceController.sections

        SectionNode {
            id: sectionDelegate
            sectionData: modelData
            // z 高于背景 MouseArea（z=0），确保节点 MouseArea 优先接收事件
            z: 1
            // 性能优化：本地缓存的 EqPos，拖拽结束时通过 sectionPositionChanged 信号更新
            // 避免全量 refresh() 重建 Repeater 导致连续拖拽卡顿
            property real localEqX: modelData.eqX
            property real localEqY: modelData.eqY
            // 根据 EqPos 和当前 EqCenter 实时计算屏幕坐标
            // 拖拽中实时应用 dragOffset，节点跟随鼠标移动
            // 注意：用 isSelected（动态绑定）而非 modelData.selected（快照值）
            //   选中状态变化后 sections 列表未重建，modelData.selected 仍是旧值，
            //   会导致多节点拖拽时不应用 dragOffset（节点不跟随鼠标）
            x: (localEqX - workspaceController.eqCenter.x) * workspaceController.ratio + workspaceView.width / 2
              + ((modelData.sectionId === workspaceController.draggingSectionId
                  || (workspaceController.massDragging && isSelected))
                 ? workspaceController.dragOffset.x : 0)
            y: (localEqY - workspaceController.eqCenter.y) * workspaceController.ratio + workspaceView.height / 2
              + ((modelData.sectionId === workspaceController.draggingSectionId
                  || (workspaceController.massDragging && isSelected))
                 ? workspaceController.dragOffset.y : 0)
            // 尺寸完全由 Qt 内容驱动（不依赖 ImGui 的 EqW/EqH）
            // 宽度：max(WidthFix, widthBase) * ratio（widthBase 已用真实 FontHeight 计算）
            // 高度：header + 行数 * 行高 + margin，随 ratio 等比缩放，内容显示全
            width: implicitWidth
            height: implicitHeight
            // 渲染后回报实际屏幕尺寸到 C++，同步 sd.EqSize（对应 ImGui 实时更新 EqSize）
            // 框选命中检测依赖准确的 EqSize，否则框选位置和命中模块不一致
            onWidthChanged: workspaceController.reportSectionSize(modelData.sectionId, width, height)
            onHeightChanged: workspaceController.reportSectionSize(modelData.sectionId, width, height)
            Component.onCompleted: workspaceController.reportSectionSize(modelData.sectionId, width, height)
            isSelected: {
                // 性能优化：通过 selectedRevision 触发重新评估，不依赖全量 refresh
                workspaceController.selectedRevision
                return workspaceController.isSectionSelected(modelData.sectionId)
            }
            // 性能优化：拖拽结束时只更新被拖拽节点的位置，不重建整个 Repeater
            // 注意：不调用 getSectionData（会触发 buildSectionItem → SectionLineModel::refresh，
            //        重建行模型导致卡顿）。直接读 IBR_SectionMap 的 EqPos 通过专用轻量接口。
            Connections {
                target: workspaceController
                function onSectionPositionChanged(sid) {
                    if (sid === modelData.sectionId) {
                        var pos = workspaceController.getSectionEqPos(modelData.sectionId)
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

    // 右键菜单
    ContextMenu {
        id: contextMenu
    }

    // 连接右键菜单信号
    Connections {
        target: workspaceController
        function onContextMenuRequested(x, y) {
            contextMenu.popup(x, y)
        }
    }

    // 空状态提示
    Text {
        anchors.centerIn: parent
        visible: workspaceController.sections.length === 0 && workspaceController.isProjectOpen
        color: "#5a5a5a"
        font.pixelSize: 14
        text: qsTr("从左侧模块面板拖入模块开始编辑")
    }

    // 未打开项目提示
    Text {
        anchors.centerIn: parent
        visible: !workspaceController.isProjectOpen
        color: "#5a5a5a"
        font.pixelSize: 14
        text: qsTr("请先打开或新建项目")
    }
}
