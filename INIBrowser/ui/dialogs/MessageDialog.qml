// MessageDialog.qml
// 通用消息弹窗，对应 IBR_PopupManager::MessageModal + UseDefaultOK
// 由 DialogController.popupRequested 信号触发显示
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Dialog {
    id: root
    modal: true
    anchors.centerIn: parent
    // v3 批次 2.2：尺寸由 ImGui Popup.Size 驱动，{0,0} 时用默认值兜底（对应 ImGui SetSize 注释）
    width: popupSize.width > 0 ? popupSize.width : 420
    height: popupSize.height > 0 ? popupSize.height : 200
    padding: 0
    closePolicy: canClose ? (Dialog.CloseOnEscape | Dialog.CloseOnPressOutside) : Dialog.NoAutoClose

    property bool canClose: true
    property string dialogTitle: ""
    property var textList: []
    // v3 批次 2.2：ImGui Popup.Size 转发（默认 {0,0} 表示自动尺寸）
    property size popupSize: Qt.size(0, 0)

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
            text: root.dialogTitle
            color: "#cccccc"
            font.pixelSize: 13
            font.bold: true
        }
    }

    contentItem: ColumnLayout {
        anchors.fill: parent
        anchors.margins: 16
        spacing: 12

        // 显示所有文本（支持多段）
        ScrollView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true

            ColumnLayout {
                width: parent.width
                spacing: 8

                Repeater {
                    model: root.textList
                    Text {
                        Layout.fillWidth: true
                        text: modelData
                        color: "#d4d4d4"
                        font.pixelSize: 12
                        wrapMode: Text.Wrap
                    }
                }
            }
        }

        // 按钮区
        Row {
            Layout.alignment: Qt.AlignRight
            spacing: 12

            Button {
                width: 96; height: 32
                text: qsTr("确定")
                enabled: root.canClose
                onClicked: root.close()

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
        }
    }

    // 监听 DialogController 信号
    Connections {
        target: dialogController
        // v3 批次 2.2：新增 size 参数（对应 ImGui Popup.Size）
        function onPopupRequested(title, texts, modal, canClose, size) {
            root.dialogTitle = title
            root.textList = texts
            root.canClose = canClose
            root.popupSize = size
            root.open()
        }
        // 阶段 11.5：hasPopup 变 false 时自动关闭弹窗（对应 ClearCurrentPopup）
        // 修复：SetWaitingPopup 创建的 canClose=false 弹窗无法手动关闭，
        // 项目加载完成后 ClearCurrentPopup 触发 hasPopup=false，此时自动关闭
        function onHasPopupChanged() {
            if (!dialogController.hasPopup) {
                root.close()
            }
        }
    }
}
