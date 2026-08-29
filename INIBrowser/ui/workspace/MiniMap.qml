// MiniMap.qml
// 迷你地图组件，对应 IBR_FullView::DrawView() (line 71-162)
// 绘制所有 Section 矩形（fit 自适应）+ 视图框 + 点击定位（不画画布边缘外框，最大占用空间）
// 内容固定 fit 全览（模块 + 视口），不提供内容缩放；
// 可在独立"视图窗口"（Main.qml miniMapWindow）中拖动/调整大小使用
import QtQuick

Canvas {
    id: root
    property var sections: []
    property var viewRect: ({ x: 0, y: 0, w: 0, h: 0 })  // 视口在 Eq 坐标
    property var eqCenter: ({ x: 0, y: 0 })
    property real viewScale: 20.0  // 固定缩放因子（保留兼容）
    // 阶段 13.4：背景拖拽状态（对应 IBR_WorkSpace::IsBgDragging）
    // true 时视图框用 CenterCrossColor，false 时用 ClipFrameLineColor
    property bool isBgDragging: false

    // 画布边缘外框（world bounds 轮廓）显示开关：
    // 侧边栏内嵌迷你地图保留（默认 true）；独立"视图窗口"不画（Main.qml 设 false）
    property bool showWorldBorder: true

    // 画布大小是否随视口变化（world bounds 是否包含 viewRect）：
    // 侧边栏（ViewPanel）开启 true：视图框拖到模块范围以外时画布随之扩展（对齐 ImGui
    // UpdateCurrentEqMax 画布随内容扩展的行为）；
    // 视图窗口（Main.qml）保持 false：画布大小固定，不随拖动位置改变
    property bool includeViewportInWorld: false

    // 画布边缘与组件（窗口）边缘的固定像素边距：
    // 等比 fit 缩放后，贴边方向距离 = canvasPadding（固定），非贴边方向因居中 ≥ canvasPadding
    // 取代原 `* 0.9` 的相对边距（5% × 窗口尺寸，随窗口大小变化）
    property int canvasPadding: 10

    // 点击定位信号
    signal clicked(variant eqPos)

    onSectionsChanged: {
        if (workspaceController.diagLogEnabled()) {
            var desc = "none"
            if (sections.length > 0) {
                // 只拼前 20 项：全量拼接在 1471 节点时每行 ~50KB，且 reportSectionSize
                // 合并前的每次 emit 都打一行（实测累计 ~350MB，占部署日志主体）
                var cap = Math.min(sections.length, 20)
                var parts = []
                for (var i = 0; i < cap; i++) {
                    var s = sections[i]
                    parts.push("[" + s.sectionId + ":" + s.eqX + "," + s.eqY + "," + s.eqW + "," + s.eqH + "]")
                }
                if (sections.length > cap)
                    parts.push("…(" + (sections.length - cap) + " more)")
                desc = parts.join(" ")
            }
            console.log("[REFRESH-DIAG] MiniMap onSectionsChanged count=" + sections.length + " " + desc)
        }
        requestPaint()
    }
    onViewRectChanged: requestPaint()
    onEqCenterChanged: requestPaint()
    onIsBgDraggingChanged: requestPaint()

    onPaint: {
        var _perf = workspaceController.diagLogEnabled()
        if (_perf) workspaceController.perfBegin("QML.MiniMap.onPaint")
        var ctx = getContext("2d");
        ctx.reset();

        if (_perf)
            console.log("[REFRESH-DIAG] MiniMap onPaint sections=" + sections.length + " w=" + width + " h=" + height)

        if (sections.length === 0) { if (_perf) workspaceController.perfEnd(); return; }

        // 计算所有 Section 的边界
        // world bounds 默认仅由模块范围决定；侧边栏（includeViewportInWorld=true）时
        // 额外包含视口：视图框拖到模块范围以外会撑大画布（对齐 ImGui 画布随内容扩展）
        var minX = Infinity, minY = Infinity, maxX = -Infinity, maxY = -Infinity;
        for (var i = 0; i < sections.length; i++) {
            var s = sections[i];
            minX = Math.min(minX, s.eqX);
            minY = Math.min(minY, s.eqY);
            maxX = Math.max(maxX, s.eqX + s.eqW);
            maxY = Math.max(maxY, s.eqY + s.eqH);
        }

        // 侧边栏：画布大小跟随视口（视图框拖出画布外时扩展）
        if (root.includeViewportInWorld) {
            minX = Math.min(minX, viewRect.x);
            minY = Math.min(minY, viewRect.y);
            maxX = Math.max(maxX, viewRect.x + viewRect.w);
            maxY = Math.max(maxY, viewRect.y + viewRect.h);
        }

        var worldW = maxX - minX;
        var worldH = maxY - minY;
        if (worldW < 1 || worldH < 1) { if (_perf) workspaceController.perfEnd(); return; }

        // 缩放到迷你地图尺寸（fit 自适应），画布与组件边缘留固定像素边距 canvasPadding
        // 贴边方向距离 = canvasPadding（固定），非贴边方向因等比居中 ≥ canvasPadding
        var pad = root.canvasPadding;
        var availW = width - 2 * pad;
        var availH = height - 2 * pad;
        if (availW < 1 || availH < 1) { if (_perf) workspaceController.perfEnd(); return; }
        var scale = Math.min(availW / worldW, availH / worldH);
        var offsetX = pad + (availW - worldW * scale) / 2;
        var offsetY = pad + (availH - worldH * scale) / 2;

        function eqToMini(x, y) {
            return {
                x: (x - minX) * scale + offsetX,
                y: (y - minY) * scale + offsetY
            };
        }

        // 画布外框（world bounds 轮廓，仅模块范围）：侧边栏内嵌迷你地图保留，
        // 视图窗口通过 showWorldBorder=false 关闭（用户要求：侧边栏保留、浮窗不画）
        if (root.showWorldBorder) {
            var wUL = eqToMini(minX, minY);
            var wDR = eqToMini(maxX, maxY);
            ctx.strokeStyle = "#3c3c3c";
            ctx.lineWidth = 1;
            ctx.strokeRect(wUL.x, wUL.y, wDR.x - wUL.x, wDR.y - wUL.y);
        }

        // 绘制 Section 矩形（对应 ImGui DrawView AddRectFilled + AddRect）
        // 编组块等透明填充色的模块（registerColor alpha=0）仅靠填充不可见，
        // 需补画 1px 描边轮廓（对应 ImGui AddRect 用 WindowBg 色画边框），否则小地图画不出来。
        for (var i = 0; i < sections.length; i++) {
            var s = sections[i];
            var ul = eqToMini(s.eqX, s.eqY);
            var dr = eqToMini(s.eqX + s.eqW, s.eqY + s.eqH);
            var w = dr.x - ul.x;
            var h = dr.y - ul.y;

            ctx.fillStyle = s.ignored ? "#5a5a5a" : (s.registerColor || "#808080");
            ctx.globalAlpha = s.hidden ? 0.3 : 0.8;
            ctx.fillRect(ul.x, ul.y, w, Math.max(2, h));
            // 描边：任何颜色（含透明）的模块都有轮廓可见，对应 ImGui AddRect(WindowBg)
            ctx.globalAlpha = s.hidden ? 0.3 : 0.8;
            ctx.strokeStyle = "#1e1e1e";
            ctx.lineWidth = 1;
            ctx.strokeRect(ul.x, ul.y, w, h);
        }
        ctx.globalAlpha = 1.0;

        // 视图框（当前视野，对应 ImGui ClipVUL+CPos 矩形）
        // 阶段 13.4：拖拽背景时切色（对应 IBR_FullView.cpp:160）
        // IsBgDragging ? CenterCrossColor : ClipFrameLineColor
        // 半透明填充标示视野范围，边框标定边界；viewRect 数据由调用方按
        // viewportEqRect 实时提供（跟随 eqCenter/ratio/视口尺寸变化）
        var vpUL = eqToMini(viewRect.x, viewRect.y);
        var vpDR = eqToMini(viewRect.x + viewRect.w, viewRect.y + viewRect.h);
        ctx.fillStyle = isBgDragging ? "rgba(133,133,133,0.12)" : "rgba(0,122,204,0.12)";
        ctx.fillRect(vpUL.x, vpUL.y, vpDR.x - vpUL.x, vpDR.y - vpUL.y);
        ctx.strokeStyle = isBgDragging ? "#858585" : "#007acc";
        ctx.lineWidth = 1.5;
        ctx.strokeRect(vpUL.x, vpUL.y, vpDR.x - vpUL.x, vpDR.y - vpUL.y);

        // 中心十字
        var center = eqToMini(eqCenter.x, eqCenter.y);
        ctx.strokeStyle = "#858585";
        ctx.lineWidth = 1;
        ctx.beginPath();
        ctx.moveTo(center.x - 4, center.y);
        ctx.lineTo(center.x + 4, center.y);
        ctx.moveTo(center.x, center.y - 4);
        ctx.lineTo(center.x, center.y + 4);
        ctx.stroke();
        if (_perf) workspaceController.perfEnd()
    }

    // 阶段 13.4：鼠标按下时持续定位（对应 IBR_FullView.cpp:185-189）
    // ImGui: if (IsItemHovered() && IsMouseDown(Left)) ChangeOffsetPos(MP - CRect)
    // Qt: 用 onPositionChanged 替代 onClicked，按下时持续跟随鼠标
    MouseArea {
        anchors.fill: parent
        // 按下时立即定位一次
        onPressed: (mouse) => {
            root.locateTo(mouseX, mouseY)
        }
        // 按住拖动时持续定位
        onPositionChanged: (mouse) => {
            if (pressed) {
                root.locateTo(mouseX, mouseY)
            }
        }
    }

    // 逆变换：迷你地图坐标 -> Eq 坐标，发出 clicked 信号
    // 注意：与 onPaint 一致，world bounds 按 includeViewportInWorld 决定是否含视口
    function locateTo(mx, my) {
        if (sections.length === 0) return;

        var minX = Infinity, minY = Infinity, maxX = -Infinity, maxY = -Infinity;
        for (var i = 0; i < sections.length; i++) {
            var s = sections[i];
            minX = Math.min(minX, s.eqX);
            minY = Math.min(minY, s.eqY);
            maxX = Math.max(maxX, s.eqX + s.eqW);
            maxY = Math.max(maxY, s.eqY + s.eqH);
        }
        // 侧边栏：画布大小跟随视口（与 onPaint 一致）
        if (root.includeViewportInWorld) {
            minX = Math.min(minX, viewRect.x);
            minY = Math.min(minY, viewRect.y);
            maxX = Math.max(maxX, viewRect.x + viewRect.w);
            maxY = Math.max(maxY, viewRect.y + viewRect.h);
        }

        var worldW = maxX - minX;
        var worldH = maxY - minY;
        var pad = root.canvasPadding;
        var availW = width - 2 * pad;
        var availH = height - 2 * pad;
        if (availW < 1 || availH < 1) return;
        var scale = Math.min(availW / worldW, availH / worldH);
        var offsetX = pad + (availW - worldW * scale) / 2;
        var offsetY = pad + (availH - worldH * scale) / 2;

        var eqX = (mx - offsetX) / scale + minX;
        var eqY = (my - offsetY) / scale + minY;

        if (workspaceController.diagLogEnabled())
            console.log("[REFRESH-DIAG] MiniMap locateTo mx=" + mx + " my=" + my
                        + " worldW=" + worldW + " worldH=" + worldH
                        + " scale=" + scale + " eqX=" + eqX + " eqY=" + eqY
                        + " viewRect=" + viewRect.x + "," + viewRect.y + "," + viewRect.w + "," + viewRect.h)

        // 视图框限制在画布内（参考 GitHub nodify Minimap.MaxViewportOffset 思路）：
        // includeViewportInWorld=false（视图窗口）时画布固定为模块范围，不随视口扩展；
        // 视口中心 clamp 到 [min+半视口, max-半视口]，视图框拖到画布边缘即停、不出框。
        // includeViewportInWorld=true（侧边栏）时画布已含视口，clamp 恒成立，跳过。
        if (!root.includeViewportInWorld) {
            var halfW = viewRect.w / 2;
            var halfH = viewRect.h / 2;
            if (worldW <= viewRect.w) {
                eqX = (minX + maxX) / 2;  // 视口比画布宽：居中
            } else {
                eqX = Math.max(minX + halfW, Math.min(maxX - halfW, eqX));
            }
            if (worldH <= viewRect.h) {
                eqY = (minY + maxY) / 2;  // 视口比画布高：居中
            } else {
                eqY = Math.max(minY + halfH, Math.min(maxY - halfH, eqY));
            }
        }

        root.clicked({ x: eqX, y: eqY });
    }
}
