// ConfirmDialog3.qml
// 阶段 11.3：三态确认弹窗，对应 ImGui AskIfSave（IBR_ProjManager.cpp:221-249）
// 三按钮：保存 / 不保存 / 取消
// 由 DialogController.confirm3Requested(title, message, actionId) 信号触发
import QtQuick
import QtQuick.Controls

Dialog {
    id: root
    modal: true
    anchors.centerIn: parent
    width: 480
    height: 200
    padding: 0
    closePolicy: Dialog.NoAutoClose  // 三态弹窗不允许外部点击关闭，必须点按钮

    property string actionId: ""
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

        // 消息文本
        Text {
            width: parent.width
            height: parent.height - 48 - 16
            text: root.messageText
            color: "#d4d4d4"
            font.pixelSize: 13
            wrapMode: Text.Wrap
            verticalAlignment: Text.AlignTop
        }

        // 三按钮区（对应 IBR_ProjManager.cpp:236-248 三个 ImGui::Button）
        Row {
            anchors.horizontalCenter: parent.horizontalCenter
            spacing: 12

            // 保存按钮（对应 GUI_AskIfSave_Yes）
            Button {
                width: 110; height: 32
                text: qsTr("保存")
                background: Rectangle {
                    color: parent.hovered ? "#007acc" : "#3c3c3c"
                    border.color: "#007acc"; border.width: 1; radius: 3
                }
                contentItem: Text {
                    text: parent.text; color: "#ffffff"; font.pixelSize: 13
                    horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter
                }
                onClicked: {
                    // result=1: 保存
                    dialogController.notifyConfirmResult3(root.actionId, 1)
                    root.close()
                }
            }

            // 不保存按钮（对应 GUI_AskIfSave_No）
            Button {
                width: 110; height: 32
                text: qsTr("不保存")
                background: Rectangle {
                    color: parent.hovered ? "#3c3c3c" : "#2d2d2d"
                    border.color: "#3c3c3c"; border.width: 1; radius: 3
                }
                contentItem: Text {
                    text: parent.text; color: "#cccccc"; font.pixelSize: 13
                    horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter
                }
                onClicked: {
                    // result=2: 不保存
                    dialogController.notifyConfirmResult3(root.actionId, 2)
                    root.close()
                }
            }

            // 取消按钮（对应 GUI_AskIfSave_Cancel）
            Button {
                width: 110; height: 32
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
                    // result=0: 取消
                    dialogController.notifyConfirmResult3(root.actionId, 0)
                    root.close()
                }
            }
        }
    }
}
