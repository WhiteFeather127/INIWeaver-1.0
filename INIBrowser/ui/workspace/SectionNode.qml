// SectionNode.qml
// Section 节点组件，对应 IBR_SectionData::RenderUI() 及 8 个子方法
// 显示：标题栏 + 内容区（折叠/展开）+ 状态样式（冻结/隐藏/选中/拖拽）
// 交互：左键拖拽移动、单击选中、右键菜单、双击编辑、折叠按钮
import QtQuick
import QtQuick.Controls

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
    // 因为 beginMoveSection 不再调用 refresh()（避免 Repeater 重建丢失 mouse grab），
    // sectionData.dragging 在拖拽期间为 false，需通过 draggingSectionId/massDragging 判断
    readonly property bool isDragging: (sectionData.dragging || false)
        || (sectionData.sectionId === workspaceController.draggingSectionId)
        || (workspaceController.massDragging && (sectionData.selected || false))
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
    readonly property int fontTitle: Math.max(1, Math.round(12 * _r))
    readonly property int fontBody: Math.max(1, Math.round(11 * _r))
    readonly property int fontSmall: Math.max(1, Math.round(10 * _r))
    readonly property int fontBtn: Math.max(1, Math.round(11 * _r))

    // 尺寸（由内容自适应驱动，所有部分随 _r 等比缩放，保证缩放时块间距与块尺寸比例恒定）
    // 宽度：max(WidthFix, wbase) * ratio（对应 ImGui IBR_WorkSpace.cpp:1840-1841）
    // 高度：标题栏 + 内容区，折叠时只保留标题栏
    implicitWidth: Math.max(sectionData.widthFix || 0, sectionData.widthBase || 221) * _r
    implicitHeight: header.height + contentContainer.height + (contentContainer.visible ? Math.round(4 * _r) : 0)

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
    onXChanged: updateAllCenters()
    onYChanged: updateAllCenters()
    onIsCollapsedChanged: updateAllCenters()
    onCollapsedInComposedChanged: updateAllCenters()
    onIsDraggingChanged: updateAllCenters()
    Connections {
        target: workspaceController
        function onEqCenterChanged() { updateAllCenters() }
        function onRatioChanged() { updateAllCenters() }
        function onDragOffsetChanged() { if (root.isDragging) updateAllCenters() }
    }

    function updateAllCenters() {
        // 头部坐标：拖拽中不回写（松手时 isDragging→false 触发回写新位置）
        if (!root.isDragging) {
            headLineRN.updateRNCenter()
        }
        // 键行坐标：始终回写（LineRow 内部根据 isDragging 决定是否减去 dragOffset）
        for (var i = 0; i < lineColumnRepeater.count; ++i) {
            var item = lineColumnRepeater.itemAt(i)
            if (item && item.updateLinkNodeCenter) {
                item.updateLinkNodeCenter()
            }
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
            // 标题栏高度随 ratio 缩放，保证块整体高度线性缩放（与 EqPos 间距的线性缩放一致）
            height: Math.round(28 * root._r)
            // 注册表颜色填充标题栏（对应 ImGui UCol），Ignore 时用暗色
            color: isIgnored ? "#1e1e1e" : registerColor
            radius: 3

            // E3：标题栏 RadioButton（对应 IBR_SectionData.cpp:757-774 ImGui::RadioButton）
            // 既是整块拖拽源视觉标记，还是折叠态连线锚点
            // Ignore 节点 RadioButton 变白（对应 TempWbg）
            Rectangle {
                id: headLineRN
                // Import 块 RadioButton 居中（对应 ImGui ImportCount>0 时 X=半宽-0.5*FontHeight）
                // 普通块 RadioButton 在左侧（对应 ImGui X=0.7*FontHeight）
                anchors.horizontalCenter: root.isImport ? parent.horizontalCenter : undefined
                anchors.left: root.isImport ? undefined : parent.left
                anchors.leftMargin: root.isImport ? 0 : 8
                anchors.verticalCenter: parent.verticalCenter
                width: Math.max(8, Math.round(10 * root._r))
                height: width
                radius: width / 2  // Circle 样式（对应 GlobalNodeStyle=Circle）
                // Ignore → 白色半透明（TempWbg）；否则用白色边框圆点（标题栏已是注册表颜色）
                color: isIgnored ? "#80ffffff" : "#e0e0e0"
                border.color: "#1e1e1e"
                border.width: 1
                visible: !isComment

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

                // sectionDrag 拖拽源（对应 ImGui IBR_SectionData.cpp:761-774 BeginDragDropSource IBR_SecDrag）
                // 从标题 RadioButton 拖出 → 拖到目标节点 DropArea → mergeSectionToSection 合并
                // 单击圆点 → 选中节点（对应 ImGui CurOnRender_Clicked）
                MouseArea {
                    id: headRNMouseArea
                    anchors.fill: parent
                    acceptedButtons: Qt.LeftButton
                    hoverEnabled: false
                    drag.target: sectionDragInitiator
                    drag.threshold: 4
                    drag.axis: Drag.XAndYAxis
                    preventStealing: true
                    onClicked: {
                        // 单击 RadioButton 选中节点（非子模块）
                        if (!root.isSubModule) {
                            workspaceController.toggleSelectSection(root.sectionData.sectionId, false)
                        }
                    }
                }

                Item {
                    id: sectionDragInitiator
                    visible: false
                    Drag.active: headRNMouseArea.drag.active
                    Drag.dragType: Drag.Internal
                    Drag.mimeData: {
                        "sectionDrag": JSON.stringify({
                            sourceId: root.sectionData.sectionId || 0
                        })
                    }
                    Drag.supportedActions: Qt.CopyAction
                    Drag.proposedAction: Qt.CopyAction
                    Drag.keys: ["sectionDrag"]
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

        // 内容区（对应 ImGui RenderUI_Lines / RenderUI_Virtual）
        // 普通块：行列表用 SectionLineModel 暴露的 OnShow/LinkNode/Links 等字段
        // 虚拟块：递归渲染 IncludingModules 子模块（对应 RenderUI_Virtual fold_left 累加 FinalY）
        Item {
            id: contentContainer
            anchors.top: header.bottom
            anchors.left: parent.left
            anchors.right: parent.right
            // 子模块在父虚拟块中折叠时只显示标题栏（对应 CollapsedInComposed）
            visible: !isComment && !isCollapsed && !collapsedInComposed
            // 高度由 Qt 内容驱动（Column.implicitHeight 累加所有行高度），不依赖 EqH
            height: visible ? (isVirtualBlock ? virtualBlockContainer.height
                                              : lineColumn.implicitHeight + Math.round(8 * root._r))
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
                anchors.margins: Math.round(4 * root._r)
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
                spacing: Math.round(6 * root._r)

                Repeater {
                    model: (isVirtualBlock && sectionData.includingModules) ? sectionData.includingModules : []

                    delegate: Item {
                        id: subModuleSlot
                        width: virtualBlockContainer.width
                        // 高度由子 SectionNode 内容决定（对应 ImGui FinalY）
                        // 隐藏的子模块不占垂直空间（对应 ImGui 中 Hidden 计数但不渲染）
                        height: (subModuleSlot.subData.hidden || false) ? 0 : subNodeLoader.item ? subNodeLoader.item.height : 0
                        visible: !(subModuleSlot.subData.hidden || false)

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
                                item.sectionData = subModuleSlot.subData
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
                text: qsTr("显示所有内部块 (") + (sectionData.hiddenCount || 0) + ")"
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
                onClicked: workspaceController.toggleCollapseInComposed(
                    sectionData.sectionId, !(sectionData.collapsedInComposed || false))
            }
        }

        // 注释块样式（对应 RenderUI_Comment IBR_SectionData.cpp:813-833 InputTextMultiline）
        // C11：注释块可编辑（对应 ImGui InputTextMultiline）
        TextArea {
            anchors.fill: parent
            anchors.margins: 8
            visible: isComment
            text: sectionData.displayName || ""
            color: "#858585"
            font.pixelSize: root.fontTitle
            wrapMode: TextArea.Wrap
            verticalAlignment: TextArea.AlignTop
            selectByMouse: true
            // 编辑后回写 DisplayName（对应 ImGui InputTextMultiline 回调）
            onEditingFinished: {
                if (text !== sectionData.displayName) {
                    workspaceController.updateCommentText(sectionData.sectionId, text)
                }
            }
            background: Rectangle {
                color: "transparent"
            }
        }
    }

    // LinkPoint 已移至每行右端（LinkNodePoint.qml），对应 ImGui RadioButton 位置
    // 不再在节点左右边缘放 LinkPoint

    // C4 + E2：节点 Acceptor DropTarget（对应 IBR_SectionData.cpp:477-563 RenderUI_Acceptor）
    // 接收两种拖拽：
    //   1. sectionDrag（整块拖入）→ MergeLine 合并
    //   2. lineDrag（连线拖入）→ 合并链接（对应 IBR_LineDrag 路径）
    DropArea {
        id: mergeDropArea
        anchors.fill: parent
        // 同时接收内部节点拖拽和连线拖拽
        keys: ["sectionDrag", "lineDrag"]

        onEntered: {
            var linePayload = drag.getDataAsString("lineDrag")
            if (linePayload && linePayload.length > 0) {
                // ===== lineDrag 路径（对应 RenderUI_Acceptor IBR_LineDrag 分支） =====
                var info = JSON.parse(linePayload)
                var srcId = info.sourceId
                if (srcId === sectionData.sectionId) return  // 不可连自身
                // v3 批次 1.4：LinkLimit=0 拖拽预览（对应 ImGui DrawDragPreviewIcon_LinkLim0）
                // 拖拽源 LinkLimit=0 时无法建立链接，通知源端显示红色叉号 + "无效链接"文本
                if (info.linkLimit === 0) {
                    workspaceController.setDragInvalidLink(true)
                    mergePreviewRect.visible = true
                    mergePreviewRect.color = "#d63b3b"
                    mergePreviewText.text = qsTr("无效链接")
                    return
                }
                // 类型校验（对应 Acceptor_CheckLinkType）
                // 修复：原调 lineModel.checkLinkType(info.lineIdx, sectionData.sectionId) 存在 Bug
                //   lineModel 是目标 section 的 model，info.lineIdx 是源行索引
                //   导致 srcId==dstId，类型校验失效。改用 checkMergePreview 正确传入源/目标 sectionId
                var previewCode = workspaceController.checkMergePreview(
                    srcId, sectionData.sectionId, info.linkType, true)
                var previewText = workspaceController.mergePreviewText(
                    srcId, sectionData.sectionId, info.linkType, true)
                mergePreviewRect.visible = true
                mergePreviewRect.color = previewCode === 0 ? "#4fc3f7" : "#d63b3b"
                mergePreviewText.text = previewText
            } else {
                // ===== sectionDrag 路径（对应 RenderUI_Acceptor IBR_SecDrag 分支） =====
                var secPayload = drag.getDataAsString("sectionDrag")
                var secInfo = secPayload ? JSON.parse(secPayload) : {}
                var dragId = secInfo.sourceId || 0
                if (dragId === sectionData.sectionId) return  // 不可合并自身
                var previewCode = workspaceController.checkMergePreview(
                    dragId, sectionData.sectionId, "", false)
                // 0=不可合并(类型不匹配), 1=可合并, 2=已存在, 3=其他
                mergePreviewRect.visible = true
                mergePreviewRect.color = previewCode === 1 ? "#4fc3f7"
                                      : previewCode === 2 ? "#d6a23b"
                                      : "#d63b3b"
                mergePreviewText.text = workspaceController.mergePreviewText(
                    dragId, sectionData.sectionId, "", false)
            }
        }
        onExited: {
            mergePreviewRect.visible = false
            mergePreviewText.text = ""
            // v3 批次 1.4：清除拖拽无效链接状态
            workspaceController.setDragInvalidLink(false)
        }
        onDropped: {
            var linePayload = drag.getDataAsString("lineDrag")
            if (linePayload && linePayload.length > 0) {
                // ===== lineDrag drop：创建链接 =====
                var info = JSON.parse(linePayload)
                var srcId = info.sourceId
                if (srcId === sectionData.sectionId) return
                // v3 批次 1.4：LinkLimit=0 不创建链接（对应 ImGui DrawDragPreviewIcon_LinkLim0 不创建链接）
                if (info.linkLimit === 0) {
                    workspaceController.setDragInvalidLink(false)
                    mergePreviewRect.visible = false
                    mergePreviewText.text = ""
                    return
                }
                var lineModel = sectionData.lineModel
                if (lineModel) {
                    // destKey 传空字符串，由 C++ 侧查询目标 DLK
                    lineModel.createLink(info.lineIdx, sectionData.sectionId, "")
                }
                mergePreviewRect.visible = false
                mergePreviewText.text = ""
            } else {
                // ===== sectionDrag drop：合并节点 =====
                var secPayload2 = drag.getDataAsString("sectionDrag")
                var secInfo2 = secPayload2 ? JSON.parse(secPayload2) : {}
                var dragId2 = secInfo2.sourceId || 0
                if (dragId2 === sectionData.sectionId) return
                var ok = workspaceController.mergeSectionToSection(dragId2, sectionData.sectionId)
                mergePreviewRect.visible = false
                mergePreviewText.text = ""
            }
            // v3 批次 1.4：清除拖拽无效链接状态
            workspaceController.setDragInvalidLink(false)
        }
    }

    // C4：合并预览覆盖层
    Rectangle {
        id: mergePreviewRect
        anchors.fill: parent
        visible: false
        color: "#4fc3f7"
        opacity: 0.2
        radius: 3
        z: 20
        border.color: "#4fc3f7"
        border.width: 2

        Text {
            id: mergePreviewText
            anchors.bottom: parent.bottom
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.bottomMargin: 2
            color: "#ffffff"
            font.pixelSize: root.fontSmall
            text: ""
        }
    }

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

        onPressed: {
            dragStart = Qt.point(mouseX, mouseY)
            hasDragged = false
            dragStarted = false
            if (mouse.button === Qt.LeftButton) {
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
                // 拖动结束：endDrag 内部会处理选中状态
                // - 单节点拖动 → endMoveSection 选中该模块（对应 ImGui ActivateAndEdit）
                // - 多节点拖动 → endMassDrag 取消选中（对应 ImGui IsMassAfter=false）
                if (!isSubModule && dragStarted) {
                    workspaceController.endDrag()
                } else if (!hasDragged) {
                    // 单击选中（additive = 是否按住 Ctrl/Shift）
                    var additive = (mouse.modifiers & Qt.ControlModifier) || (mouse.modifiers & Qt.ShiftModifier)
                    workspaceController.toggleSelectSection(sectionData.sectionId, additive)
                }
            } else if (mouse.button === Qt.RightButton) {
                // 右键菜单（单节点）
                sectionContextMenu.sectionId = sectionData.sectionId
                sectionContextMenu.popup(nodeMouseArea, mouseX, mouseY)
            }
        }

        onDoubleClicked: {
            if (mouse.button === Qt.LeftButton) {
                // 双击进入编辑模式（对应 IBR_EditFrame::ActivateAndEdit）
                workspaceController.activateAndEdit(sectionData.sectionId)
            }
        }
    }

    // 单节点右键菜单（对应 IBR_SectionData.cpp:566-806 RenderUI_TitleBar 右键菜单）
    // 阶段 12.5：智能互斥 Ignore/Freeze/Hide + 补齐 Rename/RegRename/Decompose/EditText
    // Comment 块特殊菜单：无 RegRename/Freeze/Hide/Fold（对应 IBR_SectionData.cpp:584-626）
    Menu {
        id: sectionContextMenu
        property var sectionId: 0

        // ===== 编辑操作 =====
        MenuItem {
            text: qsTr("重命名 (F2)")
            onTriggered: workspaceController.renameSelected()
        }
        MenuItem {
            text: qsTr("重命名寄存器名 (F3)")
            visible: !isComment
            onTriggered: workspaceController.renameRegisterSelected()
        }
        MenuItem {
            text: qsTr("编辑文本")
            onTriggered: workspaceController.enterEditTextMode(sectionContextMenu.sectionId)
        }

        MenuSeparator {}

        // ===== 复制/剪切/粘贴/删除/克隆 =====
        MenuItem {
            text: qsTr("复制")
            onTriggered: {
                workspaceController.toggleSelectSection(sectionContextMenu.sectionId, true)
                workspaceController.copySelected()
            }
        }
        MenuItem {
            text: qsTr("剪切")
            onTriggered: {
                workspaceController.toggleSelectSection(sectionContextMenu.sectionId, true)
                workspaceController.cutSelected()
            }
        }
        MenuItem {
            text: qsTr("粘贴")
            onTriggered: workspaceController.paste()
        }
        MenuItem {
            text: qsTr("克隆")
            onTriggered: {
                workspaceController.toggleSelectSection(sectionContextMenu.sectionId, true)
                workspaceController.duplicateSelected()
            }
        }
        MenuItem {
            text: qsTr("删除")
            onTriggered: {
                workspaceController.toggleSelectSection(sectionContextMenu.sectionId, false)
                workspaceController.deleteSelected()
            }
        }

        MenuSeparator {}

        // ===== 智能互斥：Ignore/Freeze/Hide（对应 IBR_SectionData.cpp:629-676） =====
        // 仅显示当前可执行的操作，而非同时显示两个相反选项
        MenuItem {
            text: isIgnored ? qsTr("取消忽略") : qsTr("忽略")
            visible: !isComment
            onTriggered: workspaceController.toggleIgnore(sectionContextMenu.sectionId)
        }
        MenuItem {
            text: isFrozen ? qsTr("解冻") : qsTr("冻结")
            visible: !isComment
            onTriggered: workspaceController.toggleFreeze(sectionContextMenu.sectionId)
        }
        MenuItem {
            text: isHidden ? qsTr("显示") : qsTr("隐藏")
            visible: !isComment
            onTriggered: workspaceController.toggleHide(sectionContextMenu.sectionId)
        }

        MenuSeparator {}

        // ===== 虚拟块操作（对应 IBR_SectionData.cpp:677-695, 712-719） =====
        MenuItem {
            text: qsTr("缩合")
            onTriggered: {
                workspaceController.toggleSelectSection(sectionContextMenu.sectionId, true)
                workspaceController.composeSelected()
            }
        }
        // 虚拟块：折叠/展开所有内部块
        MenuItem {
            text: qsTr("折叠内部块")
            visible: sectionData.isVirtualBlock || false
            enabled: !(sectionData.isComposedAllFold || false)
            onTriggered: workspaceController.foldComposed(sectionContextMenu.sectionId)
        }
        MenuItem {
            text: qsTr("展开内部块")
            visible: sectionData.isVirtualBlock || false
            enabled: (sectionData.isComposedAllFold || false)
            onTriggered: workspaceController.unfoldComposed(sectionContextMenu.sectionId)
        }
        // 虚拟块：解散
        MenuItem {
            text: qsTr("解散虚拟块")
            visible: sectionData.isVirtualBlock || false
            onTriggered: workspaceController.decomposeSection(sectionContextMenu.sectionId)
        }
    }
}
