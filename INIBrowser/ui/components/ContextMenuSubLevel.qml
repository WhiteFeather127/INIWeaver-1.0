// ContextMenuSubLevel.qml
// 级联子菜单层（Popup 纯渲染），对应 ImGui DelayedPopupAction 渲染的独立子菜单小窗。
// 只做渲染 + 信号冒泡，层级管理（创建/裁剪/收起）统一由 ContextMenuHost 处理
// （Host 通过坐标判定鼠标是否在层内，本层不设拦截 MouseArea）。
// 复用 ContextMenuItemList 渲染 item/separator/submenu，样式与主菜单一致（圆角暗色）。
import QtQuick
import QtQuick.Controls

Popup {
    id: root
    padding: 0
    focus: false
    // 子菜单不响应外部点击关闭，由主菜单统一生命周期管理
    closePolicy: Popup.NoAutoClose

    background: Rectangle {
        color: "#252526"
        border.color: "#3c3c3c"
        border.width: 1
        radius: 6
    }

    property var itemDescs: []
    // 勾选状态共享表（与主菜单同一实例，保证跨层一致）
    property var checkedStates: ({})
    property var levelRef: root

    // ===== 冒泡信号（由 ContextMenuHost 统一处理） =====
    signal actionTriggered(string action)
    signal submenuRequested(var itemDesc, point globalPos, var ref)
    signal folderHoverChanged(bool on)
    signal hoverLeft()

    contentItem: ContextMenuItemList {
        id: list
        itemDescs: root.itemDescs
        checkedStates: root.checkedStates
        levelRef: root.levelRef
        onActionTriggered: (action) => root.actionTriggered(action)
        onSubmenuRequested: (desc, g, ref) => root.submenuRequested(desc, g, ref)
        onFolderHoverChanged: (on) => root.folderHoverChanged(on)
        onHoverLeft: root.hoverLeft()
    }
}
