// SelectionActionButton.qml
// 框选操作栏的小按钮（对应 MassAfter 状态的 SmallButtonAlignLeft）
import QtQuick
import QtQuick.Controls

Button {
    height: 24
    padding: 8
    background: Rectangle {
        color: parent.hovered ? "#3c3c3c" : "#2d2d2d"
        border.color: "#3c3c3c"
        border.width: 1
        radius: 3
    }
    contentItem: Text {
        text: parent.text
        color: "#cccccc"
        font.pixelSize: 11
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
    }
}
