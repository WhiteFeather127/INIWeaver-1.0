// WaitingDialog.qml
// 等待/加载弹窗：对应 ImGui 版的模态等待框（处理耗时操作时显示）
// 监听 DialogController.waitingShown / waitingHidden 信号
import QtQuick
import QtQuick.Controls

Dialog {
    id: root
    modal: true
    closePolicy: Dialog.NoAutoClose  // 不允许点击外部关闭
    anchors.centerIn: parent
    width: 320
    height: 120
    padding: 0
    visible: false

    background: Rectangle {
        color: "#252526"
        border.color: "#3c3c3c"
        border.width: 1
        radius: 4
    }

    contentItem: Column {
        anchors.fill: parent
        anchors.margins: 20
        spacing: 16

        Text {
            id: messageText
            width: parent.width
            color: "#d4d4d4"
            font.pixelSize: 13
            wrapMode: Text.Wrap
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
            text: (i18n.rev, i18n.tr("GUI_WaitingText"))
        }

        // 加载动画
        BusyIndicator {
            anchors.horizontalCenter: parent.horizontalCenter
            running: root.visible
            implicitWidth: 36
            implicitHeight: 36
        }
    }

    // 接收 DialogController 的信号
    Connections {
        target: dialogController
        function onWaitingShown(message) {
            messageText.text = message.length > 0 ? message : (i18n.rev, i18n.tr("GUI_WaitingText"))
            root.open()
        }
        function onWaitingHidden() {
            root.close()
        }
    }
}
