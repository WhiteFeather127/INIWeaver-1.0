// AppToolTip.qml
// 全局统一悬停提示框（Popup 单例）—— 项目内所有"鼠标放上去的提示"的唯一入口。
// 统一为暗色方角 + 固定字号，对齐 ImGui IBR_ToolTip 扁平提示。
// 不随工作区缩放变化（字体固定，不乘 ratio）。
//
// 用法：
//   // Main.qml 实例化，挂 Overlay.overlay（与 ContextMenuHost 同架构）：
//   //   AppToolTip { id: appToolTip; parent: Overlay.overlay }
//   // 各 hover 源（菜单按钮/连线节点/模块项等）：
//   //   onEntered: appToolTip.show(text, screenX, screenY)
//   //   onExited:  appToolTip.hide()
import QtQuick
import QtQuick.Controls

Popup {
    id: root
    padding: 0
    closePolicy: Popup.NoAutoClose  // 提示框由 show/hide 显式控制，不点外部关闭

    // 提示框最大宽度（超出则文本换行，避免长文本把框拉宽到屏幕外）
    property int maxTipWidth: 420

    contentItem: Text {
        id: tipText
        leftPadding: 6
        rightPadding: 6
        topPadding: 4
        bottomPadding: 4
        color: "#e8e8e8"
        // 固定字号：不乘 ratio，不随工作区缩放变化
        font.pixelSize: 12
        wrapMode: Text.Wrap
        text: ""
    }

    background: Rectangle {
        color: "#e6242424"
        radius: 0  // 方角（对齐 imgui 扁平提示）
        border.color: "#3c3c3c"
        border.width: 1
    }

    // 统一显示入口：text=提示内容，screenX/Y=屏幕坐标（自动转 Overlay 坐标并钳制到屏幕内）
    function show(text, screenX, screenY) {
        tipText.text = text || ""
        if (root.tipText.implicitWidth > root.maxTipWidth - 12) {
            // 文本超宽：限宽换行（contentItem 宽度先按上限钳制，Popup 按 contentItem 隐式宽铺开）
            root.contentItem.width = root.maxTipWidth - 12
        }
        var o = Overlay.overlay.mapFromGlobal(screenX, screenY)
        var w = root.tipText.implicitWidth
        root.x = Math.max(2, Math.min(o.x, Overlay.overlay.width - w - 12))
        root.y = Math.max(2, Math.min(o.y, Overlay.overlay.height - root.implicitHeight - 4))
        root.open()
    }

    function hide() {
        root.close()
        tipText.text = ""
    }
}