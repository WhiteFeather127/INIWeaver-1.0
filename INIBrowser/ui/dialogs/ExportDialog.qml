// ExportDialog.qml
// 导出对话框，对应 ImGui 版 IBR_ProjectManager::OutputAction（IBR_ProjManager.cpp:672-816）：
// 输出目录 + 按 INI 类型逐行设置文件名 + 目录错误提示 + 文件已存在警告
// 打开前由 Main.qml 调用 loadData() 从 ProjectController.exportDialogData() 拉取数据
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs

Dialog {
    id: root
    modal: true
    anchors.centerIn: parent
    width: 560
    height: Math.min(620, 250 + iniRows.length * 44)
    padding: 0
    title: qsTr("导出")

    // 数据行 [{name: INI 类型名, fileName: 默认文件名}]，由 loadData() 拉取
    property var iniRows: []

    // 打开前调用：从业务层拉取导出数据快照
    function loadData()
    {
        var data = projectController.exportDialogData()
        dirField.text = data.dir
        iniRows = data.inis
    }

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
        spacing: 8

        // 上部固定区：目录选择 + 错误提示
        ColumnLayout {
            Layout.fillWidth: true
            Layout.margins: 16
            spacing: 8

            RowLayout {
                Layout.fillWidth: true
                spacing: 4

                Text {
                    text: qsTr("输出目录")
                    color: "#d4d4d4"
                    font.pixelSize: 12
                }

                TextField {
                    id: dirField
                    Layout.fillWidth: true
                    Layout.preferredHeight: 30
                    color: "#d4d4d4"
                    font.pixelSize: 12
                    placeholderText: qsTr("选择或输入输出目录...")

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

            // 目录错误提示（对应 ImGui 版 GUI_Output_Error1/Error2）
            Text {
                Layout.fillWidth: true
                visible: dirField.text.length === 0
                         || !projectController.dirExists(dirField.text)
                text: dirField.text.length === 0
                      ? qsTr("路径为空")
                      : qsTr("目录不存在")
                color: "#f48771"
                font.pixelSize: 11
                wrapMode: Text.Wrap
            }
        }

        // 中部滚动区：按 INI 类型逐行设置文件名（对应 ImGui 版 Ignore=false 的行）
        Flickable {
            id: iniFlick
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.leftMargin: 16
            Layout.rightMargin: 16
            contentHeight: iniColumn.implicitHeight
            clip: true
            boundsBehavior: Flickable.StopAtBounds

            ColumnLayout {
                id: iniColumn
                width: iniFlick.width
                spacing: 8

                Repeater {
                    id: iniRepeater
                    model: root.iniRows

                    delegate: RowLayout {
                        id: iniRow
                        Layout.fillWidth: true
                        spacing: 4

                        // 供导出按钮采集用户输入的文件名
                        function fileNameText() { return nameField.text }

                        Text {
                            text: modelData.name
                            color: "#cccccc"
                            font.pixelSize: 12
                            Layout.preferredWidth: 110
                            elide: Text.ElideRight
                        }

                        TextField {
                            id: nameField
                            Layout.fillWidth: true
                            Layout.preferredHeight: 28
                            text: modelData.fileName
                            color: "#d4d4d4"
                            font.pixelSize: 12

                            background: Rectangle {
                                color: "#1e1e1e"
                                border.color: nameField.activeFocus ? "#007acc" : "#3c3c3c"
                                border.width: 1
                                radius: 3
                            }
                        }

                        // 文件已存在警告（对应 ImGui 版 GUI_Output_Warning1，
                        // 绑定目录/文件名变化自动刷新，等价 ImGui 每帧刷新）
                        Text {
                            visible: dirField.text.length > 0
                                     && nameField.text.length > 0
                                     && projectController.fileExists(dirField.text + "/" + nameField.text)
                            text: qsTr("文件已存在")
                            color: "#d7ba7d"
                            font.pixelSize: 11
                        }
                    }
                }

                // 无可导出 INI 时的提示
                Text {
                    Layout.fillWidth: true
                    visible: root.iniRows.length === 0
                    text: qsTr("当前项目没有可导出的 INI")
                    color: "#888888"
                    font.pixelSize: 12
                    horizontalAlignment: Text.AlignHCenter
                }
            }
        }

        // 底部按钮区
        Row {
            Layout.fillWidth: true
            Layout.margins: 16
            Layout.alignment: Qt.AlignRight
            spacing: 12

            Button {
                width: 96; height: 32
                text: qsTr("导出")
                // 目录有效即可导出（对应 ImGui 版 OK 启用条件：WP 非空且 IsExistingDir）
                enabled: dirField.text.length > 0
                         && projectController.dirExists(dirField.text)
                onClicked: {
                    // 构造 {INI 类型名: 文件名} 映射（对应 ImGui 版 OK 按钮：
                    // 只采集显示行的 Buf；隐藏行由 AutoOutputAction 用默认名）
                    var iniNames = {}
                    for (var i = 0; i < iniRepeater.count; i++)
                    {
                        var row = iniRepeater.itemAt(i)
                        iniNames[root.iniRows[i].name] = row.fileNameText()
                    }
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
                var url = folderDialog.selectedFolder.toString()
                if (url.startsWith("file:///")) {
                    url = decodeURIComponent(url.substring(8))
                }
                dirField.text = url
            }
        }
    }
}
