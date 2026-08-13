// SelectionBox.qml
// 框选矩形组件，对应 IBR_SelectMode::RenderUI_MassSelect (IBR_Components.cpp:978-1000)
// 仅在 MassSelecting 状态下显示半透明蓝色选择框
// 框选释放后矩形消失，选中状态保持，操作通过右键菜单（ContextMenu.qml）触发
import QtQuick

Rectangle {
    id: root
    color: "#33007acc"
    border.color: "#007acc"
    border.width: 1
    visible: width > 2 && height > 2
    z: 200
}
