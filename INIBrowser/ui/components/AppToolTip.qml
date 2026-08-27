// AppToolTip.qml
// 全局统一悬停提示框 —— 项目内所有"鼠标放上去的提示"的唯一入口。
// 统一为暗色方角 + 固定字号，对齐 ImGui IBR_ToolTip 扁平提示。
// 不随工作区缩放变化（字体固定，不乘 ratio）。
//
// 实现要点：这是一个挂在 Overlay.overlay 上的"纯渲染" Item（enabled=false），
// 只负责绘制，完全不参与鼠标命中/焦点链 —— 对下方任何鼠标操作零影响
// （Popup 即便非 modal，其内容区也可能处于命中链顶层/抢焦点，故弃用）。
//
// 用法：
//   // Main.qml 实例化，挂 Overlay.overlay（与 ContextMenuHost 同架构）：
//   //   AppToolTip { id: appToolTip; parent: Overlay.overlay }
//   // 各 hover 源（菜单按钮/连线节点/模块项等）：
//   //   onEntered: appToolTip.show(text, screenX, screenY[, source])
//   //   onExited:  appToolTip.hide(source)
//
// 修复要点：
//   1. 防抖隐藏（Timer 120ms）：快速跨多个源滑动时 onExited 高频触发，延迟 hide
//      避免提示框闪烁/瞬间消失（旧 hide 后新 show 的时序竞争）。
//   2. 优先显示在参考点上方（空间不足才下方）：框不落在鼠标前进路径上，
//      避免从上往下滑动时框出现在鼠标即将经过的区域造成"不显示"。
//   3. 源身份校验（source 参数）：相邻条目快速切换、或跨区域串扰时，旧条目滞后的
//      onExited(hide) 可能晚于新条目 onEntered(show) 到达，会误把刚显示的提示关掉。
//      规定：带 source 的 hide 仅在 source == 当前显示源时才生效；无源的提示
//      （如侧边栏按钮）不受任何带源 hide 的影响。
import QtQuick
import QtQuick.Controls

Item {
    id: root

    // 纯渲染：不接收鼠标/键盘/焦点，对下方所有交互零影响（只画不挡）
    enabled: false
    visible: false
    // 顶层 z：盖过 overlay 中的其他浮层（菜单等），确保提示可见
    z: 10000

    // 提示框最大宽度（超出则文本换行，避免长文本把框拉宽到屏幕外）
    property int maxTipWidth: 420

    // 当前提示归属的源对象（show 时记录）；hide(source) 仅当 source 与之一致才生效，
    // 用于抵御相邻条目切换时滞后的 onExited 事件
    property var activeSource: null

    // 防抖隐藏：鼠标快速滑过多个源时，onExited 高频触发；延迟一定时间再隐藏，
    // 若期间又有新 show 则取消，避免提示框闪烁/切换丢显
    Timer {
        id: hideTimer
        interval: 120
        onTriggered: {
            followTimer.stop()
            root.visible = false
            tipText.text = ""
            root.activeSource = null
        }
    }

    // 跟随鼠标：默认 hover 提示锚在光标下方（对齐 imgui IBR_ToolTip 用 GetMousePos 每帧定位，
    // PosY = MouseY + LineHeight，底部放不下才上移）。"below" 按钮提示不跟随。
    property bool followMouse: true
    property var _lastM: null
    Timer {
        id: followTimer
        interval: 33   // ~30fps，逐帧跟随鼠标（imgui 本来每帧重定位）
        repeat: true
        onTriggered: {
            if (!root.visible || !root.followMouse) return
            var m = workspaceController.globalMousePos()
            // 鼠标没动就不重排，避免静止时反复触发布局
            if (root._lastM && m.x === root._lastM.x && m.y === root._lastM.y) return
            root._lastM = m
            root.repositionToMouse()
        }
    }

    // 背景：暗色方角（对齐 imgui 扁平提示）
    Rectangle {
        id: bg
        anchors.fill: parent
        color: "#e6242424"
        radius: 0
        border.color: "#3c3c3c"
        border.width: 1
    }

    // 内容文本：x/y 预留内边距，width/height 由 show() 按内容精确计算，wrap 自动换行
    Text {
        id: tipText
        x: 7
        y: 5
        color: "#e8e8e8"
        // 固定字号：不乘 ratio，不随工作区缩放变化
        font.pixelSize: 12
        wrapMode: Text.Wrap
    }

    // 统一显示入口：text=提示内容，screenX/Y=屏幕坐标（"below" 用；跟随鼠标时仅作参考，忽略坐标）
    // source=归属源对象（可选）：相邻条目切换时用它校验滞后的 hide，避免误关提示
    // place=摆放方位（可选）："below"→ 居中显示在参考点正下方（顶边栏按钮用），不跟随鼠标；
    //                         缺省→ 跟随鼠标（对齐 imgui GetMousePos 逐帧定位）
    function show(text, screenX, screenY, source, place) {
        activeSource = source || null
        tipText.text = text || ""
        hideTimer.stop()  // 新的 show 取消 pending 的防抖隐藏
        // 内容换行宽度：受 maxTipWidth 限制；再据此算框总尺寸（内容 + 内边距 + 边框）
        tipText.width = (tipText.implicitWidth > maxTipWidth) ? maxTipWidth : tipText.implicitWidth
        root.width = tipText.width + 16                       // 左7 + 右7 + 边框2
        root.height = tipText.implicitHeight + 12             // 上5 + 下5 + 边框2
        followMouse = (place !== "below")
        if (place === "below") {
            // 顶边栏按钮：框水平居中对齐按钮中心，显示在按钮正下方
            var o = Overlay.overlay.mapFromGlobal(screenX, screenY)
            root.x = Math.max(2, Math.min(o.x - root.width / 2, Math.max(2, Overlay.overlay.width - root.width - 2)))
            root.y = Math.min(o.y + 4, Math.max(4, Overlay.overlay.height - root.height - 4))
        } else {
            // 跟随鼠标初始定位一次，并启动跟随定时器
            followTimer.restart()
            root.repositionToMouse()
        }
        root.visible = true
    }

    // 按当前光标定位（imgui IBR_ToolTip）：锚在光标 x、光标 y 下方一行，
    // 底部放不下则上移；右侧放不下则翻到光标左侧。关键：始终在光标下方，绝不遮住指针。
    function repositionToMouse() {
        if (!workspaceController) return
        var m = workspaceController.globalMousePos()
        var o = Overlay.overlay.mapFromGlobal(m.x, m.y)
        var rightOK = o.x + root.width <= Overlay.overlay.width - 2
        var x = rightOK ? Math.max(2, o.x) : Math.max(2, o.x - root.width)
        var y = o.y + 14   // 光标下方一行（line height 约 14）
        if (y + root.height > Overlay.overlay.height - 4)
            y = Math.max(4, Overlay.overlay.height - root.height - 4)
        root.x = x
        root.y = y
    }

    // source=归属源对象（可选）。带 source 的 hide 仅在 source 为当前显示源时才生效，
    // 防御相邻条目切换/跨区域串扰时滞后的 onExited；无源 hide（侧边栏等旧调用）恒生效。
    function hide(source) {
        if (source) {
            // 当前显示无源提示（或源不匹配）时，忽略此带源 hide —— 属滞后/串扰事件
            if (activeSource === null && tipText.text.length > 0) {
                return
            }
            if (activeSource !== null && source !== activeSource) {
                return
            }
        }
        hideTimer.start()
        followTimer.stop()
    }
}