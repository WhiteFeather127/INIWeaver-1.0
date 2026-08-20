// ExportModuleDialog.qml
// 导出模块对话框，对应 ImGui GUI_ExportModule → OutputSelected（IBR_WorkSpace.cpp:746-836）
// 由 workspaceController.outputModuleRequested 信号触发显示（Main.qml 连接）
// 字段：模块名称 + 描述 + 输出路径（.ini），确认后调用 workspaceController.outputSelectedModule
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs

Dialog {
    id: root
    modal: true
    anchors.centerIn: parent
    width: 520
    height: 300
    padding: 0
    closePolicy: Dialog.CloseOnEscape | Dialog.CloseOnPressOutside
    title: (i18n.rev, i18n.tr("GUI_OutputModule_Title"))

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
        // 不设 anchors.fill：填满会连同 header(36px) 一起覆盖，导致顶部「模块」标签被标题栏遮住。
        // 交给 Dialog 自动在 header 下方布局，margin 用 Layout.margins 控制（对齐 ExportDialog 写法）
        Layout.margins: 16
        spacing: 12

        // 模块名称（对应 GUI_OutputModule_Name，必填）
        ColumnLayout {
            Layout.fillWidth: true
            spacing: 4

            Text {
                text: (i18n.rev, i18n.tr("GUI_OutputModule_Name"))
                color: "#cccccc"
                font.pixelSize: 12
            }

            TextField {
                id: nameField
                Layout.fillWidth: true
                Layout.preferredHeight: 30
                color: "#d4d4d4"
                font.pixelSize: 12
                background: Rectangle {
                    color: "#1e1e1e"
                    border.color: nameField.activeFocus ? "#007acc" : "#3c3c3c"
                    border.width: 1
                    radius: 3
                }
            }
        }

        // 描述（对应 GUI_OutputModule_Desc）
        ColumnLayout {
            Layout.fillWidth: true
            spacing: 4

            Text {
                text: (i18n.rev, i18n.tr("GUI_OutputModule_Desc"))
                color: "#cccccc"
                font.pixelSize: 12
            }

            TextField {
                id: descField
                Layout.fillWidth: true
                Layout.preferredHeight: 30
                color: "#d4d4d4"
                font.pixelSize: 12
                background: Rectangle {
                    color: "#1e1e1e"
                    border.color: descField.activeFocus ? "#007acc" : "#3c3c3c"
                    border.width: 1
                    radius: 3
                }
            }
        }

        // 输出路径（对应 GUI_OutputModule_OutputPath，带浏览按钮）
        ColumnLayout {
            Layout.fillWidth: true
            spacing: 4

            Text {
                text: (i18n.rev, i18n.tr("GUI_OutputModule_OutputPath"))
                color: "#cccccc"
                font.pixelSize: 12
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 4

                TextField {
                    id: pathField
                    Layout.fillWidth: true
                    Layout.preferredHeight: 30
                    color: "#d4d4d4"
                    font.pixelSize: 12
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
                    text: (i18n.rev, i18n.tr("GUI_Browse") + "...")
                    onClicked: saveFileDialog.open()

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

        // 校验提示（对应 GUI_OutputModule_Error1：名称必填）
        Text {
            Layout.fillWidth: true
            text: (i18n.rev, i18n.tr("GUI_OutputModule_Error1"))
            color: "#858585"
            font.pixelSize: 11
            visible: true
        }

        Item { Layout.fillHeight: true }

        // 按钮区
        Row {
            Layout.alignment: Qt.AlignRight
            spacing: 12

            Button {
                width: 96; height: 32
                text: (i18n.rev, i18n.tr("GUI_OK"))
                enabled: nameField.text.trim().length > 0 && pathField.text.trim().length > 0
                onClicked: {
                    if (workspaceController.outputSelectedModule(
                            nameField.text.trim(), descField.text.trim(), pathField.text.trim())) {
                        root.close()
                    }
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
                text: (i18n.rev, i18n.tr("GUI_Cancel"))
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

        // 保存文件对话框（对应 ImGui SelectFileName 保存窗口）
        FileDialog {
            id: saveFileDialog
            title: (i18n.rev, i18n.tr("GUI_OutputModule_Title"))
            fileMode: FileDialog.SaveFile
            nameFilters: [(i18n.rev, i18n.tr("GUI_OutputModule_Type1")), (i18n.rev, i18n.tr("GUI_OutputModule_Type2"))]
            selectedFile: pathField.text.length > 0 ? pathField.text : workspaceController.defaultModuleExportPath()

            onAccepted: {
                var url = saveFileDialog.selectedFile.toString()
                if (url.startsWith("file:///")) {
                    url = decodeURIComponent(url.substring(8))
                }
                pathField.text = url
            }
        }
    }

    // 打开时用默认路径初始化（对应 ImGui GenerateModulePath），并聚焦名称输入框
    onOpened: {
        pathField.text = workspaceController.defaultModuleExportPath()
        nameField.forceActiveFocus()
    }
}
