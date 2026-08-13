// ImportIniDialog.qml
// 导入 INI 对话框，对应 IBR_ProjectManager::ImportIniAction 触发的 ImGui 弹窗
// 由 DialogController.importRequested(path) 信号触发显示
// 提供文件选择 + 导入按钮，最终调用 projectController.importIni()
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs

Dialog {
    id: root
    modal: true
    anchors.centerIn: parent
    width: 480
    height: 200
    padding: 0
    closePolicy: Dialog.CloseOnEscape | Dialog.CloseOnPressOutside
    title: qsTr("导入 INI")

    // 由 DialogController.importRequested(path) 注入
    property string initialPath: ""

    background: Rectangle {
        color: "#252526"
        border.color: "#3c3c3c"
        border.width: 1
        radius: 4
    }

    header: Rectangle {
        height: 36
        color: "#2d2d2d"
        radius: 4

        Text {
            anchors.left: parent.left
            anchors.leftMargin: 12
            anchors.verticalCenter: parent.verticalCenter
            text: root.title
            color: "#cccccc"
            font.pixelSize: 13
            font.bold: true
        }
    }

    contentItem: ColumnLayout {
        anchors.fill: parent
        anchors.margins: 16
        spacing: 12

        Text {
            Layout.fillWidth: true
            text: qsTr("选择要导入的 INI 文件：")
            color: "#d4d4d4"
            font.pixelSize: 13
            wrapMode: Text.Wrap
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 4

            TextField {
                id: pathField
                Layout.fillWidth: true
                Layout.preferredHeight: 30
                text: root.initialPath
                color: "#d4d4d4"
                font.pixelSize: 12
                placeholderText: qsTr("INI 文件路径...")

                background: Rectangle {
                    color: "#1e1e1e"
                    border.color: pathField.activeFocus ? "#007acc" : "#3c3c3c"
                    border.width: 1
                    radius: 3
                }
            }

            Button {
                Layout.preferredWidth: 72
                Layout.preferredHeight: 30
                text: qsTr("浏览...")
                onClicked: fileDialog.open()

                background: Rectangle {
                    color: parent.hovered ? "#3c3c3c" : "#2d2d2d"
                    border.color: "#3c3c3c"
                    border.width: 1
                    radius: 3
                }
                contentItem: Text {
                    text: parent.text
                    color: "#cccccc"
                    font.pixelSize: 12
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
            }
        }

        Item { Layout.fillHeight: true }

        // 按钮区
        Row {
            Layout.alignment: Qt.AlignRight
            spacing: 12

            Button {
                width: 96; height: 32
                text: qsTr("导入")
                enabled: pathField.text.length > 0
                onClicked: {
                    projectController.importIni(pathField.text)
                    root.close()
                }

                background: Rectangle {
                    color: parent.enabled ? (parent.hovered ? "#007acc" : "#3c3c3c")
                                           : "#252525"
                    border.color: parent.enabled ? "#007acc" : "transparent"
                    border.width: 1
                    radius: 3
                }
                contentItem: Text {
                    text: parent.text
                    color: parent.enabled ? "#ffffff" : "#5a5a5a"
                    font.pixelSize: 13
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
            }

            Button {
                width: 96; height: 32
                text: qsTr("取消")
                onClicked: root.close()

                background: Rectangle {
                    color: parent.hovered ? "#3c3c3c" : "#2d2d2d"
                    border.color: "#3c3c3c"
                    border.width: 1
                    radius: 3
                }
                contentItem: Text {
                    text: parent.text
                    color: "#cccccc"
                    font.pixelSize: 13
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
            }
        }

        FileDialog {
            id: fileDialog
            title: qsTr("选择 INI 文件")
            nameFilters: ["INI 文件 (*.ini)", "所有文件 (*)"]
            onAccepted: {
                var url = fileDialog.currentFile.toString()
                if (url.startsWith("file:///")) {
                    url = decodeURIComponent(url.substring(8))
                }
                pathField.text = url
            }
        }
    }
}
