// LinkRenderer.qml
// 连线渲染组件，对应 IBR_WorkSpace::RenderUI_Links() (IBR_WorkSpace.cpp:1433-1610)
// 起点 pa = IBR_NodeSession.LastCenter（由 LinkNodePoint 回写，经 linkEndpoints 下发）
// 终点 pb = 目标节点标题栏 RadioButton 位置（由 sections 的 screenX/Y/W/H 近似计算）
// 曲线形状 4 种分支：
//   1. IsSelfLinked && Collapsed → 不画
//   2. Straight（pb.x - pa.x >= FontHeight*5）→ 单段 Bezier
//   3. IsSelfLinked → 两段 Bezier 回环
//   4. FromImport → 单段 Bezier
//   5. 其他 → S 形双段 Bezier
import QtQuick

Canvas {
    id: root
    anchors.fill: parent

    // 连线数据（由 WorkspaceController.links 提供）
    property var links: []
    // Section 数据（用于查找目标端坐标）
    property var sections: []
    // 连线端点表：[{sessionId, x, y, isCollapsed}, ...]（由 WorkspaceController.linkEndpoints 提供）
    property var linkEndpoints: []
    // D21：端点 map（key = "sessionId:destId"），供 pb 按 key 查找，避免索引依赖
    property var linkEndpointsMap: ({})
    // 当前选中 Section ID（高亮连线）
    property var focusedSectionId: 0
    // 当前缩放比例（用于计算 FontHeight）
    property real ratio: 1.0
    // 阶段 2：拖拽偏移量（拖拽中实时更新 pa/pb）
    property point dragOffset: Qt.point(0, 0)

    // FontHeight 对应 ImGui 的 FontHeight（基准 13px × ratio）
    readonly property real fontHeight: 13.0 * ratio
    readonly property real lineWidth: fontHeight / 5.0

    onLinksChanged: requestPaint()
    onSectionsChanged: requestPaint()
    onLinkEndpointsChanged: requestPaint()
    onLinkEndpointsMapChanged: requestPaint()
    onFocusedSectionIdChanged: requestPaint()
    onRatioChanged: requestPaint()
    onDragOffsetChanged: requestPaint()

    // 构建 sessionId → {x, y, isCollapsed} 查找表
    function buildEndpointMap() {
        var map = {};
        for (var i = 0; i < linkEndpoints.length; i++) {
            var ep = linkEndpoints[i];
            map[ep.sessionId] = { x: ep.x, y: ep.y, isCollapsed: ep.isCollapsed };
        }
        return map;
    }

    // 构建 sectionId → {screenX, screenY, screenW, screenH, dragging} 查找表
    function buildSectionMap() {
        var map = {};
        // 修复：拖拽状态同时检查 draggingSectionId/massDragging
        // 因为 beginMoveSection 不再调用 refresh()，sectionData.dragging 在拖拽期间为 false
        var dragId = workspaceController.draggingSectionId;
        var massDrag = workspaceController.massDragging;
        for (var i = 0; i < sections.length; i++) {
            var s = sections[i];
            var isDragging = (s.dragging || false)
                || s.sectionId === dragId
                || (massDrag && (s.selected || false));
            map[s.sectionId] = {
                screenX: s.screenX || 0,
                screenY: s.screenY || 0,
                screenW: s.screenW || 180,
                screenH: s.screenH || 120,
                collapsed: s.collapsed || false,
                dragging: isDragging
            };
        }
        return map;
    }

    // 计算目标端坐标 pb（对应 ImGui RSD->ReWindowUL + RSD->ReOffset）
    // ReOffset = Import ? {W/2 - FontHeight/2, HalfLine} : {FontHeight*0.7, HalfLine}
    // 阶段 2：拖拽中的目标节点加入 dragOffset
    function computeDestPoint(dstSec, fromImport) {
        var halfLine = fontHeight * 0.5;
        var pbX, pbY;
        var baseX = dstSec.screenX;
        var baseY = dstSec.screenY;
        if (dstSec.dragging) {
            baseX += dragOffset.x;
            baseY += dragOffset.y;
        }
        if (fromImport) {
            pbX = baseX + dstSec.screenW * 0.5 - fontHeight * 0.5;
        } else {
            pbX = baseX + fontHeight * 0.7;
        }
        pbY = baseY + halfLine;
        return { x: pbX, y: pbY };
    }

    onPaint: {
        var ctx = getContext("2d");
        ctx.reset();

        if (links.length === 0) return;

        var endpointMap = buildEndpointMap();
        var sectionMap = buildSectionMap();

        for (var i = 0; i < links.length; i++) {
            var link = links[i];

            // 起点 pa = Session.LastCenter（对应 ImGui IBR_NodeSession::GetSessionValue(Link.SourceID).LastCenter）
            var srcEp = endpointMap[link.sourceSessionId];
            if (!srcEp) continue;

            // 目标端 pb
            // dstSec 可能为 undefined：IsIncluded 子模块由虚拟块 Loader 渲染（不在 sections 列表）、
            // Hidden section 被 refreshSections 过滤。此时用 C++ 端 linkEndpointsMap 的精确 pb 兜底
            var dstSec = sectionMap[link.destId];
            // 源 section（用于判断源节点是否拖拽）
            var srcSec = sectionMap[link.sourceId];

            // pa 叠加 dragOffset（当源节点拖拽时）
            // 端点表中的 pa 是拖拽前的坐标（拖拽中不回写，见 SectionNode.qml onXChanged），
            // 需要叠加 dragOffset 让连线跟随节点移动
            var paX = srcEp.x;
            var paY = srcEp.y;
            if (srcSec && srcSec.dragging) {
                paX += dragOffset.x;
                paY += dragOffset.y;
            }
            var pa = { x: paX, y: paY };

            // D21：终点 pb 用 map 查找（key = "sessionId:destId"），不再依赖索引顺序
            // 对应 ImGui pb = ActiveLines[DestKey].AcceptCenter[LineMult] 或 ReWindowUL+ReOffset
            var pb;
            var pbPrecise = false;
            var mapKey = link.sourceSessionId + ":" + link.destId;
            var ep = linkEndpointsMap[mapKey];
            if (ep && ep.pbValid) {
                // 精确 pb 叠加 dragOffset（当目标节点拖拽时）
                var pbX = ep.pbX;
                var pbY = ep.pbY;
                if (dstSec && dstSec.dragging) {
                    pbX += dragOffset.x;
                    pbY += dragOffset.y;
                }
                pb = { x: pbX, y: pbY };
                pbPrecise = true;
            }
            // 修复：dstSec 不在 sections 列表时，若 C++ 端已算出精确 pb 则直接用，否则无法画
            if (!dstSec) {
                if (!pbPrecise) continue;  // 既无 dstSec 又无精确 pb，跳过
                // 有精确 pb，继续画（dragOffset 已在上面叠加）
            } else if (!pbPrecise) {
                // 回退：边界近似（对应无回写时的兜底，computeDestPoint 内部叠加 dragOffset）
                pb = computeDestPoint(dstSec, link.fromImport);
            }

            // 分支 1：IsSelfLinked && Collapsed → 不画（对应 ImGui line 1511）
            if (link.isSelfLinked && srcEp.isCollapsed) continue;

            // 颜色（对应 ImGui line 1474-1487）
            var col = link.color;
            var isFocused = (link.sourceId === focusedSectionId || link.destId === focusedSectionId);
            if (isFocused) {
                col = "#4fc3f7";  // FocusLineColor
            } else if (!col || col === "#00000000") {
                col = "#cccccc";  // LegalLineColor（默认）
            }

            // 拖拽中半透明（对应 ImGui IBR_WorkSpace.cpp:1488 Col.Value.w *= TransparencyBase() * 0.625f）
            // v3 批次 1.3：补 TransparencyBase 乘数（默认 0.8 → 实际 alpha = 0.5）
            var alpha = 1.0;
            var base = settingController.transparencyBase;
            if (!base || base <= 0) base = 0.8;  // 保护：设置未加载时使用默认值
            if (link.isSrcDragging) alpha = base * 0.625;

            ctx.strokeStyle = col;
            ctx.lineWidth = isFocused ? lineWidth + 2.0 : lineWidth;
            ctx.globalAlpha = alpha;

            var midX = (pa.x + pb.x) / 2.0;
            var midY = (pa.y + pb.y) / 2.0;
            var straight = (pb.x - pa.x >= fontHeight * 5.0);

            ctx.beginPath();
            ctx.moveTo(pa.x, pa.y);

            if (straight) {
                // 分支 2：Straight → 单段 Bezier（对应 ImGui line 1512-1518）
                var s_cp1x = (pa.x + 4 * pb.x) / 5;
                var s_cp1y = pa.y;
                var s_cp2x = (4 * pa.x + pb.x) / 5;
                var s_cp2y = pb.y;
                ctx.bezierCurveTo(s_cp1x, s_cp1y, s_cp2x, s_cp2y, pb.x, pb.y);
            } else if (link.isSelfLinked) {
                // 分支 3：IsSelfLinked → 两段 Bezier 回环（对应 ImGui line 1522-1537）
                var sl_cp1x = (7 * pa.x - 2 * pb.x) / 5;
                var sl_cp1y = (3 * pb.y - pa.y) / 2;
                var sl_cp2x = pa.x;
                var sl_cp2y = (3 * pb.y - pa.y) / 2;
                ctx.bezierCurveTo(sl_cp1x, sl_cp1y, sl_cp2x, sl_cp2y, midX, midY);

                var sl2_cp1x = pb.x;
                var sl2_cp1y = (3 * pa.y - pb.y) / 2;
                var sl2_cp2x = (7 * pb.x - 2 * pa.x) / 5;
                var sl2_cp2y = (3 * pa.y - pb.y) / 2;
                ctx.bezierCurveTo(sl2_cp1x, sl2_cp1y, sl2_cp2x, sl2_cp2y, pb.x, pb.y);
            } else if (link.fromImport) {
                // 分支 4：FromImport → 单段 Bezier（对应 ImGui line 1544-1552）
                var fi_cp1x = (pa.x + 4 * pb.x) / 5;
                var fi_cp1y = pa.y;
                var fi_cp2x = (4 * pa.x + pb.x) / 5;
                var fi_cp2y = pb.y;
                ctx.bezierCurveTo(fi_cp1x, fi_cp1y, fi_cp2x, fi_cp2y, pb.x, pb.y);
            } else {
                // 分支 5：普通 S 形双段 Bezier（对应 ImGui line 1558-1571）
                var n_cp1x = pa.x + fontHeight * 5.0;
                var n_cp1y = pa.y;
                var n_cp2x = pa.x + fontHeight * 5.0;
                var n_cp2y = (3 * pa.y + pb.y) / 4;
                ctx.bezierCurveTo(n_cp1x, n_cp1y, n_cp2x, n_cp2y, midX, midY);

                var n2_cp1x = pb.x - fontHeight * 5.0;
                var n2_cp1y = (3 * pb.y + pa.y) / 4;
                var n2_cp2x = pb.x - fontHeight * 5.0;
                var n2_cp2y = pb.y;
                ctx.bezierCurveTo(n2_cp1x, n2_cp1y, n2_cp2x, n2_cp2y, pb.x, pb.y);
            }

            ctx.stroke();
        }

        ctx.globalAlpha = 1.0;
    }
}
