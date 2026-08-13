// ConfirmDialog.qml
// 通用确认弹窗，对应 IBR_PopupManager::MessageModal / Yes-No 链式弹窗
// 由 DialogController.confirmRequested(title, message, actionId) 信号触发
import QtQuick
import QtQuick.Controls

Dialog {
    id: root
    modal: true
    anchors.centerIn: parent
    width: 420
    height: 180
    padding: 0
    closePolicy: canClose ? Dialog.CloseOnEscape | Dialog.CloseOnPressOutside : Dialog.NoAutoClose

    property string actionId: ""
    property bool canClose: true
    property string messageText: ""

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

    contentItem: Column {
        anchors.fill: parent
        anchors.margins: 16
        spacing: 16

        // 消息文本（对应 PushTextBack）
        Text {
            width: parent.width
            height: parent.height - 48 - 16
            text: root.messageText
            color: "#d4d4d4"
            font.pixelSize: 13
            wrapMode: Text.Wrap
            verticalAlignment: Text.AlignTop
        }

        // 按钮区（对应 PushMsgBack 的 Yes/No）
        Row {
            anchors.horizontalCenter: parent.horizontalCenter
            spacing: 12

            Button {
                width: 96; height: 32
                text: qsTr("确定")
                background: Rectangle {
                    color: parent.hovered ? "#007acc" : "#3c3c3c"
                    border.color: "#007acc"; border.width: 1; radius: 3
                }
                contentItem: Text {
                    text: parent.text; color: "#ffffff"; font.pixelSize: 13
                    horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter
                }
                onClicked: {
                    dialogController.notifyConfirmResult(root.actionId, true)
                    root.close()
                }
            }

            Button {
                width: 96; height: 32
                text: qsTr("取消")
                background: Rectangle {
                    color: parent.hovered ? "#3c3c3c" : "#2d2d2d"
                    border.color: "#3c3c3c"; border.width: 1; radius: 3
                }
                contentItem: Text {
                    text: parent.text; color: "#cccccc"; font.pixelSize: 13
                    horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter
                }
                onClicked: {
                    dialogController.notifyConfirmResult(root.actionId, false)
                    root.close()
                }
            }
        }
    }

    onRejected: {
        if (canClose) {
            dialogController.notifyConfirmResult(root.actionId, false)
        }
    }
}
