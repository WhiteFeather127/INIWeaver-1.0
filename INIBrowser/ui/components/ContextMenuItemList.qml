// ContextMenuItemList.qml
// 统一右键菜单项渲染层（item / separator / submenu 三种类型）。
// 供 ContextMenuHost（主菜单）与 ContextMenuSubLevel（级联子菜单）共用，
// 对齐 ImGui 的 SmallButtonAlignLeft 按钮列表（连续排列、hover 高亮）。
//
// 统一数据协议 itemDescs（元素）：
//   { type: "item", text, action, enabled?, checkable?, checked?, desc? }  普通项（desc=ToolTip）
//   { type: "separator" }                                                 分隔线
//   { type: "submenu", text, action, items? }                             级联子菜单（action=路径或标识）
//
// 信号约定：
//   actionTriggered(action)        普通项点击（checkable 项点击只切换勾选，不关闭）
//   submenuRequested(desc, globalPos, ref)  子菜单项 hover，请求在右侧展开（globalPos=屏幕坐标）
//   folderHoverChanged(on)        子菜单项 hover 状态（供收起管理）
//   hoverLeft()                   鼠标离开任一项
import QtQuick
import QtQuick.Controls

Item {
    id: root
    // 尺寸由宿主控制（anchors.fill）；implicit 尺寸供 Popup 自适应
    // 宽度随最长文本自适应（不再固定 180）；高度按内容（delegate implicitHeight 累加）
    implicitWidth: root.computedWidth()
    implicitHeight: contentCol.implicitHeight

    // 文本度量：用真实像素宽（advanceWidth）算菜单宽度，配合文字自适应
    FontMetrics {
        id: menuFont
        font.pixelSize: 12
    }

    // 菜单宽度 = 最长文本宽 + 行内边距；submenu 因带箭头比 item 更宽
    function computedWidth() {
        var w = 0
        for (var i = 0; i < root.itemDescs.length; ++i) {
            var d = root.itemDescs[i]
            if (!d || d.type === "separator") continue
            var lw = menuFont.advanceWidth(d.text || "")
            var pad = (d.type === "submenu") ? (26 + 18 + 6) : (d.checkable ? 26 + 10 : 12 + 10)
            var rowW = lw + pad
            if (rowW > w) w = rowW
        }
        // 给个最小宽度，避免过窄；加边框余量
        return Math.max(120, w + 8)
    }

    // 菜单项描述（统一数据协议）
    property var itemDescs: []
    // 勾选状态共享表（action -> bool），由宿主维护并随勾选实时更新
    property var checkedStates: ({})
    // 本层标识（子菜单裁剪用，宿主传入）
    property var levelRef: null

    // ===== 信号 =====
    signal actionTriggered(string action)
    signal submenuRequested(var itemDesc, point globalPos, var ref)
    signal folderHoverChanged(bool on)
    signal hoverLeft()

    Column {
        id: contentCol
        width: root.implicitWidth
        spacing: 0

        Repeater {
            model: root.itemDescs

            delegate: Item {
                id: row
                width: contentCol.width
                // 分隔线留 7px 行高（上下间距），普通/子菜单项 30px
                // 注意用 implicitHeight（显式 height 不参与 Column 聚合），供 Popup 自适应高度
                implicitHeight: row.isSep ? 7 : 30
                height: implicitHeight

                readonly property bool isSep: modelData && modelData.type === "separator"
                readonly property bool isSub: modelData && modelData.type === "submenu"
                readonly property bool isItem: modelData && !isSep && !isSub
                readonly property string act: (modelData && modelData.action) || ""
                readonly property bool rowEnabled: !modelData || modelData.enabled !== false
                readonly property bool rowChecked: root.checkedStates[row.act] === true

                // ===== 分隔线 =====
                Rectangle {
                    visible: row.isSep
                    anchors.left: parent.left
                    anchors.leftMargin: 4
                    anchors.right: parent.right
                    anchors.rightMargin: 4
                    anchors.verticalCenter: parent.verticalCenter
                    height: 1
                    color: "#1e1e1e"
                }

                // ===== 普通项 =====
                Rectangle {
                    id: itemBg
                    visible: row.isItem
                    anchors.fill: parent
                    anchors.leftMargin: 2
                    anchors.rightMargin: 2
                    anchors.topMargin: 1
                    anchors.bottomMargin: 1
                    radius: 4
                    color: row.rowEnabled && itemMouse.containsMouse ? "#3c3c3c" : "transparent"
                }
                // 勾选标记（checkable 项）
                Text {
                    visible: !!row.isItem && !!modelData && !!modelData.checkable
                    anchors.left: parent.left
                    anchors.leftMargin: 9
                    anchors.verticalCenter: parent.verticalCenter
                    text: row.rowChecked ? "✓" : ""
                    color: "#4fc3f7"
                    font.pixelSize: 11
                }
                // 文本
                Text {
                    visible: row.isItem
                    anchors.left: parent.left
                    anchors.leftMargin: (modelData && modelData.checkable) ? 26 : 12
                    anchors.right: parent.right
                    anchors.rightMargin: 10
                    anchors.verticalCenter: parent.verticalCenter
                    text: (modelData && modelData.text) || ""
                    color: !row.rowEnabled ? "#5a5a5a"
                         : (itemMouse.containsMouse ? "#ffffff" : "#d4d4d4")
                    font.pixelSize: 12
                    elide: Text.ElideRight
                }
                MouseArea {
                    id: itemMouse
                    visible: row.isItem
                    anchors.fill: parent
                    hoverEnabled: true
                    enabled: row.rowEnabled
                    onEntered: root.folderHoverChanged(false)
                    onExited: root.hoverLeft()
                    // 模块描述悬停提示（统一用全局 appToolTip，对应 ImGui IBR_ModuleTree DescLong）
                    onContainsMouseChanged: {
                        if (itemMouse.containsMouse && modelData && modelData.desc) {
                            var g = itemMouse.mapToGlobal(itemMouse.width / 2, itemMouse.height + 4)
                            appToolTip.show(modelData.desc, g.x, g.y)
                        } else {
                            appToolTip.hide()
                        }
                    }
                    onClicked: {
                        if (modelData && modelData.checkable) {
                            // checkable：只切换勾选，不关闭菜单（对齐 ImGui RadioButton UseLink）
                            root.checkedStates[row.act] = !row.rowChecked
                        } else {
                            root.actionTriggered(row.act)
                        }
                    }
                }

                // ===== 子菜单项 =====
                Rectangle {
                    id: subBg
                    visible: row.isSub
                    anchors.fill: parent
                    anchors.leftMargin: 2
                    anchors.rightMargin: 2
                    anchors.topMargin: 1
                    anchors.bottomMargin: 1
                    radius: 4
                    color: subMouse.containsMouse ? "#3c3c3c" : "transparent"
                }
                Text {
                    visible: row.isSub
                    anchors.left: parent.left
                    anchors.leftMargin: 26
                    anchors.right: parent.right
                    anchors.rightMargin: 18
                    anchors.verticalCenter: parent.verticalCenter
                    text: (modelData && modelData.text) || ""
                    color: subMouse.containsMouse ? "#ffffff" : "#cccccc"
                    font.pixelSize: 12
                    elide: Text.ElideRight
                }
                Text {
                    visible: row.isSub
                    anchors.right: parent.right
                    anchors.rightMargin: 6
                    anchors.verticalCenter: parent.verticalCenter
                    text: ">"
                    color: "#858585"
                    font.pixelSize: 10
                }
                MouseArea {
                    id: subMouse
                    visible: row.isSub
                    anchors.fill: parent
                    hoverEnabled: true
                    onEntered: {
                        root.folderHoverChanged(true)
                        // 本项右缘顶部 → 屏幕坐标（子菜单弹出位置）
                        var g = subMouse.mapToGlobal(parent.width, 0)
                        root.submenuRequested(modelData, Qt.point(g.x, g.y), root.levelRef)
                    }
                    onExited: {
                        root.folderHoverChanged(false)
                        root.hoverLeft()
                    }
                }
            }
        }
    }
}
