// LineRow.qml
// 单行 delegate，对应 ImGui RenderUI_Line + RenderUI_Node
// 左侧：OnShow 描述文本（ShowRegName ? key : OnShow/DescShort）
// 右侧：LinkNode 圆点（RadioButton，条件显示）
// Import SubSec：LinkNode 居中（ImportCenter）
// 双击切换 IICStatus（Input 显示文本框 / Link 显示 LinkNode）
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../components"

Item {
    id: root

    // ===== 输入属性 =====
    property var sectionData: ({})
    property var lineModel: null
    property int rowIndex: -1
    property int fontBody: 11
    property int fontSmall: 10

    // model roles 绑定（由 SectionNode delegate 显式传入）
    property string onShowText: ""
    property string descLong: ""
    property bool hasLinkNode: false
    property color linkCol: "#cccccc"
    property int linkLimit: 0
    property bool isEmpty: true
    property bool isInherit: false
    property bool isImport: false
    property string linkType: ""
    property var sessionId: 0
    property var links: []
    property string exportValue: ""
    property int lineIdx: 0
    property int lineMult: 0
    property int compIdx: 0
    property string keyName: ""

    // 修复：行重建/布局完成门控。delegate 创建到首次布局完成前（layoutDone=false），
    // onXChanged/onYChanged 回写的中间坐标（行堆叠在第一个节点位置）会污染 acceptCenter 缓存，
    // 故跳过；onCompleted 用 Qt.callLater 在布局完成后置 true 并回写一次最终坐标。
    // 布局完成后（layoutDone=true）立即回写，保证拖动模块/画布时连线跟手不滞后。
    property bool layoutDone: false

    // D14：IICStatus 持久化（绑定 model role，对应 ImGui Status_Workspace/ComponentStatus）
    // false=Link 态（显示 RadioButton），true=Input 态（显示文本框）
    // 由 SectionLineModel::rebuildEntries 从业务层 Data 读取，toggleInputMode 写回
    property bool isInputMode: false
    property int keyType: 0  // 键输入类型：0=String, 1=Bool, 2=IIF
    property bool boolChecked: false  // Bool 键当前布尔值（keyType==1 时有效）
    // IIF 多分量单行自然宽度（供 SectionNode 扩展模块宽度以容纳 IIF，0=非 IIF）
    property real iifNaturalWidth: 0
    // IIF 分量列表刷新计数：写回共享 ValueID 后递增，强制 Repeater 重读 iifComponents，
    // 使共享同一 ValueID 的多个分量一起更新显示
    property int iifRevision: 0
    // IIF 首行是否为链接节点行（Multiple "+" 需避开首行右端的链接节点，对齐首行左移）
    property bool iifFirstRowHasNode: false

    // 监听模型写回通知：刷新本行 IIF 分量列表与自然宽度
    Connections {
        target: root.lineModel
        function onIifDataChanged(r) {
            if (r === root.rowIndex) {
                root.iifRevision++
                root.recomputeIifNaturalWidth()
            }
        }
    }

    // 布尔文本判真（IIF 布尔分量勾选状态；yes/true/t/1 → 真）
    function boolTrue(t) {
        var s = (t || "").trim().toLowerCase()
        return !(s === "no" || s === "0" || s === "n" || s === "false" || s === "f")
    }

    // 获取当前行 IIF 分量列表
    function iifList() {
        if (!root.lineModel || root.rowIndex < 0) return []
        return root.lineModel.iifComponents(root.rowIndex)
    }

    // IIF 行建模（对齐 imgui Immediate 模式）：
    // 默认每个非标记分量新起一行；samel 把下一个并入当前行；newl 强制换行；sep 单独成"分隔线行"。
    // 每行 { isSep:bool, node:linkCell|null, cells:[普通cell...] }，供下方行 Repeater 渲染。
    function iifRows() {
        if (!root.lineModel || root.rowIndex < 0) return []
        var all = root.lineModel.iifComponents(root.rowIndex)
        var rows = []
        var cur = null
        var same = false
        function newRow() { var r = { isSep: false, node: null, cells: [] }; rows.push(r); return r }
        for (var i = 0; i < all.length; i++) {
            var c = all[i]
            var t = c.type
            if (t === "samel") { same = true; continue }
            if (t === "newl") { cur = null; same = false; continue }
            if (t === "sep") { rows.push({ isSep: true, node: null, cells: [] }); cur = null; same = false; continue }
            if (!same) cur = null
            if (!cur) cur = newRow()
            if (t === "link") cur.node = c
            else cur.cells.push(c)
            same = false
        }
        return rows
    }

    // ===== IIF 分量通用辅助（对齐侧边栏 EditPanel.qml） =====
    // 选项数组（combo/radio/choice 的 opts：[{key,label,desc}...]）
    function iifOptArr(comp) { return (comp && comp.opts) ? comp.opts : [] }
    // combo 当前选中索引（以 opts[i].key 与 value 匹配）
    function iifComboIndex(comp) {
        var arr = iifOptArr(comp)
        var v = (comp && comp.value) || ""
        for (var i = 0; i < arr.length; i++) if (arr[i].key === v) return i
        return arr.length > 0 ? 0 : -1
    }
    // choice 已选 key 集合（按 delim 切分当前值）
    function iifChoiceSelected(comp) {
        var set = {}
        var v = (comp && comp.value) || ""
        var d = (comp && comp.delim) || ","
        var parts = String(v).split(d)
        for (var i = 0; i < parts.length; i++) if (parts[i]) set[parts[i]] = true
        return set
    }
    // choice 勾选切换写回：重算 delim 连接串
    function iifSetChoice(comp, key, on) {
        var d = (comp && comp.delim) || ","
        var set = iifChoiceSelected(comp)
        if (on) set[key] = true; else delete set[key]
        var out = []
        for (var k in set) if (set[k] === true) out.push(k)
        if (root.lineModel) root.lineModel.setIifComponentValue(root.rowIndex, comp.idx || comp.compIdx || 0, out.join(d))
    }
    // color 值 → QML 色串：#RRGGBB（支持 "255,0,0" Int 模式与 "#xxx" 十六进制）
    function iifColorToHex(v) {
        if (!v) return "#000000"
        var s = "" + v
        s = s.trim()
        if (s.charAt(0) === "#") return s
        var parts = s.split(",")
        if (parts.length >= 3) {
            var r = parseInt(parts[0], 10), g = parseInt(parts[1], 10), b = parseInt(parts[2], 10)
            if (!isNaN(r) && !isNaN(g) && !isNaN(b)) {
                function h(n) { var x = n.toString(16); return x.length === 1 ? "0" + x : x }
                return "#" + h(Math.max(0, Math.min(255, r))) + h(Math.max(0, Math.min(255, g))) + h(Math.max(0, Math.min(255, b)))
            }
        }
        return s
    }
    // 单个计量分量（非 fillWidth）的自然宽：text/locale/setter/bool/radio/choice
    function iifCellNaturalWidth(cc) {
        if (!cc) return 30
        var t = cc.type
        if (t === "bool") return (cc.label ? cc.label.length * root.fontBody * 0.6 + 4 : 0) + root.fontBody * 1.4 + 4 + 4
        if (t === "text" || t === "locale" || t === "setter")
            return (cc.label ? cc.label.length * root.fontBody * 0.6 + 4 : 0)
                   + (cc.value ? cc.value.length * root.fontBody * 0.6 : 0) + 4
        if (t === "radio" || t === "choice") {
            var w = (cc.label ? cc.label.length * root.fontBody * 0.6 + 4 : 0)
            var arr = iifOptArr(cc)
            for (var i = 0; i < arr.length; i++) {
                w += (arr[i].label || arr[i].key || "").length * root.fontBody * 0.6
                     + (t === "radio" ? (root.fontBody * 1.4 + 4) : (root.fontBody * 1.4 + 4 + 2)) + 3
                if (i > 0) w += 6
            }
            return w + 4
        }
        // fillWidth 类：输入/整数/下拉/色板/滑条 给一个基础宽
        return root.fontBody * 12
    }

    // IIF 整行自然宽：onShow 标签 + 各建模行（label+控件 / 节点区）最大宽
    function recomputeIifNaturalWidth() {
        if (!(root.isInputMode && root.keyType === 2) || !root.lineModel) {
            root.iifNaturalWidth = 0
            root.iifFirstRowHasNode = false
            return
        }
        // 组件构造/回收瞬间 onShowLabel 子对象可能尚为 null，延迟到布局后重试，避免 TypeError
        if (!root.onShowLabel) {
            Qt.callLater(() => root.recomputeIifNaturalWidth())
            return
        }
        var rows = root.iifRows()
        root.iifFirstRowHasNode = (rows.length > 0) && (rows[0].node != null)
        var maxRow = 0
        for (var r = 0; r < rows.length; r++) {
            var row = rows[r]
            if (row.isSep) continue
            var w = 0
            for (var k = 0; k < row.cells.length; k++) {
                var c = row.cells[k]
                w += root.iifCellNaturalWidth(c)
            }
            if (row.node) w += ((row.node.label ? row.node.label.length * root.fontBody * 0.6 : 0)
                               + root.fontSmall * 1.5 + 8)   // 节点 Short 标签 + 节点区
            if (w > maxRow) maxRow = w
        }
        root.iifNaturalWidth = root.onShowLabel.implicitWidth + 6 + maxRow
    }
    // IIF 分量总高：每个分量/分隔都作为占空间的渲染行，模块高度随行数自适应。
    // 逐行累加（分隔线 6px，普通行 fontBody*2）+ 行间距；非 IIF 返回 0。
    function iifTotalHeight() {
        if (!(root.isInputMode && root.keyType === 2) || !root.lineModel) return 0
        var rows = root.iifRows()
        var h = 0
        for (var i = 0; i < rows.length; i++)
            h += rows[i].isSep ? 6 : root.fontBody * 2
        if (rows.length > 1) h += (rows.length - 1) * 2   // Column spacing
        if (workspaceController.diagLogEnabled())
            console.log("[IIF-DIAG] totalHeight row=" + root.rowIndex + " key='" + root.keyName + "' rows=" + rows.length + " h=" + h)
        return h
    }
    onIsInputModeChanged: root.recomputeIifNaturalWidth()
    onKeyTypeChanged: root.recomputeIifNaturalWidth()
    onRowIndexChanged: root.recomputeIifNaturalWidth()
    onLineModelChanged: root.recomputeIifNaturalWidth()
    onVisibleChanged: if (root.visible) root.recomputeIifNaturalWidth()
    Component.onCompleted: root.recomputeIifNaturalWidth()

    // 行级增行按钮 + 右键菜单临时态（对应 ImGui WorkSpaceLine 多个会话级标志）
    // isMultiple：InputType.Multiple（"+" 增行按钮显示条件）
    // specialAccept：SpecialAccept 临时态（行右键菜单切换，不持久化）
    // inputOnShow：InputOnShow 编辑态（行右键菜单 EditDesc 切换，显示 OnShow 描述编辑框）
    property bool isMultiple: false
    property bool specialAccept: false
    property bool inputOnShow: false

    // 拖拽状态（由 SectionNode 传入）
    // 拖拽中实时回写 LastCenter（减去 dragOffset），松手时回写新位置
    property bool isDragging: false

    // 行高随字体等比缩放（fontBody 已按 ratio 缩放）
    // IIF 行：多个链接分量靠右各自堆叠一行时需加高以容纳整列
    // 注意：不能引用子对象 root.iifEdit.height —— 组件构造首轮求值 iifEdit 尚为 null，
    //       绑定抛异常被禁用，之后 iifEdit.height 变化也不会触发重算 → 高度恒 0 → IIF 溢出重叠。
    //       改为直接调 iifTotalHeight()，并把 rowIndex/iifRevision 纳入依赖触发重算。
    height: (root.isInputMode && root.keyType === 2 && root.lineModel && root.rowIndex >= 0 && root.iifRevision >= 0)
            ? Math.max(root.fontBody * 2, root.iifTotalHeight())
            : root.fontBody * 2

    // 布局完成后回写（解决 Component.onCompleted 时布局未完成导致 LastCenter=0）
    onWidthChanged: updateLinkNodeCenter()
    // 松手时 isDragging 变 false，立即回写新位置（不含 dragOffset）
    onIsDraggingChanged: updateLinkNodeCenter()
    // 注：画布平移/缩放/拖拽 dragOffset 变化由 SectionNode.updateAllCenters() 统一触发，
    //     此处不再单独监听，避免重复回写

    // 左侧 OnShow 描述文本（对应 ImGui wline.RenderUI(OnShow)）
    Text {
        id: onShowLabel
        anchors.left: parent.left
        anchors.leftMargin: 8
        anchors.verticalCenter: parent.verticalCenter
        // 编辑行注释（inputOnShow）时隐藏原文本，由 descEditField 替代（对应 ImGui
        // InputOnShow 分支替换 onShow 文本输入框，而非叠加覆盖）
        visible: !root.inputOnShow
        // Input 态：文本占自然宽度（随字体等比缩放），输入框占剩余宽度
        //   对应 ImGui: TextEx(Hint.Short) 自然宽度 + SameLine + SetNextItemWidth(剩余)
        //   用 implicitWidth（Text 全文本自然宽度，不依赖布局完成，避免 contentWidth 初始为 0 导致塌缩）
        //   IIF 行：模块宽度已按"标签自然宽+分量宽"自适应预留，标签直接取全自然宽（不截断）；
        //   其他 Input 态（String）仍需 0.7 上限给输入框/圆点留空间。
        // Link 态：文本占满左侧，留出 LinkNode 空间
        width: root.isInputMode
               ? (root.keyType === 2 ? implicitWidth : Math.min(implicitWidth, parent.width * 0.7))
               : parent.width - linkNode.width - 24
        text: root.onShowText
        color: "#d4d4d4"
        font.pixelSize: root.fontBody
        elide: Text.ElideRight

        // 双击切换 Input 态（对应 IBG_InputType.cpp:307 翻转 IICStatus）
        // 用 TapHandler 而非 MouseArea，避免拦截单击（单击需穿透到 nodeMouseArea 选中模块）
        TapHandler {
            acceptedButtons: Qt.LeftButton
            acceptedDevices: PointerDevice.Mouse | PointerDevice.TouchPad
            onTapped: {
                if (tapCount === 2) {
                    if (root.lineModel) {
                        root.lineModel.toggleInputMode(root.rowIndex)
                    }
                }
            }
        }

        // 长注释提示：统一用全局悬停提示框 appToolTip，鼠标放上立即显示，无延迟
        //（对应 imgui IBR_ToolTip）
        // 用 onContainsMouseChanged 而非 onEntered/onExited：
        // LineRow 常因数据 refresh 被重建，重建后若鼠标已悬停其上，新 MouseArea 的
        // onEntered 不会重新触发（只有 enter 事件），而 containsMouse 会在重建后
        // 自动求值为 true 并触发 onContainsMouseChanged → 提示稳定显示（类原生绑定式）
        MouseArea {
            id: onShowLabelMouse
            anchors.fill: parent
            acceptedButtons: Qt.NoButton
            hoverEnabled: true
            onContainsMouseChanged: {
                if (onShowLabelMouse.containsMouse) {
                    if (root.descLong.length === 0) return
                    // 提示落在标签下方（屏幕坐标）
                    var g = onShowLabel.mapToGlobal(0, onShowLabel.height + 4)
                    appToolTip.show(root.descLong, g.x, g.y)
                } else {
                    appToolTip.hide()
                }
            }
        }
    }

    // 右侧 LinkNode 圆点（对应 IBR_LinkNode::RenderUI_Node 的 RadioButton）
    LinkNodePoint {
        id: linkNode
        // 圆点交互优先级高于行级右键 MouseArea（lineRightClickMA）：
        // 右键圆点应弹连线节点自己的菜单（对应 IBR_LinkNode.cpp:479-540），而非键行菜单。
        // lineRightClickMA 覆盖整行且在 linkNode 之后声明，同一 z 下会先抢走圆点的右键，
        // 故把圆点 z 提到它之上。
        z: 2
        // D15：DefaultCenter 位置（对应 ImGui IBR_LinkNode.cpp:202-205）
        // ImGui: Center.x = 行末 - 1.5*FontHeight，左上角 = Center - size/2
        // Import SubSec 居中（ImportCenter），否则行末 1.5*fontSmall
        x: root.isImport ? parent.width / 2 - width / 2
                         : parent.width - 1.5 * root.fontSmall - width / 2
        anchors.verticalCenter: parent.verticalCenter

        sectionData: root.sectionData
        lineModel: root.lineModel
        rowIndex: root.rowIndex
        sessionId: root.sessionId
        links: root.links
        linkLimit: root.linkLimit
        linkCol: root.linkCol
        hasLinkNode: root.hasLinkNode
        isEmpty: root.isEmpty
        isInherit: root.isInherit
        isImport: root.isImport
        fontSmall: root.fontSmall
        // D9 修复：传入 linkType 字符串（StrPoolID name）
        linkType: root.linkType
        // 行级 DropTarget 定位源行所需信息
        keyName: root.keyName
        lineMult: root.lineMult

        // Input 态不显示 LinkNode（对应 ImGui Input 分支不调 RenderUI_Node）
        visible: root.hasLinkNode && !root.isInputMode
        // D14：双击 LinkNode 切回 Link 态（对应 IBG_InputType.cpp 双击 Hint 切换）
        // 通过 toggleInputMode 写回业务层 Data
        onDoubleClicked: {
            if (root.lineModel) {
                root.lineModel.toggleInputMode(root.rowIndex)
            }
        }

        // layout 后回写坐标到 lineModel（供 LinkRenderer 查表）
        // layoutDone 门控：行重建期间跳过中间坐标，避免污染缓存
        onXChanged: if (root.layoutDone) root.updateLinkNodeCenter()
        onYChanged: if (root.layoutDone) root.updateLinkNodeCenter()
        // 修复：onCompleted 延迟到事件循环末尾（Qt.callLater），此时 Positioner 已完成布局。
        // 直接 onCompleted 回写会读到行重建瞬间的初始坐标（所有行堆叠在第一个节点位置），
        // 污染 acceptCenter 缓存 → 该模块所有连线起点画到第一个节点。
        Component.onCompleted: Qt.callLater(() => { root.layoutDone = true; root.updateLinkNodeCenter() })
    }

    // 无 LinkNode 的行显示灰色 Disabled 点（对应 RenderUI_Node_Disabled）
    Rectangle {
        id: disabledNode
        // D15：与 LinkNodePoint 保持一致的位置公式
        x: root.isImport ? parent.width / 2 - width / 2
                         : parent.width - 1.5 * root.fontSmall - width / 2
        anchors.verticalCenter: parent.verticalCenter
        width: 10
        height: 10
        radius: root.isInherit ? 1 : 5
        color: "#5a5a5a"
        visible: !root.hasLinkNode && !root.isInputMode

        // 修复：无 LinkNode 的行也需回写位置，否则 Data_String 类型行
        // （键值为块名）的连线源端点 LastCenter 始终为 (0,0) 导致连线不可见
        // 对应 ImGui IBB_IniLine_Data_String::RenderUI 无条件调 UpdateLink
        // layoutDone 门控：行重建期间跳过中间坐标，避免污染缓存
        onXChanged: if (root.layoutDone) root.updateLinkNodeCenter()
        onYChanged: if (root.layoutDone) root.updateLinkNodeCenter()
        // 修复：onCompleted 延迟回写（同 linkNode，避免行重建时污染坐标）
        Component.onCompleted: Qt.callLater(() => { root.layoutDone = true; root.updateLinkNodeCenter() })
    }

    // Input 态文本框（双击行文本切换显示）
    // 对应 ImGui RenderIICInputText Input 分支（IBG_InputType.cpp:1377-1401）：
    //   TextEx(Hint.Short) + SameLine + InputText(剩余宽度)
    //   InputText 始终可编辑，单击直接输入，双击切回 Link 态
    //   不监听失焦切换状态（ImGui 没有 focusOut 切态逻辑）
    // container.z:1 让 TextField 在 nodeMouseArea 之上，单击直接编辑，无需额外 MouseArea
    TextField {
        id: inputField
        anchors.left: onShowLabel.right
        anchors.leftMargin: 4
        anchors.right: parent.right
        anchors.rightMargin: 4
        anchors.verticalCenter: parent.verticalCenter
        visible: root.isInputMode && root.keyType === 0  // 仅 String 用文本框（Bool/IIF 用专属控件）
        height: root.fontBody * 2
        // 绑定 exportValue：ImGui 每帧用 CurrentValue 重新渲染 InputText
        text: root.exportValue
        font.pixelSize: root.fontBody
        color: "#ce9178"
        selectByMouse: true
        horizontalAlignment: Text.AlignRight
        verticalAlignment: Text.AlignVCenter
        background: Rectangle {
            color: "#1e1e1e"
            border.color: "#007acc"
            border.width: 1
            radius: 2
        }

        // 双击切回 Link 态（对应 ImGui IsMouseDoubleClicked → Status.InputMethod = Link）
        TapHandler {
            acceptedButtons: Qt.LeftButton
            acceptedDevices: PointerDevice.Mouse | PointerDevice.TouchPad
            onTapped: {
                if (tapCount === 2) {
                    if (root.lineModel) {
                        root.lineModel.toggleInputMode(root.rowIndex)
                    }
                }
            }
        }

        // 实时提交（对应 ImGui Changed → ModifyFunc，每帧检测变化即提交）
        onTextEdited: {
            if (text !== root.exportValue) {
                root.lineModel.modifyValue(root.rowIndex, text)
            }
        }

        // Escape 取消编辑，切回 Link 态
        Keys.onEscapePressed: {
            text = root.exportValue
            if (root.lineModel) {
                root.lineModel.toggleInputMode(root.rowIndex)
            }
        }
    }

    // ===== Bool 键勾选框（对应 ImGui IIC_Bool 对话框切换，IBB_IniLine.cpp:477-488） =====
    // Bool 行 Input 态显示勾选框而非文本框：勾取/取消即翻转值（StrBoolType 决定文本格式）
    // 点击 → lineModel.toggleBoolValue 写回业务层并刷新
    Rectangle {
        id: boolEdit
        visible: root.isInputMode && root.keyType === 1
        anchors.left: onShowLabel.right
        anchors.leftMargin: 6
        anchors.verticalCenter: parent.verticalCenter
        width: root.fontBody * 1.4 + 4
        height: root.fontBody * 1.4 + 4
        radius: 3
        color: boolMA.containsMouse ? "#3a3a3a" : "#1e1e1e"
        border.color: root.boolChecked ? "#007acc" : "#5a5a5a"
        border.width: 1

        // 勾标记
        Text {
            visible: root.boolChecked
            anchors.centerIn: parent
            text: "✓"
            color: "#4ec9b0"
            font.pixelSize: root.fontBody
            font.bold: true
        }

        MouseArea {
            id: boolMA
            anchors.fill: parent
            preventStealing: true  // 阻止点击穿透到 nodeMouseArea（避免误选中模块）
            hoverEnabled: true
            onClicked: {
                if (root.lineModel) root.lineModel.toggleBoolValue(root.rowIndex)
            }
        }
    }

    // ===== IIF 多分量编辑器（对应 ImGui IBG_InputForm 多分量渲染, IBG_InputType.cpp:149-245） =====
    // IIF 行 Input 态按分量渲染为 文本/输入框/布尔勾框/分隔/换行 等核心分量的水平流
    // 分量描述由 SectionLineModel::iifComponents 导出；输入分量编辑经 setIifComponentValue 写回
    Column {
        id: iifEdit
        visible: root.isInputMode && root.keyType === 2
        anchors.left: onShowLabel.right
        anchors.leftMargin: 6
        anchors.right: parent.right
        anchors.rightMargin: 4
        anchors.verticalCenter: parent.verticalCenter
        spacing: 2
        // 每个分量都作为占空间的渲染部分：显式高度 = 行数×行高，模块随分量自适应
        // 绑定依赖 lineModel/rowIndex/keyType/iifRevision，三者在数据就绪或变化时都会重算
        height: (root.lineModel && root.keyType === 2 && root.rowIndex >= 0 && root.iifRevision >= 0)
                ? root.iifTotalHeight() : 0
        onHeightChanged: { if (root.lineModel && workspaceController.diagLogEnabled())
            console.log("[IIF-DIAG] iifEdit.height row=" + root.rowIndex + " key='" + root.keyName + "' h=" + height + " rootH=" + root.height) }

        Repeater {
            model: (root.keyType === 2 && root.iifRevision >= 0) ? root.iifRows() : []
            delegate: Item {
                id: iifRow
                property var rowData: modelData
                width: iifEdit.width
                height: (rowData && rowData.isSep) ? 6 : root.fontBody * 2
                Component.onCompleted: if (workspaceController.diagLogEnabled())
                    console.log("[IIF-DIAG] rowCreate row=" + root.rowIndex + " key='" + root.keyName + "' idx=" + index
                                + " isSep=" + (rowData && rowData.isSep) + " cells=" + ((rowData && rowData.comps) ? rowData.comps.length : 0)
                                + " h=" + height)
                onYChanged: if (workspaceController.diagLogEnabled())
                    console.log("[IIF-DIAG] rowY row=" + root.rowIndex + " idx=" + index + " y=" + y + " h=" + height + " parentH=" + iifEdit.height)

                // 分隔线行（对应 IIC_Separator）
                Rectangle {
                    visible: rowData && rowData.isSep
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.verticalCenter: parent.verticalCenter
                    height: 1
                    color: "#3a3a3a"
                }

                // 普通行：左侧 cells（label+控件），右侧 [节点标签 + 节点]
                RowLayout {
                    visible: !(rowData && rowData.isSep)
                    anchors.fill: parent
                    spacing: 4

                    // 普通 cells（label + 控件）
                    Repeater {
                        model: (rowData && rowData.cells) ? rowData.cells : []
                        delegate: Item {
                            id: iifCell
                            property var cc: modelData
                            // fillWidth 类填满可用宽；measure 类（text/locale/setter/bool/radio/choice）按内容自然宽
                            property bool isFlex: (cc.type === "input" || cc.type === "int" || cc.type === "combo"
                                                  || cc.type === "color" || cc.type === "slider")
                            Layout.fillWidth: isFlex
                            Layout.preferredWidth: root.iifCellNaturalWidth(cc)
                            Layout.minimumWidth: 30
                            height: root.fontBody * 2

                            // Short 标签（imgui TextEx(Hint.Short)）——每个交互分量都有的可见 Hint
                            Text {
                                id: cellLabel
                                visible: (cc.label || "").length > 0
                                anchors.left: parent.left
                                anchors.verticalCenter: parent.verticalCenter
                                text: cc.label || ""
                                color: "#9cdcfe"
                                font.pixelSize: root.fontBody
                                elide: Text.ElideRight
                            }

                            // 内容左偏移（label 之后）
                            property real ctlX: cellLabel.visible ? cellLabel.width + 3 : 0

                            // 纯文本/本地化/赋值串（只读，本身就是内容，无 label）
                            Text {
                                id: cellTxtVal
                                visible: (cc.type === "text" || cc.type === "locale" || cc.type === "setter")
                                anchors.left: parent.left
                                anchors.verticalCenter: parent.verticalCenter
                                text: cc.value || ""
                                color: "#c586c0"
                                font.pixelSize: root.fontBody
                                elide: Text.ElideRight
                            }

                            // 布尔勾选框（可见 label + 勾选框）
                            Rectangle {
                                visible: cc.type === "bool"
                                width: root.fontBody * 1.4 + 4
                                height: width
                                x: iifCell.ctlX
                                anchors.verticalCenter: parent.verticalCenter
                                radius: 3
                                color: cbMA.containsMouse ? "#3a3a3a" : "#1e1e1e"
                                border.color: root.boolTrue(cc.value) ? "#007acc" : "#5a5a5a"
                                border.width: 1
                                Text {
                                    visible: root.boolTrue(cc.value)
                                    anchors.centerIn: parent
                                    text: "✓"
                                    color: "#4ec9b0"
                                    font.pixelSize: root.fontBody
                                    font.bold: true
                                }
                                MouseArea {
                                    id: cbMA
                                    anchors.fill: parent
                                    preventStealing: true
                                    hoverEnabled: true
                                    onClicked: {
                                        if (root.lineModel)
                                            root.lineModel.setIifComponentValueBool(root.rowIndex, cc.idx || cc.compIdx, !root.boolTrue(cc.value))
                                    }
                                }
                            }

                            // 枚举单选组（radio）：一组互斥圆点
                            Row {
                                visible: cc.type === "radio"
                                x: iifCell.ctlX
                                anchors.verticalCenter: parent.verticalCenter
                                spacing: 6
                                Repeater {
                                    model: root.iifOptArr(cc)
                                    delegate: Row {
                                        property var opt: modelData
                                        spacing: 3
                                        Rectangle {
                                            width: root.fontBody * 1.4 + 4
                                            height: width
                                            radius: width / 2
                                            color: (opt.key === cc.value) ? "#2d6db5" : "#1e1e1e"
                                            border.color: (opt.key === cc.value) ? "#007acc" : "#5a5a5a"
                                            border.width: 1
                                            anchors.verticalCenter: parent.verticalCenter
                                            MouseArea {
                                                anchors.fill: parent
                                                preventStealing: true
                                                hoverEnabled: true
                                                onClicked: {
                                                    if (root.lineModel)
                                                        root.lineModel.setIifComponentValue(root.rowIndex, cc.idx || cc.compIdx, opt.key)
                                                }
                                            }
                                        }
                                        Text {
                                            text: opt.label || opt.key || ""
                                            color: "#d4d4d4"
                                            font.pixelSize: root.fontBody
                                            verticalAlignment: Text.AlignVCenter
                                            MouseArea {
                                                anchors.fill: parent
                                                acceptedButtons: Qt.NoButton
                                                hoverEnabled: true
                                                onContainsMouseChanged: {
                                                    if (containsMouse && opt.desc && opt.desc.length > 0) {
                                                        var gr = mapToGlobal(width / 2, height + 2)
                                                        appToolTip.show(opt.desc, gr.x, gr.y)
                                                    } else appToolTip.hide()
                                                }
                                            }
                                        }
                                    }
                                }
                            }

                            // 多选组（choice）：每选项一个勾选框（当前值按 delim 切分）
                            Row {
                                visible: cc.type === "choice"
                                x: iifCell.ctlX
                                anchors.verticalCenter: parent.verticalCenter
                                spacing: 6
                                Repeater {
                                    model: root.iifOptArr(cc)
                                    delegate: Row {
                                        property var opt: modelData
                                        property var sel: root.iifChoiceSelected(cc)
                                        spacing: 3
                                        Rectangle {
                                            id: choiceBox
                                            width: root.fontBody * 1.4 + 4
                                            height: width
                                            radius: 3
                                            color: cbChk.containsMouse ? "#3a3a3a" : "#1e1e1e"
                                            border.color: (opt.key.length > 0 && sel[opt.key] === true) ? "#007acc" : "#5a5a5a"
                                            border.width: 1
                                            Text {
                                                visible: opt.key.length > 0 && sel[opt.key] === true
                                                anchors.centerIn: parent
                                                text: "✓"
                                                color: "#4ec9b0"
                                                font.pixelSize: root.fontBody
                                                font.bold: true
                                            }
                                            MouseArea {
                                                id: cbChk
                                                anchors.fill: parent
                                                preventStealing: true
                                                hoverEnabled: true
                                                onClicked: {
                                                    var on = !(sel[opt.key] === true)
                                                    root.iifSetChoice(cc, opt.key, on)
                                                }
                                            }
                                        }
                                        Text {
                                            text: opt.label || opt.key || ""
                                            color: "#d4d4d4"
                                            font.pixelSize: root.fontBody
                                            verticalAlignment: Text.AlignVCenter
                                            MouseArea {
                                                anchors.fill: parent
                                                acceptedButtons: Qt.NoButton
                                                hoverEnabled: true
                                                onContainsMouseChanged: {
                                                    if (containsMouse && opt.desc && opt.desc.length > 0) {
                                                        var g1 = mapToGlobal(width / 2, height + 2)
                                                        appToolTip.show(opt.desc, g1.x, g1.y)
                                                    } else appToolTip.hide()
                                                }
                                            }
                                        }
                                    }
                                }
                            }

                            // 输入/整数分量：label + 占满剩余宽的输入框
                            TextField {
                                id: cellInput
                                visible: (cc.type === "input" || cc.type === "int")
                                x: iifCell.ctlX
                                width: parent.width - x
                                height: root.fontBody * 2
                                anchors.verticalCenter: parent.verticalCenter
                                text: cc.value || ""
                                font.pixelSize: root.fontBody
                                color: "#ce9178"
                                verticalAlignment: Text.AlignVCenter
                                selectByMouse: true
                                readOnly: cc.readOnly || false
                                background: Rectangle {
                                    color: "#1e1e1e"
                                    border.color: "#007acc"
                                    border.width: 1
                                    radius: 2
                                }
                                onEditingFinished: {
                                    if (root.lineModel)
                                        root.lineModel.setIifComponentValue(root.rowIndex, cc.idx || cc.compIdx, text)
                                }
                            }

                            // 下拉组合框（combo）
                            ComboBox {
                                visible: cc.type === "combo"
                                x: iifCell.ctlX
                                width: parent.width - x
                                height: root.fontBody * 2
                                anchors.verticalCenter: parent.verticalCenter
                                model: root.iifOptArr(cc)
                                textRole: "label"
                                currentIndex: root.iifComboIndex(cc)
                                font.pixelSize: root.fontBody
                                onActivated: {
                                    var arr = root.iifOptArr(cc)
                                    var o = (index >= 0 && index < arr.length) ? arr[index] : null
                                    if (o && root.lineModel)
                                        root.lineModel.setIifComponentValue(root.rowIndex, cc.idx || cc.compIdx, o.key)
                                }
                                background: Rectangle {
                                    color: "#1e1e1e"
                                    border.color: "#007acc"
                                    border.width: 1
                                    radius: 2
                                }
                                contentItem: Text {
                                    text: currentText
                                    color: "#ce9178"
                                    font.pixelSize: root.fontBody
                                    verticalAlignment: Text.AlignVCenter
                                    horizontalAlignment: Text.AlignLeft
                                    leftPadding: 4
                                    elide: Text.ElideRight
                                }
                                indicator: Rectangle {
                                    anchors.verticalCenter: parent.verticalCenter
                                    anchors.right: parent.right
                                    anchors.rightMargin: 4
                                    width: 0; height: 0
                                }
                            }

                            // 色板（color）：色块 + 值输入（label + 占满剩余宽）
                            Row {
                                visible: cc.type === "color"
                                x: iifCell.ctlX
                                width: parent.width - x
                                anchors.verticalCenter: parent.verticalCenter
                                spacing: 4
                                Rectangle {
                                    width: root.fontBody * 1.6
                                    height: width
                                    radius: 3
                                    color: root.iifColorToHex(cc.value)
                                    border.color: "#5a5a5a"
                                    border.width: 1
                                    anchors.verticalCenter: parent.verticalCenter
                                }
                                TextField {
                                    text: cc.value || ""
                                    height: root.fontBody * 2
                                    width: parent.width - parent.spacing - (root.fontBody * 1.6)
                                    anchors.verticalCenter: parent.verticalCenter
                                    font.pixelSize: root.fontBody
                                    color: "#ce9178"
                                    verticalAlignment: Text.AlignVCenter
                                    selectByMouse: true
                                    background: Rectangle {
                                        color: "#1e1e1e"
                                        border.color: "#007acc"
                                        border.width: 1
                                        radius: 2
                                    }
                                    onEditingFinished: {
                                        if (root.lineModel)
                                            root.lineModel.setIifComponentValue(root.rowIndex, cc.idx || cc.compIdx, text)
                                    }
                                }
                            }

                            // 整型滑条（slider）：滑条 + 当前值
                            Row {
                                visible: cc.type === "slider"
                                x: iifCell.ctlX
                                width: parent.width - x
                                anchors.verticalCenter: parent.verticalCenter
                                spacing: 6
                                Slider {
                                    id: sliderCtrl
                                    from: cc.min || 0
                                    to: cc.max || 100
                                    value: parseInt(cc.value || "0", 10)
                                    height: root.fontBody * 2
                                    width: Math.max(80, parent.width - parent.spacing - root.fontBody * 4)
                                    anchors.verticalCenter: parent.verticalCenter
                                    onValueChanged: {
                                        if (!pressed && cc.value !== "" + sliderCtrl.value && root.lineModel)
                                            root.lineModel.setIifComponentValue(root.rowIndex, cc.idx || cc.compIdx, "" + parseInt(sliderCtrl.value, 10))
                                    }
                                }
                                Text {
                                    text: cc.value || "0"
                                    color: "#ce9178"
                                    font.pixelSize: root.fontBody
                                    verticalAlignment: Text.AlignVCenter
                                    anchors.verticalCenter: parent.verticalCenter
                                }
                            }

                            // 分量悬停提示（匹配 imgui IBR_ToolTip(Hint.Long)，缺省 Short）
                            MouseArea {
                                anchors.fill: parent
                                acceptedButtons: Qt.NoButton
                                hoverEnabled: true
                                onContainsMouseChanged: {
                                    if (containsMouse) {
                                        if (!cc.tooltip || cc.tooltip.length === 0) return
                                        var g = parent.mapToGlobal(0, parent.height + 2)
                                        appToolTip.show(cc.tooltip, g.x, g.y)
                                    } else {
                                        appToolTip.hide()
                                    }
                                }
                            }
                        }
                    }

                    // 行末处：把节点推到右端的弹性占位（仅当本行有链接节点时占用）
                    Item { Layout.fillWidth: true; visible: rowData && rowData.node != null }

                    // 链接节点：左侧 Short 标签 + 右端节点（对齐 imgui RenderUI_Node）
                    Text {
                        property var cc: rowData && rowData.node ? rowData.node : null
                        Layout.preferredWidth: implicitWidth
                        Layout.alignment: Qt.AlignVCenter
                        visible: cc && (cc.label || "").length > 0
                        text: cc ? (cc.label || "") : ""
                        color: "#9cdcfe"
                        font.pixelSize: root.fontBody
                        MouseArea {
                            anchors.fill: parent
                            acceptedButtons: Qt.NoButton
                            hoverEnabled: true
                            onContainsMouseChanged: {
                                if (containsMouse && parent.cc && parent.cc.tooltip && parent.cc.tooltip.length > 0) {
                                    var g = parent.mapToGlobal(parent.width / 2, parent.height + 2)
                                    appToolTip.show(parent.cc.tooltip, g.x, g.y)
                                } else {
                                    appToolTip.hide()
                                }
                            }
                        }
                    }
                    LinkNodePoint {
                        property var cc: rowData && rowData.node ? rowData.node : null
                        visible: cc != null
                        flowNode: true
                        Layout.preferredWidth: root.fontSmall * 1.5
                        Layout.preferredHeight: root.fontSmall * 1.5
                        Layout.alignment: Qt.AlignVCenter
                        sectionData: root.sectionData
                        lineModel: root.lineModel
                        rowIndex: root.rowIndex
                        keyName: root.keyName
                        lineMult: cc ? (cc.lineMult || 0) : 0
                        compIdx: cc ? (cc.idx || cc.compIdx || 0) : 0
                        iifHint: cc ? (cc.tooltip || "") : ""
                        links: cc ? (cc.links || []) : []
                        linkLimit: cc ? (cc.linkLimit || 0) : 0
                        linkCol: cc ? (cc.linkCol || "#cccccc") : "#cccccc"
                        hasLinkNode: cc ? (cc.hasLinkNode || false) : false
                        isEmpty: cc ? (cc.isEmpty !== false) : true
                        isInherit: false
                        isImport: false
                        fontSmall: root.fontSmall
                        linkType: cc ? (cc.linkType || "") : ""
                        // 双击切回 Input 态（对应 RenderUI_Node 双击 Status.InputMethod=Input）
                        onDoubleClicked: {
                            if (root.lineModel) root.lineModel.toggleInputMode(root.rowIndex)
                        }
                    }
                }
            }
        }
    }

    // ===== 行级 "+" 增行按钮（对应 IBR_Misc.cpp:358-368, 372-385） =====
    // 仅 isMultiple 行显示，位于 LinkNode 左侧
    // 调 lineModel.addLine → bsec->MergeLine(Key, Index_AlwaysNew, Form, Replace)
    // 修复：IIF 行 isInputMode 恒为 true（显示分量 Flow），原 `!isInputMode` 条件会让
    //       Multiple 的 IIF 键永远不显示加号。改为仅按 isMultiple 判定；IIF 行并排靠右。
    Rectangle {
        id: addLineButton
        visible: root.isMultiple && !root.inputOnShow
        // Import 行 LinkNode 居中，"+" 放在居中点左侧；其他行放在 LinkNode 左侧；
        // IIF 行（keyType==2）或 Input 态无行级 LinkNode 时，"+" 放在模块右端对齐节点。
        x: (root.isImport && root.keyType !== 2)
           ? (parent.width / 2 - width - 4)
           : (root.keyType === 2)
             // IIF 行：加号靠模块左侧、与第一行分量对齐（imgui 放 BaseCursorY=首行，EndCursor.x=左侧）
             ? 6
             : (root.isInputMode
                ? (parent.width - width - 4)
                : (linkNode.x - width - 4))
        // IIF 行已按分量加高（多行堆叠），"+" 垂直居中会落到整个块中央，应对齐顶部第一行；
        // 非 IIF 行保持垂直居中（行高即一行）。
        y: root.keyType === 2
           ? Math.round((root.fontBody * 2 - height) / 2)
           : (parent.height - height) / 2
        width: root.fontSmall * 1.4
        height: root.fontSmall * 1.4
        radius: 2
        color: addLineMA.containsMouse ? "#3a3a3a" : "#2a2a2a"
        border.color: "#5a5a5a"
        border.width: 1

        Text {
            anchors.centerIn: parent
            text: "+"
            color: "#d4d4d4"
            font.pixelSize: root.fontSmall
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
        }

        MouseArea {
            id: addLineMA
            anchors.fill: parent
            hoverEnabled: true
            // 阻止点击穿透到 nodeMouseArea（避免误选中模块）
            preventStealing: true
            onClicked: {
                if (root.lineModel) {
                    root.lineModel.addLine(root.rowIndex)
                }
            }
        }
    }

    // ===== OnShow 描述编辑框（对应 IBR_Misc.cpp:64-82 InputOnShow 分支） =====
    // inputOnShow=true 时显示，覆盖在 onShowLabel 位置
    // Enter 提交（对应 ImGui EnterReturnsTrue）→ editDesc 写入 + toggleInputOnShow 关闭
    // Escape 取消 → toggleInputOnShow 关闭（不写入）
    TextField {
        id: descEditField
        // 覆盖在 onShowLabel 位置（对应 ImGui InputOnShow 分支替换 onShow 文本输入框）。
        // 靠模块左侧、与 onShowLabel 同宽：仅覆盖原注释文本区域，不盖右侧 LinkNode 与键值。
        anchors.left: onShowLabel.left
        anchors.right: onShowLabel.right
        anchors.verticalCenter: parent.verticalCenter
        visible: root.inputOnShow
        // 编辑期置顶，避免外层 MouseArea（nodeMouseArea 等）抢占点击导致点击自身时焦点丢失
        z: 100
        height: root.fontBody * 2
        // 初始文本：onShowLabel 当前显示文本（空描述时为空串）
        // 对应 ImGui: if(OnShow == EmptyOnShowDesc) EditOnShow = ""
        text: root.onShowText
        font.pixelSize: root.fontBody
        color: "#d4d4d4"
        selectByMouse: true
        horizontalAlignment: Text.AlignLeft
        verticalAlignment: Text.AlignVCenter
        background: Rectangle {
            color: "#1e1e1e"
            border.color: "#007acc"
            border.width: 1
            radius: 2
        }

        onVisibleChanged: {
            if (visible) {
                // 显示时聚焦并全选（对应 ImGui InputText 自动聚焦）
                forceActiveFocus()
                selectAll()
            }
        }

        // Enter 提交（对应 ImGui EnterReturnsTrue）
        onAccepted: {
            if (root.lineModel) {
                root.lineModel.editDesc(root.rowIndex, text)
                root.lineModel.toggleInputOnShow(root.rowIndex)
            }
        }

        // 失焦 = 相当于按回车（对齐 ImGui：InputText 失焦即提交关闭，非 Esc 取消）
        // 点击输入框自身（含最右侧空白/选中文本）会短暂丢焦点又重获，若同步提交会把编辑误关。
        // 用 Qt.callLater 延迟到焦点稳定后，按鼠标落点判断：落在输入框内 → 重新聚焦保持编辑；
        // 落在框外（点击画布/他处）→ 提交并关闭。
        onActiveFocusChanged: {
            if (!activeFocus && visible) {
                Qt.callLater(() => {
                    if (!visible || !root.lineModel) return
                    var gp = workspaceController.globalMousePos()
                    var pr = root.mapFromGlobal(Qt.point(gp.x, gp.y))
                    var inField = (pr.x >= descEditField.x - 2
                                   && pr.x <= descEditField.x + descEditField.width + 2
                                   && pr.y >= descEditField.y - 2
                                   && pr.y <= descEditField.y + descEditField.height + 2)
                    if (inField) {
                        descEditField.forceActiveFocus()
                    } else {
                        root.lineModel.editDesc(root.rowIndex, text)
                        root.lineModel.toggleInputOnShow(root.rowIndex)
                    }
                })
            }
        }

        // Escape 取消（对应 ImGui Esc 关闭 InputText）
        Keys.onEscapePressed: {
            if (root.lineModel) {
                root.lineModel.toggleInputOnShow(root.rowIndex)
            }
        }
    }

    // ===== 行级右键菜单触发 MouseArea（对应 IBR_Misc.cpp:201-252 RightClick） =====
    // 仅捕获右键，不拦截左键（让 onShowLabel TapHandler 和 nodeMouseArea 正常工作）
    MouseArea {
        id: lineRightClickMA
        anchors.fill: parent
        acceptedButtons: Qt.RightButton
        // 不阻止悬浮事件穿透（让 onShowLabelMouse 仍能显示 ToolTip）
        hoverEnabled: false
        onClicked: {
            if (mouse.button === Qt.RightButton) {
                var globalPos = lineRightClickMA.mapToGlobal(mouse.x, mouse.y)
                // 多选态（MassAfter）右键选中模块的键行：应弹模块多选菜单而非键行菜单
                //（对齐 ImGui：多选状态下右键任意选中模块位置弹多选操作菜单）
                // 键行所属的父模块被多选且当前处于多选操作态时，右键转发到多选菜单
                if (workspaceController.inputState === 4
                    && workspaceController.isSectionSelected(root.sectionData.sectionId)
                    && workspaceController.massTargetIds().length > 1) {
                    contextMenuHost.show(workspaceView.massAfterDescs(), globalPos.x, globalPos.y,
                                         (a) => workspaceView.dispatchMassAction(a))
                } else {
                    contextMenuHost.show(root.buildLineDescs(), globalPos.x, globalPos.y,
                                         (a) => root.dispatchLineAction(a))
                }
            }
        }
    }

    // ===== 行级右键菜单（对应 IBR_Misc.cpp:201-252 WorkSpaceLine 右键菜单 4 项） =====
    // 统一写法：构建 itemDescs 提交给单例 ContextMenuHost
    function buildLineDescs() {
        return [
            // 1. 切换输入态（对应 GUI_SwitchIICStatus → CC->SwitchInput = true）
            { type: "item", text: (i18n.rev, i18n.tr("GUI_SwitchIICStatus")), action: "toggleInput" },
            // 2. SpecialAccept 开关（对应 GUI_SpecialAcceptOff/On）
            // 文本根据当前态切换：已开启显示"关闭特殊接受"，未开启显示"开启特殊接受"
            { type: "item", text: root.specialAccept ? (i18n.rev, i18n.tr("GUI_SpecialAcceptOff")) : (i18n.rev, i18n.tr("GUI_SpecialAcceptOn")),
              action: "toggleSpecial" },
            // 3. 删除行（对应 GUI_RemoveLine → pbk->RemoveLine(Key)）
            { type: "item", text: (i18n.rev, i18n.tr("GUI_RemoveLine")), action: "removeLine" },
            // 4. 编辑描述（对应 GUI_EditDesc → CC->InputOnShow = !CC->InputOnShow）
            { type: "item", text: (i18n.rev, i18n.tr("GUI_EditDesc")), action: "editDesc" }
        ]
    }

    function dispatchLineAction(action) {
        if (!root.lineModel) return
        switch (action) {
        case "toggleInput":   root.lineModel.toggleInputMode(root.rowIndex); break
        case "toggleSpecial": root.lineModel.toggleSpecialAccept(root.rowIndex); break
        case "removeLine":    root.lineModel.removeLine(root.rowIndex); break
        case "editDesc":      root.lineModel.toggleInputOnShow(root.rowIndex); break
        }
    }

    // 回写 LinkNode 屏幕坐标到 lineModel（供 WorkspaceController::rebuildLinkEndpoints 同步）
    // 布局完成后立即回写（保证拖动模块/画布时连线跟手）；
    // 行重建期间的中间坐标由 layoutDone 门控拦截（见 onXChanged/onYChanged）。
    function updateLinkNodeCenter(force) {
        doUpdateLinkNodeCenter(force)
    }

    function doUpdateLinkNodeCenter(force) {
        if (!root.lineModel || root.rowIndex < 0) return
        // 画布平移中 / 缩放叠加中：端点表保持快照不重建（canvasDragOffset/zoomTransform 叠加渲染），
        // 跳过回写，避免每帧全量端点表重建（拖动画布/缩放帧率被拖垮）。
        // force=true 用于缩放收尾（SectionNode.updateAllCenters 传入）：缩放后必须回写缩放后
        // 坐标，否则重建端点表读到缩放前旧值 → 缓存"缩放前连线"→ 拖画布时连线错位/放缩
        if (!force && (workspaceController.inputState === 1 || workspaceController.zoomPending)) return
        // 对应 ImGui IBR_LinkNode::UpdateLink：对所有 Data_String 行无条件设置 LastCenter
        // ImGui: Center = IsImport ? ImportCenter() : DefaultCenter()
        //   DefaultCenter = GetLineEndPos() - {FontHeight*1.5, HalfLine}（行末位置）
        //   ImportCenter  = {WindowPos.x + WindowWidth*0.5, HalfLine}（窗口水平居中）
        // Qt 版三种状态：
        //   1. Link 态 + hasLinkNode：用 linkNode 中心（= DefaultCenter 位置）
        //   2. Link 态 + !hasLinkNode：用 disabledNode 中心（同 DefaultCenter 位置）
        //   3. Input 态：用 onShowLabel 行末位置（对应 ImGui Input 态仍调 UpdateLink 用 DefaultCenter）
        var node = linkNode.visible ? linkNode : disabledNode
        var cx, cy
        if (node.visible) {
            var nodePos = node.mapToItem(workspaceView, node.width / 2, node.height / 2)
            cx = nodePos.x
            cy = nodePos.y
        } else if (root.isInputMode) {
            // Input 态：用 onShowLabel 行末位置（对应 ImGui DefaultCenter = 行末 - 1.5*FontHeight）
            // onShowLabel 右边缘 = parent.width - linkNode.width - 24（Link 态宽度公式）
            // 行末 = onShowLabel 右边缘，垂直居中
            var labelEndX = root.width - root.fontSmall * 1.5
            var pos = root.mapToItem(workspaceView, labelEndX, root.height / 2)
            cx = pos.x
            cy = pos.y
        } else {
            return
        }
        // 拖拽中：减去 dragOffset，存储原位置（不含拖拽位移）
        // 原因：mapToItem 返回的坐标含 dragOffset（SectionNode.x 已含 dragOffset），
        //       若直接回写，端点表 pa 含 dragOffset，LinkRenderer 又叠加 → 重复叠加
        //       减去后 LastCenter=原位置，LinkRenderer 叠加 dragOffset 实时修正 → 速度匹配
        // 松手后：dragOffset=0，不减，LastCenter=新位置 → 无回弹
        var dx = root.isDragging ? workspaceController.dragOffset.x : 0
        var dy = root.isDragging ? workspaceController.dragOffset.y : 0
        if (workspaceController.diagLogEnabled()) console.log("[LINK-DIAG] LineRow doUpdate sid=" + root.sectionId + " row=" + root.rowIndex + " key=" + root.keyName + " cx=" + (cx-dx) + " cy=" + (cy-dy) + " isDragging=" + root.isDragging)
        root.lineModel.setLinkNodeCenter(root.rowIndex, cx - dx, cy - dy)
        // 阶段 3：同一 RadioButton 位置也作为行级接受点回写（对应 ImGui AcceptCenter）
        // 该坐标作为连线终点 pb 的行精确值
        // 按 keyName+mult 直接回写：rowIndex 在 m_entries 重建后可能错位，
        // setAcceptCenter(row) 会存错行导致 Warhead 被 Projectile 坐标覆盖，改按 key 免疫
        root.lineModel.setAcceptCenterByKey(root.keyName, root.lineMult, cx - dx, cy - dy)
    }
}
