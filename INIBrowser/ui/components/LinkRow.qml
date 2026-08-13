// LinkRow.qml
// 链接行组件：标签 + 复制按钮 + 打开按钮（对应 ImGui URLOpr lambda）
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: root
    Layout.fillWidth: true
    Layout.preferredHeight: 36
    color: "#2d2d2d"
    radius: 3

    property string label: ""
    property string url: ""

    Text {
        anchors.left: parent.left
        anchors.leftMargin: 12
        anchors.verticalCenter: parent.verticalCenter
        text: root.label
        color: "#cccccc"
        font.pixelSize: 12
    }

    Row {
        anchors.right: parent.right
        anchors.rightMargin: 8
        anchors.verticalCenter: parent.verticalCenter
        spacing: 4

        Button {
            text: qsTr("复制")
            width: 44; height: 22
            onClicked: {
                // 对应 ImGui LogToClipboard
                clipboard.setText(root.url)
            }
            background: Rectangle {
                color: parent.hovered ? "#3c3c3c" : "#2d2d2d"
                border.color: "#3c3c3c"; border.width: 1; radius: 3
            }
            contentItem: Text {
                text: parent.text; color: "#cccccc"; font.pixelSize: 11
                horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter
            }
        }

        Button {
            text: qsTr("打开")
            width: 44; height: 22
            onClicked: {
                // 对应 ImGui ShellExecuteA
                Qt.openUrlExternally(root.url)
            }
            background: Rectangle {
                color: parent.hovered ? "#3c3c3c" : "#2d2d2d"
                border.color: "#3c3c3c"; border.width: 1; radius: 3
            }
            contentItem: Text {
                text: parent.text; color: "#cccccc"; font.pixelSize: 11
                horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter
            }
        }
    }

    // 剪贴板（对应 ImGui LogToClipboard）
    TextEdit {
        id: clipboard
        visible: false
        function setText(text) {
            textEditHelper.text = text
            textEditHelper.selectAll()
            textEditHelper.copy()
        }
    }

    TextEdit {
        id: textEditHelper
        visible: false
    }
}
