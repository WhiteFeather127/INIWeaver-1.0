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

    // 点击定位信号
    signal clicked(variant eqPos)

    onSectionsChanged: requestPaint()
    onViewRectChanged: requestPaint()
    onEqCenterChanged: requestPaint()
    onIsBgDraggingChanged: requestPaint()

    onPaint: {
        var ctx = getContext("2d");
        ctx.reset();

        if (sections.length === 0) return;

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
        if (worldW < 1 || worldH < 1) return;

        // 缩放到迷你地图尺寸（fit 自适应，随组件尺寸变化自动重算）
        var scaleX = width / worldW;
        var scaleY = height / worldH;
        var scale = Math.min(scaleX, scaleY) * 0.9;  // 留边距

        var offsetX = (width - worldW * scale) / 2;
        var offsetY = (height - worldH * scale) / 2;

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

        // 绘制 Section 矩形
        for (var i = 0; i < sections.length; i++) {
            var s = sections[i];
            var ul = eqToMini(s.eqX, s.eqY);
            var dr = eqToMini(s.eqX + s.eqW, s.eqY + s.eqH);

            ctx.fillStyle = s.ignored ? "#5a5a5a" : (s.registerColor || "#808080");
            ctx.globalAlpha = s.hidden ? 0.3 : 0.8;
            ctx.fillRect(ul.x, ul.y, dr.x - ul.x, Math.max(2, dr.y - ul.y));
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
        var scaleX = width / worldW;
        var scaleY = height / worldH;
        var scale = Math.min(scaleX, scaleY) * 0.9;
        var offsetX = (width - worldW * scale) / 2;
        var offsetY = (height - worldH * scale) / 2;

        var eqX = (mx - offsetX) / scale + minX;
        var eqY = (my - offsetY) / scale + minY;
        root.clicked({ x: eqX, y: eqY });
    }
}
