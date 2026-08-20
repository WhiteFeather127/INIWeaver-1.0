// ShpTypeDialog.qml
// 阶段 13.3.2：SHP 文件类型选择弹窗，对应 ImGui IBR_ProjManager.cpp:1129-1217
// 每个文件显示文件名 + 4 个 RadioButton（动画/建筑/步兵/车辆）
// 由 DialogController.shpTypeSelectionRequested 信号触发
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Dialog {
    id: root
    modal: true
    anchors.centerIn: parent
    width: 520
    height: Math.min(400, 80 + shpListView.count * 56)
    padding: 0
    closePolicy: Dialog.NoAutoClose  // 必须点确定/取消按钮
    title: (i18n.rev, i18n.tr("GUI_LoadFile"))

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

        // 提示文本（对应 ImGui locc("GUI_SHPToLoad")）
        Text {
            text: (i18n.rev, i18n.tr("GUI_SHPToLoad"))
            color: "#d4d4d4"
            font.pixelSize: 13
            Layout.fillWidth: true
        }

        // 文件列表（每个文件一行：文件名 + 4 个 RadioButton）
        ScrollView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true

            ListView {
                id: shpListView
                model: shpNamesModel
                interactive: false

                ListModel { id: shpNamesModel }

                delegate: Rectangle {
                    width: shpListView.width
                    height: 48
                    color: "transparent"

                    Text {
                        id: fileNameText
                        anchors.left: parent.left
                        anchors.verticalCenter: parent.verticalCenter
                        width: 120
                        text: model.name
                        color: "#cccccc"
                        font.pixelSize: 12
                        elide: Text.ElideRight
                    }

                    // 4 个 RadioButton（对应 ImGui RadioButton GUI_LoadImage_Anim/Building/Infantry/Vehicle）
                    // type: 0=Animation, 1=Building, 2=Infantry, 3=Vehicle
                    Row {
                        anchors.left: fileNameText.right
                        anchors.leftMargin: 8
                        anchors.verticalCenter: parent.verticalCenter
                        spacing: 8

                        RadioButton {
                            text: (i18n.rev, i18n.tr("GUI_LoadImage_Anim"))
                            checked: model.type === 0
                            onToggled: model.type = 0
                        }
                        RadioButton {
                            text: (i18n.rev, i18n.tr("GUI_LoadImage_Building"))
                            checked: model.type === 1
                            onToggled: model.type = 1
                        }
                        RadioButton {
                            text: (i18n.rev, i18n.tr("GUI_LoadImage_Infantry"))
                            checked: model.type === 2
                            onToggled: model.type = 2
                        }
                        RadioButton {
                            text: (i18n.rev, i18n.tr("GUI_LoadImage_Vehicle"))
                            checked: model.type === 3
                            onToggled: model.type = 3
                        }
                    }

                    // 分隔线
                    Rectangle {
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.bottom: parent.bottom
                        height: 1
                        color: "#1e1e1e"
                    }
                }
            }
        }

        // 确定/取消按钮（对应 ImGui locc("GUI_OK") / locc("GUI_Cancel")）
        Row {
            Layout.alignment: Qt.AlignHCenter
            spacing: 12

            Button {
                width: 96; height: 32
                text: (i18n.rev, i18n.tr("GUI_OK"))
                background: Rectangle {
                    color: parent.hovered ? "#007acc" : "#3c3c3c"
                    border.color: "#007acc"; border.width: 1; radius: 3
                }
                contentItem: Text {
                    text: parent.text; color: "#ffffff"; font.pixelSize: 13
                    horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter
                }
                onClicked: {
                    // 收集每个文件的类型，调用 confirmShpTypeSelection
                    var types = []
                    for (var i = 0; i < shpNamesModel.count; ++i) {
                        types.push(shpNamesModel.get(i).type)
                    }
                    dialogController.confirmShpTypeSelection(types)
                    root.close()
                }
            }

            Button {
                width: 96; height: 32
                text: (i18n.rev, i18n.tr("GUI_Cancel"))
                background: Rectangle {
                    color: parent.hovered ? "#3c3c3c" : "#2d2d2d"
                    border.color: "#3c3c3c"; border.width: 1; radius: 3
                }
                contentItem: Text {
                    text: parent.text; color: "#cccccc"; font.pixelSize: 13
                    horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter
                }
                onClicked: {
                    dialogController.cancelShpTypeSelection()
                    root.close()
                }
            }
        }
    }

    // 监听 shpTypeSelectionRequested 信号，打开弹窗并加载数据
    Connections {
        target: dialogController
        function onShpTypeSelectionRequested() {
            shpNamesModel.clear()
            var names = dialogController.shpFileNames()
            for (var i = 0; i < names.length; ++i) {
                // 默认 type=0（Animation），对应 ImGui SHPSolution.Type 初始值 0
                shpNamesModel.append({ name: names[i], type: 0 })
            }
            root.open()
        }
    }
}
