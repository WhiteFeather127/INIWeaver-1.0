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
    // 画布平移偏移（屏幕像素）：拖动画布时端点表保持快照，渲染时对所有端点叠加此偏移
    //（来自 workspaceController.canvasDragOffset，端点表重建时清零）
    property point canvasOffset: Qt.point(0, 0)
    // 视口偏移（屏幕像素）：拖拽侧边栏调宽/窗口 resize 时视口中心变化，所有模块整体
    // 平移而端点表是上次 rebuild 的快照 → 渲染时叠加此偏移使连线端点与模块即时对齐
    //（来自 workspaceController.viewportOffsetX/Y，端点表重建时归零）
    property real viewportOffsetX: workspaceController.viewportOffsetX
    property real viewportOffsetY: workspaceController.viewportOffsetY

    // FontHeight 对应 ImGui 的 FontHeight（基准 13px × ratio）
    readonly property real fontHeight: 13.0 * ratio
    readonly property real lineWidth: fontHeight / 5.0

    onLinksChanged: requestPaint()
    onSectionsChanged: { cachedSectionMap = null; selectionSetDirty = true; requestPaint() }
    onLinkEndpointsChanged: { cachedEndpointMap = null; requestPaint() }
    onLinkEndpointsMapChanged: requestPaint()
    onFocusedSectionIdChanged: requestPaint()
    onRatioChanged: requestPaint()
    // 拖拽中 dragOffset 每帧变化只需 requestPaint：缓存内容（各节点的 dragging 标志）只依赖
    // 离散状态（draggingSectionId/massDragging/lastDraggedId/hasOffset/selectedRevision），
    // 由 onPaint 内 _sectionMapKey 门控重建——此前每拖一帧都全量重建 1471 项 map 且逐节点
    // 调 1-2 次 C++ 查询（1471×2 次跨语言往返/帧），是拖拽卡顿热点之一
    onDragOffsetChanged: requestPaint()
    onCanvasOffsetChanged: requestPaint()
    onViewportOffsetXChanged: requestPaint()
    onViewportOffsetYChanged: requestPaint()
    // 性能优化：缓存端点/节点查找表。缩放补间动画每帧只变 ratio/eqCenter，
    // links/sections/linkEndpoints 均不变，每帧重建 map 是纯浪费；
    // 数据变化时清缓存，拖拽中由 _sectionMapKey 检测状态切换后重建
    property var cachedEndpointMap: null
    property var cachedSectionMap: null
    property string _sectionMapKey: ""
    // 选中集 O(1) 查询缓存：一次 massTargetSnapshot() 跨语言往返取回全部选中 id 建 JS Set，
    // 替代 onPaint 每条连线 2 次 + buildSectionMap 每节点 1 次的逐项 isSectionSelected 调用
    //（1471 模块全选后每帧 4000+ 次往返）。selectedRevision 变化（框选预览每帧也 bump）或
    // sectionsChanged（导入 MassSelect 等不走 revision 的路径）时置脏重建。
    property var _selectionSet: null
    property bool selectionSetDirty: true
    property int _selectionSetRevision: -1
    // 拖拽成员集缓存（按 dragRoot 失效）
    property var _dragMembersCache: null
    property var _dragMembersRoot: 0
    function getSelectionSet() {
        if (_selectionSet === null || selectionSetDirty
            || _selectionSetRevision !== workspaceController.selectedRevision) {
            var arr = workspaceController.massTargetSnapshot()
            var s = new Set()
            for (var i = 0; i < arr.length; i++) s.add(String(arr[i]))
            _selectionSet = s
            selectionSetDirty = false
            _selectionSetRevision = workspaceController.selectedRevision
        }
        return _selectionSet
    }
    // sectionMap 缓存门控键：离散拖拽/选中状态任一变化才允许重建
    function sectionMapKey() {
        return workspaceController.draggingSectionId + ":" + workspaceController.massDragging
             + ":" + workspaceController.hasLastDragged() + ":" + workspaceController.lastDraggedId()
             + ":" + (dragOffset.x !== 0 || dragOffset.y !== 0)
             + ":" + workspaceController.selectedRevision
    }
    // 缩放叠加（仿 canvasOffset）：缩放中端点表保持快照，按 zoomBaseRatio/zoomBaseCenter
    // 基准把端点坐标换算到当前 ratio/eqCenter 下，与节点即时缩放保持一致（缩放停止后端点
    // 表重建，zoomPending 变 false，换算自动失效）
    Connections {
        target: workspaceController
        function onZoomBaseChanged() { requestPaint() }
        // 修复：多选拖动开始/结束时 massDragging 翻转，sectionMap 的 dragging 标志需重算
        //（buildSectionMap 改用 isSectionSelected 实时查询，缓存失效后才会重新查询）
        function onMassDraggingChanged() { cachedSectionMap = null; requestPaint() }
        // 选中态变化：focusedSectionId 的 NOTIFY 绑在 sectionsChanged 上，单选路径不发射，
        // 属性值不会刷新。改用选中态实时判断连线高亮，选中变化时重绘。
        function onSelectedRevisionChanged() { requestPaint() }
    }

    // 缩放换算：把端点表快照坐标（基准 ratio/eqCenter 下的屏幕坐标）换算到当前视图。
    // 公式：E = (S_b - V)/r_b + C_b → S_c = (E - C_c)*r_c + V
    //      = (S_b - V)*(r_c/r_b) + (C_b - C_c)*r_c + V
    function zoomTransform(px, py) {
        if (!workspaceController.zoomPending) return { x: px, y: py }
        var rBase = workspaceController.zoomBaseRatio
        if (rBase <= 0) return { x: px, y: py }
        var rCur = ratio
        var vx = width / 2.0, vy = height / 2.0
        var k = rCur / rBase
        var cBase = workspaceController.zoomBaseCenter
        var cCur = workspaceController.eqCenter
        return {
            x: (px - vx) * k + (cBase.x - cCur.x) * rCur + vx,
            y: (py - vy) * k + (cBase.y - cCur.y) * rCur + vy
        }
    }

    // 构建 sessionId → {x, y, isCollapsed} 查找表
    function buildEndpointMap() {
        var _perf = workspaceController.diagLogEnabled()
        if (_perf) workspaceController.perfBegin("QML.LinkRenderer.buildEndpointMap")
        var map = {};
        for (var i = 0; i < linkEndpoints.length; i++) {
            var ep = linkEndpoints[i];
            map[ep.sessionId] = { x: ep.x, y: ep.y, isCollapsed: ep.isCollapsed };
        }
        if (_perf) workspaceController.perfEnd()
        return map;
    }

    // 构建 sectionId → {screenX, screenY, screenW, screenH, dragging} 查找表
    function buildSectionMap() {
        var _perf = workspaceController.diagLogEnabled()
        if (_perf) workspaceController.perfBegin("QML.LinkRenderer.buildSectionMap")
        var map = {};
        // 修复：拖拽状态同时检查 draggingSectionId/massDragging
        // 因为 beginMoveSection 不再调用 refresh()，sectionData.dragging 在拖拽期间为 false
        var dragId = workspaceController.draggingSectionId;
        var massDrag = workspaceController.massDragging;
        // 选中集批量查询：一次 massTargetSnapshot 建 Set（见 getSelectionSet 注释），
        // 替代此前每节点一次 isSectionSelected 跨语言调用
        var selSet = root.getSelectionSet();
        // 修复"松手连线弹"：刚结束拖拽的节点（lastDraggedId，dragOffset 尚未清零）继续叠加
        // dragOffset，使连线在端点表重建完成前持续跟随鼠标终点，不弹回拖拽前位置
        // 哨兵值用 INVALID_MODULE_ID（非 0）：0 同时是"无最近拖拽"与"块 0 合法 ID"会导致
        // 会话首次拖动任意模块时块 0 被误判为刚拖过、其连线端点跟着 dragOffset 串动
        var hasLastDragged = workspaceController.hasLastDragged();
        var lastDragId = workspaceController.lastDraggedId();
        var hasOffset = (workspaceController.dragOffset.x !== 0
                         || workspaceController.dragOffset.y !== 0);
        for (var i = 0; i < sections.length; i++) {
            var s = sections[i];
            // 修复：多选拖动时 s.selected 是拖拽前快照（beginMassDrag 不调 refresh 避免
            // Repeater 销毁 delegate 丢失 mouse grab），selectAll 后快照 selected 全 false →
            // massDrag && s.selected 永远 false → dstDragging=false → onPaint 不叠加 dragOffset
            // → 隐藏键行 pbX（旧 EqPos）不动、pbY（m_sectionAcceptPoint 实时回写）动 → 只上下动。
            // 改用选中集实时查询 MassTarget，拖拽中状态切换（_sectionMapKey 变化）时重建缓存，
            // 查询结果始终新鲜。
            // 修复（松手一帧偏移）：多选松手后 massDrag=false，节点已跳终点（sectionPositionChanged
            // 同步 emit），但端点表 rebuild 是 Queued 异步。单节点靠 lastDraggedId+hasOffset 叠加
            // dragOffset 让端点=旧基准+dragOffset=终点渡过这几帧；多选有 N 个节点但 lastDraggedId
            // 只记 front，非 front 节点端点不叠加→旧基准，节点已终点→偏移一个 dragOffset。
            // 新增 isLastMassDragged 过渡判断：松手后到收尾 rebuild 清 dragOffset 前，对所有被拖
            // 节点叠加 dragOffset，端点=旧基准+dragOffset=终点，与节点一致，无偏移。
            var isDragging = (s.dragging || false)
                || s.sectionId === dragId
                || (hasLastDragged && s.sectionId === lastDragId && hasOffset)
                || (massDrag && selSet.has(String(s.sectionId)))
                || (hasOffset && workspaceController.isLastMassDragged(s.sectionId));
            if (workspaceController.diagLogEnabled() && isDragging && hasLastDragged && s.sectionId === lastDragId && hasOffset && lastDragId !== dragId) {
                console.log("[DRAG-DIAG] buildSectionMap: section", s.sectionId,
                            "marked dragging via lastDragId+hasOffset (dragId=" + dragId
                            + ", lastDragId=" + lastDragId
                            + ", dragOffset=" + JSON.stringify(workspaceController.dragOffset) + ")")
            }
            map[s.sectionId] = {
                screenX: s.screenX || 0,
                screenY: s.screenY || 0,
                screenW: s.screenW || 180,
                screenH: s.screenH || 120,
                collapsed: s.collapsed || false,
                dragging: isDragging
            };
        }
        if (_perf) workspaceController.perfEnd()
        return map;
    }

    // 判断"哪些节点随当前拖动的编组块一起移动"（含 groupId 自身 + 任意层级子模块）：
    // 拖动编组（虚拟块）时整组移动，但内部子模块不在 sections 快照（dstSec==undefined），
    // 默认不叠加 dragOffset → 子模块连线停在原地。
    // 性能重构：原 isMovedByDrag(sectionId) 每条连线调 1-2 次 isInComposedOf（C++ BFS），
    // 2193 条连线 = 每帧 4000+ 次跨语言遍历。改为每帧开始时 buildDragMemberSet 一次调用
    // 取回成员全集建 JS Set，逐连线 O(1) 判断（onPaint 内 dragMembers）。
    // 松手过渡：拖动结束后 draggingSectionId=INVALID，但端点表 rebuild 是 Queued 异步，
    // 叠加失效会让折叠子模块连线先弹回旧基准一帧（回弹）。故用 lastDraggedId（刚拖完的
    // 编组块，dragOffset 尚未清零）替代 draggingSectionId 继续判定组内子模块跟随，
    // 直到收尾 cleanup 重建端点表 + 清 dragOffset 后过渡自动失效。与 buildSectionMap 顶层
    // 节点用 lastDraggedId+hasOffset 叠加同源一致。
    function buildDragMemberSet(groupId) {
        var arr = workspaceController.composedMembersList(groupId)
        var s = new Set()
        for (var i = 0; i < arr.length; i++) s.add(String(arr[i]))
        if (workspaceController.diagLogEnabled())
            console.log("[DRAG-DIAG] buildDragMemberSet root=" + groupId + " size=" + arr.length + " members=" + arr.join(","))
        return s
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
        var _perf = workspaceController.diagLogEnabled()
        if (_perf) workspaceController.perfBegin("QML.LinkRenderer.onPaint")
        var ctx = getContext("2d");
        ctx.reset();

        if (links.length === 0) { if (_perf) workspaceController.perfEnd(); return; }

        // 连线偏差诊断：每帧一次，输出渲染叠加偏移与视口状态（与 [EP-DIAG] 端点表日志
        // 联用——端点表坐标 + 本帧叠加偏移 = onPaint 实际画出的 pa/pb）
        if (_perf) console.log("[LINK-DIAG] paintBegin links=" + links.length
            + " canvasOff=(" + canvasOffset.x + "," + canvasOffset.y + ")"
            + " vpOff=(" + viewportOffsetX + "," + viewportOffsetY + ")"
            + " dragOff=(" + workspaceController.dragOffset.x + "," + workspaceController.dragOffset.y + ")"
            + " eqCenter=(" + workspaceController.eqCenter.x + "," + workspaceController.eqCenter.y + ")"
            + " ratio=" + ratio)

        // 用缓存查找表（数据变化时由 onXxxChanged 清空，动画/平移期间直接复用）
        var endpointMap = root.cachedEndpointMap;
        if (endpointMap === null) {
            endpointMap = buildEndpointMap();
            root.cachedEndpointMap = endpointMap;
        }
        // sectionMap 缓存按离散状态键门控：拖拽中 dragOffset 每帧变但标志不变 → 不重建
        var sectionMap = root.cachedSectionMap;
        var mapKey = root.sectionMapKey();
        if (sectionMap === null || mapKey !== root._sectionMapKey) {
            sectionMap = buildSectionMap();
            root.cachedSectionMap = sectionMap;
            root._sectionMapKey = mapKey;
        }

        // 选中集（连线高亮 O(1) 查询）与拖拽成员集（每帧一次跨语言调用）。
        // 类型注意：links 的 sourceId/destId 是 C++ QString::number 下发的字符串
        //（防大整数精度丢失），而 sections 快照的 sectionId 是数字——所有 Set 查找
        // 统一 String() 归一，否则 has() 恒 false（拖拽叠加/高亮静默失效）。
        // hasOffsetNow + dragMembers 联合判断"是否随拖拽叠加 dragOffset"：
        // - 松手过渡期（hasLastDragged && hasOffset）：以 lastDraggedId 的编组成员为准
        // - 拖拽中（draggingSectionId）：以拖拽目标的编组成员为准
        // 语义与原 isMovedByDrag(sectionId) && hasOffset 完全一致，跨语言调用从
        // 每连线 1-2 次降为每帧 1 次。
        var selSet = root.getSelectionSet();
        var hasOffsetNow = (workspaceController.dragOffset.x !== 0
                            || workspaceController.dragOffset.y !== 0);
        var dragRoot = 0;
        if (workspaceController.hasLastDragged() && hasOffsetNow) {
            dragRoot = workspaceController.lastDraggedId();
        } else if (workspaceController.draggingSectionId) {
            dragRoot = workspaceController.draggingSectionId;
        }
        // 成员集按 dragRoot 缓存：成员关系在拖拽期间不变，无需每帧跨语言 BFS
        if (dragRoot !== root._dragMembersRoot) {
            root._dragMembersRoot = dragRoot
            root._dragMembersCache = dragRoot ? root.buildDragMemberSet(dragRoot) : null
        }
        var dragMembers = root._dragMembersCache;
        if (_perf && hasOffsetNow) console.log("[DRAG-DIAG] paint dragRoot=" + dragRoot
            + " memberSet=" + (dragMembers ? dragMembers.size : "null")
            + " dragOff=(" + workspaceController.dragOffset.x + "," + workspaceController.dragOffset.y + ")")

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

            // pa 叠加 canvasOffset（画布平移）+ dragOffset（当源节点拖拽时）
            // 端点表中的 pa 是拖拽前的坐标（拖拽中不回写，见 SectionNode.qml onXChanged），
            // 需要叠加偏移让连线跟随节点移动；缩放中先按基准换算（zoomTransform）
            var srcT = zoomTransform(srcEp.x, srcEp.y);
            var paX = srcT.x + viewportOffsetX + canvasOffset.x;
            var paY = srcT.y + viewportOffsetY + canvasOffset.y;
            // 拖拽跟随：源节点本身被拖（srcSec.dragging）或源模块位于当前拖动的编组块内
            // （子模块不在 sections 快照、srcSec 可能 undefined，dragMembers 判断）。
            // 端点世界缓存存的是拖拽前基准（回写时减去 dragOffset / cull 前回写），
            // 叠加 dragOffset 对所有节点一致正确。
            if ((srcSec && srcSec.dragging)
                || (dragMembers && dragMembers.has(String(link.sourceId)) && hasOffsetNow)) {
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
                // 精确 pb 叠加 canvasOffset（画布平移）+ dragOffset（当目标节点拖拽时）
                // 缩放中先按基准换算（zoomTransform）
                var pbT = zoomTransform(ep.pbX, ep.pbY);
                var pbX = pbT.x + viewportOffsetX + canvasOffset.x;
                var pbY = pbT.y + viewportOffsetY + canvasOffset.y;
                // 拖拽跟随：目标节点本身被拖（dstSec.dragging）或目标模块位于当前拖动的编组块内
                // （子模块 dstSec 为 undefined，dragMembers 判断，否则编组拖动时其连线停原地）。
                if ((dstSec && dstSec.dragging)
                    || (dragMembers && dragMembers.has(String(link.destId)) && hasOffsetNow)) {
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
                pb.x += viewportOffsetX + canvasOffset.x;
                pb.y += viewportOffsetY + canvasOffset.y;
            }

            // 分支 1：IsSelfLinked && Collapsed → 不画（对应 ImGui line 1511）
            if (link.isSelfLinked && srcEp.isCollapsed) continue;

            // ===== 视口剔除：曲线包围盒不与视口相交则整条跳过 =====
            // 大画布（2318 条连线）绝大部分连线两端都在视口外——既浪费路径构建+描边耗时，
            // 又会出现"横穿视口但两端都不可见的线"（用户视为幻影线）。
            // 包围盒按控制点保守外扩：S 形控制点横向伸出 fontHeight*5，自回环纵向伸出
            // |dy|/2（曲线包含于控制点凸包内，此外扩保证不误删可见段）。
            var f5 = fontHeight * 5.0;
            var bbDX = Math.abs(pb.x - pa.x), bbDY = Math.abs(pb.y - pa.y);
            var bbPadX = f5 + bbDX * 0.4 + 40;
            var bbPadY = bbDY * 0.5 + f5 + 40;
            if (Math.max(pa.x, pb.x) + bbPadX < 0
                || Math.min(pa.x, pb.x) - bbPadX > width
                || Math.max(pa.y, pb.y) + bbPadY < 0
                || Math.min(pa.y, pb.y) - bbPadY > height)
                continue;

            if (workspaceController.diagLogEnabled()) console.log("[LINK-DIAG] onPaint link[" + i + "] srcMod=" + link.sourceId + " srcSess=" + link.sourceSessionId + " destId=" + link.destId + " destKey=" + link.destKey + " pbPrecise=" + pbPrecise + " pa=" + pa.x + "," + pa.y + " pb=" + (pb ? (pb.x + "," + pb.y) : "null") + " dstDragging=" + (dstSec ? dstSec.dragging : "noSec") + " pbEp=" + (ep ? (ep.pbX + "," + ep.pbY + ",valid=" + ep.pbValid) : "noEp"))

            // 颜色（对应 ImGui IBR_WorkSpace.cpp:1497-1504）
            // ImGui: CurSection.ID == Rsec.ID(目标) || CurSection.ID == SrcModuleID(源) → FocusLineColor
            // 选中态用 selSet O(1) 查询（替代每连线 2 次 isSectionSelected 跨语言调用）
            var col = link.color;
            var isFocused = selSet.has(String(link.sourceId)) || selSet.has(String(link.destId));
            if (isFocused) {
                col = "#ffffff";  // FocusLineColor（对齐 ImGui 深色主题 255,255,255 = 白，IBR_Misc.cpp:1032）
            } else if (!col || col.a === 0) {
                // 对齐 ImGui IBR_WorkSpace.cpp:1506-1507：LinkColW=(Link.Color>>A)&0xFF; Col = LinkColW>0 ? Link.Color : LegalLineColor。
                // 用 alpha 字节判断"是否有业务色"（不能只比对 "#00000000"——QColor(alpha=0, RGB 非零) 会
                // 序列化成 "#RRGGBB00" 形式的透明色，画出来完全不可见导致"平时看不到连线、选中才见白线"）。
                col = "#cccccc";  // LegalLineColor（默认）
            }

            // 拖拽中半透明（对应 ImGui IBR_WorkSpace.cpp:1488 Col.Value.w *= TransparencyBase() * 0.625f）
            // v3 批次 1.3：补 TransparencyBase 乘数（默认 0.8 → 实际 alpha = 0.5）
            var alpha = 1.0;
            var base = settingController.transparencyBase;
            if (!base || base <= 0) base = 0.8;  // 保护：设置未加载时使用默认值
            if (link.isSrcDragging) alpha = base * 0.625;

            ctx.strokeStyle = col;
            // 高亮仅改颜色不变粗细（用户要求去除 isFocused 时的 +2.0 加粗）
            ctx.lineWidth = lineWidth;
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
        if (_perf) workspaceController.perfEnd()
    }
}
