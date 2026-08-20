// MenuBar.qml
// 顶部菜单栏：8 个菜单按钮，禁用规则对齐 ImGui 版本（IBR_Panel.cpp:210-225）
// 菜单 ID: File=0 Modules=1 View=2 List=3 Edit=4 Setting=5 About=6 Debug=7
import QtQuick
import QtQuick.Controls

Rectangle {
    id: root
    color: "#2d2d2d"
    implicitHeight: 32

    // 帧率（由 Main.qml 绑定，对应 ImGui::GetIO().Framerate）
    property real fpsValue: 0

    // 禁用规则对照 ImGui：
    //   File/Modules/Setting/About：始终启用
    //   View/List：仅项目打开时启用 (IBR_ProjectManager::IsOpen)
    //   Edit：永远禁用 (IBR_Panel.cpp:219)
    //   Debug：仅 -debugmenu 启动时启用 (EnableDebugList)
    Row {
        anchors.fill: parent
        anchors.leftMargin: 8
        anchors.rightMargin: 8
        spacing: 2

        Repeater {
            // model 只存静态数据，enabled 在 delegate 中动态绑定
            model: [
                { menuId: 0, text: (i18n.rev, i18n.tr("GUI_MenuItem_File")) },
                { menuId: 1, text: (i18n.rev, i18n.tr("GUI_MenuItem_Modules")) },
                { menuId: 2, text: (i18n.rev, i18n.tr("GUI_MenuItem_View")) },
                { menuId: 3, text: (i18n.rev, i18n.tr("GUI_MenuItem_List")) },
                { menuId: 4, text: (i18n.rev, i18n.tr("GUI_MenuItem_Edit")) },
                { menuId: 5, text: (i18n.rev, i18n.tr("GUI_MenuItem_Setting")) },
                { menuId: 6, text: (i18n.rev, i18n.tr("GUI_MenuItem_About")) },
                { menuId: 7, text: (i18n.rev, i18n.tr("GUI_MenuItem_Debug")) }
            ]

            delegate: Button {
                id: btn
                width: 56
                height: parent.height
                text: modelData.text
                // 禁用规则动态绑定，对齐 ImGui 版本（IBR_Panel.cpp:210-225）：
                //   File/Modules/Setting/About：始终启用
                //   View/List：仅项目打开时启用
                //   Edit：永远禁用（IBR_Panel.cpp:219）
                //   Debug：仅 -debugmenu 启动时启用
                enabled: {
                    switch (modelData.menuId) {
                        case 0: case 1: case 5: case 6: return true
                        case 2: case 3: return projectController.isOpen
                        case 4: return menuController.canEdit
                        case 7: return menuController.debugMenuEnabled
                        default: return false
                    }
                }
                flat: true
                checkable: true
                checked: menuController.activeMenu === modelData.menuId
                onClicked: {
                    menuController.activeMenu = modelData.menuId
                }

                // 快捷键 ToolTip（对应 ImGui 菜单按钮的 Alt+X 快捷键提示）
                ToolTip.text: {
                    switch (modelData.menuId) {
                        case 0: return (i18n.rev, i18n.tr("GUI_MenuItem_File") + " (Alt+F)")
                        case 1: return (i18n.rev, i18n.tr("GUI_MenuItem_Modules") + " (Alt+M)")
                        case 2: return (i18n.rev, i18n.tr("GUI_MenuItem_View") + " (Alt+V)")
                        case 3: return (i18n.rev, i18n.tr("GUI_MenuItem_List") + " (Alt+L)")
                        case 4: return (i18n.rev, i18n.tr("GUI_MenuItem_Edit") + " (Alt+E)")
                        case 5: return (i18n.rev, i18n.tr("GUI_MenuItem_Setting") + " (Alt+S)")
                        case 6: return (i18n.rev, i18n.tr("GUI_MenuItem_About") + " (Alt+A)")
                        case 7: return (i18n.rev, i18n.tr("GUI_MenuItem_Debug") + " (Alt+D)")
                        default: return ""
                    }
                }
                ToolTip.visible: hovered
                ToolTip.delay: 500

                background: Rectangle {
                    color: btn.checked ? "#007acc"
                         : btn.hovered ? "#3c3c3c"
                         : "transparent"
                    radius: 3
                }

                contentItem: Text {
                    text: btn.text
                    color: btn.checked ? "#ffffff"
                         : btn.enabled ? "#cccccc"
                         : "#5a5a5a"
                    font.pixelSize: 13
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
            }
        }
    }

    // 右侧标题信息（对应 MainStage.h:165-167 GUI_TopRightHint）
    // 格式：AppName V版本 平均FPS xx.x（对应 language.ini:197 GUI_TopRightHint=%s V%s 平均FPS %.1f）
    Text {
        anchors.right: parent.right
        anchors.rightMargin: 12
        anchors.verticalCenter: parent.verticalCenter
        text: (i18n.rev, i18n.trF("GUI_TopRightHint",
              [projectController.appName, projectController.versionString, root.fpsValue.toFixed(1)]))
        color: "#858585"
        font.pixelSize: 12
    }
}
