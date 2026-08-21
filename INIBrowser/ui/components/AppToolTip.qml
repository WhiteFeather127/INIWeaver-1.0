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
//
// 方向性修复：
//   1. 防抖隐藏（Timer 120ms）：快速跨多个源滑动时 onExited 高频触发，延迟 close
//      避免 popup 闪烁/瞬间消失（旧 hide 后新 show 的时序竞争）。
//   2. 优先显示在参考点上方（空间不足才下方）：popup 不落在鼠标前进路径上，
//      避免从上往下滑动时 popup 出现在鼠标即将经过的区域造成"不显示"。
import QtQuick
import QtQuick.Controls

Popup {
    id: root
    padding: 0
    closePolicy: Popup.NoAutoClose  // 提示框由 show/hide 显式控制，不点外部关闭

    // 提示框最大宽度（超出则文本换行，避免长文本把框拉宽到屏幕外）
    property int maxTipWidth: 420

    // 防抖隐藏：鼠标快速滑过多个源时，onExited 高频触发；延迟一定时间再 close，
    // 若期间又有新 show 则取消，避免 popup 闪烁/切换丢显
    Timer {
        id: hideTimer
        interval: 120
        onTriggered: {
            root.close()
            tipText.text = ""
        }
    }

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
        // 换行上限：隐式宽超过此值时才按上限换行（否则自然单行）
        onImplicitWidthChanged: {
            var cap = root.maxTipWidth
            width = (implicitWidth > cap - 12) ? (cap - 12) : implicitWidth
        }
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
        var cap = maxTipWidth - 12
        tipText.width = (tipText.implicitWidth > cap) ? cap : tipText.implicitWidth
        hideTimer.stop()  // 新的 show 取消 pending 的防抖隐藏
        var o = Overlay.overlay.mapFromGlobal(screenX, screenY)
        var w = Math.max(2, Math.min(o.x, Overlay.overlay.width - tipText.width - 14))
        var hh = root.implicitHeight
        // 优先显示在参考点上方（不遮挡鼠标前进路径），上方不足才下方
        var above = o.y - hh - 10
        var below = o.y + 10
        root.x = w
        root.y = (above >= 4) ? above : Math.min(below, Overlay.overlay.height - hh - 4)
        root.open()
    }

    function hide() {
        hideTimer.start()
    }
}