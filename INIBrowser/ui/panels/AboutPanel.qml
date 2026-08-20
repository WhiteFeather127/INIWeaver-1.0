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
        title: (i18n.rev, i18n.tr("GUI_About_License"))
        modal: true
        anchors.centerIn: parent
        width: 480; height: 400

        background: Rectangle { color: "#252526"; border.color: "#3c3c3c"; border.width: 1; radius: 4 }

        contentItem: ScrollView {
            clip: true
            Column {
                anchors.margins: 0
                width: licenseDialog.width - 32
                spacing: 8
                Text {
                    width: parent.width
                    color: "#e0e0e0"
                    font.pixelSize: 12
                    wrapMode: Text.Wrap
                    text: (i18n.rev, i18n.tr("GUI_About_LicenseInfo_1"))
                }
                Text {
                    width: parent.width
                    color: "#e0e0e0"
                    font.pixelSize: 12
                    wrapMode: Text.Wrap
                    text: (i18n.rev, i18n.tr("GUI_About_LicenseInfo_2"))
                }
                Text {
                    width: parent.width
                    color: "#e0e0e0"
                    font.pixelSize: 12
                    wrapMode: Text.Wrap
                    text: (i18n.rev, i18n.tr("GUI_About_LicenseInfo_3"))
                }
                Text {
                    width: parent.width
                    color: "#e0e0e0"
                    font.pixelSize: 12
                    wrapMode: Text.Wrap
                    text: (i18n.rev, i18n.tr("GUI_About_LicenseInfo_4"))
                }
            }
        }

        standardButtons: Dialog.Close
    }

    // 鸣谢弹窗（对应 IBR_PopupManager Popup GUI_About_Acknowledgments）
    Dialog {
        id: ackDialog
        title: (i18n.rev, i18n.tr("GUI_About_Acknowledgments"))
        modal: true
        anchors.centerIn: parent
        width: 400; height: 300

        background: Rectangle { color: "#252526"; border.color: "#3c3c3c"; border.width: 1; radius: 4 }

        contentItem: Text {
            color: "#e0e0e0"
            font.pixelSize: 12
            wrapMode: Text.Wrap
            text: (i18n.rev, i18n.tr("GUI_About_AcknowledgmentsInfo"))
        }

        standardButtons: Dialog.Close
    }
}
