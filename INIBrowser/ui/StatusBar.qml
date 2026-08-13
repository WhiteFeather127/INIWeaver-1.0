// StatusBar.qml
// 底部提示栏：固定 26px 高度，对应 IBR_Components.cpp IBR_HintManager::RenderUI()
// 阶段 5：颜色区分（普通/错误/警告）+ 停留时间倒计时（对应 HintStayTimeMillis=3000）
// 阶段 10.3：修复 -1 永不倒计时语义 + 空闲轮播 hint.txt（对应 IBR_Components.cpp:857-864）
// 对应 ImGui 行为：底栏背景始终可见（DList->AddRectFilled 总是绘制），无 hint 时背景=WindowBg
// FPS 显示在顶栏右侧（对应 MainStage.h:167 GUI_TopRightHint），底栏只显示 hint
import QtQuick

Rectangle {
    id: root
    height: 26
    // 根据 hintType 切换背景色（对应 IBR_Components.cpp:788-895）
    // type: 0=普通蓝, 1=错误红, 2=警告黄
    // ImGui 原版无 hint 时底栏=WindowBg(#0F0F0F)，比工作区背景深所以可见
    // Qt 版窗口背景=#1e1e1e，底栏用 #2d2d2d（与菜单栏一致）保证启动时可见
    color: hintText.length === 0 ? "#2d2d2d"
          : hintType === 1 ? "#d63b3b"
          : hintType === 2 ? "#d6a23b"
          : "#007acc"

    // 顶部边线（对应 ImGui DList->AddLine 绘制上边框）
    Rectangle {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        height: 1
        color: "#3c3c3c"
    }

    property string hintText: ""
    property int hintType: 0      // 0=普通, 1=错误, 2=警告
    property int hintDurationMs: 0  // -1=永不倒计时, 0=默认(视为-1), >0=倒计时毫秒

    // 对应 ImGui RenderUI 启动即显示行为：
    // ImGui 首帧 HintTimer.GetMilli()=0 < 5000 不切换 UseHint(初始0)，
    // 但 !Hint.empty() 为 true，立即渲染 Hint[0]（固定第一条，非随机）。
    // 5 秒后 HintTimer.GetMilli() > 5000 才随机切换。
    Component.onCompleted: {
        if (dialogController.customHint.length === 0) {
            var firstHint = dialogController.firstHint()
            if (firstHint.length > 0) {
                root.hintText = firstHint
                root.hintType = 0
                root.hintDurationMs = -1
            }
        }
    }

    // 倒计时 Timer（对应 IBR_Components.cpp:869-881 CountDownTimer）
    // 仅在 hintDurationMs > 0 时启动
    Timer {
        id: hintTimer
        repeat: false
        onTriggered: {
            root.hintText = ""
            root.hintType = 0
            root.hintDurationMs = 0
        }
    }

    // 阶段 10.3：空闲轮播 Timer（对应 IBR_Components.cpp:857-864 HintTimer）
    // ImGui 的 HintTimer 每帧检查 GetMilli()>5000，无论当前是否有 hint 显示，
    // 5 秒后总是切换 UseHint。Qt 版用 repeat Timer 在无 customHint 时持续运行。
    Timer {
        id: idleHintTimer
        interval: 5000  // 对应 IBR_Components.cpp:804 HintChangeMillis
        repeat: true
        // 无自定义 hint 时持续运行（对应 ImGui !Hint.empty() 时每帧检查 HintTimer）
        running: dialogController.customHint.length === 0
        onTriggered: {
            // 仅在无自定义 hint 时轮播（有 customHint 时由 Connections 处理）
            if (dialogController.customHint.length === 0)
            {
                var nextHint = dialogController.nextRandomHint()
                if (nextHint.length > 0)
                {
                    // 轮播 hint 使用普通类型，不倒计时（-1）
                    root.hintText = nextHint
                    root.hintType = 0
                    root.hintDurationMs = -1
                    hintTimer.stop()
                }
            }
        }
    }

    // 阶段 10.3：监听 customHint 变化，非空时立即显示
    Connections {
        target: dialogController
        function onCustomHintChanged() {
            var ch = dialogController.customHint
            if (ch.length > 0)
            {
                // 自定义 hint 优先，不倒计时
                root.hintText = ch
                root.hintType = 0
                root.hintDurationMs = -1
                hintTimer.stop()
            }
            else if (root.hintDurationMs === -1)
            {
                // 自定义 hint 被清除时，清空当前显示的轮播/自定义 hint
                root.hintText = ""
                root.hintType = 0
                root.hintDurationMs = 0
            }
        }
    }

    // 设置 hint 并启动倒计时
    // 对应 IBR_Components.cpp:882-895 SetHint(Str, TimeLimitMillis)
    // durationMs 语义：
    //   -1 = 永不倒计时（对应 ImGui TimeLimitMillis == -1）
    //    0 = 视为 -1（永不倒计时）
    //   >0 = 倒计时指定毫秒
    function setHint(text, type, durationMs) {
        root.hintText = text
        root.hintType = type || 0
        // 0 视为 -1（对应 ImGui 默认不倒计时）
        var effective = (durationMs === 0) ? -1 : durationMs
        root.hintDurationMs = effective
        if (effective > 0) {
            hintTimer.interval = effective
            hintTimer.restart()
        } else {
            // -1 永不倒计时，停止定时器
            hintTimer.stop()
        }
    }

    // 左侧 hint 文本（对应 ImGui AddText 绘制 GetHint()）
    Text {
        anchors.left: parent.left
        anchors.leftMargin: 12
        anchors.verticalCenter: parent.verticalCenter
        text: root.hintText
        color: "#ffffff"
        font.pixelSize: 12
        elide: Text.ElideRight
        width: parent.width - 24
    }
}
