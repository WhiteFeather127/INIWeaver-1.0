// WorkspaceView.qml
// 工作区主容器，对应 IBR_WorkSpace::RenderUI() 主流程
// 包含：背景 + Section 节点 Repeater + 连线渲染 + 框选 + 右键菜单
import QtQuick
import QtQuick.Controls
import INIWeaver
import "../components"

Item {
    id: workspaceView
    // 注：不要使用 anchors.fill: parent，本组件由 RowLayout 管理（Main.qml:91）
    // Layout 会自动设置 x/y/width/height，使用 anchors 会导致 undefined behavior
    // clip: true 防止模块节点溢出到侧边栏/菜单栏之上
    clip: true

    // 拖拽预览框（文件末尾 dragPreview）暴露为属性：
    // QML 中 id 不能通过 "." 从其他组件文件访问，子组件（LinkNodePoint/SectionNode）
    // 必须经此属性更新预览框位置（跟随鼠标）
    property alias dragPreviewItem: dragPreview

    // 缩放补间动画（标准 QML NumberAnimation 驱动）
    // 根因：QSG_NO_VSYNC=1 关闭 vsync 后，默认动画驱动依赖渲染循环持续刷帧才推进，
    // 不再产生中间帧 → NumberAnimation 瞬跳。QtMain.cpp 已设置
    // QSG_USE_SIMPLE_ANIMATION_DRIVER=1（官方建议，基于全局 QElapsedTimer）；
    // 动画驱动仍需渲染循环逐帧 advance，而 SceneGraph 在无 vsync 下是"内容变化才渲染"，
    // 故下方 window.afterRendering 在动画运行期间每帧 requestUpdate 强制持续渲染，
    // 使标准 NumberAnimation 逐帧推进。
    // C++ onWheel 仍只设目标 Ratio 与锚点 emit zoomTweenRequested；
    // 动画每帧写入 zoomAnimRatio 时经 onZoomAnimRatioChanged 调 applyZoomRatio 应用到 C++ 全局态。
    property real zoomAnimRatio: 1.0
    // restart/abort 的瞬停抑制：stop() 同步触发 onRunningChanged(false)，此时不应收尾
    property bool zoomSuppressFinish: false

    // 诊断：[COMP-LIVE] 圆点实际位置 vs 世界缓存投影 对账的全局打印计数（限 300 条）
    property int compLiveCount: 0
    // 诊断：已打"从未回写"标记的分量（key = "sec:key:comp"），避免重复刷屏
    property var compNoWriteSeen: ({})

    // ===== 共享视口态（模块壳与 LinkRenderer 读同一份）=====
    // 连线端点表改存世界坐标（Eq 空间绝对点）后，连线的屏幕位置由这里投影得到：
    //   screen = worldEq × viewRatio + viewBase
    // 等价于 (worldEq − eqCenter) × ratio + viewCenter（与 C++ EqPosToRePos 同式）。
    // 模块壳 sectionDelegate.updatePosition() 用的是这三个量，LinkRenderer 也读这三个量——
    // 二者不是"两份代码写对同一个表达式"，而是同一份数据，物理上不可能分叉。
    // 提升到这里之前它们挂在 sectionRepeater 上，连线无法读取。
    readonly property real viewRatio: workspaceController.ratio
    readonly property real viewBaseX: workspaceView.width / 2 - workspaceController.eqCenter.x * workspaceController.ratio
    readonly property real viewBaseY: workspaceView.height / 2 - workspaceController.eqCenter.y * workspaceController.ratio

    NumberAnimation {
        id: zoomTweenAnim
        target: workspaceView
        property: "zoomAnimRatio"
        duration: 120
        easing.type: Easing.OutCubic
        onRunningChanged: {
            if (!running && !workspaceView.zoomSuppressFinish)
                workspaceController.zoomTweenFinished()
        }
    }
    // 动画期间强制持续渲染：每帧渲染后若动画仍在运行则再次 requestUpdate，
    // 形成持续渲染循环直到动画结束（running=false 后不再请求，自动停止）
    Connections {
        target: window
        function onAfterRendering() {
            // 性能探针：每渲染帧记一次帧间隔（窗口统计见 [PERF-DIAG] 日志）
            if (workspaceController.diagLogEnabled()) workspaceController.perfFrame()
            if (zoomTweenAnim.running) window.requestUpdate()
        }
    }
    // 动画每帧写入 zoomAnimRatio 时应用到 C++ 全局态（Ratio/EqCenter/拖拽基准修正/
    // dragOffset/预览线在 C++ applyZoomRatio 内原子完成）
    onZoomAnimRatioChanged: {
        workspaceController.applyZoomRatio(zoomAnimRatio)
    }

    // 同步视口尺寸到 C++ IBR_RealCenter，供 EqPosToRePos 计算屏幕坐标
    // 对应 ImGui 版本 IBR_Misc.cpp:484-493 IBR_RealCenter::Update()
    function reportViewportGlobal() {
        var g = mapToGlobal(0, 0)
        workspaceController.setViewportGlobal(g.x, g.y)
    }
    onWidthChanged: { workspaceController.setViewportSize(width, height); workspaceView.reportViewportGlobal() }
    onHeightChanged: { workspaceController.setViewportSize(width, height); workspaceView.reportViewportGlobal() }
    onXChanged: workspaceView.reportViewportGlobal()
    onYChanged: workspaceView.reportViewportGlobal()
    Component.onCompleted: {
        workspaceController.setViewportSize(width, height)
        workspaceView.reportViewportGlobal()
    }

    // 查找指定 workspaceView 坐标系坐标下的 SectionNode（供 LinkPoint 拖放检测）
    // 顶层 Repeater 只含顶层模块；编组内子模块由 SectionNode.hitTestChild 递归命中
    //（子模块实际坐标由 QML Column 布局决定，不能用 C++ EqPos 盒测试）。
    // 从后向前遍历（后渲染的在上层），返回命中的最深层 sectionData，未命中返回 null
    function findSectionAt(screenX, screenY) {
        for (var i = sectionRepeater.count - 1; i >= 0; --i) {
            var item = sectionRepeater.itemAt(i)
            if (item && item.hitTestChild) {
                var hit = item.hitTestChild(screenX, screenY)
                if (hit) return hit
            }
        }
        return null
    }

    // 命中 sectionId 的字符串版本：命中返回 sectionId（String），未命中返回 ""。
    // 用途与 C++ hitTestSectionStr 一致，但覆盖编组内子模块（按实际渲染矩形递归命中）。
    function findHitSectionIdStr(wsX, wsY) {
        var node = findSectionAt(wsX, wsY)
        return (node && node.sectionId !== undefined) ? String(node.sectionId) : ""
    }

    // 行为A：命中 acceptor 键行 → 返回 { sectionId, keyName, color, x, y }（接收点中心坐标，
    // workspaceView 坐标系），未命中返回 null。顶层 Repeater 反序遍历，递归命中编组内子模块。
    function findHitAcceptorPoint(wsX, wsY) {
        for (var i = sectionRepeater.count - 1; i >= 0; --i) {
            var item = sectionRepeater.itemAt(i)
            if (item && item.hitTestAcceptor) {
                var hit = item.hitTestAcceptor(wsX, wsY)
                if (hit) return hit
            }
        }
        return null
    }

    // 临时拖拽连线（对应 IBR_WorkSpace.cpp:1436-1459 拖动中的 Bezier）
    // D22：双层 Bezier 光晕效果 + ImGui 控制点公式
    Canvas {
        id: draggingLinkCanvas
        anchors.fill: parent
        visible: false
        z: 60

        property real fromX: 0
        property real fromY: 0
        property real toX: 0
        property real toY: 0
        // D22：拖拽源颜色（对应 ImGui LinkNodeContext::CurDragCol）
        property color dragCol: "#cccccc"
        property real ratio: workspaceController.ratio

        onRatioChanged: requestPaint()

        onPaint: {
            var _perf = workspaceController.diagLogEnabled()
            if (_perf) workspaceController.perfBegin("QML.DragLinkPreview.onPaint")
            var ctx = getContext("2d")
            ctx.reset()
            // D22：ImGui 控制点公式（对应 IBR_WorkSpace.cpp:1448-1449）
            // cp1 = (pa.x + 4*pb.x)/5, pa.y; cp2 = (4*pa.x + pb.x)/5, pb.y
            var cp1x = (fromX + 4 * toX) / 5
            var cp1y = fromY
            var cp2x = (4 * fromX + toX) / 5
            var cp2y = toY
            var fontHeight = 13.0 * ratio
            var lineWidth = fontHeight / 5.0

            // D22：外层 FocusLineColor 光晕（宽度 +2）
            ctx.strokeStyle = "#4fc3f7"
            ctx.lineWidth = lineWidth + 2.0
            ctx.beginPath()
            ctx.moveTo(fromX, fromY)
            ctx.bezierCurveTo(cp1x, cp1y, cp2x, cp2y, toX, toY)
            ctx.stroke()

            // D22：内层拖拽源颜色
            ctx.strokeStyle = dragCol
            ctx.lineWidth = lineWidth
            ctx.beginPath()
            ctx.moveTo(fromX, fromY)
            ctx.bezierCurveTo(cp1x, cp1y, cp2x, cp2y, toX, toY)
            ctx.stroke()
            if (_perf) workspaceController.perfEnd()
        }
    }

    Connections {
        target: workspaceController
        function onDraggingLinkChanged(fx, fy, tx, ty) {
            draggingLinkCanvas.fromX = fx
            draggingLinkCanvas.fromY = fy
            draggingLinkCanvas.toX = tx
            draggingLinkCanvas.toY = ty
            draggingLinkCanvas.visible = true
            draggingLinkCanvas.requestPaint()
        }
        function onDraggingLinkCleared() {
            draggingLinkCanvas.visible = false
        }
        // 缩放补间启动/续接（C++ 滚轮事件触发）：从当前实际 Ratio 向新目标平滑过渡，
        // 连续滚轮续接不跳变。restart 的 stop() 瞬停由 zoomSuppressFinish 抑制收尾。
        function onZoomTweenRequested() {
            workspaceView.zoomSuppressFinish = true
            zoomTweenAnim.stop()
            zoomTweenAnim.from = workspaceController.ratio
            zoomTweenAnim.to = workspaceController.zoomTargetRatio
            zoomTweenAnim.start()
            workspaceView.zoomSuppressFinish = false
        }
        // 缩放补间强制中止（C++ 收尾路径调用，如拖拽结束/视口跳转前）：
        // Ratio 已由 C++ 直接推到目标，这里只停动画，不再逐帧改值
        function onZoomTweenAbortRequested() {
            workspaceView.zoomSuppressFinish = true
            zoomTweenAnim.stop()
            workspaceView.zoomSuppressFinish = false
        }
    }

    // 工作区背景
    Rectangle {
        anchors.fill: parent
        color: "#1e1e1e"

        // 网格背景（视觉参考）——网格固定在 Eq 世界坐标系上：
        // 网格线与模块保持相对位置，平移画布（EqCenter 变化）/缩放（ratio 变化）时
        // 网格随世界坐标同步移动（对应 ImGui 原版网格随视口移动的行为，而非固定屏幕网格）。
        // 参考：QML 无限画布方案（startX = floor(-translate/zoom/grid)*grid 只画可视区域）
        Canvas {
            id: gridCanvas
            anchors.fill: parent
            property real ratio: workspaceController.ratio
            property real ecX: workspaceController.eqCenter.x
            property real ecY: workspaceController.eqCenter.y
            onRatioChanged: requestPaint()
            onEcXChanged: requestPaint()
            onEcYChanged: requestPaint()

            onPaint: {
                var _perf = workspaceController.diagLogEnabled()
                if (_perf) workspaceController.perfBegin("QML.Grid.onPaint")
                var ctx = getContext("2d");
                ctx.reset();
                ctx.strokeStyle = "#252525";
                ctx.lineWidth = 1;
                var vx = width / 2, vy = height / 2;
                // 世界网格间距（Eq 单位，对应基准 40px @ ratio=1）
                var gridEq = 40;
                // 缩放时保持视觉密度：屏幕间距过小（<16px）时倍增世界间距，
                // 防止 ratio 很小（缩小）时网格线过密导致性能与视觉问题
                var screenGap = gridEq * ratio;
                while (screenGap < 16 && gridEq < 2560) {
                    gridEq *= 2;
                    screenGap = gridEq * ratio;
                }
                // 可视范围对应的 Eq 区间（只画可视区域，避免全屏过多线条）
                var leftEq = ecX - vx / ratio;
                var rightEq = ecX + vx / ratio;
                var topEq = ecY - vy / ratio;
                var bottomEq = ecY + vy / ratio;
                // 从可视区域内第一条网格线开始（floor 到 gridEq 整数倍，参考无限画布方案）
                var startEqX = Math.floor(leftEq / gridEq) * gridEq;
                var startEqY = Math.floor(topEq / gridEq) * gridEq;
                // 单 path 一次 stroke 画全部网格线（原逐线 beginPath+stroke 的状态切换
                // 在缩放补间每帧重绘时是明显开销，合并后大幅降低 Canvas 重绘成本）
                // 屏幕坐标 = (eqX - eqCenter) * ratio + 视口中心（与 SectionNode 同公式）
                ctx.beginPath();
                for (var ex = startEqX; ex <= rightEq; ex += gridEq) {
                    var sx = (ex - ecX) * ratio + vx;
                    ctx.moveTo(sx, 0);
                    ctx.lineTo(sx, height);
                }
                for (var ey = startEqY; ey <= bottomEq; ey += gridEq) {
                    var sy = (ey - ecY) * ratio + vy;
                    ctx.moveTo(0, sy);
                    ctx.lineTo(width, sy);
                }
                ctx.stroke();
                if (_perf) workspaceController.perfEnd()
            }
        }
    }

    // 连线层（在节点下方）
    LinkRenderer {
        id: linkRenderer
        anchors.fill: parent
        links: workspaceController.links
        sections: workspaceController.sections
        linkEndpoints: workspaceController.linkEndpoints
        linkEndpointsMap: workspaceController.linkEndpointsMap
        focusedSectionId: workspaceController.focusedSectionId
        ratio: workspaceController.ratio
        // 阶段 2：传入 dragOffset，拖拽中实时更新连线端点
        // 这是唯一的"补偿层"：拖拽期间节点 EqPos 未落盘，模块靠 dragTerm、连线靠 dragOffset，
        // 同一个临时量。平移/缩放/视口变化已无补偿层——端点表是世界坐标，每帧投影即可。
        dragOffset: workspaceController.dragOffset
    }

    // 鼠标交互区（空白处）
    // 修复：移除 hoverEnabled，避免背景 MouseArea 窃取 SectionNode 的鼠标移动事件
    // 背景仅在 pressed/released/wheel 时处理，不主动跟踪 hover
    MouseArea {
        id: mouseArea
        anchors.fill: parent
        acceptedButtons: Qt.LeftButton | Qt.RightButton
        hoverEnabled: false
        z: 0  // 确保在 SectionNode（z>=1）之下

        onPressed: {
            workspaceController.onMousePress(mouseX, mouseY, mouse.button)
        }

        onPositionChanged: {
            if (pressed) {
                workspaceController.onMouseMove(mouseX, mouseY)
            }
        }

        onReleased: {
            workspaceController.onMouseRelease(mouseX, mouseY, mouse.button)
        }

        // 阶段 8.3：双击事件转发到 ProjectController，供 DebugPanel UIState 显示
        // 对应 ImGui IsMouseDoubleClicked(ImGuiMouseButton_Left)
        // 阶段 12.7：双击空白触发模块搜索（对应 IBR_WorkSpace.cpp:961-969 SearchModuleAlt::RenderUI）
        onDoubleClicked: {
            if (mouse.button === Qt.LeftButton) {
                projectController.markDblClickLeft()
                workspaceController.onDoubleClickEmpty(mouseX, mouseY)
            }
        }

        onWheel: {
            workspaceController.onWheel(wheel.x, wheel.y, wheel.angleDelta.y)
        }
    }

    // 模块拖放接收区已废弃：Qt Drag/DropArea 在本窗口/Layout 环境下不可靠（拖拽源在
    // 列表 delegate 内被裁剪，自动拖拽不投递到 DropArea）。改为侧边栏手动拖拽
    // （ModulesPanel onReleased 换算全局坐标 → 视口坐标 → placeModuleByKey 放置）。

    // Section 节点层（z-order 高于背景 MouseArea，确保节点可接收鼠标事件）
    // 性能优化：拖拽平移时不全量 refresh，而是根据 eqCenter 实时计算屏幕坐标
    // 对应 IBR_Misc.h:136 EqPosToRePos: (EqPos - EqCenter) * Ratio + Center
    // Center = 视口中心 = (width/2, height/2)
    // 节点拖拽中：根据 dragOffset（屏幕坐标偏移）实时更新位置，避免每帧 refresh
    Repeater {
        id: sectionRepeater
        // 增量模型（QAbstractListModel）：新建/删除模块时只增删变化的 delegate，
        // 避免整表替换重建全部 SectionNode（卡顿根因）
        model: workspaceController.sectionsModel

        // ===== 共享视口状态（已提升到 workspaceView 根级）=====
        // 平移/缩放时下面各属性整体重算一次；壳只读它们，不再各自解引用
        // eqCenter/ratio/width（1471 壳 × 3 绑定 × ~25 次属性读/帧 → 每壳 ~8 次）。
        // 位置基准：x = localEqX * workspaceView.viewRatio + workspaceView.viewBaseX
        //（EqPosToRePos 展开式）。与 LinkRenderer 的 world→screen 投影共用同一组量，
        // 已提升到 workspaceView 根级（见根级属性注释），壳直接引用不再各自缓存。
        // 外扩 1 屏的活跃视口矩形（Eq 坐标；cull 判定用）
        readonly property real viewMinX: workspaceController.eqCenter.x - workspaceView.width / workspaceController.ratio
        readonly property real viewMaxX: workspaceController.eqCenter.x + workspaceView.width / workspaceController.ratio
        readonly property real viewMinY: workspaceController.eqCenter.y - workspaceView.height / workspaceController.ratio
        readonly property real viewMaxY: workspaceController.eqCenter.y + workspaceView.height / workspaceController.ratio

        // ===== 懒加载壳（culling shell） =====
        // 每项常驻的轻量 Item，只承担几何职责（位置缓存/x/y 绑定/拖拽偏移/选中态）与
        // 视口相交判定；SectionNode 实体由 Loader 按视口按需创建/销毁。
        // 导入 1471 节点时 refreshSections 从全量实例化 ~10s 降为只创建视口内节点；
        // 常驻内存/GC 压力同步大幅下降。
        // 不依赖 delegate 的功能不受影响：LinkRenderer（C++ 快照绘制）、MiniMap、
        // C++ 框选/单点命中（遍历 IBR_SectionMap）、拖拽状态机、编组子模块（父节点渲染）。
        // 视口外连线端点退化为 EqPos 兜底（标题栏锚点），节点入视口创建后回写恢复精确。
        Item {
            id: sectionDelegate
            // QAbstractItemModel 的 delegate 中 modelData 不生成（Qt6 实测 undefined），
            // 改用 role 访问：roleNames 定义了 "sectionData" role
            property var sectionData: model.sectionData
            // 命中测试/查找用（findHitSectionIdStr 读取 .sectionId）
            readonly property var sectionId: sectionData.sectionId
            // z 高于背景 MouseArea（z=0），确保节点 MouseArea 优先接收事件
            z: 1
            // 性能优化：本地缓存的 EqPos。⚠不能用绑定（localEqX: sectionData.eqX）：
            // 拖拽结束的 sectionPositionChanged 处理器会赋值 localEqX，赋值即断绑定，
            // 之后 compose/undo/refresh 更新的 eqX 永远传不进壳 → 组渲染在旧位置，
            // 而连线端点（世界缓存按实时 EqPos 投影）在新位置 → 编组连线偏移。
            // 改为双路径显式同步：dataChanged（全量）+ sectionPositionChanged（增量）。
            property real localEqX: 0
            property real localEqY: 0
            function syncLocalEq() {
                localEqX = sectionData.eqX
                localEqY = sectionData.eqY
                updatePosition()
                // 重定位后必须补一次全量回写：编组成员的头部接受点回写只能靠
                // 组的 updateAllCenters 级联，缺了它成员头缓存停在旧位置 →
                // 端点表（世界缓存优先）投影出偏移的 pb（编组连线偏移根因）。
                // callLater 合帧：多个壳在同一帧的 dataChanged 风暴只触发一次执行。
                // 回写后同步重建端点表：这是世界缓存变更（不是视口变化），
                // 端点表必须重新解析才能反映新值。
                if (nodeLoader.item)
                    Qt.callLater(function() {
                        if (nodeLoader.item && nodeLoader.item.updateAllCenters) {
                            nodeLoader.item.updateAllCenters()
                            workspaceController.flushLinkEndpointsRebuild()
                        }
                    })
            }
            // dataChanged → model.sectionData 被替换 → 此处理器触发（全量同步路径）
            onSectionDataChanged: syncLocalEq()
            // 快照尺寸缓存（只在 dataChanged 时重算；inViewport 判定读自有属性而非 map）
            property real eqW: sectionData.eqW || 0
            property real eqH: sectionData.eqH || 0
            // 选中态（动态绑定，拖拽偏移与 Loader 内容共用）
            property bool isSelected: {
                // 性能优化：通过 selectedRevision 触发重新评估，不依赖全量 refresh
                workspaceController.selectedRevision
                return workspaceController.isSectionSelected(sectionData.sectionId)
            }

            // ===== 命令式位置（绑定瘦身） =====
            // 旧实现 x/y 为声明式绑定：平移/缩放时 1471 个壳 × 3 绑定全部重算（每壳
            // ~25 次跨上下文属性读，约 3-4ms/帧）。改为 updatePosition() 显式更新，
            // 且只对视口内壳调用——视口外壳的位置无人消费，激活时（onInViewportChanged）
            // 再算。公式与触发源和旧绑定完全一致：
            //   x = (localEqX - eqCenter.x)*ratio + width/2 + dragOffset(拖拽中)
            //     = localEqX * viewRatio + viewBaseX + dragOffset
            // 触发：eqCenter/ratio/dragOffset 变化（仅视口内壳）、sectionPositionChanged、
            //       inViewport 翻转、localEq 变化、创建时。
            function updatePosition() {
                var dragTermX = 0, dragTermY = 0
                if (sectionData.sectionId === workspaceController.draggingSectionId
                    || (workspaceController.massDragging && isSelected)) {
                    dragTermX = workspaceController.dragOffset.x
                    dragTermY = workspaceController.dragOffset.y
                }
                x = localEqX * workspaceView.viewRatio + workspaceView.viewBaseX + dragTermX
                y = localEqY * workspaceView.viewRatio + workspaceView.viewBaseY + dragTermY
            }
            Component.onCompleted: syncLocalEq()
            onInViewportChanged: {
                updatePosition()
                // 激活时走合帧回写+flush 路径：与 syncVisibleNode 相同的级联入口，
                // 确保整组（含成员级联）的回写与端点表重建在同一 callLater 批次完成，
                // 消除激活风暴中各成员不同布局阶段的混值。
                if (inViewport) {
                    var it = nodeLoader.item
                    if (it && it.updateAllCenters)
                        Qt.callLater(function() {
                            if (nodeLoader.item && nodeLoader.item.updateAllCenters) {
                                nodeLoader.item.updateAllCenters()
                                workspaceController.flushLinkEndpointsRebuild()
                            }
                        })
                }
            }
            onLocalEqXChanged: updatePosition()
            onLocalEqYChanged: updatePosition()
            // 视口变化（eqCenter/ratio/尺寸）只需重定位壳，不再触发坐标回写级联。
            // 【语义变化】回写写的是世界坐标缓存，不随视口变化；连线端点同样是世界坐标，
            // 由 LinkRenderer 每帧投影跟随。此前这里要对每个可见节点的所有行做
            // mapToItem + C++ 回写，是平移/缩放的主要性能开销，现在彻底消失。
            function syncVisibleNode() {
                if (inViewport) updatePosition()
            }
            Connections {
                target: workspaceController
                function onEqCenterChanged() { syncVisibleNode() }
                function onRatioChanged() { syncVisibleNode() }
                // 拖拽偏移：仅被拖/多选中的可见壳需要跟随（updateAllCenters 由
                // SectionNode 自身的 onDragOffsetChanged → isDragging 级联处理）
                function onDragOffsetChanged() {
                    if (inViewport
                        && (sectionData.sectionId === workspaceController.draggingSectionId
                            || (workspaceController.massDragging && isSelected)))
                        updatePosition()
                }
            }
            // 视口尺寸变化（侧边栏拖宽/窗口 resize）：可见壳重定位（无需回写）
            Connections {
                target: workspaceView
                function onWidthChanged() { syncVisibleNode() }
                function onHeightChanged() { syncVisibleNode() }
            }

            // 视口相交判定（Eq 坐标系）：节点矩形 (localEqX/Y, eqW/eqH) 与外扩 1 屏的
            // 活跃视口矩形相交才激活 Loader。eqCenter/ratio 的依赖收敛到 sectionRepeater
            // 的共享属性（每帧整体重算一次）；eqW/eqH/localEq 为自有属性（快照更新时才变）。
            // localEqX/Y 参与判定：拖拽结束后位置更新即时生效。
            readonly property bool inViewport: {
                return !(localEqX > sectionRepeater.viewMaxX || localEqX + eqW < sectionRepeater.viewMinX
                         || localEqY > sectionRepeater.viewMaxY || localEqY + eqH < sectionRepeater.viewMinY)
            }

            Loader {
                id: nodeLoader
                active: sectionDelegate.inViewport
                sourceComponent: sectionNodeComponent
                // culling 状态同步到 C++：停用时该节点的屏幕坐标回写（LastCenter/
                // acceptPoint）已过期，端点表重建须跳过这些来源、落 EqPos 兜底，
                // 否则平移后视口外节点的连线停在旧屏幕位置（大偏差根因）。
                // 激活时解除门控，delegate 创建 → 回写恢复精确端点。
                onActiveChanged: workspaceController.setSectionCulled(
                                     sectionDelegate.sectionData.sectionId, !active)
            }
            Component {
                id: sectionNodeComponent
                SectionNode {
                    sectionData: sectionDelegate.sectionData
                    // 尺寸完全由 Qt 内容驱动（不依赖 ImGui 的 EqW/EqH）
                    width: implicitWidth
                    height: implicitHeight
                    isSelected: sectionDelegate.isSelected
                    // 渲染后回报实际屏幕尺寸到 C++，同步 sd.EqSize（对应 ImGui 实时更新 EqSize）
                    // 框选命中检测依赖准确的 EqSize，否则框选位置和命中模块不一致
                    // 缩放方案：width/height 为逻辑尺寸（内部已 scale=_r），回报需乘 _r 得视觉尺寸，
                    // 否则 EqSize = 逻辑/ratio 会随缩放漂移。仅在逻辑尺寸变化时触发（缩放不变）→ 天然节流
                    onWidthChanged: {
                        if (workspaceController.diagLogEnabled()) console.log("[LINK-DIAG] SectionNode onWidthChanged sid=" + sectionData.sectionId + " w=" + width + " h=" + height)
                        workspaceController.reportSectionSize(sectionData.sectionId, width * workspaceController.ratio, height * workspaceController.ratio)
                    }
                    onHeightChanged: {
                        if (workspaceController.diagLogEnabled()) console.log("[LINK-DIAG] SectionNode onHeightChanged sid=" + sectionData.sectionId + " w=" + width + " h=" + height)
                        workspaceController.reportSectionSize(sectionData.sectionId, width * workspaceController.ratio, height * workspaceController.ratio)
                    }
                    Component.onCompleted: {
                        workspaceController.reportSectionSize(sectionData.sectionId, width * workspaceController.ratio, height * workspaceController.ratio)
                        // 创建/懒加载激活后，布局完成时做一次全量坐标回写（头部 + 行圆点）。
                        // 壳位置命令式化后，激活路径没有其他 updateAllCenters 触发源：
                        // 行回写有自身 onCompleted，但头部接受点（m_sectionAcceptPoint）
                        // 没有——迷你地图跳转激活编组后成员连线 pb 停留在上一次激活位置
                        //（偏 300+px）的根因。callLater 排在布局完成后，坐标已是最终值。
                        Qt.callLater(function() {
                            updateAllCenters()
                            workspaceController.flushLinkEndpointsRebuild()
                        })
                    }
                }
            }

            // 命中测试转发：WorkspaceView.findSectionAt/findHitAcceptorPoint 遍历
            // itemAt 取 delegate 调用这两个方法；未激活（视口外）的壳返回 null 不参与命中
            //（鼠标只能点到可见内容，可见节点必然已创建，语义不变）
            function hitTestChild(screenX, screenY) {
                var it = nodeLoader.item
                return (it && it.hitTestChild) ? it.hitTestChild(screenX, screenY) : null
            }
            function hitTestAcceptor(wsX, wsY) {
                var it = nodeLoader.item
                return (it && it.hitTestAcceptor) ? it.hitTestAcceptor(wsX, wsY) : null
            }

            // 性能优化：拖拽结束时只更新被拖拽节点的位置，不重建整个 Repeater
            // 注意：不调用 getSectionData（会触发 buildSectionItem → SectionLineModel::refresh，
            //        重建行模型导致卡顿）。直接读 IBR_SectionMap 的 EqPos 通过专用轻量接口。
            Connections {
                target: workspaceController
                    function onSectionPositionChanged(sid) {
                    if (sid === sectionDelegate.sectionData.sectionId) {
                        var pos = workspaceController.getSectionEqPos(sectionDelegate.sectionData.sectionId)
                        sectionDelegate.localEqX = pos.x
                        sectionDelegate.localEqY = pos.y
                        sectionDelegate.updatePosition()
                        // 【修复】模块 EqPos 变化但 SectionNode.x 是相对壳的：壳移动时
                        // SectionNode 自身 onXChanged 不触发 → 圆点世界缓存不回写 →
                        // rel 锚定"回写时的 topAncestor"，读取用当前 topAncestor → 连线
                        // 投影偏移 ΔtopAnc（拖拽/批量移动后"偏移后不动"根因）。
                        // 旧架构靠 onInputStateChanged→updateAllCenters 兜底，三层补偿
                        // 删除时一并移除。这里在 EqPos 变更后补一次回写 + 端点表重建。
                        var it = nodeLoader.item
                        if (it && it.updateAllCenters)
                            Qt.callLater(function() {
                                if (nodeLoader.item && nodeLoader.item.updateAllCenters) {
                                    nodeLoader.item.updateAllCenters()
                                    workspaceController.flushLinkEndpointsRebuild()
                                }
                            })
                    }
                }
            }
        }
    }

    // 框选矩形
    // selectionRect 存储的是 Eq 坐标，需转换为屏幕坐标（与 SectionNode 同公式）
    // screenX = (eqX - eqCenter.x) * ratio + width/2
    // screenW = eqW * ratio
    SelectionBox {
        id: selectionBox
        x: (workspaceController.selectionRect.x - workspaceController.eqCenter.x) * workspaceController.ratio + workspaceView.width / 2
        y: (workspaceController.selectionRect.y - workspaceController.eqCenter.y) * workspaceController.ratio + workspaceView.height / 2
        width: workspaceController.selectionRect.width * workspaceController.ratio
        height: workspaceController.selectionRect.height * workspaceController.ratio
    }

    // ===== 右键菜单（单例统一，host 定义于 Main.qml 根级，此处经动态作用域引用）=====
    // 所有右键入口统一经 contextMenuHost.show() 提交内容（对齐 ImGui IBR_PopupManager 单例）

    // 多选态菜单内容（对应 ImGui MassAfter 右键，IBR_WorkSpace.cpp:1196-1311）
    // 顺序：复制 → 剪切 → 粘贴 → 忽略/取消忽略 → 冻结/解冻 → 隐藏/显示 → 缩合 → 导出模块 → 克隆 → 删除
    function massAfterDescs() {
        var descs = []
        if (workspaceController.massTargetIds().length === 0) return descs
        descs.push({ type: "item", text: (i18n.rev, i18n.tr("GUI_Copy")), action: "copy" })
        descs.push({ type: "item", text: (i18n.rev, i18n.tr("GUI_Cut")), action: "cut" })
        descs.push({ type: "item", text: (i18n.rev, i18n.tr("GUI_Paste")), action: "paste" })
        // 智能互斥：全忽略只显示"取消忽略"，否则只显示"忽略"；冻结/隐藏同理
        if (workspaceController.selectedAllIgnored())
            descs.push({ type: "item", text: (i18n.rev, i18n.tr("GUI_NoIgnore")), action: "unignore" })
        else
            descs.push({ type: "item", text: (i18n.rev, i18n.tr("GUI_Ignore")), action: "ignore" })
        if (workspaceController.selectedAllFrozen())
            descs.push({ type: "item", text: (i18n.rev, i18n.tr("GUI_UnfreezeSec")), action: "unfreeze" })
        else
            descs.push({ type: "item", text: (i18n.rev, i18n.tr("GUI_FreezeSec")), action: "freeze" })
        if (workspaceController.selectedAllHidden())
            descs.push({ type: "item", text: (i18n.rev, i18n.tr("GUI_ShowSec")), action: "show" })
        else
            descs.push({ type: "item", text: (i18n.rev, i18n.tr("GUI_HideSec")), action: "hide" })
        descs.push({ type: "item", text: (i18n.rev, i18n.tr("GUI_Compose")), action: "compose" })
        descs.push({ type: "item", text: (i18n.rev, i18n.tr("GUI_ExportModule")), action: "outputModule" })
        descs.push({ type: "item", text: (i18n.rev, i18n.tr("GUI_Duplicate")), action: "duplicate" })
        descs.push({ type: "item", text: (i18n.rev, i18n.tr("GUI_Delete")), action: "delete" })
        return descs
    }

    // 多选态动作分发
    function dispatchMassAction(action) {
        switch (action) {
        case "copy":        workspaceController.copySelected(); break
        case "cut":         workspaceController.cutSelected(); break
        case "paste":       workspaceController.paste(); break
        case "unignore":    workspaceController.ignoreSelected(false); break
        case "ignore":      workspaceController.ignoreSelected(true); break
        case "unfreeze":    workspaceController.freezeSelected(false); break
        case "freeze":      workspaceController.freezeSelected(true); break
        case "show":        workspaceController.hideSelected(false); break
        case "hide":        workspaceController.hideSelected(true); break
        case "compose":     workspaceController.composeSelected(); break
        case "outputModule": workspaceController.requestOutputModuleDialog(); break
        case "duplicate":   workspaceController.duplicateSelected(); break
        case "delete":      workspaceController.deleteSelected(); break
        }
    }

    // 空白右键动作分发（模块树项在 host 内部处理，这里只处理操作项）
    function dispatchEmptyAction(action) {
        switch (action) {
        case "selectAll":      workspaceController.selectAll(); break
        case "paste":          workspaceController.paste(); break
        case "refreshRegName": projectController.refreshAllRegName(); break
        // 特殊块创建：在空白右键菜单弹出位置（placePos，RePos）创建
        case "createComment":   if (contextMenuHost.placePos) workspaceController.createCommentBlock(contextMenuHost.placePos.x, contextMenuHost.placePos.y); break
        case "createSingleVal": if (contextMenuHost.placePos) workspaceController.createSingleValBlock(contextMenuHost.placePos.x, contextMenuHost.placePos.y); break
        }
    }

    // 连接右键菜单信号
    Connections {
        target: workspaceController
        function onContextMenuRequested(x, y) {
            var g = workspaceView.mapToGlobal(x, y)
            // 有选中模块 → 多选操作菜单；无选中（空白右键）→ 模块树菜单
            if (workspaceController.massTargetIds().length > 0) {
                contextMenuHost.placePos = null
                contextMenuHost.show(workspaceView.massAfterDescs(), g.x, g.y, (a) => workspaceView.dispatchMassAction(a))
            } else {
                // 记录右键的工作区视口坐标，模块菜单选中项时放到该位置
                contextMenuHost.placePos = { x: x, y: y }
                contextMenuHost.show(contextMenuHost.moduleTreeDescs(), g.x, g.y, (a) => workspaceView.dispatchEmptyAction(a))
            }
        }
    }

    // 空状态提示
    Text {
        anchors.centerIn: parent
        visible: workspaceController.sections.length === 0 && workspaceController.isProjectOpen
        color: "#5a5a5a"
        font.pixelSize: 14
        text: (i18n.rev, i18n.tr("GUI_WorkspaceEmpty"))
    }

    // 拖拽目标预览（对应 ImGui DrawDragPreviewIcon，IBR_SectionData.cpp:108-129）
    // ImGui 原版 BeginDragDropSource 内的内容作为拖拽图像跟随鼠标显示（对勾/叉 + "链接到: ..."文本）。
    // 放在 workspaceView 最上层（z=200 > 节点最高 z=100），确保不被任何块遮挡；
    // 位置由拖拽源（键行圆点/标题栏圆点）onPositionChanged 更新（workspaceView 坐标）。
    // 有效（蓝）→ 对勾；无效（红/橙）→ 叉；边框随状态色，使其明显。
    Item {
        id: dragPreview
        x: -1000
        y: -1000
        z: 200
        // 连线拖拽中即显示源标签（不对齐模块时仅源标签跟随鼠标，对齐 imgui 拖拽图像；
        // 命中模块后 dragTargetSectionId 非 0，则下方目标行一并显示）
        visible: workspaceController.hasDraggingLink || workspaceController.hasDragTarget
        width: dragPreviewCol.implicitWidth + 6
        height: dragPreviewCol.implicitHeight + 4

        Rectangle {
            anchors.fill: parent
            color: "#e6262626"
            radius: 3
            // 未命中模块时（仅显示源标签）用白框；命中模块后按目标状态色（绿/红）
            border.color: workspaceController.hasDragTarget
                         ? (workspaceController.dragTargetColor === "#4fc3f7" ? "#4fc3f7" : "#ff5050")
                         : "#ffffff"
            border.width: 1
        }
        Column {
            id: dragPreviewCol
            anchors.left: parent.left
            anchors.leftMargin: 3
            anchors.verticalCenter: parent.verticalCenter
            spacing: 2
            // 连线源预览（对应 imgui BeginDragDropSource 源文本，IBR_LinkNode.cpp:570-573）
            Text {
                visible: workspaceController.dragSourceText.length > 0
                text: workspaceController.dragSourceText
                color: "#d4d4d4"
                // 还原：拖拽预览框非悬停提示，字体随工作区缩放
                font.pixelSize: Math.max(1, Math.round(10 * workspaceController.ratio))
            }
            // 目标命中行（对勾/叉 + 目标文字），仅悬停在模块上时显示
            Row {
                visible: workspaceController.hasDragTarget
                spacing: 3
                // 对勾 / 叉（对应 DrawCheckmark / DrawCross）
                Text {
                    text: workspaceController.dragTargetColor === "#4fc3f7" ? "✓" : "✕"
                    color: workspaceController.dragTargetColor === "#4fc3f7" ? "#4fc3f7" : "#ff5050"
                    font.pixelSize: Math.max(1, Math.round(10 * workspaceController.ratio))
                }
                // 预览文本（"链接到: Xxx -> Yyy" / "无效链接" / "类型不匹配" 等）
                Text {
                    text: workspaceController.dragTargetText
                    color: workspaceController.dragTargetColor === "#4fc3f7" ? "#4fc3f7" : "#ff5050"
                    font.pixelSize: Math.max(1, Math.round(10 * workspaceController.ratio))
                }
            }
        }
    }
}
