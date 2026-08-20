// ErrorQueueDialog.qml
// 阶段 11.2：错误队列弹窗，对应 IBR_ErrorCollector.cpp:50-97 ShowErrorPopupImpl
// 由 DialogController.currentErrorChanged 信号触发显示
// 支持 4 类错误（JsonParse/ModuleParse/LoadConfig/ExportIni），含 ±4 行上下文
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Dialog {
    id: root
    modal: true
    anchors.centerIn: parent
    width: 640
    height: 480
    padding: 0
    closePolicy: Dialog.NoAutoClose  // 错误弹窗必须点按钮关闭，不允许外部点击

    // 当前错误条目（来自 dialogController.currentError()）
    property var currentError: ({})
    property bool hasError: currentError.valid === true

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
            // 对应 IBR_ErrorCollector.cpp:55 _W.Title（GUI_JsonParseError/GUI_ModuleParseError 等）
            text: root.currentError.valid ? root.currentError.title : (i18n.rev, i18n.tr("GUI_ErrorQueueTitle"))
            color: "#f14c4c"
            font.pixelSize: 13
            font.bold: true
        }

        // 队列剩余计数
        Text {
            anchors.right: parent.right
            anchors.rightMargin: 12
            anchors.verticalCenter: parent.verticalCenter
            visible: dialogController.errorQueueSize > 1
            text: (i18n.rev, i18n.trF("GUI_ErrorQueueRemain", [dialogController.errorQueueSize]))
            color: "#858585"
            font.pixelSize: 11
        }
    }

    contentItem: ColumnLayout {
        anchors.fill: parent
        anchors.margins: 16
        spacing: 12

        // Info 提示（对应 IBR_ErrorCollector.cpp:65 ImGui::Text(W.Info.c_str())）
        Text {
            Layout.fillWidth: true
            visible: root.currentError.valid && root.currentError.info.length > 0
            text: root.currentError.valid ? root.currentError.info : ""
            color: "#d4d4d4"
            font.pixelSize: 12
            wrapMode: Text.Wrap
        }

        // 错误内容区
        ScrollView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true

            TextArea {
                id: errorTextArea
                readOnly: true
                color: "#f14c4c"
                font.family: "Consolas"
                font.pixelSize: 12
                wrapMode: TextArea.Wrap
                text: {
                    if (!root.currentError.valid) return ""
                    // 阶段 11.2/B5：JsonParse 错误显示 ±4 行上下文
                    // 对应 IBR_ErrorCollector.cpp:71-92
                    if (root.currentError.forJson) {
                        var lines = root.currentError.contextLines || []
                        var result = ""
                        for (var i = 0; i < lines.length; ++i) {
                            result += lines[i] + "\n"
                        }
                        if (root.currentError.errorLine > 0) {
                            result += (i18n.rev, i18n.trF("Error_JsonParseErrorLine",
                                        [root.currentError.errorLine])) + "\n"
                        }
                        result += (i18n.rev, i18n.tr("GUI_SeeLogForDetails"))
                        return result
                    }
                    // 非 JsonParse 错误直接显示完整 ErrorStr
                    return root.currentError.errorStr || ""
                }

                background: Rectangle {
                    color: "#1e1e1e"
                    border.color: "#3c3c3c"
                    border.width: 1
                    radius: 3
                }
            }
        }

        // 按钮区
        Row {
            Layout.alignment: Qt.AlignRight
            spacing: 12

            Button {
                width: 110; height: 32
                // 队列还有下一条时显示"下一条"，否则显示"关闭"
                // 对应 IBR_ErrorCollector.cpp:58-59 ErrorListShown++ + ShowErrorPopupImpl
                text: dialogController.errorQueueSize > 1 ? (i18n.rev, i18n.tr("GUI_ErrorQueueNext")) : (i18n.rev, i18n.tr("GUI_ErrorQueueClose"))
                background: Rectangle {
                    color: parent.hovered ? "#007acc" : "#3c3c3c"
                    border.color: "#007acc"; border.width: 1; radius: 3
                }
                contentItem: Text {
                    text: parent.text; color: "#ffffff"; font.pixelSize: 13
                    horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter
                }
                onClicked: {
                    // 推进到下一条错误（对应 ErrorListShown++）
                    dialogController.advanceErrorQueue()
                    // 如果队列已空，关闭弹窗
                    if (dialogController.errorQueueSize === 0) {
                        root.close()
                    }
                }
            }

            Button {
                width: 110; height: 32
                text: (i18n.rev, i18n.tr("GUI_ErrorQueueClear"))
                // 对应 IBR_ErrorCollector::ClearDelayed
                visible: dialogController.errorQueueSize > 1
                background: Rectangle {
                    color: parent.hovered ? "#3c3c3c" : "#2d2d2d"
                    border.color: "#3c3c3c"; border.width: 1; radius: 3
                }
                contentItem: Text {
                    text: parent.text; color: "#cccccc"; font.pixelSize: 13
                    horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter
                }
                onClicked: {
                    dialogController.clearErrorQueue()
                    root.close()
                }
            }
        }
    }

    // 监听 currentErrorChanged 信号自动显示
    Connections {
        target: dialogController
        function onCurrentErrorChanged() {
            root.currentError = dialogController.currentError()
            if (root.currentError.valid && !root.visible) {
                root.open()
            } else if (!root.currentError.valid && root.visible) {
                root.close()
            }
        }
    }
}
