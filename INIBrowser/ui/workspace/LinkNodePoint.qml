// LinkNodePoint.qml
// 单行 LinkNode 圆点，对应 ImGui IBR_LinkNode::RenderUI_Node（IBR_LinkNode.cpp:383-637）
// 职责：
//   1. RadioButton 绘制（IsInherit → RoundedSquare，其他 → Circle，对应 GlobalNodeStyle）
//   2. 颜色由 SectionLineModel::computeNodeColor 计算（AdjustNodeCol）
//   3. 单击：!Empty && LinkLimit==1 → ModifyAndShow("")（解除唯一链接）
//   4. 右键菜单：LinkLimit==1 → "解除链接" 单按钮；其他 → 列出所有链接 + "解除所有链接"
//   5. Hover ToolTip：显示链接目标（ShowReg ? TargetValue : DisplayName，逗号分隔）
//   6. DragDropSource：Drag.mimeData 携带 lineDrag payload
//   7. layout 后通过 mapToItem(workspaceView) 回写坐标到 lineModel.setLinkNodeCenter
import QtQuick
import QtQuick.Controls

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
    Rectangle {
        id: nodeCircle
        anchors.fill: parent
        // IsInherit → RoundedSquare（小圆角矩形）；其他 → Circle（全圆）
        radius: root.isInherit ? 2 : (width / 2)
        color: root.linkCol
        border.color: "#1e1e1e"
        border.width: 1
        // 空链接时半透明提示（对应 IllegalLineColor 红色由 linkCol 已传入）
        opacity: root.isEmpty ? 0.85 : 1.0
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
        return names.length > 0 ? qsTr("链接到: ") + names.join(", ") : ""
    }

    // ===== 交互 MouseArea（对应 RadioButton 点击/右键/Hover/DragDropSource） =====
    MouseArea {
        id: hoverArea
        anchors.fill: parent
        acceptedButtons: Qt.LeftButton | Qt.RightButton
        hoverEnabled: true
        // 拖拽源激活：左键按下 + 移动超过阈值
        // 对应 ImGui BeginDragDropSource
        drag.target: dragInitiator
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

        // 拖拽开始时发信号（供 LineRow / WorkspaceController 同步状态）
        onPressed: root.pressed()
        onReleased: {
            root.released()
            // 拖拽结束：清除 Bezier 预览
            workspaceController.clearDraggingLink()
        }

        // 拖拽中：实时更新 Bezier 预览（对应 ImGui RenderUI_Links 每帧绘制）
        onPositionChanged: {
            if (drag.active) {
                // 起点 = LinkNodePoint 中心在 workspaceView 坐标系中的位置
                var fromPos = root.mapToItem(workspaceView, root.width / 2, root.height / 2)
                // 终点 = 鼠标在 workspaceView 坐标系中的位置
                var toPos = mapToItem(workspaceView, mouse.x, mouse.y)
                workspaceController.setDraggingLink(fromPos.x, fromPos.y, toPos.x, toPos.y)
            }
        }
    }

    // ===== 拖拽 Initiator（不可见，仅用于启动 Drag） =====
    // 对应 ImGui BeginDragDropSource + SetDragDropPayload("IBR_LineDrag", ...)
    // QML 用 Drag.Internal + mimeData 携带 lineDrag payload
    Item {
        id: dragInitiator
        visible: false
        x: 0
        y: 0
        width: 1
        height: 1

        Drag.active: hoverArea.drag.active
        Drag.dragType: Drag.Internal
        Drag.mimeData: {
            "lineDrag": JSON.stringify({
                sourceId: root.sectionData.sectionId || 0,
                lineIdx: root.rowIndex,      // 行在 model 中的索引
                sessionId: String(root.sessionId),
                linkLimit: root.linkLimit,
                // D9 修复：linkType 应为 StrPoolID name 字符串（对应 ImGui LineDragData.TypeAlt）
                // 原误填 linkCol（QColor），导致接收方类型校验失效
                linkType: root.linkType,
                // 行级 DropTarget 定位源行所需信息
                keyName: root.keyName,
                lineMult: root.lineMult
            })
        }
        Drag.supportedActions: Qt.CopyAction
        Drag.proposedAction: Qt.CopyAction
        Drag.keys: ["lineDrag"]
    }

    // v3 批次 1.4：LinkLimit=0 拖拽预览（对应 ImGui DrawDragPreviewIcon_LinkLim0）
    // 仅当本节点正在拖拽且 DropArea 接受 payload 时显示红色叉号 + "无效链接" 文本
    // 对应 IBR_SectionData.cpp:96-106：if (IsDragDropPayloadBeingAccepted()) DrawCross + Text "无效链接"
    Item {
        id: invalidLinkPreview
        visible: root.linkLimit === 0
                 && dragInitiator.Drag.active
                 && workspaceController.dragInvalidLink
        anchors.left: parent.right
        anchors.leftMargin: 4
        anchors.verticalCenter: parent.verticalCenter
        width: invalidLinkRow.implicitWidth
        height: invalidLinkRow.implicitHeight

        Row {
            id: invalidLinkRow
            spacing: 2
            // 红色叉号（对应 DrawCross(P, FontHeight, ErrorTextColor)）
            Text {
                text: "✕"
                color: "#ff5050"
                font.pixelSize: root.fontSmall
                anchors.verticalCenter: parent.verticalCenter
            }
            // "无效链接" 文本（对应 ImGui::TextColored(ErrorTextColor, locc("GUI_Preview_InvalidLink"))）
            Text {
                text: qsTr("无效链接")
                color: "#ff5050"
                font.pixelSize: root.fontSmall
                anchors.verticalCenter: parent.verticalCenter
            }
        }
    }

    // ===== 右键菜单（对应 IBR_LinkNode.cpp:483-539） =====
    // LinkLimit==1：单按钮"解除链接"
    // 其他：列出所有链接 + RadioButton 切换 UseLink + "解除所有链接"
    Menu {
        id: linkMenu

        // D17：每次打开前重置 checked 状态（对应 ImGui PushMsgBack 每次创建新 Popup）
        onAboutToShow: {
            for (var i = 0; i < linkItemInstantiator.count; i++) {
                var item = linkItemInstantiator.objectAt(i)
                if (item) item.checked = true
            }
        }

        // D17：菜单关闭时应用 UseLink 状态（对应 ImGui RadioButton 切换后重建 NewValue）
        onClosed: {
            if (root.linkLimit === 1 || root.links.length === 0) return
            if (!root.lineModel) return
            var keepIdxs = []
            for (var i = 0; i < linkItemInstantiator.count; i++) {
                var item = linkItemInstantiator.objectAt(i)
                if (item && item.checked) keepIdxs.push(i)
            }
            root.lineModel.applyLinkStates(root.rowIndex, keepIdxs)
        }

        MenuItem {
            text: qsTr("解除链接")
            visible: root.linkLimit === 1
            onTriggered: {
                if (root.lineModel) {
                    root.lineModel.deleteAllLinks(root.rowIndex)
                }
            }
        }

        // LinkLimit != 1 时显示所有链接列表
        // D17：用 checkable MenuItem 实现二态切换（对应 ImGui RadioButton UseLink）
        MenuItem {
            text: qsTr("(无链接)")
            visible: root.linkLimit !== 1 && root.links.length === 0
            enabled: false
        }

        Instantiator {
            id: linkItemInstantiator
            model: root.linkLimit !== 1 ? root.links : []
            delegate: MenuItem {
                // D17：checkable 二态切换（对应 ImGui RadioButton("", ll.UseLink)）
                checkable: true
                checked: true
                text: (modelData.destDisplayName || modelData.destKey || qsTr("(未知)"))
                     + (modelData.isSelfLink ? qsTr(" [自连]") : "")
                // 单击切换 checked，不立即删除，关闭菜单时统一应用
                onTriggered: {
                    checked = !checked
                }
            }
            onObjectAdded: (index, object) => linkMenu.insertItem(index + 1, object)
            onObjectRemoved: (index, object) => linkMenu.removeItem(object)
        }

        MenuSeparator {
            visible: root.linkLimit !== 1 && root.links.length > 0
        }

        MenuItem {
            text: qsTr("解除所有链接")
            visible: root.linkLimit !== 1 && root.links.length > 0
            onTriggered: {
                if (root.lineModel) {
                    root.lineModel.deleteAllLinks(root.rowIndex)
                }
            }
        }
    }

    // D19：传鼠标坐标（对应 ImGui SetRightClickMenu 传 GetMousePos）
    function showRightMenu(globalX, globalY) {
        linkMenu.popup(globalX, globalY)
    }

    // ===== 信号（供 LineRow 监听） =====
    signal clicked()
    signal doubleClicked()
    signal pressed()
    signal released()
}
