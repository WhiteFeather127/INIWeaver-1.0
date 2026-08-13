// ExportDialog.qml
// 导出对话框，对应 IBR_ProjectManager::OutputAction 触发的 ImGui 弹窗
// 由 DialogController.exportRequested 信号触发显示
// 字段：输出目录 + INI 文件名（取自 Project.LastOutputDir / LastOutputIniName）
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs

Dialog {
    id: root
    modal: true
    anchors.centerIn: parent
    width: 520
    height: 240
    padding: 0
    closePolicy: Dialog.CloseOnEscape | Dialog.CloseOnPressOutside
    title: qsTr("导出 INI")

    // 由 Main.qml 通过 open() 调用，可绑定初始值
    property string outputDir: ""
    property string iniName: ""

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

        // 输出目录
        ColumnLayout {
            Layout.fillWidth: true
            spacing: 4

            Text {
                text: qsTr("输出目录")
                color: "#cccccc"
                font.pixelSize: 12
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 4

                TextField {
                    id: dirField
                    Layout.fillWidth: true
                    Layout.preferredHeight: 30
                    text: root.outputDir
                    color: "#d4d4d4"
                    font.pixelSize: 12
                    placeholderText: qsTr("选择输出目录...")

                    background: Rectangle {
                        color: "#1e1e1e"
                        border.color: dirField.activeFocus ? "#007acc" : "#3c3c3c"
                        border.width: 1
                        radius: 3
                    }
                }

                Button {
                    Layout.preferredWidth: 72
                    Layout.preferredHeight: 30
                    text: qsTr("浏览...")
                    onClicked: folderDialog.open()

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
        }

        // INI 文件名
        ColumnLayout {
            Layout.fillWidth: true
            spacing: 4

            Text {
                text: qsTr("INI 文件名")
                color: "#cccccc"
                font.pixelSize: 12
            }

            TextField {
                id: nameField
                Layout.fillWidth: true
                Layout.preferredHeight: 30
                text: root.iniName
                color: "#d4d4d4"
                font.pixelSize: 12
                placeholderText: qsTr("输入 INI 文件名（不含路径）")

                background: Rectangle {
                    color: "#1e1e1e"
                    border.color: nameField.activeFocus ? "#007acc" : "#3c3c3c"
                    border.width: 1
                    radius: 3
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
                text: qsTr("导出")
                enabled: dirField.text.length > 0 && nameField.text.length > 0
                onClicked: {
                    // 构造 iniNames 映射：当前项目所有 INI 都使用用户输入的文件名
                    //（简化处理：单 INI 场景，所有 INI 共享同一文件名）
                    var iniNames = {}
                    iniNames["default"] = nameField.text
                    projectController.exportIni(dirField.text, iniNames)
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

        FolderDialog {
            id: folderDialog
            onAccepted: {
                var url = folderDialog.currentFolder.toString()
                if (url.startsWith("file:///")) {
                    url = decodeURIComponent(url.substring(8))
                }
                dirField.text = url
            }
        }
    }
}
