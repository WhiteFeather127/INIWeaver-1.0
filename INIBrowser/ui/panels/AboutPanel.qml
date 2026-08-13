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

        // 版本信息（对应 ImGui TextWrapped GUI_About1，读取 Version）
        Text {
            Layout.fillWidth: true
            Layout.leftMargin: 8; Layout.rightMargin: 8
            Layout.topMargin: 8
            text: qsTr("INIWeaver 织网者") + " v" + projectController.versionString
            color: "#e0e0e0"; font.pixelSize: 16; font.bold: true
            wrapMode: Text.Wrap
        }

        Text {
            Layout.fillWidth: true
            Layout.leftMargin: 8; Layout.rightMargin: 8
            text: qsTr("Copyright © INIWeaver Contributors")
            color: "#909090"; font.pixelSize: 12
            wrapMode: Text.Wrap
        }

        // 第三方库（对应 ImGui Text "GLFW/Dear ImGui/CJSON/FmtLib"）
        Text {
            Layout.fillWidth: true
            Layout.leftMargin: 8; Layout.rightMargin: 8
            text: qsTr("Powered by: Qt6 / GLFW / Dear ImGui / cJSON / FmtLib")
            color: "#909090"; font.pixelSize: 12
            wrapMode: Text.Wrap
        }

        // 分隔线
        Rectangle {
            Layout.fillWidth: true
            Layout.leftMargin: 8; Layout.rightMargin: 8
            Layout.preferredHeight: 1; color: "#1e1e1e"
        }

        // 致谢（对应 ImGui TextWrapped GUI_About5/6/8）
        Text {
            Layout.fillWidth: true
            Layout.leftMargin: 8; Layout.rightMargin: 8
            text: qsTr("致谢")
            color: "#e0e0e0"; font.pixelSize: 13; font.bold: true
            wrapMode: Text.Wrap
        }

        Text {
            Layout.fillWidth: true
            Layout.leftMargin: 8; Layout.rightMargin: 8
            text: qsTr("钢铁之锤 / Kenosis / 白羽鸽 / 九千天华")
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
            text: qsTr("许可证信息")
            font.pixelSize: 13
            onClicked: licenseDialog.open()
        }

        // 致谢信息按钮（对应 ImGui Button GUI_About_Acknowledgments）
        StyledButton {
            Layout.fillWidth: true
            Layout.leftMargin: 8; Layout.rightMargin: 8
            Layout.preferredHeight: 36
            text: qsTr("鸣谢")
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
        Text {
            Layout.fillWidth: true
            Layout.leftMargin: 8; Layout.rightMargin: 8
            text: qsTr("相关链接")
            color: "#e0e0e0"; font.pixelSize: 13; font.bold: true
            wrapMode: Text.Wrap
        }

        // 文档链接
        LinkRow {
            Layout.leftMargin: 8; Layout.rightMargin: 8
            label: qsTr("在线文档")
            url: "https://inibrowser-02-chinese.readthedocs.io/zh-cn/latest/Info.html"
        }

        // BUG 反馈
        LinkRow {
            Layout.leftMargin: 8; Layout.rightMargin: 8
            label: qsTr("问题反馈")
            url: "https://docs.qq.com/form/page/DWXdKYUFRV1dHSnNE"
        }

        // 仓库链接
        LinkRow {
            Layout.leftMargin: 8; Layout.rightMargin: 8
            label: qsTr("GitHub 仓库")
            url: "https://github.com/ra2diy/INIWeaver-1.0"
        }

        // 许可证链接
        LinkRow {
            Layout.leftMargin: 8; Layout.rightMargin: 8
            label: qsTr("LGPL 许可证")
            url: "https://www.gnu.org/licenses/old-licenses/lgpl-2.1.html"
        }

        Item { Layout.fillHeight: true; Layout.preferredHeight: 16 }
    }

    // 许可证弹窗（对应 IBR_PopupManager Popup GUI_About_License）
    Dialog {
        id: licenseDialog
        title: qsTr("许可证信息")
        modal: true
        anchors.centerIn: parent
        width: 480; height: 400

        background: Rectangle { color: "#252526"; border.color: "#3c3c3c"; border.width: 1; radius: 4 }

        contentItem: ScrollView {
            clip: true
            Text {
                width: licenseDialog.width - 32
                color: "#e0e0e0"
                font.pixelSize: 12
                wrapMode: Text.Wrap
                text: qsTr("INIWeaver 基于 LGPL-2.1 许可证发布。\n\n源代码仓库：https://github.com/ra2diy/INIWeaver-1.0\n许可证全文：https://www.gnu.org/licenses/old-licenses/lgpl-2.1.html\n\n本程序使用以下第三方库：\n- Qt6 (LGPLv3)\n- GLFW (zlib License)\n- Dear ImGui (MIT License)\n- cJSON (MIT License)\n- fmtlib (MIT License)")
            }
        }

        standardButtons: Dialog.Close
    }

    // 鸣谢弹窗（对应 IBR_PopupManager Popup GUI_About_Acknowledgments）
    Dialog {
        id: ackDialog
        title: qsTr("鸣谢")
        modal: true
        anchors.centerIn: parent
        width: 400; height: 300

        background: Rectangle { color: "#252526"; border.color: "#3c3c3c"; border.width: 1; radius: 4 }

        contentItem: Text {
            color: "#e0e0e0"
            font.pixelSize: 12
            wrapMode: Text.Wrap
            text: qsTr("感谢所有为 INIWeaver 项目做出贡献的开发者、测试者和翻译者。\n\n特别感谢：\n钢铁之锤 / Kenosis / 白羽鸽 / 九千天华\n\n感谢社区提供的宝贵反馈与支持。")
        }

        standardButtons: Dialog.Close
    }
}
