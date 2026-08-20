// EditPanel.qml
// 编辑菜单面板，对应 IBR_Panel.cpp ControlPanel_Edit() (line 147-163)
// 完整移植 IBR_EditFrame 命名空间（IBR_Misc.cpp:587-940）：
// - 顶部：切换文本编辑按钮 + 粘贴刷新注册名复选框
// - 新增行：Key = Value + "＋"按钮
// - 键值列表：OnShow开关 + Key名 + 值编辑 + 描述编辑 + 删除行
// - 文本编辑模式：多行文本编辑 + 保存/不保存退出
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../components"

Item {
    id: editPanel

    // 主容器
    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 4
        visible: !editPanelController.isEmpty
        spacing: 4

        // ===== 文本编辑模式 =====
        // 对应 IBR_EditFrame::RenderUI_TextEdit（IBR_Misc.cpp:795-822）
        ColumnLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            visible: editPanelController.onTextEdit
            spacing: 4

            RowLayout {
                Layout.fillWidth: true
                spacing: 4

                Text {
                    text: (i18n.rev, i18n.tr("GUI_TextEditModeTitle"))
                    color: "#cccccc"
                    font.pixelSize: 13
                }
                Item { Layout.fillWidth: true }
                Button {
                    text: (i18n.rev, i18n.tr("GUI_ExitAndSave"))
                    onClicked: editPanelController.exitTextEdit(true)
                    background: Rectangle {
                        color: parent.hovered ? "#3c3c3c" : "#2d2d2d"
                        border.color: "#3c3c3c"; border.width: 1; radius: 3
                    }
                    contentItem: Text {
                        text: parent.text; color: "#cccccc"; font.pixelSize: 13
                        horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter
                    }
                }
                Button {
                    text: (i18n.rev, i18n.tr("GUI_ExitNoSave"))
                    onClicked: editPanelController.exitTextEdit(false)
                    background: Rectangle {
                        color: parent.hovered ? "#3c3c3c" : "#2d2d2d"
                        border.color: "#3c3c3c"; border.width: 1; radius: 3
                    }
                    contentItem: Text {
                        text: parent.text; color: "#cccccc"; font.pixelSize: 13
                        horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter
                    }
                }
            }

            ScrollView {
                Layout.fillWidth: true
                Layout.fillHeight: true
                TextArea {
                    text: editPanelController.textEditContent
                    color: "#e0e0e0"
                    placeholderTextColor: "#909090"
                    font.family: "Consolas, Microsoft YaHei"
                    font.pixelSize: 13
                    background: Rectangle { color: "#1e1e1e"; border.color: "#3c3c3c"; border.width: 1 }
                    onTextChanged: editPanelController.textEditContent = text
                    selectByMouse: true
                    wrapMode: TextArea.NoWrap
                }
            }
        }

        // ===== 正常编辑模式 =====
        // 对应 IBR_EditFrame::RenderUI（IBR_Misc.cpp:900-933）
        ColumnLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            visible: !editPanelController.onTextEdit
            spacing: 4

            // 模块名标题
            Text {
                Layout.fillWidth: true
                text: editPanelController.displayName
                color: "#cccccc"
                font.pixelSize: 14
                font.bold: true
                elide: Text.ElideRight
            }

            // 顶部工具栏：切换文本编辑 + 粘贴刷新注册名
            // 对应 RenderUI_SwitchToText + RenderUI_UseOwnName
            RowLayout {
                Layout.fillWidth: true
                spacing: 4

                Button {
                    text: (i18n.rev, i18n.tr("GUI_SwitchToTextEdit"))
                    onClicked: editPanelController.switchToText()
                    background: Rectangle {
                        color: parent.hovered ? "#3c3c3c" : "#2d2d2d"
                        border.color: "#3c3c3c"; border.width: 1; radius: 3
                    }
                    contentItem: Text {
                        text: parent.text; color: "#cccccc"; font.pixelSize: 13
                        horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter
                    }
                }
                Item { Layout.fillWidth: true }
                CheckBox {
                    text: (i18n.rev, i18n.tr("GUI_RefreshRegisterOnPaste"))
                    checked: editPanelController.needtoMangle
                    onToggled: editPanelController.toggleUseOwnName()
                    contentItem: Text {
                        text: parent.text
                        color: "#cccccc"
                        font.pixelSize: 13
                        verticalAlignment: Text.AlignVCenter
                        leftPadding: parent.indicator.width + parent.spacing
                    }
                }
            }

            // 新增行区域（对应 RenderUI_NewLine，IBR_Misc.cpp:687-744）
            RowLayout {
                Layout.fillWidth: true
                spacing: 4

                TextField {
                    id: newKeyField
                    Layout.fillWidth: true
                    placeholderText: qsTr("Key")
                    color: "#e0e0e0"
                    placeholderTextColor: "#909090"
                    font.pixelSize: 13
                    background: Rectangle { color: "#2d2d2d"; border.color: newKeyField.activeFocus ? "#007acc" : "#3c3c3c"; border.width: 1; radius: 2 }
                }
                Text { text: "="; color: "#cccccc"; font.pixelSize: 13 }
                TextField {
                    id: newValueField
                    Layout.fillWidth: true
                    placeholderText: qsTr("Value")
                    color: "#e0e0e0"
                    placeholderTextColor: "#909090"
                    font.pixelSize: 13
                    background: Rectangle { color: "#2d2d2d"; border.color: newValueField.activeFocus ? "#007acc" : "#3c3c3c"; border.width: 1; radius: 2 }
                    onAccepted: addNewLine()
                }
                Button {
                    text: "＋"
                    onClicked: addNewLine()
                    background: Rectangle {
                        color: parent.hovered ? "#3c3c3c" : "#2d2d2d"
                        border.color: "#3c3c3c"; border.width: 1; radius: 3
                    }
                    contentItem: Text {
                        text: parent.text; color: "#cccccc"; font.pixelSize: 13
                        horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter
                    }
                }
            }

            // 初始值提示（对应 RenderUI_NewLine 的 TextDisabled，IBR_Misc.cpp:737-748）
            // 当 Value 为空且 Key 非空时，显示该 Key 的默认初始值
            Text {
                Layout.fillWidth: true
                visible: newValueField.text.length === 0 && newKeyField.text.length > 0
                text: {
                    var v = editPanelController.getInitialValue(newKeyField.text)
                    return v.length > 0 ? (i18n.rev, i18n.trF("GUI_UseInitialValue", [v])) : ""
                }
                color: "#909090"
                font.pixelSize: 12
                elide: Text.ElideRight
            }

            // 分隔线
            Rectangle {
                Layout.fillWidth: true
                height: 1
                color: "#3c3c3c"
            }

            // 键值列表（对应 RenderUI_Lines，IBR_Misc.cpp:872-898）
            ScrollView {
                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true

                ListView {
                    id: linesListView
                    model: editPanelController.editLines
                    spacing: 2

                    delegate: Rectangle {
                        width: linesListView.width
                        height: lineColumn.implicitHeight + 8
                        color: "transparent"

                        property bool showDescEdit: false

                        ColumnLayout {
                            id: lineColumn
                            anchors.fill: parent
                            anchors.margins: 2
                            spacing: 2

                            // 主行：OnShow开关 + Key名 + 值编辑
                            RowLayout {
                                Layout.fillWidth: true
                                spacing: 4

                                // OnShow 开关（对应 RenderUI_OnShow 的 RadioButton）
                                // 注意：Qt 的 RadioButton 选中后无法点击取消，且 onToggled+onClicked
                                // 会双次触发 toggleOnShow 导致状态抵消，改用 CheckBox 支持双向切换
                                StyledCheckBox {
                                    checked: modelData.onShow || false
                                    onToggled: editPanelController.toggleOnShow(modelData.keyName)
                                    rightPadding: 0
                                }

                                // Key 名
                                Text {
                                    Layout.preferredWidth: 80
                                    text: modelData.keyName
                                    color: modelData.missing ? "#f48771" : "#e0e0e0"
                                    font.pixelSize: 13
                                    elide: Text.ElideRight
                                    ToolTip.text: modelData.hint || ""
                                    ToolTip.visible: hoverHandler.hovered && (modelData.hint || "").length > 0
                                    ToolTip.delay: 500
                                    HoverHandler {
                                        id: hoverHandler
                                    }
                                    MouseArea {
                                        anchors.fill: parent
                                        acceptedButtons: Qt.RightButton
                                        onClicked: showDescEdit = !showDescEdit
                                    }
                                }

                                // 缺失行数据红色提示（对应 TextColored IllegalLineColor，IBR_Misc.cpp:903）
                                Text {
                                    Layout.fillWidth: true
                                    visible: modelData.missing || false
                                    text: (i18n.rev, i18n.tr("GUI_MissingLineData"))
                                    color: "#f48771"
                                    font.pixelSize: 13
                                }

                                // 值编辑控件（缺失行不显示）
                                TextField {
                                    Layout.fillWidth: true
                                    visible: !(modelData.missing || false)
                                    text: modelData.value || ""
                                    color: "#e0e0e0"
                                    placeholderTextColor: "#909090"
                                    font.pixelSize: 13
                                    background: Rectangle {
                                        color: "#2d2d2d"
                                        border.color: parent.activeFocus ? "#007acc" : "#3c3c3c"
                                        border.width: 1
                                        radius: 2
                                    }
                                    onEditingFinished: editPanelController.setLineValue(modelData.keyName, text)
                                    ToolTip.text: modelData.hint || ""
                                    ToolTip.visible: hovered && (modelData.hint || "").length > 0
                                    ToolTip.delay: 500
                                }
                            }

                            // OnShow 描述编辑区域（右键 Key 名切换显示）
                            // 对应 RenderUI_OnShow 的 InputOnShow 分支（IBR_Misc.cpp:842-869）
                            RowLayout {
                                Layout.fillWidth: true
                                visible: showDescEdit
                                spacing: 4

                                TextField {
                                    Layout.fillWidth: true
                                    text: modelData.onShowDesc || ""
                                    placeholderText: (i18n.rev, i18n.tr("GUI_EditDesc"))
                                    color: "#e0e0e0"
                                    placeholderTextColor: "#909090"
                                    font.pixelSize: 12
                                    background: Rectangle {
                                        color: "#2d2d2d"
                                        border.color: parent.activeFocus ? "#007acc" : "#3c3c3c"
                                        border.width: 1
                                        radius: 2
                                    }
                                    onEditingFinished: editPanelController.setOnShowDesc(modelData.keyName, text)
                                }
                                Button {
                                    text: (i18n.rev, i18n.tr("GUI_RemoveLine"))
                                    onClicked: editPanelController.removeLine(modelData.keyName)
                                    background: Rectangle {
                                        color: parent.hovered ? "#3c3c3c" : "#2d2d2d"
                                        border.color: "#3c3c3c"; border.width: 1; radius: 3
                                    }
                                    contentItem: Text {
                                        text: parent.text; color: "#cccccc"; font.pixelSize: 12
                                        horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    function addNewLine() {
        if (newKeyField.text.length > 0) {
            editPanelController.addLine(newKeyField.text, newValueField.text)
            newKeyField.clear()
            newValueField.clear()
        }
    }
}
