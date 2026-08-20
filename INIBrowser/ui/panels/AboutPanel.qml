// AboutPanel.qml
// 关于菜单面板，对应 IBR_Panel.cpp ControlPanel_About() (line 234-313)
// 布局：版本信息 + 第三方库 + 致谢 + 链接按钮（文档/BUG反馈/仓库/许可证）
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../components"

ScrollView {
    id: root
    clip: true
    contentWidth: availableWidth

    ColumnLayout {
        width: root.availableWidth
        spacing: 8

        // 版本信息（对应 ImGui TextWrapped GUI_About1，参数1用 AppName）
        Text {
            Layout.fillWidth: true
            Layout.leftMargin: 8; Layout.rightMargin: 8
            Layout.topMargin: 8
            text: (i18n.rev, i18n.trF("GUI_About1",
                  [i18n.tr("AppName"), projectController.versionString]))
            color: "#e0e0e0"; font.pixelSize: 16; font.bold: true
            wrapMode: Text.Wrap
        }

        Text {
            Layout.fillWidth: true
            Layout.leftMargin: 8; Layout.rightMargin: 8
            text: (i18n.rev, i18n.tr("GUI_About_Copyright"))
            color: "#909090"; font.pixelSize: 12
            wrapMode: Text.Wrap
        }

        // 第三方库（对应 ImGui Text "GLFW/Dear ImGui/CJSON/FmtLib"）
        Text {
            Layout.fillWidth: true
            Layout.leftMargin: 8; Layout.rightMargin: 8
            text: "Powered by: Qt6 / GLFW / Dear ImGui / cJSON / FmtLib"
            color: "#909090"; font.pixelSize: 12
            wrapMode: Text.Wrap
        }

        // 分隔线
        Rectangle {
            Layout.fillWidth: true
            Layout.leftMargin: 8; Layout.rightMargin: 8
            Layout.preferredHeight: 1; color: "#1e1e1e"
        }

        // 致谢（对应 ImGui TextWrapped GUI_About5/6/8 + 名单裸串）
        Text {
            Layout.fillWidth: true
            Layout.leftMargin: 8; Layout.rightMargin: 8
            text: (i18n.rev, i18n.tr("GUI_About5"))
            color: "#e0e0e0"; font.pixelSize: 12
            wrapMode: Text.Wrap
        }

        Text {
            Layout.fillWidth: true
            Layout.leftMargin: 16; Layout.rightMargin: 8
            text: "钢铁之锤 / Kenosis / 白羽鸽"
            color: "#909090"; font.pixelSize: 12
            wrapMode: Text.Wrap
        }

        Text {
            Layout.fillWidth: true
            Layout.leftMargin: 8; Layout.rightMargin: 8
            text: (i18n.rev, i18n.tr("GUI_About6"))
            color: "#e0e0e0"; font.pixelSize: 12
            wrapMode: Text.Wrap
        }

        Text {
            Layout.fillWidth: true
            Layout.leftMargin: 16; Layout.rightMargin: 8
            text: "Kenosis"
            color: "#909090"; font.pixelSize: 12
            wrapMode: Text.Wrap
        }

        Text {
            Layout.fillWidth: true
            Layout.leftMargin: 8; Layout.rightMargin: 8
            text: (i18n.rev, i18n.tr("GUI_About8"))
            color: "#e0e0e0"; font.pixelSize: 12
            wrapMode: Text.Wrap
        }

        Text {
            Layout.fillWidth: true
            Layout.leftMargin: 16; Layout.rightMargin: 8
            text: "九千天华"
            color: "#909090"; font.pixelSize: 12
            wrapMode: Text.Wrap
        }

        Text {
            Layout.fillWidth: true
            Layout.leftMargin: 8; Layout.rightMargin: 8
            text: (i18n.rev, i18n.tr("GUI_About2"))
            color: "#909090"; font.pixelSize: 12
            wrapMode: Text.Wrap
        }

        // 分隔线
        Rectangle {
            Layout.fillWidth: true
            Layout.leftMargin: 8; Layout.rightMargin: 8
            Layout.preferredHeight: 1; color: "#1e1e1e"
        }

        // 许可证按钮（对应 ImGui Button GUI_About_License）
        StyledButton {
            Layout.fillWidth: true
            Layout.leftMargin: 8; Layout.rightMargin: 8
            Layout.preferredHeight: 36
            text: (i18n.rev, i18n.tr("GUI_About_License"))
            font.pixelSize: 13
            onClicked: licenseDialog.open()
        }

        // 致谢信息按钮（对应 ImGui Button GUI_About_Acknowledgments）
        StyledButton {
            Layout.fillWidth: true
            Layout.leftMargin: 8; Layout.rightMargin: 8
            Layout.preferredHeight: 36
            text: (i18n.rev, i18n.tr("GUI_About_Acknowledgments"))
            font.pixelSize: 13
            onClicked: ackDialog.open()
        }

        // 分隔线
        Rectangle {
            Layout.fillWidth: true
            Layout.leftMargin: 8; Layout.rightMargin: 8
            Layout.preferredHeight: 1; color: "#1e1e1e"
        }

        // 链接区域（对应 URLOpr lambda）
        // 文档链接
        LinkRow {
            Layout.leftMargin: 8; Layout.rightMargin: 8
            label: (i18n.rev, i18n.tr("GUI_About7"))
            url: "https://inibrowser-02-chinese.readthedocs.io/zh-cn/latest/Info.html"
        }

        // BUG 反馈
        LinkRow {
            Layout.leftMargin: 8; Layout.rightMargin: 8
            label: (i18n.rev, i18n.tr("GUI_About3"))
            url: "https://docs.qq.com/form/page/DWXdKYUFRV1dHSnNE"
        }

        // 仓库链接
        LinkRow {
            Layout.leftMargin: 8; Layout.rightMargin: 8
            label: (i18n.rev, i18n.tr("GUI_About4"))
            url: "https://github.com/ra2diy/INIWeaver-1.0"
        }

        // 许可证链接
        LinkRow {
            Layout.leftMargin: 8; Layout.rightMargin: 8
            label: (i18n.rev, i18n.tr("GUI_About_License"))
            url: "https://www.gnu.org/licenses/old-licenses/lgpl-2.1.html"
        }

        Item { Layout.fillHeight: true; Layout.preferredHeight: 16 }
    }

    // 许可证弹窗（对应 IBR_PopupManager Popup GUI_About_License）
    Dialog {
        id: licenseDialog
        // 显式挂到 Overlay.overlay 再居中：本弹窗声明在 AboutPanel 内，默认 parent 是左侧栏
        // （宽 280），anchors.centerIn: parent 会居中到侧栏而非整个窗口，导致弹窗不居中。
        // parent: Overlay.overlay 保证居中参考物为整个窗口内容区。
        parent: Overlay.overlay
        modal: true
        anchors.centerIn: parent
        width: 520
        // 高度随许可证文本内容自适应（含 36px header + contentItem 上下 12px 边距）
        height: Math.min(520, 36 + licenseColumn.implicitHeight + 24)
        padding: 0
        closePolicy: Dialog.CloseOnEscape | Dialog.CloseOnPressOutside

        background: Rectangle { color: "#252526"; border.color: "#3c3c3c"; border.width: 1; radius: 4 }

        // 统一弹窗风格：自定义 header（标题 + 关闭按钮），不设 anchors.fill 以免遮挡标题区
        header: Rectangle {
            height: 36
            color: "#2d2d2d"
            radius: 4

            Text {
                anchors.left: parent.left
                anchors.leftMargin: 12
                anchors.verticalCenter: parent.verticalCenter
                text: (i18n.rev, i18n.tr("GUI_About_License"))
                color: "#cccccc"
                font.pixelSize: 13
                font.bold: true
            }

            // 关闭按钮
            Rectangle {
                anchors.right: parent.right
                anchors.rightMargin: 8
                anchors.verticalCenter: parent.verticalCenter
                width: 20; height: 20
                radius: 3
                color: licenseCloseMouse.containsMouse ? "#3c3c3c" : "transparent"
                Text {
                    anchors.centerIn: parent
                    text: "×"
                    color: "#cccccc"
                    font.pixelSize: 16
                }
                MouseArea {
                    id: licenseCloseMouse
                    anchors.fill: parent
                    hoverEnabled: true
                    onClicked: licenseDialog.close()
                }
            }
        }

        contentItem: ScrollView {
            clip: true
            Layout.margins: 12
            Column {
                id: licenseColumn
                anchors.margins: 0
                width: licenseDialog.width - 32
                spacing: 8
                // Column 的 implicitHeight 由子项自动累加（含 wrap 换行后的高度），对话框高度据此自适应
                Text {
                    id: text1
                    width: parent.width
                    color: "#e0e0e0"
                    font.pixelSize: 12
                    wrapMode: Text.Wrap
                    text: (i18n.rev, i18n.tr("GUI_About_LicenseInfo_1"))
                }
                Text {
                    id: text2
                    width: parent.width
                    color: "#e0e0e0"
                    font.pixelSize: 12
                    wrapMode: Text.Wrap
                    text: (i18n.rev, i18n.tr("GUI_About_LicenseInfo_2"))
                }
                Text {
                    id: text3
                    width: parent.width
                    color: "#e0e0e0"
                    font.pixelSize: 12
                    wrapMode: Text.Wrap
                    text: (i18n.rev, i18n.tr("GUI_About_LicenseInfo_3"))
                }
                Text {
                    id: text4
                    width: parent.width
                    color: "#e0e0e0"
                    font.pixelSize: 12
                    wrapMode: Text.Wrap
                    text: (i18n.rev, i18n.tr("GUI_About_LicenseInfo_4"))
                }
            }
        }
    }

    // 鸣谢弹窗（对应 IBR_PopupManager Popup GUI_About_Acknowledgments）
    Dialog {
        id: ackDialog
        // 同许可证弹窗：显式挂到整个窗口的 Overlay.overlay 再居中，避免居中到 280px 侧栏
        parent: Overlay.overlay
        modal: true
        anchors.centerIn: parent
        width: 460
        // 高度随鸣谢文本内容自适应（含 36px header + contentItem 上下 12px 边距）
        height: Math.min(520, 36 + ackText.implicitHeight + 24)
        padding: 0
        closePolicy: Dialog.CloseOnEscape | Dialog.CloseOnPressOutside

        background: Rectangle { color: "#252526"; border.color: "#3c3c3c"; border.width: 1; radius: 4 }

        // 统一弹窗风格：自定义 header（标题 + 关闭按钮）
        header: Rectangle {
            height: 36
            color: "#2d2d2d"
            radius: 4

            Text {
                anchors.left: parent.left
                anchors.leftMargin: 12
                anchors.verticalCenter: parent.verticalCenter
                text: (i18n.rev, i18n.tr("GUI_About_Acknowledgments"))
                color: "#cccccc"
                font.pixelSize: 13
                font.bold: true
            }

            // 关闭按钮
            Rectangle {
                anchors.right: parent.right
                anchors.rightMargin: 8
                anchors.verticalCenter: parent.verticalCenter
                width: 20; height: 20
                radius: 3
                color: ackCloseMouse.containsMouse ? "#3c3c3c" : "transparent"
                Text {
                    anchors.centerIn: parent
                    text: "×"
                    color: "#cccccc"
                    font.pixelSize: 16
                }
                MouseArea {
                    id: ackCloseMouse
                    anchors.fill: parent
                    hoverEnabled: true
                    onClicked: ackDialog.close()
                }
            }
        }

        contentItem: ColumnLayout {
            Layout.margins: 12
            Text {
                id: ackText
                Layout.fillWidth: true
                color: "#e0e0e0"
                font.pixelSize: 12
                wrapMode: Text.Wrap
                verticalAlignment: Text.AlignVCenter
                horizontalAlignment: Text.AlignHCenter
                text: (i18n.rev, i18n.tr("GUI_About_AcknowledgmentsInfo"))
            }
        }
    }
}
