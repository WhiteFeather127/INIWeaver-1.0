// ViewPanel.qml
// 视图菜单面板，对应 IBR_FullView::RenderUI() (IBR_FullView.cpp:171-192)
// 布局：缩放比例滑块 + 迷你地图（MiniMap 阶段 5.4 实现）
// 项目未打开时显示"请先打开项目"提示（对应 ControlPanel_WaitOpen）
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../workspace"
import "../components"

Item {
    id: root

    // 请求打开独立"视图窗口"（Main.qml miniMapWindow 显示迷你地图浮窗）
    signal openMiniMapWindow()

    // 项目未打开时显示提示（对应 ControlPanel_WaitOpen）
    Text {
        anchors.centerIn: parent
        anchors.verticalCenterOffset: -16
        visible: !projectController.isOpen
        color: "#5a5a5a"
        font.pixelSize: 13
        text: qsTr("请先打开项目")
    }

    // 项目打开后显示视图控件
    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 8
        spacing: 12
        visible: projectController.isOpen

        // 缩放比例滑块（对应 ImGui SliderFloat, RatioMin=25, RatioMax=200）
        // 注：实际 Ratio 读写由阶段 5 WorkspaceController 提供，此处为 UI 占位
        Text {
            text: qsTr("缩放比例")
            color: "#cccccc"
            font.pixelSize: 13
            font.bold: true
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 8

            Slider {
                id: zoomSlider
                Layout.fillWidth: true
                from: 25
                to: 200
                value: Math.round(workspaceController.ratio * 100)
                stepSize: 5
                onMoved: workspaceController.zoomTo(value / 100.0)

                background: Rectangle {
                    x: zoomSlider.leftPadding
                    y: zoomSlider.topPadding + zoomSlider.availableHeight / 2 - height / 2
                    implicitWidth: 200
                    implicitHeight: 4
                    width: zoomSlider.availableWidth
                    height: implicitHeight
                    radius: 2
                    color: "#3c3c3c"

                    Rectangle {
                        width: zoomSlider.visualPosition * parent.width
                        height: parent.height
                        radius: 2
                        color: "#007acc"
                    }
                }

                handle: Rectangle {
                    x: zoomSlider.leftPadding + zoomSlider.visualPosition * (zoomSlider.availableWidth - width)
                    y: zoomSlider.topPadding + zoomSlider.availableHeight / 2 - height / 2
                    implicitWidth: 14
                    implicitHeight: 14
                    radius: 7
                    color: zoomSlider.pressed ? "#ffffff" : "#cccccc"
                    border.width: 1
                    border.color: "#007acc"
                }
            }

            Text {
                text: Math.round(workspaceController.ratio * 100) + "%"
                color: "#cccccc"
                font.pixelSize: 12
                Layout.preferredWidth: 48
                horizontalAlignment: Text.AlignRight
            }
        }

        // 分隔线
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 1
            color: "#1e1e1e"
        }

        // 迷你地图标题 + 视图窗口按钮
        RowLayout {
            Layout.fillWidth: true
            spacing: 8

            Text {
                text: qsTr("迷你地图")
                color: "#cccccc"
                font.pixelSize: 13
                font.bold: true
                Layout.fillWidth: true
            }

            // 打开独立"视图窗口"（可拖动/调整大小的迷你地图浮窗，见 Main.qml miniMapWindow）
            StyledButton {
                text: qsTr("视图窗口")
                implicitWidth: 76
                implicitHeight: 24
                onClicked: root.openMiniMapWindow()
            }
        }

        // 迷你地图（对应 IBR_FullView::DrawView）
        MiniMap {
            id: miniMap
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.minimumHeight: 200
            sections: workspaceController.sections
            // 侧边栏保留画布边缘外框（showWorldBorder 默认 true，仅视图窗口关闭）
            // 画布大小随视口变化：视图框拖到画布外时画布扩展（对齐 ImGui；视图窗口固定不扩）
            includeViewportInWorld: true
            viewRect: {
                // 显式依赖 eqCenter/ratio：viewportEqRect 的 NOTIFY 只有 workspaceRectChanged，
                // 平移（eqCenterChanged）/缩放（ratioChanged）后视图框必须跟随更新，
                // 否则视图框停在旧视野，与画布实际显示不符
                workspaceController.eqCenter
                workspaceController.ratio
                var r = workspaceController.viewportEqRect
                return { x: r.x, y: r.y, w: r.width, h: r.height }
            }
            eqCenter: workspaceController.eqCenter
            // 阶段 13.4：背景拖拽状态（inputState==1 为 BgDragging）
            isBgDragging: workspaceController.inputState === 1
            // 点击迷你地图定位到对应坐标（对应 IBR_FullView::ChangeOffsetPos）
            onClicked: (pos) => {
                workspaceController.centerViewTo(pos.x, pos.y)
            }
        }
    }
}
