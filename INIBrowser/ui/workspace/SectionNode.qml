// SectionNode.qml
// Section 节点组件，对应 IBR_SectionData::RenderUI() 及 8 个子方法
// 显示：标题栏 + 内容区（折叠/展开）+ 状态样式（冻结/隐藏/选中/拖拽）
// 交互：左键拖拽移动、单击选中、右键菜单、双击编辑、折叠按钮
import QtQuick
import QtQuick.Controls
import "../components"

Item {
    id: root

    // 数据属性（由 WorkspaceView 的 Repeater 传入）
    property var sectionData: ({})
    property bool isSelected: false

    // 计算属性
    readonly property bool isFrozen: sectionData.frozen || false
    readonly property bool isHidden: sectionData.hidden || false
    readonly property bool isIgnored: sectionData.ignored || false
    readonly property bool isComment: sectionData.isComment || false
    // 修复：拖拽视觉状态同时检查 draggingSectionId/massDragging
    // 因为 beginMoveSection/beginMassDrag 不再调用 refresh()（避免 Repeater 重建丢失 mouse grab），
    // sectionData.dragging/sectionData.selected 在拖拽期间均为快照旧值（false），需通过
    // draggingSectionId/massDragging + isSelected（动态绑定 isSectionSelected）实时判断。
    // 多选拖动时若用快照 sectionData.selected（false）→ isDragging=false → updateAllCenters
    // 不跳过头部回写 → m_sectionAcceptPoint 被实时刷新（y 跟随），而隐藏行 pbX 用旧 EqPos（不动），
    // onPaint 又因 dstSec.dragging=false 不叠加 dragOffset → 端点只上下动不左右动。
    // 改用 isSelected（WorkspaceView 已通过 selectedRevision+isSectionSelected 动态绑定）后，
    // isDragging=true → 头部回写跳过 → m_sectionAcceptPoint 保持旧值 → pbX/pbY 均旧 →
    // onPaint 叠加 dragOffset → x/y 都正确跟随，且无 y 双重叠加。
    readonly property bool isDragging: (sectionData.dragging || false)
        || (sectionData.sectionId === workspaceController.draggingSectionId)
        || (workspaceController.massDragging && isSelected)
    readonly property bool isEditing: sectionData.isEditing || false
    readonly property bool isCollapsed: sectionData.collapsed || false
    readonly property bool isVirtualBlock: sectionData.isVirtualBlock || false
    readonly property bool isIncluded: sectionData.isIncluded || false
    readonly property bool collapsedInComposed: sectionData.collapsedInComposed || false
    readonly property color registerColor: sectionData.registerColor || "#808080"
    // Import 块标识（ImportCount > 0），对应 ImGui NotAsImported=false
    // ImGui 中导入块的 RadioButton 居中显示（IBR_SectionData.cpp:745-754）
    readonly property bool isImport: sectionData.isImport || false

    // 阶段 D3：子模块标识（递归渲染时由父传入，禁用顶层拖拽行为）
    // 对应 ImGui RenderUI_Virtual 中子模块在父窗口内渲染，不参与顶层窗口管理
    property bool isSubModule: false

    // C11：字体随 Ratio 缩放（对应 ImGui SetWindowFontScale(Ratio)）
    // 必须用真实 ratio（不钳制），字体也去掉最小值钳制，
    // 否则缩小时字体被钳到 8px 而行高跟着停住，但块其他部分继续缩小，
    // 导致行高占比过大、块被拉长。
    readonly property real _r: workspaceController.ratio
    // ===== 缩放方案：内部尺寸/字体全部逻辑化，根节点用 GPU scale 随 _r 缩放 =====
    // 缩放动画期间内部尺寸/字体不变 → 零文本重排/零布局传播（此前每帧全场景重排 15ms）。
    // 位置 x/y 仍为屏幕坐标（WorkspaceView 绑定），transformOrigin 左上保证缩放不偏位。
    // 虚拟块内子模块（isSubModule=true）不重复缩放：由父虚拟块 scale 统一缩放。
    scale: isSubModule ? 1.0 : _r
    transformOrigin: Item.TopLeft
    // ===== 缩放方案结束 =====
    readonly property int fontTitle: 12
    readonly property int fontBody: 11
    readonly property int fontSmall: 10
    readonly property int fontBtn: 11

    // 尺寸（由内容自适应驱动；缩放方案下为逻辑尺寸，视觉尺寸 = 逻辑 × scale(_r)）
    // 宽度：max(WidthFix, wbase)（对应 ImGui IBR_WorkSpace.cpp:1840-1841）
    // 高度：标题栏 + 内容区，折叠时只保留标题栏
    implicitWidth: Math.max(sectionData.widthFix || 0, sectionData.widthBase || 221)
    implicitHeight: header.height + contentContainer.height + (contentContainer.visible ? 4 : 0)

    // 拖拽时置顶 + 透明度（对应 IBR_WorkSpace.cpp:1673-1708 完整透明度计算）
    // v3 批次 1.2：全状态透明度对齐 ImGui：
    //   - 拖拽节点：base * 0.82（行 1676）
    //   - Ignore+MassSelected：base * 0.667（行 1681）
    //   - 普通节点：base（行 1707，所有非拖拽非 Ignore 节点 WindowBg 也乘 TransparencyBase）
    //   - Ignore 非 MassSelected：仅调色（无 alpha 乘数），由 container.color 处理
    z: isDragging ? 100 : (isSelected ? 50 : 1)
    opacity: {
        var base = settingController.transparencyBase;  // 默认 0.8
        if (!base || base <= 0) base = 0.8;  // 保护：设置未加载时 WindowTransparencyLevel=0，使用默认值
        if (isDragging) return base * 0.82;
        if (isIgnored && isSelected) return base * 0.667;
        return base;
    }

    // 可见性
    visible: !isHidden

    // 统一的画布变化更新入口
    // 对应 ImGui 立即模式每帧重算 HeadLineRN + 所有行 LinkNode 坐标
    // 触发场景：节点移动(x/y)、画布平移(EqCenter)、缩放(Ratio)、折叠状态、拖拽(dragOffset)
    // 拖拽中：跳过头部回写（避免 dragOffset 污染 m_sectionAcceptPoint），
    //         LineRow 自身处理 dragOffset（减去后回写 LastCenter）
    // 非拖拽：头部 + 所有 LineRow 全部回写
    onXChanged: {
        updateAllCenters()
    }
    onYChanged: updateAllCenters()
    onIsCollapsedChanged: updateAllCenters()
    onCollapsedInComposedChanged: updateAllCenters()
    onIsDraggingChanged: updateAllCenters()
    Connections {
        target: workspaceController
        function onEqCenterChanged() { updateAllCenters() }
        function onRatioChanged() { updateAllCenters() }
        function onDragOffsetChanged() { if (root.isDragging) updateAllCenters() }
        // 缩放叠加结束：回写缩放后的行圆点坐标（先于 C++ QueuedConnection 的端点表重建入队，
        // 与画布平移收尾的 onInputStateChanged 同模式：先回写、后重建，pb 读到新基准坐标）。
        // force=true：缩放收尾必须绕过 inputState==1/zoomPending 跳过检查——若缩放后立即
        // 拖画布（onMousePress 已把 inputState 置 1），callLater 执行时默认检查会 return，
        // 行圆点停在缩放前旧坐标 → 重建端点表缓存"缩放前连线"→ 拖画布时连线错位/放缩。
        function onZoomFinalizeRequested() {
            Qt.callLater(() => updateAllCenters(true))
        }
        // 状态切换（含画布平移结束 state 1 → 其他）：延迟到事件循环末尾触发全量坐标回写。
        // 必须延迟：updateInputState 的 emit 发生时 dragOffset 等拖拽状态可能还没清零，
        // 同步回写会读到含旧 dragOffset 的位置污染基准。Qt.callLater 先于 C++ 的
        // QueuedConnection 端点表重建入队 → 先回写、后重建，端点表固化为平移后的正确基准。
        function onInputStateChanged() {
            if (workspaceController.inputState !== 1) {
                Qt.callLater(() => updateAllCenters())
            }
        }
    }

    function updateAllCenters(force) {
        // 画布平移中 / 缩放叠加中：端点表保持快照不重建（canvasDragOffset/zoomPending 叠加渲染），
        // 跳过全部回写避免每帧全量端点表重建（拖动画布/缩放帧率被拖垮）。
        // force=true 用于缩放收尾（onZoomFinalizeRequested）：缩放结束后必须把缩放后的
        // 行圆点坐标回写（即使随后立即进入画布平移 inputState==1），否则重建端点表会
        // 读到缩放前旧坐标 → 缓存"缩放前连线"→ 拖画布时连线错位/放缩
        if (!force && (workspaceController.inputState === 1 || workspaceController.zoomPending)) return
        // 头部坐标：拖拽中不回写（松手时 isDragging→false 触发回写新位置）
        if (!root.isDragging) {
            headLineRN.updateRNCenter()
        }
        // 键行坐标：始终回写（LineRow 内部根据 isDragging 决定是否减去 dragOffset）
        for (var i = 0; i < lineColumnRepeater.count; ++i) {
            var item = lineColumnRepeater.itemAt(i)
            if (item && item.updateLinkNodeCenter) {
                item.updateLinkNodeCenter(force)
            }
        }
        // 折叠子模块：嵌套在父虚拟块 Column 内，父块移动/变化时其自身 onXChanged 不触发，
        // 必须由父块级联回写 headLineRN/RadioButton 坐标，否则折叠子模块的连线与拖拽/伸缩不同步。
        if (subModuleRepeater) {
            for (var j = 0; j < subModuleRepeater.count; ++j) {
                var slot = subModuleRepeater.itemAt(j)
                if (slot && slot.subNodeLoader && slot.subNodeLoader.item) {
                    var sub = slot.subNodeLoader.item
                    // 递归级联：子模块自身也可能是虚拟块（嵌套编组），其内部子模块一并更新
                    if (sub.updateAllCenters) sub.updateAllCenters(force)
                }
            }
        }
    }

    // 拖拽目标命中 + 预览（供标题栏圆点拖拽复用）
    // toX/toY 为鼠标在 workspaceView 坐标系的坐标；srcId 为拖拽源节点；linkType/isLinkDrag 区分连线/合并
    // 命中 → checkMergePreview/mergePreviewText 生成预览 → setDragTarget 通知目标节点显示预览框
    // 未命中或命中源自身 → setDragTarget(0) 清除预览
    function updateDragTarget(toX, toY, srcId, linkType, isLinkDrag) {
        var targetId = workspaceController.hitTestSection(toX, toY)
        if (targetId && targetId !== srcId) {
            var code = workspaceController.checkMergePreview(srcId, targetId, linkType, isLinkDrag)
            var text = workspaceController.mergePreviewText(srcId, targetId, linkType, isLinkDrag)
            // 颜色规则沿用原 DropArea onEntered 逻辑：
            //   lineDrag：code==0 蓝、否则红
            //   sectionDrag：code==1 蓝、code==2 橙、否则红（0=允许,2=无默认链接 key）
            var color = (code === 0) ? "#4fc3f7"
                      : (isLinkDrag ? "#d63b3b"
                                    : (code === 2 ? "#d6a23b" : "#d63b3b"))
            workspaceController.setDragTarget(targetId, color, text)
        } else {
            workspaceController.setDragTarget(0, "", "")
        }
    }

    // 修复：移除 Drag.active，因为它会接管鼠标事件导致 nodeMouseArea.onPositionChanged 停止触发
    // 节点移动完全由 C++ updateDrag 处理（对应 ImGui SetWindowPos）
    // 节点合并改用 onReleased 时检查鼠标位置是否在另一节点上（后续实现）
    // 原 Drag 系统会阻止 MouseArea 的 mouse move 事件，导致节点拖不动

    // 主容器
    // z:1 让 container 在 nodeMouseArea 之上
    // 非交互控件（Text/Rectangle）点击穿透到 nodeMouseArea（选中/拖拽）
    // 交互控件（TextField）的点击由自身处理
    // onShowLabel 双击用 TapHandler（不拦截单击），保证单击仍能穿透选中
    // 是否为多选状态（MassAfter=4 / MassSelecting=2）
    // 单选用边框，多选用覆盖层变色（对应 ImGui：单选 ActivateAndEdit 有焦点边框，多选 MassAfter 用 ForegroundCoverColor）
    readonly property bool isMassSelectState: workspaceController.inputState === 4
                                              || workspaceController.inputState === 2

    Rectangle {
        id: container
        anchors.fill: parent
        z: 1
        // Ignore 节点颜色变暗（对应 IBR_WorkSpace.cpp:1677-1699 DarkMode 加 0.3 亮度）
        color: isIgnored ? "#1a1a1a" : "#2d2d2d"
        // 边框：编辑/拖拽用青色，单选选中用蓝色，多选选中不加框（用覆盖层）
        border.color: isEditing ? "#4fc3f7"
                               : (isDragging ? "#4fc3f7"
                                   : (isSelected && !isMassSelectState ? "#007acc" : "#3c3c3c"))
        border.width: (isEditing || isDragging || (isSelected && !isMassSelectState)) ? 2 : 1
        radius: 3

        // 多选覆盖层（对应 ImGui IBR_Components.cpp:994 CommitRectFilled(ForegroundCoverColor)）
        // 仅多选状态显示，单选用边框
        Rectangle {
            id: selectionOverlay
            anchors.fill: parent
            color: "#5a0091ff"
            visible: isSelected && isMassSelectState && !isEditing
            radius: parent.radius
            z: 10
        }

        // 标题栏（对应 RenderUI_TitleBar）
        // 背景色用注册表颜色填充（对应 ImGui AddRectFilled(UCol)，IBR_SectionData.cpp:738）
        // Ignore 时变暗
        Rectangle {
            id: header
            anchors.top: parent.top
            anchors.left: parent.left
            anchors.right: parent.right
            // 标题栏高度（逻辑尺寸，视觉高度 = 逻辑 × scale）
            height: 28
            // 注册表颜色填充标题栏（对应 ImGui UCol），Ignore 时用暗色
            color: isIgnored ? "#1e1e1e" : registerColor
            radius: 3

            // E3：标题栏 RadioButton（对应 IBR_SectionData.cpp:757-774 ImGui::RadioButton）
            // 既是整块拖拽源视觉标记，还是折叠态连线锚点
            // Ignore 节点 RadioButton 变白（对应 TempWbg）
            // 仅负责视觉；拖拽源挂在 headDragProxy 上（对应 ImGui BeginDragDropSource IBR_SecDrag）
            Rectangle {
                id: headLineRN
                // Import 块 RadioButton 居中（对应 ImGui ImportCount>0 时 X=半宽-0.5*FontHeight）
                // 普通块 RadioButton 在左侧（对应 ImGui X=0.7*FontHeight）
                anchors.horizontalCenter: root.isImport ? parent.horizontalCenter : undefined
                anchors.left: root.isImport ? undefined : parent.left
                anchors.leftMargin: root.isImport ? 0 : 8
                anchors.verticalCenter: parent.verticalCenter
                // 与键行 LinkNode 同尺寸同样式（统一节点视觉）：fontSmall*1.5 逻辑尺寸，circle 圆点
                // 两层绘制：底层透明灰色圆（hover 提亮），上层不透明颜色层
                width: root.fontSmall * 1.5
                height: width
                radius: width / 2  // Circle 样式（对应 GlobalNodeStyle=Circle）
                // 低一位图层：透明灰色圆，鼠标放上后提高亮度
                // 内层颜色圆用 anchors.centerIn 与之同心（颜色圆圆心 = 灰圆圆心）
                color: headRNMouseArea.containsMouse ? "#c0c8c8c8" : "#77a0a0a0"
                visible: !isComment

                // 上层：不透明颜色层（对齐 imgui）：非 Ignore 用默认 CheckMark
                //（深色主题 0.26/0.59/0.98 ≈ #4296fa，imgui_draw.cpp:213）；Ignore 用 TempWbg 白色
                //（对应 IBR_SectionData.cpp:743/744）
                // 显式按父中心计算 x/y 居中，保证与底层灰色圆严格同心（不做 anchors 子像素近似）
                Rectangle {
                    x: (parent.width - width) / 2
                    y: (parent.height - height) / 2
                    width: parent.width * 0.7
                    height: width
                    radius: width / 2  // Circle 样式
                    color: isIgnored ? "#ffffff" : "#4296fa"
                }

                // 阶段 1/3：坐标回写（对应 ImGui HeadLineRN → Session.LastCenter）
                // 折叠态：回写 setHeadLineRN（连线源端点汇聚到头部，对应 RenderUI_Collapsed）
                // 非折叠态：回写 setSectionAcceptPoint（目标接受点，对应 ReWindowUL+ReOffset）
                function updateRNCenter() {
                    if (root.sectionData === undefined || root.sectionData.sectionId === undefined) return;
                    if (!visible) return;
                    var globalPos = headLineRN.mapToItem(workspaceView, headLineRN.width/2, headLineRN.height/2)
                    if (root.isCollapsed || root.collapsedInComposed) {
                        workspaceController.setHeadLineRN(root.sectionData.sectionId, globalPos.x, globalPos.y)
                    } else {
                        workspaceController.setSectionAcceptPoint(root.sectionData.sectionId, globalPos.x, globalPos.y)
                    }
                }
                Component.onCompleted: updateRNCenter()
            }

            // sectionDrag 拖拽代理（对应 ImGui IBR_SectionData.cpp:761-774 BeginDragDropSource IBR_SecDrag）
            // 仅作为 headRNMouseArea.drag.target 的可移动载体（目标命中改由鼠标坐标命中测试
            // hitTestSection 驱动，不再使用 Qt Drag/DropArea，因此无需 Drag attached 属性）。
            // 不可见 Item 不参与渲染，不影响布局。
            Item {
                id: headDragProxy
                width: headLineRN.width
                height: headLineRN.height
                visible: false
            }

            // sectionDrag 拖拽源 MouseArea（对应 ImGui IBR_SectionData.cpp:761-774 BeginDragDropSource IBR_SecDrag）
            // 从标题 RadioButton 拖出 → 鼠标命中目标节点 → mergeSectionToSection 合并
            // 单击圆点 → 选中节点（对应 ImGui CurOnRender_Clicked）
            // 注意：drag.target 必须是非祖先（兄弟），否则移动 target 会连带移动 MouseArea 自身
            MouseArea {
                id: headRNMouseArea
                anchors.fill: headLineRN
                acceptedButtons: Qt.LeftButton
                hoverEnabled: true  // 驱动底层透明灰色圆的悬停提亮（headLineRN.color 绑定 containsMouse）
                drag.target: headDragProxy
                drag.threshold: 4
                drag.axis: Drag.XAndYAxis
                preventStealing: true
                onPressed: {
                    // 拖拽代理对齐到圆点位置（drag.target 移动的起始点）
                    headDragProxy.x = headLineRN.x
                    headDragProxy.y = headLineRN.y
                }
                // 滚轮转发：合并拖拽期间 grab 在本 MouseArea，转发滚轮保证拖动中可缩放
                onWheel: {
                    var wsPt = mapToItem(workspaceView, wheel.x, wheel.y)
                    workspaceController.onWheel(wsPt.x, wsPt.y, wheel.angleDelta.y)
                }
                onReleased: {
                    // 用拖拽终点（或按下位置）命中目标节点，命中则合并（对应 IBR_SecDrag 落点）
                    var toPos = mapToItem(workspaceView, mouse.x, mouse.y)
                    var targetId = workspaceController.hitTestSection(toPos.x, toPos.y)
                    if (targetId && targetId !== (root.sectionData.sectionId || 0)) {
                        workspaceController.mergeSectionToSection(root.sectionData.sectionId, targetId)
                    }
                    workspaceController.clearDraggingLink()
                }
                // 拖拽中：实时更新 Bezier 预览线 + 目标命中预览（对应 ImGui RenderUI_Links）
                onPositionChanged: {
                    if (drag.active) {
                        // 起点 = 标题栏圆点中心在 workspaceView 坐标系中的位置
                        var fromPos = headLineRN.mapToItem(workspaceView, headLineRN.width / 2, headLineRN.height / 2)
                        // 终点 = 鼠标在 workspaceView 坐标系中的位置
                        var toPos = mapToItem(workspaceView, mouse.x, mouse.y)
                        workspaceController.setDraggingLink(fromPos.x, fromPos.y, toPos.x, toPos.y)
                        // 预览框跟随鼠标（对应 ImGui 拖拽图像）：toPos 已是 workspaceView 坐标
                        workspaceView.dragPreviewItem.x = toPos.x + 8
                        workspaceView.dragPreviewItem.y = toPos.y + 8
                        // 目标命中 + 预览（sectionDrag：合并）
                        root.updateDragTarget(toPos.x, toPos.y, root.sectionData.sectionId, "", false)
                    }
                }
                onClicked: {
                    // 单击 RadioButton 选中节点（非子模块）
                    if (!root.isSubModule) {
                        workspaceController.toggleSelectSection(root.sectionData.sectionId, false)
                    }
                }
            }

            // 显示名（阶段 12.9：根据 showRegName 切换显示名/寄存器名）
            // ImGui 中 Import 块（NotAsImported=false）不显示显示名，只显示居中的 RadioButton
            // 非 Import 块显示名占满标题栏右侧
            Text {
                anchors.left: headLineRN.right
                anchors.leftMargin: 6
                anchors.right: parent.right
                anchors.rightMargin: 4
                anchors.verticalCenter: parent.verticalCenter
                visible: !root.isImport
                text: (workspaceController.showRegName
                       ? (sectionData.registerName || "")
                       : (sectionData.displayName || ""))
                color: isIgnored ? "#5a5a5a" : "#ffffff"
                font.pixelSize: root.fontTitle
                elide: Text.ElideRight
            }
        }

        // 内容区（对应 ImGui RenderUI_Lines / RenderUI_Comment / RenderUI_Virtual）
        // 普通块：行列表用 SectionLineModel 暴露的 OnShow/LinkNode/Links 等字段
        // 注释块：可编辑文本区 commentArea（对应 RenderUI_Comment InputTextMultiline）
        // 虚拟块：递归渲染 IncludingModules 子模块（对应 RenderUI_Virtual fold_left 累加 FinalY）
        Item {
            id: contentContainer
            anchors.top: header.bottom
            anchors.left: parent.left
            anchors.right: parent.right
            // 子模块在父虚拟块中折叠时只显示标题栏（对应 CollapsedInComposed）
            // 注释块也要显示（原本 `!isComment` 把注释主体整个隐藏，只剩 28px 标题栏 → 输入框看不到）
            visible: !isCollapsed && !collapsedInComposed
            // 高度由 Qt 内容驱动（Column.implicitHeight 累加所有行高度），不依赖 EqH
            // 注释块：高度取文本区隐式高度，至少 80px 保证可编辑框可见
            height: visible ? (isComment
                               ? Math.max(commentArea.implicitHeight + 8, 80)
                               : (isVirtualBlock ? virtualBlockContainer.height
                                                 : lineColumn.implicitHeight + 8))
                           : 0

            // ===== 普通块：行列表 =====
            // 用 Column + Repeater 替代 ListView：
            // ListView 有延迟加载机制，当自身高度未确定时只创建少量 delegate，
            // 导致 contentHeight 不准（与 contentContainer.height 形成死循环），
            // 模块高度远小于实际键数。Column 立即创建所有子项，implicitHeight 自动累加，
            // 这才是 Qt 自己算的尺寸。
            Column {
                id: lineColumn
                visible: !isVirtualBlock
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: parent.top
                anchors.margins: 4
                spacing: 0

                Repeater {
                    id: lineColumnRepeater
                    // model 由 SectionLineModel 提供（sectionData.lineModel）
                    model: sectionData.lineModel || null

                    delegate: LineRow {
                        width: lineColumn.width
                        // 传入 Section 上下文
                        sectionData: root.sectionData
                        lineModel: sectionData.lineModel
                        rowIndex: index
                        fontBody: root.fontBody
                        fontSmall: root.fontSmall
                        // 传入拖拽状态（拖拽中实时回写 LastCenter，减去 dragOffset）
                        isDragging: root.isDragging
                        // model roles 显式绑定
                        onShowText: model.onShow
                        descLong: model.descLong
                        hasLinkNode: model.hasLinkNode
                        linkCol: model.linkCol
                        linkLimit: model.linkLimit
                        isEmpty: model.isEmpty
                        isInherit: model.isInherit
                        isImport: model.isImport
                        linkType: model.linkType
                        sessionId: model.sessionId
                        links: model.links
                        exportValue: model.exportValue
                        lineIdx: model.lineIdx
                        lineMult: model.lineMult
                        compIdx: model.compIdx
                        keyName: model.lineKey
                        // D14：IICStatus 持久化绑定（从 model role 读取）
                        isInputMode: model.isInputMode
                        // 行级增行按钮 + 右键菜单 SpecialAccept + EditDesc
                        isMultiple: model.isMultiple
                        specialAccept: model.specialAccept
                        inputOnShow: model.inputOnShow
                    }
                }
            }

            // ===== 虚拟块：递归渲染子模块 =====
            // 对应 ImGui RenderUI_Virtual（IBR_SectionData.cpp:870-935）
            // fold_left 遍历 IncludingModules，每个子模块调 Data->RenderUI()
            // QML 用 Column 自动累加高度（对应 FinalY 累加）
            // 注意：QML 不允许组件直接递归实例化自身，必须用 Loader + Component 打破递归
            Column {
                id: virtualBlockContainer
                visible: isVirtualBlock
                anchors.left: parent.left
                anchors.right: parent.right
                // 对应 ImGui SetCursorPosY(+TextLineHeight * 0.5) 间距
                spacing: 6

                Repeater {
                    id: subModuleRepeater
                    model: (isVirtualBlock && sectionData.includingModules) ? sectionData.includingModules : []

                    delegate: Item {
                        id: subModuleSlot
                        width: virtualBlockContainer.width
                        // 高度由子 SectionNode 内容决定（对应 ImGui FinalY）
                        // 隐藏的子模块不占垂直空间（对应 ImGui 中 Hidden 计数但不渲染）
                        height: (subModuleSlot.subData.hidden || false) ? 0 : subNodeLoader.item ? subNodeLoader.item.height : 0
                        visible: !(subModuleSlot.subData.hidden || false)

                        // 折叠子模块被父 Column 重排推动时（展开/收起某个模块会推挤其他子模块的 y），
                        // 自身 x/y 属性不变不触发 SectionNode.onYChanged，此处监听 slot.y 变化级联回写，
                        // 否则其他被推动的折叠子模块连线端点停原地、与拖拽/伸缩不同步。
                        onYChanged: {
                            if (subNodeLoader.item && subNodeLoader.item.updateAllCenters) {
                                subNodeLoader.item.updateAllCenters(true)
                            }
                        }

                        // 阶段 D2：按需查询子模块数据（对应 GetSectionFromID(id).GetSectionData()）
                        // D20：依赖 sectionDataRevision，sectionsChanged 时自动重新查询
                        // ImGui 每帧调 Data->RenderUI() 天然实时，Qt 用版本号触发绑定重算
                        property var subId: modelData
                        property var subData: {
                            var _rev = workspaceController.sectionDataRevision
                            return workspaceController.getSectionData(subId)
                        }

                        // 递归渲染子模块（对应 Data->RenderUI()）
                        // 用 Loader + source 加载 SectionNode.qml 打破直接递归实例化
                        // QML 引擎在解析期禁止组件直接实例化自身，Loader 延迟到运行期加载可规避此限制
                        Loader {
                            id: subNodeLoader
                            active: !((subModuleSlot.subData.hidden || false))
                            source: "qrc:/INIWeaver/INIBrowser/ui/workspace/SectionNode.qml"
                            // 传递子模块上下文给 Loader 加载的实例
                            onLoaded: {
                                // 用绑定而非一次性赋值：子模块 sectionData 需随 sectionDataRevision
                                // 变化实时刷新（编组展开/收起改 CollapsedInComposed 后，一次性赋值拿的是旧快照，
                                // 界面永远停在折叠态）。Qt.binding 让其跟随 subModuleSlot.subData 重算。
                                item.sectionData = Qt.binding(() => subModuleSlot.subData)
                                // 性能优化：通过 selectedRevision 触发重新评估
                                item.isSelected = Qt.binding(() => {
                                    workspaceController.selectedRevision
                                    return workspaceController.isSectionSelected(subModuleSlot.subData.sectionId)
                                })
                                item.isSubModule = true
                                item.width = Qt.binding(() => subModuleSlot.width)
                            }
                        }

                        // 冻结遮罩（对应 ImGui AddRectFilled(PosUL, PosDR, FrozenMask)）
                        // 只对非隐藏的冻结子模块画遮罩
                        Rectangle {
                            anchors.fill: parent
                            color: "#80808080"
                            radius: 3
                            visible: (subModuleSlot.subData.frozen || false)
                                     && !(subModuleSlot.subData.hidden || false)
                            z: 10
                        }
                    }
                }
            }

            // ImGui 原版 RenderUI_Lines 在 LineOrder 为空时不渲染任何提示，节点内部留空
            // (IBR_SectionData.cpp:951-976 for 循环不执行，无 Empty/NoLines 文本)

            // ===== 注释块：可编辑文本区 =====
            // 对应 ImGui RenderUI_Comment InputTextMultiline；填满整个内容区（内容多时滚动）
            TextArea {
                id: commentArea
                visible: isComment
                anchors.fill: parent
                anchors.margins: 4
                clip: true
                wrapMode: TextArea.Wrap
                // 显示注释正文（对应 Bsec->Comment）。不能用 displayName：注释的 displayName 是
                // 注册名"注释块"，编辑后写回的是 Comment，若绑定 displayName 会一直显示"注释块"且被回写重置
                text: sectionData.comment || ""
                color: "#d4d4d4"
                font.pixelSize: root.fontTitle
                selectByMouse: true
                // 编辑后回写 Comment（对应 ImGui InputTextMultiline 回调）
                onEditingFinished: {
                    if (text !== (sectionData.comment || "")) {
                        workspaceController.updateCommentText(sectionData.sectionId, text)
                    }
                }
                // 可见输入框外观（深底 + 边框，区别于纯透明，明确可编辑）
                background: Rectangle {
                    color: "#1e1e1e"
                    border.color: "#3c3c3c"
                    border.width: 1
                    radius: 2
                }
            }
        }

        // 冻结遮罩（对应 FrozenMaskColor）
        Rectangle {
            anchors.fill: parent
            color: "#80808080"
            visible: isFrozen
            radius: 3
            z: 10
        }

        // 阶段 12.3：虚拟块"显示所有内部块"按钮（对应 IBR_SectionData.cpp:925-934）
        // 当虚拟块有隐藏的子模块时，底部显示按钮
        Rectangle {
            id: showAllButton
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            height: 20
            visible: (sectionData.isVirtualBlock || false) && (sectionData.hiddenCount || 0) > 0
            color: "#3c3c3c"
            radius: 3

            Text {
                anchors.centerIn: parent
                text: (i18n.rev, i18n.trF("GUI_ShowAllIncludingBlocks",
                        [sectionData.hiddenCount || 0]))
                color: "#4fc3f7"
                font.pixelSize: root.fontBody
            }

            MouseArea {
                anchors.fill: parent
                onClicked: workspaceController.showAllIncludingBlocks(sectionData.sectionId)
            }
        }

        // 阶段 12.3：被包含子块的折叠/展开按钮（对应 IBR_SectionData.cpp:835-868, 937-949）
        // isIncluded=true 的子模块显示 "+展开"/"-折叠" 小按钮
        Rectangle {
            id: foldToggle
            anchors.right: parent.right
            anchors.top: parent.top
            width: 24
            height: 16
            visible: (sectionData.isIncluded || false) && !isComment
            color: "#3c3c3c"
            radius: 2
            z: 5

            Text {
                anchors.centerIn: parent
                text: (sectionData.collapsedInComposed || false) ? "+" : "−"
                color: "#cccccc"
                font.pixelSize: root.fontTitle
            }

            MouseArea {
                anchors.fill: parent
                enabled: (sectionData.isIncluded || false) && !isComment
                onClicked: {
                    // DIAG：确认点击是否到达本 MouseArea（编组展开无反应排查）
                    if (workspaceController.diagLogEnabled())
                        console.log("[QML-FOLD] foldToggle onClicked sid=" + sectionData.sectionId
                                    + " collapsedInComposed=" + (sectionData.collapsedInComposed || false)
                                    + " isIncluded=" + (sectionData.isIncluded || false))
                    workspaceController.toggleCollapseInComposed(
                        sectionData.sectionId, !(sectionData.collapsedInComposed || false))
                }
            }
        }

        // 注释块编辑区已移至 contentContainer 内（commentArea，对应 RenderUI_Comment）
    }

    // LinkPoint 已移至每行右端（LinkNodePoint.qml），对应 ImGui RadioButton 位置
    // 不再在节点左右边缘放 LinkPoint

    // 拖拽目标预览已移至 WorkspaceView 顶层（dragPreview，跟随鼠标显示，对应 ImGui DrawDragPreviewIcon）
    // 本文件不再持有预览框，位置与内容由 headRNMouseArea.onPositionChanged 更新 workspaceView.dragPreview

    // 节点交互 MouseArea（覆盖整个节点）
    // 左键拖拽移动、单击选中、右键菜单、双击编辑
    // 阶段 12.1：根据 inputState 分发到单节点拖拽或 MassAfter 多节点拖拽
    // C11：冻结节点禁用拖拽/编辑（对应 ImGui NoInputs IBR_WorkSpace.cpp:1725）
    MouseArea {
        id: nodeMouseArea
        anchors.fill: parent
        acceptedButtons: Qt.LeftButton | Qt.RightButton
        hoverEnabled: true
        // 修复：防止父级 MouseArea（背景 mouseArea）在鼠标移出节点边界时窃取事件
        // 拖拽时鼠标可能移出节点范围，preventStealing 确保拖拽期间 nodeMouseArea 持有 grab
        preventStealing: true
        // 冻结节点不可拖拽/编辑，但仍可选中（对应 ImGui NoInputs 仅禁用交互，不禁用选择）
        enabled: !isFrozen

        property point dragStart: Qt.point(0, 0)
        property point dragStartWs: Qt.point(0, 0)
        property bool hasDragged: false
        property bool dragStarted: false  // 是否已调用 beginMoveSection/beginMassDrag
        // 鼠标坐标映射到 WorkspaceView 坐标系（避免节点拖拽时自身坐标漂移）
        // 对应 ImGui 使用全局屏幕坐标进行拖拽计算
        function mapToWorkspace(mx, my) {
            return mapToItem(workspaceView, mx, my)
        }

        // 滚轮转发：模块拖拽期间鼠标 grab 在本 MouseArea 上，显式转发滚轮到
        // 工作区缩放入口（与背景 onWheel 一致），保证拖动中滚轮缩放可用；
        // C++ 侧拖拽中缩放与非拖拽一致（鼠标锚定），模块随画布同步缩放
        onWheel: {
            var wsPt = mapToWorkspace(wheel.x, wheel.y)
            workspaceController.onWheel(wsPt.x, wsPt.y, wheel.angleDelta.y)
        }

        onPressed: {
            // DIAG：检测点击是否被 nodeMouseArea 拦截（编组展开排查；若 sid 与折叠按钮区域重叠则被拦截）
            if (workspaceController.diagLogEnabled())
                console.log("[QML-NODE] nodeMouseArea onPressed sid=" + sectionData.sectionId
                            + " x=" + mouseX + " y=" + mouseY
                            + " w=" + width + " overFold=" + (mouseX > width-30 && mouseY < 20))
            dragStart = Qt.point(mouseX, mouseY)
            hasDragged = false
            dragStarted = false
            if (mouse.button === Qt.LeftButton) {
                // 行注释（InputOnShow）编辑中：不抢左键点击。
                // 否则点击编辑框自身（含最右侧空白处）会先命中本 MouseArea → 松手触发
                // toggleSelectSection 选中模块 → 刷新重建 → 编辑框实例销毁 → 失焦/误提交。
                // 保持编辑框持久聚焦（点击编辑框内任意位置都不应失焦），键盘/失焦变速箱不变。
                if (sectionData.lineModel && sectionData.lineModel.hasActiveInputOnShow()) {
                    return
                }
                // 阶段 D3：子模块不参与顶层拖拽状态机（由父虚拟块整体拖拽）
                // 但保留单击选中与双击编辑（对应 ImGui CurOnRender_Clicked 命中即激活）
                if (!isSubModule) {
                    // 记录按下时鼠标在 WorkspaceView 中的坐标（用于计算 dragOffset）
                    var wsPt = mapToWorkspace(mouseX, mouseY)
                    dragStartWs = Qt.point(wsPt.x, wsPt.y)
                }
            }
        }

        onPositionChanged: {
            if (pressed && (pressedButtons & Qt.LeftButton)) {
                // 修复：统一使用 WorkspaceView 屏幕坐标计算偏移
                // mouseX/mouseY 是节点局部坐标，dragOffset 应用到节点 x/y 后会反向影响 mouseX
                // 导致 dx = mouseX - dragStart.x 只反映"上次更新以来的偏移"，dragOffset 会卡住
                // mapToItem(workspaceView, mx, my) 输出鼠标在 workspaceView 中的实际位置
                // 该位置不受节点位置变化影响（节点 x 移动 D → mouseX 反向减 D → 和不变）
                var wsPt = mapToWorkspace(mouseX, mouseY)
                var dx = wsPt.x - dragStartWs.x
                var dy = wsPt.y - dragStartWs.y
                if (!hasDragged && (dx*dx + dy*dy) > 9) {
                    hasDragged = true
                }
                // 阶段 D3：子模块不触发拖拽更新
                if (hasDragged && !isSubModule) {
                    // 修复：延迟触发 beginMoveSection/beginMassDrag
                    // 只在确认移动超过阈值后才进入拖拽状态机，单击选中不触发拖拽流程
                    // 避免 beginMoveSection 读取/写回 EqPos 导致的位置累积问题
                    if (!dragStarted) {
                        dragStarted = true
                        if (workspaceController.inputState === 4
                            && workspaceController.isSectionSelected(sectionData.sectionId)) {
                            // MassAfter 状态下点击已选中节点 → 多节点整组拖拽（保持选中）
                            // 对应 ImGui IBR_WorkSpace.cpp:1291-1308 MassAfter → HoldingModules
                            workspaceController.beginMassDrag(dragStartWs.x, dragStartWs.y)
                        } else {
                            // Normal 状态或 MassAfter 状态下点击未选中节点 → 单节点拖拽
                            // 对应 IBR_WorkSpace.cpp:1004 HoldingModules 进入
                            workspaceController.beginMoveSection(sectionData.sectionId, dragStartWs.x, dragStartWs.y)
                        }
                    }
                    // 阶段 12.1：统一调用 updateDrag，传 WorkspaceView 屏幕坐标
                    // C++ 侧 dragOffset = curPos - dragStartScreenPos（与 beginMoveSection/beginMassDrag 一致）
                    workspaceController.updateDrag(wsPt.x, wsPt.y)
                }
            }
        }

        onReleased: {
            if (mouse.button === Qt.LeftButton) {
                // 行注释编辑中：与 onPressed 同理，不执行选中/拖拽收尾（保持编辑框焦点）
                if (sectionData.lineModel && sectionData.lineModel.hasActiveInputOnShow()) {
                    return
                }
                // 拖动结束：endDrag 内部会处理选中状态
                // - 单节点拖动 → endMoveSection 选中该模块（对应 ImGui ActivateAndEdit）
                // - 多节点拖动 → endMassDrag 取消选中（对应 ImGui IsMassAfter=false）
                if (!isSubModule && dragStarted) {
                    workspaceController.endDrag()
                } else if (!hasDragged) {
                    // 注释块不可单击单选（对应 ImGui：点击注释块进入编辑，不触发单选；
                    // 仍可通过框选/多选选中）。普通块单击选中（additive = 是否按住 Ctrl/Shift）
                    if (!isComment) {
                        var additive = (mouse.modifiers & Qt.ControlModifier) || (mouse.modifiers & Qt.ShiftModifier)
                        workspaceController.toggleSelectSection(sectionData.sectionId, additive)
                    }
                }
            } else if (mouse.button === Qt.RightButton) {
                var g = nodeMouseArea.mapToGlobal(mouseX, mouseY)
                // 多选态（MassAfter）右键选中模块 → 多选操作菜单：模块身体任意位置都可触发
                // （对齐 ImGui：多选状态下右键任意选中模块弹多选菜单，不做标题栏限制）
                var massMenu = (workspaceController.inputState === 4
                                && workspaceController.isSectionSelected(root.sectionData.sectionId)
                                && workspaceController.massTargetIds().length > 1);
                // 右键触发条件：多选菜单 → 任意位置；单节点菜单 → 仅标题栏（对齐 ImGui
                // RenderUI_TitleBar 标题栏右键，内容区不弹菜单）
                if (massMenu || mouseY <= header.height) {
                    if (massMenu) {
                        contextMenuHost.show(workspaceView.massAfterDescs(), g.x, g.y,
                                             (a) => workspaceView.dispatchMassAction(a))
                    } else {
                        contextMenuHost.show(root.buildSectionDescs(), g.x, g.y,
                                             (a) => root.dispatchSectionAction(a))
                    }
                }
            }
        }

        onDoubleClicked: {
            if (mouse.button === Qt.LeftButton) {
                // 行注释编辑中：不进入模块编辑（保持编辑框焦点）
                if (sectionData.lineModel && sectionData.lineModel.hasActiveInputOnShow()) {
                    return
                }
                // 双击进入编辑模式（对应 IBR_EditFrame::ActivateAndEdit）
                workspaceController.activateAndEdit(sectionData.sectionId)
            }
        }
    }

    // 单节点右键菜单（对应 IBR_SectionData.cpp:566-806 RenderUI_TitleBar 右键菜单）
    // 顺序对齐 ImGui（IBR_SectionData.cpp:627-729）：
    //   忽略/冻结/隐藏 → 折叠/展开(虚拟块) → 解散 → 重命名/寄存器名/编辑文本 → 复制 → 删除
    // Comment 块特殊菜单：仅 忽略/重命名/复制/删除（对应 IBR_SectionData.cpp:584-626）
    // 统一写法：构建 itemDescs 提交给单例 ContextMenuHost
    function dispatchSectionAction(action) {
        switch (action) {
        case "ignore":    workspaceController.toggleIgnore(root.sectionData.sectionId); break
        case "freeze":    workspaceController.toggleFreeze(root.sectionData.sectionId); break
        case "hide":      workspaceController.toggleHide(root.sectionData.sectionId); break
        case "fold":      workspaceController.foldComposed(root.sectionData.sectionId); break
        case "unfold":    workspaceController.unfoldComposed(root.sectionData.sectionId); break
        case "decompose": workspaceController.decomposeSection(root.sectionData.sectionId); break
        case "rename":    workspaceController.renameSelected(); break
        case "regRename": workspaceController.renameRegisterSelected(); break
        case "editText":  workspaceController.enterEditTextMode(root.sectionData.sectionId); break
        case "copy":      workspaceController.copySection(root.sectionData.sectionId); break
        case "delete":    workspaceController.toggleSelectSection(root.sectionData.sectionId, false);
                          workspaceController.deleteSelected(); break
        }
    }

    // 按当前节点状态生成菜单项描述，顺序对齐 ImGui（IBR_SectionData.cpp:627-731）：
    //   忽略 → 冻结 → 隐藏 → [虚拟块折叠/展开] → 重命名 → 寄存器名 → 复制 → [解散] → 编辑文本 → 删除
    // Comment 块：忽略 → 重命名 → 复制 → 删除（对应 IBR_SectionData.cpp:584-626）
    // 注意：ImGui 原版无分隔线，全部项连续排列（外层 sectionData 即当前右键节点）
    function buildSectionDescs() {
        var descs = []
        // 状态操作（智能互斥：按节点当前状态只显示可执行项）
        descs.push({ type: "item", text: root.isIgnored ? (i18n.rev, i18n.tr("GUI_NoIgnore")) : (i18n.rev, i18n.tr("GUI_Ignore")), action: "ignore" })
        if (!root.isComment) {
            descs.push({ type: "item", text: root.isFrozen ? (i18n.rev, i18n.tr("GUI_UnfreezeSec")) : (i18n.rev, i18n.tr("GUI_FreezeSec")), action: "freeze" })
            descs.push({ type: "item", text: root.isHidden ? (i18n.rev, i18n.tr("GUI_ShowSec")) : (i18n.rev, i18n.tr("GUI_HideSec")), action: "hide" })
        }
        // 虚拟块折叠/展开（对应 IBR_SectionData.cpp:677-695）
        if (root.isVirtualBlock) {
            var allFold = sectionData.isComposedAllFold || false
            descs.push({ type: "item", text: allFold ? (i18n.rev, i18n.tr("GUI_UnfoldComposed")) : (i18n.rev, i18n.tr("GUI_FoldComposed")),
                         action: allFold ? "unfold" : "fold" })
        }
        // 编辑操作（对应 IBR_SectionData.cpp:697-706）
        descs.push({ type: "item", text: (i18n.rev, i18n.tr("GUI_Rename") + " (F2)"), action: "rename" })
        if (!root.isComment) {
            descs.push({ type: "item", text: (i18n.rev, i18n.tr("GUI_RegRename") + " (F3)"), action: "regRename" })
        }
        // 复制（对应 IBR_SectionData.cpp:707-711）
        descs.push({ type: "item", text: (i18n.rev, i18n.tr("GUI_Copy")), action: "copy" })
        // 解散虚拟块（对应 IBR_SectionData.cpp:712-719，Decomposable 时显示）
        if (root.isVirtualBlock) {
            descs.push({ type: "item", text: (i18n.rev, i18n.tr("GUI_Decompose")), action: "decompose" })
        }
        // 编辑文本（对应 IBR_SectionData.cpp:720-724）
        if (!root.isComment) {
            descs.push({ type: "item", text: (i18n.rev, i18n.tr("GUI_SwitchToTextEdit")), action: "editText" })
        }
        // 删除（对应 IBR_SectionData.cpp:725-727）
        descs.push({ type: "item", text: (i18n.rev, i18n.tr("GUI_Delete")), action: "delete" })
        return descs
    }
}
