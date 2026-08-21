// LinkNodePoint.qml
// 单行 LinkNode 圆点，对应 ImGui IBR_LinkNode::RenderUI_Node（IBR_LinkNode.cpp:383-637）
// 职责：
//   1. RadioButton 绘制（IsInherit → RoundedSquare，其他 → Circle，对应 GlobalNodeStyle）
//   2. 颜色由 SectionLineModel::computeNodeColor 计算（AdjustNodeCol）
//   3. 单击：!Empty && LinkLimit==1 → ModifyAndShow("")（解除唯一链接）
//   4. 右键菜单：LinkLimit==1 → "解除链接" 单按钮；其他 → 列出所有链接 + "解除所有链接"
//   5. Hover ToolTip：显示链接目标（ShowReg ? TargetValue : DisplayName，逗号分隔）
//   6. 拖拽源：hoverArea.drag 激活，onPositionChanged 鼠标命中测试驱动目标预览（hitTestSection）
//   7. layout 后通过 mapToItem(workspaceView) 回写坐标到 lineModel.setLinkNodeCenter
import QtQuick
import QtQuick.Controls
import "../components"

Item {
    id: root

    // ===== 输入属性（由 LineRow 显式传入） =====
    property var sectionData: ({})
    property var lineModel: null
    property int rowIndex: -1
    property var sessionId: 0
    property var links: []
    property int linkLimit: 0
    property color linkCol: "#cccccc"
    property bool hasLinkNode: false
    property bool isEmpty: true
    property bool isInherit: false
    property bool isImport: false
    property int fontSmall: 10
    // D9 修复：linkType 应为 StrPoolID name 字符串（供 drag payload 类型校验用）
    property string linkType: ""
    // 行级 DropTarget 定位源行所需信息（供 createLinkFromDrag 在 C++ 侧定位源行）
    property string keyName: ""
    property int lineMult: 0

    // D16：圆点尺寸随 ratio 缩放（对应 ImGui FontHeight，基准 13px）
    // fontSmall 由 LineRow 传入，SectionNode 按 _r 缩放计算
    // 放大 1.5 倍提升可点击性
    width: root.fontSmall * 1.5
    height: root.fontSmall * 1.5

    // ===== 圆点本体（对应 ImGui::RadioButton("", true, Style)） =====
    // Style = IsInherit ? ImGuiRadioButtonFlags_RoundedSquare : GlobalNodeStyle
    // 默认 GlobalNodeStyle = Circle
    // 仅负责视觉绘制；拖拽源挂在下方 dragProxy 上（对应 ImGui BeginDragDropSource + IBR_LineDrag）
    // 两层绘制：底层透明灰色圆（hover 提亮），上层不透明颜色层（对应 ImGui FrameBg 基圆 + CheckMark 内点）
    Rectangle {
        // 低一位图层：透明灰色圆，鼠标放上后提高亮度
        // 上层 nodeCircle 用 anchors.centerIn 与之同心（颜色圆圆心 = 灰圆圆心）
        id: nodeHalo
        anchors.fill: parent
        // IsInherit → RoundedSquare（小圆角矩形）；其他 → Circle（全圆）
        radius: root.isInherit ? 2 : (width / 2)
        color: hoverArea.containsMouse ? "#c0c8c8c8" : "#77a0a0a0"
    }
    Rectangle {
        // 上层：不透明颜色层（键行用 linkCol，随 AdjustNodeCol 的业务色）
        // 显式按父中心计算 x/y 居中，保证与底层灰色圆严格同心（不做 anchors 子像素近似）
        id: nodeCircle
        x: (parent.width - width) / 2
        y: (parent.height - height) / 2
        width: parent.width * 0.7
        height: width
        radius: root.isInherit ? 2 : (width / 2)
        color: root.linkCol
        opacity: 1.0
    }

    // ===== 拖拽代理（对应 ImGui BeginDragDropSource + SetDragDropPayload("IBR_LineDrag")） =====
    // 仅作为 hoverArea.drag.target 的可移动载体（目标命中改由鼠标坐标命中测试
    // hitTestSection 驱动，不再使用 Qt Drag/DropArea，因此无需 Drag attached 属性）。
    // 不可见 Item 不参与渲染，不影响布局。
    Item {
        id: dragProxy
        width: root.width
        height: root.height
        visible: false
    }

    // ===== Hover ToolTip（对应 IBR_LinkNode.cpp:541-564 IsItemHovered && !Empty） =====
    // 显示：ShowReg ? TargetValue 列表 : DisplayName 列表，逗号分隔
    ToolTip.visible: hoverArea.containsMouse && !root.isEmpty && tipText.length > 0
    ToolTip.text: tipText
    ToolTip.delay: 300

    readonly property string tipText: {
        if (root.isEmpty || root.links.length === 0) return ""
        var showReg = workspaceController.showRegName
        var names = []
        for (var i = 0; i < root.links.length; i++) {
            var lk = root.links[i]
            // ShowReg ? destKey（TargetValue）: destDisplayName
            // ImGui: ShowReg ? TargetValue() : Sec.GetDisplayName()
            // TargetValue 通常 = 目标 key 名（PoolStr(ToLoc.Key)）
            var name = showReg ? (lk.destKey || "") : (lk.destDisplayName || "")
            if (name.length > 0) names.push(name)
        }
        return names.length > 0 ? (i18n.rev, i18n.trF("GUI_Preview_LinkTo",
                                [names.join(", ")])) : ""
    }

    // ===== 交互 MouseArea（对应 RadioButton 点击/右键/Hover/DragDropSource） =====
    MouseArea {
        id: hoverArea
        anchors.fill: parent
        acceptedButtons: Qt.LeftButton | Qt.RightButton
        hoverEnabled: true
        // 拖拽源激活：左键按下 + 移动超过阈值
        // 对应 ImGui BeginDragDropSource
        // preventStealing: 按下即保持鼠标 grab，拖拽阈值满足后立刻激活 drag（对应 SectionNode）
        preventStealing: true
        drag.target: dragProxy
        drag.threshold: 4
        drag.axis: Drag.XAndYAxis

        onClicked: {
            if (mouse.button === Qt.LeftButton) {
                // 单击行为（对应 IBR_LinkNode.cpp:474-478 Clicked && !Empty && LinkLimit==1）
                if (!root.isEmpty && root.linkLimit === 1) {
                    // ModifyAndShow("") → 解除唯一链接
                    if (root.lineModel) {
                        root.lineModel.deleteAllLinks(root.rowIndex)
                    }
                }
            } else if (mouse.button === Qt.RightButton) {
                // 右键菜单（对应 IBR_LinkNode.cpp:479-540 RightClicked && !Empty）
                if (!root.isEmpty) {
                    // D19：传鼠标坐标（对应 ImGui SetRightClickMenu 传 GetMousePos）
                    var globalPos = hoverArea.mapToGlobal(mouse.x, mouse.y)
                    showRightMenu(globalPos.x, globalPos.y)
                }
            }
        }

        onDoubleClicked: {
            // 双击切回 Link 态（由 LineRow 处理，这里只发信号）
            root.doubleClicked()
        }

        // 滚轮转发：连线拖拽期间 grab 在本 MouseArea，转发滚轮保证拖动中可缩放
        onWheel: {
            var wsPt = mapToItem(workspaceView, wheel.x, wheel.y)
            workspaceController.onWheel(wsPt.x, wsPt.y, wheel.angleDelta.y)
        }

        // 拖拽开始时发信号（供 LineRow / WorkspaceController 同步状态）
        onPressed: root.pressed()
        onReleased: {
            // 用拖拽终点（或按下位置）命中目标节点，命中则建链（对应 IBR_LineDrag 落点）
            var toPos = mapToItem(workspaceView, mouse.x, mouse.y)
            var targetId = workspaceController.hitTestSection(toPos.x, toPos.y)
            if (targetId && targetId !== (root.sectionData.sectionId || 0) && root.linkLimit !== 0) {
                workspaceController.createLinkFromDrag(
                    root.sectionData.sectionId, root.keyName || "", root.lineMult || 0,
                    targetId, "", 0)
            }
            root.released()
            // 拖拽结束：清除 Bezier 预览与目标预览
            workspaceController.clearDraggingLink()
        }

        // 拖拽中：实时更新 Bezier 预览线 + 目标命中预览（对应 ImGui RenderUI_Links）
        onPositionChanged: {
            if (drag.active) {
                // 起点 = LinkNodePoint 中心在 workspaceView 坐标系中的位置
                var fromPos = root.mapToItem(workspaceView, root.width / 2, root.height / 2)
                // 终点 = 鼠标在 workspaceView 坐标系中的位置
                var toPos = mapToItem(workspaceView, mouse.x, mouse.y)
                workspaceController.setDraggingLink(fromPos.x, fromPos.y, toPos.x, toPos.y)
                // 预览框跟随鼠标（对应 ImGui 拖拽图像）：toPos 已是 workspaceView 坐标
                workspaceView.dragPreviewItem.x = toPos.x + 8
                workspaceView.dragPreviewItem.y = toPos.y + 8
                // 目标命中 + 预览（lineDrag：建链）
                var targetId = workspaceController.hitTestSection(toPos.x, toPos.y)
                if (targetId && targetId !== (root.sectionData.sectionId || 0)) {
                    if (root.linkLimit === 0) {
                        // LinkLimit=0 无法建链：源端红叉"无效链接" + 目标红色预览
                        // 对应 ImGui DrawDragPreviewIcon_LinkLim0（IBR_SectionData.cpp:96-106）
                        workspaceController.setDragInvalidLink(true)
                        workspaceController.setDragTarget(targetId, "#d63b3b", (i18n.rev, i18n.tr("GUI_Preview_InvalidLink")))
                    } else {
                        var code = workspaceController.checkMergePreview(
                            root.sectionData.sectionId, targetId, root.linkType, true)
                        var text = workspaceController.mergePreviewText(
                            root.sectionData.sectionId, targetId, root.linkType, true)
                        workspaceController.setDragTarget(targetId,
                            code === 0 ? "#4fc3f7" : "#d63b3b", text)
                    }
                } else {
                    workspaceController.setDragInvalidLink(false)
                    workspaceController.setDragTarget(0, "", "")
                }
            }
        }
    }

    // 拖拽目标预览已移至 WorkspaceView 顶层（dragPreview，跟随鼠标显示，对应 ImGui DrawDragPreviewIcon）
    // 本文件不再持有预览框，位置与内容由 hoverArea.onPositionChanged 更新 workspaceView.dragPreview

    // ===== 右键菜单（对应 IBR_LinkNode.cpp:483-539） =====
    // LinkLimit==1：单按钮"解除链接"
    // 其他：列出所有链接 + RadioButton 切换 UseLink + "解除所有链接"
    // 统一写法：构建 itemDescs 提交给单例 ContextMenuHost
    function dispatchLinkAction(action) {
        if (!root.lineModel) return
        if (action === "unlink" || action === "unlinkAll") {
            root.lineModel.deleteAllLinks(root.rowIndex)
        }
        // "link:N" 为 checkable 项，单击由 checkable 机制翻转，关闭时统一回读
    }

    // 每次打开前重建菜单项（对应 ImGui PushMsgBack 每次创建新 Popup）
    function buildLinkDescs() {
        var descs = []
        if (root.linkLimit === 1) {
            // 单一链接：只显示"解除链接"
            descs.push({ type: "item", text: (i18n.rev, i18n.tr("GUI_Unlink")), action: "unlink" })
        } else if (root.links.length === 0) {
            // 空链接：右键仅在 !isEmpty 时触发（对应 ImGui L479），此兜底分支不显示菜单项
            return descs
        } else {
            // 列出所有链接：每个链接一个 checkable 项（对应 ImGui RadioButton UseLink）
            for (var j = 0; j < root.links.length; j++) {
                var lk = root.links[j]
                // 对齐 ImGui L505/L530：显示名或注册名；无对应 GUI_ key，不追加自连标记。
                // 保留每个索引的 push（即使文本为空），保证 onMenuClosed 的 checkedStates 索引对齐，不误删链接
                descs.push({ type: "item", text: (lk.destDisplayName || lk.destKey || ""),
                             action: "link:" + j, checkable: true, checked: true })
            }
            descs.push({ type: "separator" })
            descs.push({ type: "item", text: (i18n.rev, i18n.tr("GUI_UnlinkAll")), action: "unlinkAll" })
        }
        return descs
    }

    // 标记"本圆点发起的菜单仍处于打开状态"（供 menuClosed 回读勾选状态时判定归属）
    property bool linkMenuActive: false

    // 菜单关闭时应用 UseLink 状态（对应 ImGui RadioButton 切换后重建 NewValue）
    // 从 host.checkedStates（action -> bool，checkable 项实时更新）回读当前勾选
    Connections {
        target: contextMenuHost
        function onMenuClosed() {
            if (!root.linkMenuActive) return
            root.linkMenuActive = false
            if (root.linkLimit === 1 || root.links.length === 0) return
            if (!root.lineModel) return
            var keepIdxs = []
            for (var i = 0; i < root.links.length; ++i) {
                if (contextMenuHost.checkedStates["link:" + i]) keepIdxs.push(i)
            }
            root.lineModel.applyLinkStates(root.rowIndex, keepIdxs)
        }
    }

    // D19：传鼠标坐标（对应 ImGui SetRightClickMenu 传 GetMousePos）
    function showRightMenu(globalX, globalY) {
        root.linkMenuActive = true
        contextMenuHost.show(root.buildLinkDescs(), globalX, globalY, (a) => root.dispatchLinkAction(a))
    }

    // ===== 信号（供 LineRow 监听） =====
    signal clicked()
    signal doubleClicked()
    signal pressed()
    signal released()
}
