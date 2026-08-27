// LineRow.qml
// 单行 delegate，对应 ImGui RenderUI_Line + RenderUI_Node
// 左侧：OnShow 描述文本（ShowRegName ? key : OnShow/DescShort）
// 右侧：LinkNode 圆点（RadioButton，条件显示）
// Import SubSec：LinkNode 居中（ImportCenter）
// 双击切换 IICStatus（Input 显示文本框 / Link 显示 LinkNode）
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs
import "../components"

Item {
    id: root

    // ===== 输入属性 =====
    property var sectionData: ({})
    property var lineModel: null
    property int rowIndex: -1
    property int fontBody: Math.round(11 * settingController.fontScale)
    property int fontSmall: Math.round(10 * settingController.fontScale)

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
    // 是否为 accept 目标键（InputType 带 AcceptType）：行左侧画方形接收点，可被作为具体键拖线连入
    property bool isAcceptor: false
    property color acceptorColor: "#ffffb4"
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

    // slider 值按 SlideFormat 格式化显示（printf 子集：%d/%i/%f/%.Nf/%%）
    function iifSliderFormat(v, fmt) {
        if (!fmt) return "" + v
        var out = fmt
        out = out.replace(/%%/g, "\u0000")
        out = out.replace(/%\.(\d+)f/g, function(m, n) { return v.toFixed(parseInt(n, 10)) })
        out = out.replace(/%f/g, function() { return "" + v })
        out = out.replace(/%d/g, function() { return "" + Math.round(v) })
        out = out.replace(/%i/g, function() { return "" + Math.round(v) })
        out = out.replace(/\u0000/g, "%")
        return out
    }

    // coloredFrame（UseNodeColorInFrame）：输入框底色 = 节点色压暗（对齐 imgui 暗色模式 V/S×0.7）
    function iifFrameBgColor(cc) {
        if (!cc || !cc.coloredFrame || !cc.linkCol) return "#1e1e1e"
        return Qt.darker(cc.linkCol, 1.4)
    }
    // coloredFrame 文字色：按节点色亮度选黑/白（对齐 imgui IsLightColor 判断）
    function iifFrameTextColor(cc) {
        if (!cc || !cc.coloredFrame || !cc.linkCol) return "#ce9178"
        var c = Qt.darker(cc.linkCol, 1.0)
        var lum = c.r * 0.299 + c.g * 0.587 + c.b * 0.114
        return lum > 0.5 ? "#000000" : "#ffffff"
    }
    // 对数滑条：值 → 位置百分比（0-100）；min<=0 时回退线性
    function iifLogPos(v, min, max) {
        if (max <= min || min <= 0) return 0
        var vv = Math.max(min, Math.min(max, v))
        var logMin = Math.log(min), logMax = Math.log(max)
        return (Math.log(vv) - logMin) / (logMax - logMin) * 100
    }
    // 对数滑条：位置百分比 → 值；min<=0 时回退线性
    function iifLogValue(pos, min, max) {
        if (max <= min || min <= 0) return min
        var logMin = Math.log(min), logMax = Math.log(max)
        return Math.round(Math.exp(logMin + (logMax - logMin) * pos / 100))
    }

    // 获取当前行 IIF 分量列表
    function iifList() {
        if (!root.lineModel || root.rowIndex < 0) return []
        return root.lineModel.iifComponents(root.rowIndex)
    }

    // IIF 行建模（对齐 imgui IBG_InputForm::RenderUI 流式布局）：
    // 默认每个非标记分量新起一行（Text 等自动换行）；samel 把下一个并入当前行（SameLine）；
    // newl 强制换行；sep 单独成"分隔线行"（Separator）。
    // radio/choice 是"内部流式折行"的多行块，由其自身行高占据多行（见 iifCellUnitHeight），
    // 后续内容按流式自然下移，不额外强制换行……
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
            // Constraint 不满足的分量不渲染（对齐 imgui `if (!ValueContainer.Satisfy(IC->Constraint))continue;`）
            if (c.hidden) continue
            var t = c.type
            if (t === "samel") { same = true; continue }
            if (t === "newl") {
                // NewLine 标记：在两个分量之间插入一个空行（对齐用户要求"写了 newline 就空一行"）。
                // 合并连续 newl（只产生一个空行）；行首/行尾的 newl 不产生空行。
                cur = null; same = false
                if (rows.length > 0 && !rows[rows.length - 1].isBlank)
                    rows.push({ isSep: false, isBlank: true, node: null, cells: [] })
                continue
            }
            if (t === "sep") { rows.push({ isSep: true, node: null, cells: [] }); cur = null; same = false; continue }
            // SetValue（setter）：只设置值，不渲染可见控件、不占空间（对齐 ImGui IIC_Setter_String::RenderUI 无控件）
            if (t === "setter") { continue }
            if (!same) cur = null
            if (!cur) cur = newRow()
            if (t === "link") cur.node = c
            else cur.cells.push(c)
            same = false
        }
        // 移除末尾空行（行尾 NewLine 不产生空行）
        if (rows.length > 0 && rows[rows.length - 1].isBlank) rows.pop()
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
    // color 值 → QML 色串：#RRGGBB（支持 "255,0,0" Int 模式与 "#xxx" 十六进制；
    // 对齐 ImGui IIC_ColorPanel：vMode 决定通道序（rgb/bgr/hsv），fMode 决定值格式）
    function iifColorToHex(v, cc) {
        if (!v) return "#000000"
        var s = "" + v
        s = s.trim()
        if (s.charAt(0) === "#") return s
        var vMode = (cc && cc.vMode) || "rgb"
        var parts = s.split(",")
        if (parts.length >= 3) {
            var r = parseInt(parts[0], 10), g = parseInt(parts[1], 10), b = parseInt(parts[2], 10)
            if (!isNaN(r) && !isNaN(g) && !isNaN(b)) {
                // BGR：值按 B,G,R 存储，交换 R/B
                if (vMode === "bgr") { var tmp = r; r = b; b = tmp }
                // HSV：H,S,V(0-255) → RGB
                if (vMode === "hsv") {
                    var hh = r / 255.0, ss = g / 255.0, vv = b / 255.0
                    var rr, gg, bb
                    var i = Math.floor(hh * 6), f = hh * 6 - i
                    var p = vv * (1 - ss), q = vv * (1 - f * ss), t = vv * (1 - (1 - f) * ss)
                    switch (i % 6) {
                    case 0: rr = vv; gg = t; bb = p; break
                    case 1: rr = q; gg = vv; bb = p; break
                    case 2: rr = p; gg = vv; bb = t; break
                    case 3: rr = p; gg = q; bb = vv; break
                    case 4: rr = t; gg = p; bb = vv; break
                    default: rr = vv; gg = p; bb = q; break
                    }
                    r = Math.round(rr * 255); g = Math.round(gg * 255); b = Math.round(bb * 255)
                }
                function h(n) { var x = n.toString(16); return x.length === 1 ? "0" + x : x }
                return "#" + h(Math.max(0, Math.min(255, r))) + h(Math.max(0, Math.min(255, g))) + h(Math.max(0, Math.min(255, b)))
            }
        }
        return s
    }
    // color 值格式判定：优先分量 fMode（对齐 ImGui FMode），其次按值串猜测
    function iifColorFormat(v, cc) {
        var fMode = (cc && cc.fMode) || ""
        if (fMode) return fMode
        var s = (v || "").trim()
        if (!s) return "int"
        if (s.charAt(0) === "#") return "hashhex"
        if (s.indexOf(",") >= 0) return (s.indexOf(".") >= 0) ? "float" : "int"
        return "hex"
    }
    // 取色器选中后按分量 fMode/vMode 写回，避免破坏原有 Hex/Hash_Hex/Int/Float 形式与通道序
    function iifColorWrite(cc, color) {
        if (!root.lineModel || !cc) return
        var fmt = root.iifColorFormat(cc.value, cc)
        var vMode = (cc && cc.vMode) || "rgb"
        function h(n) { var x = n.toString(16); return x.length === 1 ? "0" + x : x }
        var r = Math.round(color.r * 255), g = Math.round(color.g * 255), b = Math.round(color.b * 255)
        var val
        if (fmt === "hashhex") val = "#" + h(r) + h(g) + h(b)
        else if (fmt === "hex") val = h(r) + h(g) + h(b)
        else if (fmt === "float") val = color.r.toFixed(3) + "," + color.g.toFixed(3) + "," + color.b.toFixed(3)
        else val = r + "," + g + "," + b
        // BGR：写回时交换 R/B（值按 B,G,R 存储）
        if (vMode === "bgr") {
            if (fmt === "hashhex") val = "#" + h(b) + h(g) + h(r)
            else if (fmt === "hex") val = h(b) + h(g) + h(r)
            else if (fmt === "float") val = color.b.toFixed(3) + "," + color.g.toFixed(3) + "," + color.r.toFixed(3)
            else val = b + "," + g + "," + r
        }
        // HSV：RGB → H,S,V(0-255) 写回
        if (vMode === "hsv") {
            var max = Math.max(r, g, b) / 255, min = Math.min(r, g, b) / 255
            var d = max - min
            var hh = 0, ss = 0, vv = max
            if (d !== 0) {
                ss = d / max
                var rr = r / 255, gg = g / 255, bb = b / 255
                if (max === rr) hh = ((gg - bb) / d) % 6
                else if (max === gg) hh = (bb - rr) / d + 2
                else hh = (rr - gg) / d + 4
                hh *= 60
                if (hh < 0) hh += 360
            }
            var H = Math.round(hh / 360 * 255), S = Math.round(ss * 255), V = Math.round(vv * 255)
            if (fmt === "hashhex") val = "#" + h(H) + h(S) + h(V)
            else if (fmt === "hex") val = h(H) + h(S) + h(V)
            else if (fmt === "float") val = (H / 255).toFixed(3) + "," + (S / 255).toFixed(3) + "," + (V / 255).toFixed(3)
            else val = H + "," + S + "," + V
        }
        root.lineModel.setIifComponentValue(root.rowIndex, cc.idx || cc.compIdx || 0, val)
    }
    // ===== 文本真实度量（对齐字体大小：中文字符/字体变化时 length*0.6 估算会失真 → 重叠） =====
    // 用 FontMetrics.advanceWidth()（纯方法调用）：不写属性、不建立绑定依赖，
    // 避免 TextMetrics.text 赋值在绑定求值中修改自身依赖属性导致绑定循环。
    FontMetrics {
        id: iifFontM
        font.pixelSize: root.fontBody
    }
    // 文本真实像素宽度（advance 宽，随 fontBody 自动缩放）
    function iifTextWidth(str) {
        if (!str) return 0
        return iifFontM.advanceWidth(str)
    }
    // radio/choice 单个选项项宽（圆点/勾框 + spacing + 标签真实宽）
    function iifOptItemWidth(opt) {
        return (root.fontBody * 1.4 + 4) + 3 + root.iifTextWidth(opt.label || opt.key || "")
    }
    // radio/choice 选项按真实宽度折行分组（对齐 ImGui SameLine 流式：宽度不足自动换行；
    // MaxInOneLine>0 时每行最多 MaxInOneLine 个；SameLine=false 每项单独一行）
    // limitW<=0 表示不限宽（全部并排一行）；返回二维索引数组 rows[行][选项下标]
    function iifOptRows(cc, limitW) {
        var arr = root.iifOptArr(cc)
        var rows = []
        var cur = []
        var curW = 0
        var maxPer = (cc.maxInOneLine && cc.maxInOneLine > 0) ? cc.maxInOneLine : -1
        for (var i = 0; i < arr.length; i++) {
            var itemW = root.iifOptItemWidth(arr[i])
            if (cc.sameLine === false) { rows.push([i]); continue }
            var gap = cur.length > 0 ? 6 : 0
            if (cur.length > 0 && ((maxPer > 0 && cur.length >= maxPer)
                                   || (limitW > 0 && curW + gap + itemW > limitW))) {
                rows.push(cur); cur = [i]; curW = itemW
            } else {
                cur.push(i); curW += gap + itemW
            }
        }
        if (cur.length > 0) rows.push(cur)
        return rows
    }
    // radio/choice 折行可用宽：目标模块宽(widthFix/widthBase) - onShow 标签真实宽 - 边距
    function iifOptLimitWidth() {
        var baseW = 221
        if (root.sectionData) {
            if (root.sectionData.widthFix) baseW = root.sectionData.widthFix
            else if (root.sectionData.widthBase) baseW = root.sectionData.widthBase
        }
        var labelW = root.iifTextWidth(root.onShowText || "")
        return Math.max(60, baseW - labelW - 16)
    }

    // 单个计量分量（非 fillWidth）的自然宽：text/locale/setter/bool/radio/choice
    function iifCellNaturalWidth(cc) {
        if (!cc) return 30
        var t = cc.type
        if (t === "bool") return (cc.label ? root.iifTextWidth(cc.label) + 4 : 0) + root.fontBody * 1.4 + 4 + 4
        if (t === "text" || t === "locale" || t === "setter")
            return (cc.label ? root.iifTextWidth(cc.label) + 4 : 0)
                   + (cc.value ? root.iifTextWidth(cc.value) : 0) + 4
        if (t === "radio" || t === "choice") {
            // 按真实宽度折行：取最宽一行选项宽（含行间距），使模块宽度有界且选项不重叠
            var w = (cc.label ? root.iifTextWidth(cc.label) + 4 : 0)
            var rows = root.iifOptRows(cc, root.iifOptLimitWidth())
            var maxRowW = 0
            for (var ri = 0; ri < rows.length; ri++) {
                var rw = 0
                var rrow = rows[ri]
                for (var rj = 0; rj < rrow.length; rj++)
                    rw += root.iifOptItemWidth(root.iifOptArr(cc)[rrow[rj]]) + (rj > 0 ? 6 : 0)
                if (rw > maxRowW) maxRowW = rw
            }
            return w + maxRowW + 4
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
            if (row.node) w += (root.iifTextWidth(row.node.label || "")
                               + root.fontSmall * 1.5 + 8)   // 节点 Short 标签 + 节点区
            if (w > maxRow) maxRow = w
        }
        root.iifNaturalWidth = root.onShowLabel.implicitWidth + 6 + maxRow
    }
    // 单个分量占用高度：radio/choice 内部选项按真实宽度折行到多行时高度随之增高，
    // 使多行选项不向下溢出压到下方的控件/键行；其余单行分量维持 fontBody*2。
    // 行数必须与渲染处 iifOptRows 的分组一致（同一 limitW），否则内容溢出/重叠。
    function iifCellUnitHeight(cc) {
        if (!cc) return root.fontBody * 2
        var t = cc.type
        if (t === "radio" || t === "choice") {
            var rows = root.iifOptRows(cc, root.iifOptLimitWidth()).length
            var itemH = root.fontBody * 1.4 + 4          // 单个选项高
            var sp = 4                                   // Column spacing
            var h = rows * itemH + Math.max(0, rows - 1) * sp
            // hint(Short) 独占第一行（对齐 imgui TextWrapped(Hint.Short)）
            if ((cc.label || "").length > 0) h += root.fontBody * 2 + sp
            return Math.max(root.fontBody * 2, h)
        }
        return root.fontBody * 2
    }
    // 单行实际高度：取行内各分量最大高度（分隔行固定 6）
    function iifRowHeight(row) {
        if (!row || row.isSep) return 6
        var h = root.fontBody * 2
        var cells = row.cells
        if (cells) for (var i = 0; i < cells.length; i++)
            h = Math.max(h, root.iifCellUnitHeight(cells[i]))
        return h
    }
    // IIF 分量总高：逐行累加实际行高；模块高度随 radio/choice 多行自适应
    function iifTotalHeight() {
        if (!(root.isInputMode && root.keyType === 2) || !root.lineModel) return 0
        var rows = root.iifRows()
        var h = 0
        for (var i = 0; i < rows.length; i++)
            h += root.iifRowHeight(rows[i])
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

    // 行为A：拖线悬停本 acceptor 键行时画整行高亮框（acceptorColor 半透明边框/底色）。
    // 对应 ImGui IBR_Misc.cpp:100-122 AcceptFullArea 整行 DropTarget 预览。
    Rectangle {
        anchors.fill: parent
        visible: root.isAcceptor
                 && (workspaceController.dragAcceptorSectionId === (root.sectionData.sectionId || 0))
                 && (workspaceController.dragAcceptorRowKey === root.keyName)
        color: Qt.rgba(root.acceptorColor.r, root.acceptorColor.g, root.acceptorColor.b, 0.16)
        border.color: root.acceptorColor
        border.width: 1.5
        radius: 2
        z: -1
    }

    // 左侧 OnShow 描述文本（对应 ImGui wline.RenderUI(OnShow)）
    Text {
        id: onShowLabel
        anchors.left: parent.left
        anchors.leftMargin: 8
        // 键 hint 留在第一行（对齐 imgui TextEx(Line) + 默认 NewLineAfterDesc=false 与 IIF 同排）：
        // IIF 多行时顶部对齐第一行，普通行垂直居中
        anchors.verticalCenter: root.keyType === 2 ? undefined : parent.verticalCenter
        anchors.top: root.keyType === 2 ? parent.top : undefined
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

        // 双击切换 Input 态（对应 IBG_InputType.cpp:307 翻转 IICStatus）——整键所有分量
        // 用 TapHandler 而非 MouseArea，避免拦截单击（单击需穿透到 nodeMouseArea 选中模块）
        TapHandler {
            acceptedButtons: Qt.LeftButton
            acceptedDevices: PointerDevice.Mouse | PointerDevice.TouchPad
            onTapped: {
                if (tapCount === 2) {
                    if (root.lineModel) {
                        root.lineModel.toggleKeyInputMode(root.rowIndex)
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

    // 左侧 acceptor 方形接收点（对应 ImGui IBR_Misc.cpp:101-141）
    // 带 AcceptType 的键（Collector/Armor 等）在行左侧画一个方形彩色点，可被拖线作为「具体键」连入。
    Rectangle {
        id: acceptorNode
        visible: root.isAcceptor
        anchors.right: onShowLabel.left
        anchors.rightMargin: 4
        anchors.verticalCenter: parent.verticalCenter
        width: root.fontSmall * 1.4
        height: width
        radius: 2  // 圆角方（对齐 ImGui AddRectFilled(square_sz*0.1f)）
        color: "transparent"
        border.color: root.acceptorColor
        border.width: 1
        Rectangle {
            anchors.fill: parent
            anchors.margins: Math.max(1.0, parent.width / 6.0)
            radius: parent.radius - 0.5
            color: root.acceptorColor
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

        // 节点只在 Link 态显示（对应 ImGui：Status.InputMethod==Link 才调 RenderUI_Node 画节点；
        // Input 态只画输入框，无节点）。普通键在输入态不画此圆点，双击键名切到 Link 态后再现。
        // IIF 键（keyType===2）无行级自节点（对应 ImGui IIF 只渲染各分量节点，整行没有独立节点），
        // 且 IIF 恒为 Input 态，故此处直接按 !isInputMode 排除。
        visible: !root.isInputMode && root.hasLinkNode && root.keyType !== 2
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
        // 无 LinkNode 的灰色 Disabled 点也只在 Link 态显示（对应 imgui RenderUI_Node_Disabled 仅在
        // Status==Link 时出现）；Input 态普通键只画输入框，不画此点。
        visible: !root.isInputMode && !root.hasLinkNode && root.keyType !== 2

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
        topPadding: 0
        bottomPadding: 0
        // 绑定 exportValue：ImGui 每帧用 CurrentValue 重新渲染 InputText
        text: root.exportValue
        font.pixelSize: root.fontBody
        color: "#ce9178"
        selectByMouse: true
        horizontalAlignment: Text.AlignRight
        verticalAlignment: Text.AlignVCenter
        background: Rectangle {
            color: "#1e1e1e"
            border.color: parent.activeFocus ? "#007acc" : "#3c3c3c"
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
        // z 提到行级右键 MouseArea（lineRightClickMA）之上：分量节点的右键（弹节点菜单）
        // 不能被行级右键拦截（同 z 时后声明的 lineRightClickMA 覆盖整行会抢走分量圆点的右键）
        z: 2
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
            id: iifRowsRepeater
            model: (root.keyType === 2 && root.iifRevision >= 0) ? root.iifRows() : []
            delegate: Item {
                id: iifRow
                property var rowData: modelData
                width: iifEdit.width
                height: (rowData && rowData.isSep) ? 6 : root.iifRowHeight(rowData)
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

                // 普通行：左侧 cells（label+控件），右侧节点（对应 imgui RenderUI_Node 流式）
                RowLayout {
                    visible: !(rowData && rowData.isSep)
                    anchors.fill: parent
                    spacing: 4

                    // 行首处：链接节点的 Short 标签靠左显示（对齐普通分量 label 位置）
                    Text {
                        property var cc: rowData && rowData.node ? rowData.node : null
                        Layout.preferredWidth: implicitWidth
                        Layout.alignment: Qt.AlignVCenter
                        visible: cc && (cc.label || "").length > 0
                        text: cc ? (cc.label || "") : ""
                        color: cc && cc.disabled ? "#6e6e6e" : "#9cdcfe"
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
                            height: root.iifCellUnitHeight(cc)

                            // Short 标签（imgui TextEx(Hint.Short)）——每个交互分量都有的可见 Hint
                            Text {
                                id: cellLabel
                                // radio/choice 的 hint 由组件内部独占第一行（imgui TextWrapped(Hint.Short)），
                                // 不在此处左边横排，故对这两类隐藏
                                visible: (cc.label || "").length > 0 && cc.type !== "radio" && cc.type !== "choice"
                                anchors.left: parent.left
                                anchors.verticalCenter: parent.verticalCenter
                                text: cc.label || ""
                                color: cc.disabled ? "#6e6e6e" : "#9cdcfe"
                                font.pixelSize: root.fontBody
                                elide: Text.ElideRight
                            }

                            // 内容左偏移（label 之后）
                            property real ctlX: cellLabel.visible ? cellLabel.width + 3 : 0

                            // 纯文本/本地化/赋值串（只读，本身就是内容，无 label）
                            // 对齐 ImGui IIC_PureText/LocalizedText：Colored 用分量 Color，Wrapped 自动换行；
                            // Disabled 用禁用文本色（对齐 imgui PushStyleColor(TextDisabled)）
                            Text {
                                id: cellTxtVal
                                visible: (cc.type === "text" || cc.type === "locale" || cc.type === "setter")
                                anchors.left: parent.left
                                anchors.verticalCenter: parent.verticalCenter
                                text: cc.value || ""
                                color: cc.disabled ? "#6e6e6e"
                                     : (cc.colored ? (cc.color || "#c586c0") : "#c586c0")
                                font.pixelSize: root.fontBody
                                wrapMode: cc.wrapped ? Text.Wrap : Text.NoWrap
                                elide: cc.wrapped ? Text.ElideNone : Text.ElideRight
                            }

                            // 布尔勾选框（可见 label + 勾选框）
                            // Disabled：对齐 imgui BeginDisabled——灰显且不可点击
                            Rectangle {
                                visible: cc.type === "bool"
                                width: root.fontBody * 1.4 + 4
                                height: width
                                x: iifCell.ctlX
                                anchors.verticalCenter: parent.verticalCenter
                                radius: 3
                                color: cbMA.containsMouse ? "#3a3a3a" : "#1e1e1e"
                                border.color: root.boolTrue(cc.value) ? (cc.disabled ? "#3a5a6e" : "#007acc") : "#5a5a5a"
                                border.width: 1
                                Text {
                                    visible: root.boolTrue(cc.value)
                                    anchors.centerIn: parent
                                    text: "✓"
                                    color: cc.disabled ? "#3a6e5e" : "#4ec9b0"
                                    font.pixelSize: root.fontBody
                                    font.bold: true
                                }
                                MouseArea {
                                    id: cbMA
                                    anchors.fill: parent
                                    preventStealing: true
                                    hoverEnabled: true
                                    enabled: !cc.disabled
                                    onClicked: {
                                        if (root.lineModel)
                                            root.lineModel.setIifComponentValueBool(root.rowIndex, cc.idx || cc.compIdx, !root.boolTrue(cc.value))
                                    }
                                }
                            }

                            // 枚举单选组（radio）：对齐 ImGui IIC_EnumRadio——hint(Short) 独占第一行，
                            // 选项从下一行起按真实宽度自动折行（对齐 ImGui SameLine 流式；MaxInOneLine 限制每行上限）
                            Column {
                                visible: cc.type === "radio"
                                x: iifCell.ctlX
                                spacing: 4
                                // hint 第一行（imgui TextWrapped(Hint.Short) 后自动换行）
                                Text {
                                    visible: (cc.label || "").length > 0
                                    text: cc.label || ""
                                    color: cc.disabled ? "#6e6e6e" : "#9cdcfe"
                                    font.pixelSize: root.fontBody
                                }
                                Repeater {
                                    model: root.iifOptRows(cc, root.iifOptLimitWidth())
                                    delegate: Row {
                                        required property var modelData   // 该行选项下标数组
                                        readonly property var rowIdxArr: modelData
                                        spacing: 6
                                        Repeater {
                                            model: rowIdxArr
                                            delegate: Item {
                                                required property int index   // 行内序号
                                                readonly property var opt: root.iifOptArr(cc)[rowIdxArr[index]]
                                                width: root.iifOptItemWidth(opt)
                                                height: root.fontBody * 1.4 + 4
                                                // 圆点与文本都垂直居中（Row 顶部对齐会导致错位）
                                                Rectangle {
                                                    anchors.left: parent.left
                                                    anchors.verticalCenter: parent.verticalCenter
                                                    width: root.fontBody * 1.4 + 4
                                                    height: width
                                                    radius: width / 2
                                                    color: (opt.key === cc.value) ? (cc.disabled ? "#2a4a5e" : "#2d6db5") : "#1e1e1e"
                                                    border.color: (opt.key === cc.value) ? (cc.disabled ? "#3a5a6e" : "#007acc") : "#5a5a5a"
                                                    border.width: 1
                                                    MouseArea {
                                                        anchors.fill: parent
                                                        preventStealing: true
                                                        hoverEnabled: true
                                                        enabled: !cc.disabled
                                                        onClicked: {
                                                            if (root.lineModel)
                                                                root.lineModel.setIifComponentValue(root.rowIndex, cc.idx || cc.compIdx, opt.key)
                                                        }
                                                    }
                                                }
                                                Text {
                                                    anchors.left: parent.left
                                                    anchors.leftMargin: root.fontBody * 1.4 + 4 + 3
                                                    anchors.verticalCenter: parent.verticalCenter
                                                    text: opt.label || opt.key || ""
                                                    color: cc.disabled ? "#6e6e6e" : "#d4d4d4"
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
                                    }
                                }

                            // 多选组（choice）：对齐 ImGui IIC_MultipleChoice——hint(Short) 独占第一行，
                            // 选项从下一行起按真实宽度自动折行（对齐 ImGui SameLine 流式；MaxInOneLine 限制每行上限）
                            Column {
                                visible: cc.type === "choice"
                                x: iifCell.ctlX
                                spacing: 4
                                // hint 第一行（imgui TextWrapped(Hint.Short) 后自动换行）
                                Text {
                                    visible: (cc.label || "").length > 0
                                    text: cc.label || ""
                                    color: cc.disabled ? "#6e6e6e" : "#9cdcfe"
                                    font.pixelSize: root.fontBody
                                }
                                Repeater {
                                    model: root.iifOptRows(cc, root.iifOptLimitWidth())
                                    delegate: Row {
                                        required property var modelData   // 该行选项下标数组
                                        readonly property var rowIdxArr: modelData
                                        spacing: 6
                                        Repeater {
                                            model: rowIdxArr
                                            delegate: Item {
                                                required property int index   // 行内序号
                                                readonly property var opt: root.iifOptArr(cc)[rowIdxArr[index]]
                                                readonly property var sel: root.iifChoiceSelected(cc)
                                                width: root.iifOptItemWidth(opt)
                                                height: root.fontBody * 1.4 + 4
                                                // 勾选框与文本都垂直居中（Row 顶部对齐会导致错位）
                                                Rectangle {
                                                    anchors.left: parent.left
                                                    anchors.verticalCenter: parent.verticalCenter
                                                    width: root.fontBody * 1.4 + 4
                                                    height: width
                                                    radius: 3
                                                    color: cbChk.containsMouse ? "#3a3a3a" : "#1e1e1e"
                                                    border.color: (opt.key.length > 0 && sel[opt.key] === true) ? (cc.disabled ? "#3a5a6e" : "#007acc") : "#5a5a5a"
                                                    border.width: 1
                                                    Text {
                                                        visible: opt.key.length > 0 && sel[opt.key] === true
                                                        anchors.centerIn: parent
                                                        text: "✓"
                                                        color: cc.disabled ? "#3a6e5e" : "#4ec9b0"
                                                        font.pixelSize: root.fontBody
                                                        font.bold: true
                                                    }
                                                    MouseArea {
                                                        id: cbChk
                                                        anchors.fill: parent
                                                        preventStealing: true
                                                        hoverEnabled: true
                                                        enabled: !cc.disabled
                                                        onClicked: {
                                                            var on = !(sel[opt.key] === true)
                                                            root.iifSetChoice(cc, opt.key, on)
                                                        }
                                                    }
                                                }
                                                Text {
                                                    anchors.left: parent.left
                                                    anchors.leftMargin: root.fontBody * 1.4 + 4 + 3
                                                    anchors.verticalCenter: parent.verticalCenter
                                                    text: opt.label || opt.key || ""
                                                    color: cc.disabled ? "#6e6e6e" : "#d4d4d4"
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
                                }
                            }

                            // 输入/整数分量：label + 占满剩余宽的输入框
                            // Disabled：对齐 imgui BeginDisabled——灰显且不可编辑，但仍显示输入框
                            // ColoredFrame：对齐 imgui UseNodeColorInFrame——框底染节点色、文字按亮度黑白
                            TextField {
                                id: cellInput
                                visible: (cc.type === "input" || cc.type === "int")
                                x: iifCell.ctlX
                                width: parent.width - x
                                height: root.fontBody * 2
                                topPadding: 0
                                bottomPadding: 0
                                anchors.verticalCenter: parent.verticalCenter
                                text: cc.value || ""
                                font.pixelSize: root.fontBody
                                color: cc.disabled ? "#6e6e6e" : root.iifFrameTextColor(cc)
                                verticalAlignment: Text.AlignVCenter
                                selectByMouse: true
                                readOnly: cc.readOnly || cc.disabled || false
                                background: Rectangle {
                                    color: cc.disabled ? "#161616" : root.iifFrameBgColor(cc)
                                    border.color: parent.activeFocus ? "#007acc" : "#3c3c3c"
                                    border.width: 1
                                    radius: 2
                                }
                                onEditingFinished: {
                                    if (root.lineModel)
                                        root.lineModel.setIifComponentValue(root.rowIndex, cc.idx || cc.compIdx, text)
                                }
                                // 双击该分量输入框 → 只切该分量节点（对应 ImGui 双击分量切该分量状态）
                                TapHandler {
                                    acceptedButtons: Qt.LeftButton
                                    acceptedDevices: PointerDevice.Mouse | PointerDevice.TouchPad
                                    onTapped: {
                                        if (tapCount === 2 && root.lineModel) {
                                            root.lineModel.toggleComponentInputMode(root.rowIndex, cc.idx || cc.compIdx)
                                        }
                                    }
                                }
                            }

                            // 下拉组合框（combo）
                            // Disabled：对齐 imgui BeginDisabled——灰显且不可交互
                            // ColoredFrame：对齐 imgui UseNodeColorInFrame——框底染节点色、文字按亮度黑白
                            ComboBox {
                                visible: cc.type === "combo"
                                id: comboCtrl
                                enabled: !cc.disabled
                                x: iifCell.ctlX
                                width: parent.width - x
                                height: root.fontBody * 2
                                topPadding: 0
                                bottomPadding: 0
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
                                    color: cc.disabled ? "#161616" : root.iifFrameBgColor(cc)
                                    // 激活判定与输入框不同：ComboBox 点开弹出层后自身可能一直持有
                                    // activeFocus（选中/点外关闭也不释放）→ 用 popup.visible 判断
                                    // "下拉是否打开中"，关闭即变灰，等同于输入框失焦效果
                                    border.color: comboCtrl.popup.visible ? "#007acc" : "#3c3c3c"
                                    border.width: 1
                                    radius: 0   // 方角（对齐模块内其他控件）
                                }
                                contentItem: Text {
                                    text: comboCtrl.currentText
                                    color: cc.disabled ? "#6e6e6e" : root.iifFrameTextColor(cc)
                                    font.pixelSize: root.fontBody
                                    verticalAlignment: Text.AlignVCenter
                                    horizontalAlignment: Text.AlignLeft
                                    leftPadding: 4
                                    rightPadding: 16  // 给右侧下拉箭头留空间，避免文字与箭头重叠
                                    elide: Text.ElideRight
                                }
                                indicator: Text {
                                    anchors.verticalCenter: parent.verticalCenter
                                    anchors.right: parent.right
                                    anchors.rightMargin: 6
                                    text: "▾"
                                    color: cc.disabled ? "#5a5a5a" : "#9a9a9a"
                                    font.pixelSize: root.fontBody
                                }
                                // 下拉弹层：统一深色方角风格，替换 Qt 默认浅色圆角列表外观
                                popup: Popup {
                                    // 缩放时 SectionNode 的 GPU scale 变换不触发 Popup 重定位，
                                    // 弹层锚点会错位 → 监测 ratio，缩放中若有弹层则收起，
                                    // 缩放结束后重新打开即在正确位置（消除缩放偏移）
                                    property real ratioWatch: workspaceController.ratio
                                    onRatioWatchChanged: if (comboCtrl.popup.opened) comboCtrl.popup.close()
                                    // 打开时先把选中项滚到顶端；置 needPos，contentHeight 稳定后由 settleTimer 定位一次
                                    onOpened: {
                                        comboList.needPos = true
                                        comboList.settleTimer.restart()
                                        Qt.callLater(() => {
                                            comboList.positionViewAtIndex(comboCtrl.currentIndex, ListView.Beginning)
                                            console.log("[COMBO-DIAG] open row=" + root.rowIndex + " key='" + root.keyName
                                                        + "' cur=" + comboCtrl.currentIndex + " count=" + comboList.count
                                                        + " ratio=" + workspaceController.ratio + " contentH=" + comboList.contentHeight)
                                            console.log("[COMBO-DIAG] after posY=" + comboList.contentY)
                                        })
                                    }
                                    // popup.x/y 是相对下拉框(comboCtrl)的本地坐标，Qt 经 parent(含 GPU scale)
                                    // 映射 → y 用本地 comboCtrl.height 即贴下缘且随缩放正确；绝不手动×ratio
                                    // 或赋 overlay 绝对坐标（会造成二次偏移/带飞）。
                                    // 弹层内容是窗口级像素、不随模块 scale，故宽/行高/字体仍×ratio 匹配视觉尺寸。
                                    y: comboCtrl.height
                                    width: comboCtrl.width * workspaceController.ratio
                                    padding: 0
                                    background: Rectangle {
                                        color: "#2d2d2d"
                                        border.color: "#3c3c3c"
                                        border.width: 1
                                        radius: 0
                                    }
                                    contentItem: ListView {
                                        id: comboList
                                        clip: true
                                        // 用数值 count 作 model，delegate 经 index 直接从 opts 取元素，
                                        // 避免对象数组 model→modelData 映射异常（曾导致所有项显示同一文本）
                                        model: root.iifOptArr(cc).length
                                        width: comboCtrl.width * workspaceController.ratio
                                        // 行高=下拉框高（紧凑），列表高度上限 7 项防过高，超出滚动
                                        implicitHeight: Math.min(contentHeight,
                                            (comboCtrl.height + 2) * 7 * workspaceController.ratio)
                                        // 缩放后 contentHeight 变化即重启 50ms 稳定定时器；连续无变化(稳定)后才 positionViewAtIndex 一次，
                                        // 用 Qt 已排布好的几何定位（避免过渡中拿中间几何；确定性算的 itemH 与实测不一致会随缩放偏差）
                                        property bool needPos: false
                                        Timer {
                                            id: settleTimer
                                            interval: 50
                                            onTriggered: {
                                                if (comboList.needPos) {
                                                    comboList.needPos = false
                                                    comboList.positionViewAtIndex(comboCtrl.currentIndex, ListView.Beginning)
                                                }
                                            }
                                        }
                                        onContentHeightChanged: {
                                            if (comboList.needPos) comboList.settleTimer.restart()
                                        }
                                        onMovementStarted: { comboList.needPos = false; comboList.settleTimer.stop() }
                                        delegate: Rectangle {
                                            required property int index
                                            readonly property var opt: {
                                                var arr = root.iifOptArr(cc)
                                                return (index >= 0 && index < arr.length) ? arr[index] : null
                                            }
                                            width: comboList.width
                                            height: comboCtrl.height * workspaceController.ratio
                                            // 当前选中或悬停 → 高亮
                                            color: (index === comboCtrl.currentIndex || hoverArea.containsMouse)
                                                ? "#3e3e3e" : "transparent"
                                            Text {
                                                text: (opt && opt.label) || (opt && opt.key) || ""
                                                color: "#d4d4d4"
                                                font.pixelSize: root.fontBody * workspaceController.ratio
                                                verticalAlignment: Text.AlignVCenter
                                                leftPadding: 8
                                                rightPadding: 8
                                                elide: Text.ElideRight
                                            }
                                            MouseArea {
                                                id: hoverArea
                                                anchors.fill: parent
                                                hoverEnabled: true
                                                onClicked: {
                                                    if (opt && root.lineModel)
                                                        root.lineModel.setIifComponentValue(
                                                            root.rowIndex, cc.idx || cc.compIdx, opt.key)
                                                    comboCtrl.currentIndex = index
                                                    comboCtrl.popup.close()
                                                }
                                            }
                                        }
                                        ScrollIndicator.vertical: ScrollIndicator {
                                            width: 6
                                            contentItem: Rectangle { color: "#3c3c3c" }
                                        }
                                    }
                                }
                            }

                            // 取色器（color 分量点击色块弹出，对应 ImGui IIC_ColorPanel 的 ColorPicker3）
                            // 用 Dialog 通用 onAccepted + selectedColor：当前 Qt 版本 ColorDialog 无 onColorSelected 信号
                            ColorDialog {
                                id: colorDlg
                                title: "选择颜色"
                                onAccepted: { if (root.lineModel) root.iifColorWrite(cc, colorDlg.selectedColor) }
                            }

                            // 色板（color）：色块可点击取色 + 值输入（label + 占满剩余宽）
                            // Disabled：对齐 imgui BeginDisabled——灰显且不可取色/编辑
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
                                    color: root.iifColorToHex(cc.value, cc)
                                    border.color: "#5a5a5a"
                                    border.width: 1
                                    anchors.verticalCenter: parent.verticalCenter
                                    // 点击弹出取色器
                                    MouseArea {
                                        anchors.fill: parent
                                        hoverEnabled: true
                                        cursorShape: Qt.PointingHandCursor
                                        enabled: !cc.disabled
                                        onClicked: {
                                            colorDlg.selectedColor = root.iifColorToHex(cc.value, cc)
                                            colorDlg.open()
                                        }
                                    }
                                }
                                TextField {
                                    text: cc.value || ""
                                    height: root.fontBody * 2
                                    topPadding: 0
                                    bottomPadding: 0
                                    width: parent.width - parent.spacing - (root.fontBody * 1.6)
                                    anchors.verticalCenter: parent.verticalCenter
                                    font.pixelSize: root.fontBody
                                    color: cc.disabled ? "#6e6e6e" : root.iifFrameTextColor(cc)
                                    verticalAlignment: Text.AlignVCenter
                                    selectByMouse: true
                                    readOnly: cc.disabled || false
                                    background: Rectangle {
                                        color: cc.disabled ? "#161616" : root.iifFrameBgColor(cc)
                                        border.color: parent.activeFocus ? "#007acc" : "#3c3c3c"
                                        border.width: 1
                                        radius: 2
                                    }
                                    onEditingFinished: {
                                        if (root.lineModel)
                                            root.lineModel.setIifComponentValue(root.rowIndex, cc.idx || cc.compIdx, text)
                                    }
                                    // 双击该分量输入框 → 只切该分量节点（对应 ImGui 双击分量切该分量状态）
                                    TapHandler {
                                        acceptedButtons: Qt.LeftButton
                                        acceptedDevices: PointerDevice.Mouse | PointerDevice.TouchPad
                                        onTapped: {
                                            if (tapCount === 2 && root.lineModel) {
                                                root.lineModel.toggleComponentInputMode(root.rowIndex, cc.idx || cc.compIdx)
                                            }
                                        }
                                    }
                                }
                            }

                            // 滑条（slider）：滑条 + 当前值
                            // Disabled：对齐 imgui BeginDisabled——灰显且不可拖动
                            // Logarithmic：对齐 imgui ImGuiSliderFlags_Logarithmic——对数刻度（min>0 时生效）
                            // ColoredFrame：对齐 imgui UseNodeColorInFrame——轨道染节点色、文字按亮度黑白
                            Row {
                                visible: cc.type === "slider"
                                x: iifCell.ctlX
                                width: parent.width - x
                                anchors.verticalCenter: parent.verticalCenter
                                spacing: 6
                                Slider {
                                    id: sliderCtrl
                                    enabled: !cc.disabled
                                    // 对数滑条用 0-100 位置百分比；线性滑条用 min-max 实际值
                                    from: cc.logarithmic ? 0 : (cc.min || 0)
                                    to: cc.logarithmic ? 100 : (cc.max || 100)
                                    value: cc.logarithmic
                                           ? root.iifLogPos(parseInt(cc.value || "0", 10), cc.min || 0, cc.max || 100)
                                           : parseInt(cc.value || "0", 10)
                                    stepSize: cc.logarithmic ? 0.1 : 1
                                    height: root.fontBody * 2
                                    width: Math.max(80, parent.width - parent.spacing - root.fontBody * 4)
                                    anchors.verticalCenter: parent.verticalCenter
                                    // 细轨道，两端留 6px 让圆点不越界
                                    leftPadding: 6
                                    rightPadding: 6
                                    background: Rectangle {
                                        height: 3
                                        radius: 1
                                        color: "#3c3c3c"
                                        anchors.left: parent.left
                                        anchors.right: parent.right
                                        anchors.verticalCenter: parent.verticalCenter
                                        anchors.leftMargin: 6
                                        anchors.rightMargin: 6
                                        // 已取值部分用蓝填充（coloredFrame 时用节点色）
                                        Rectangle {
                                            width: sliderCtrl.visualPosition * parent.width
                                            height: parent.height
                                            radius: 1
                                            color: cc.disabled ? "#3a5a6e"
                                                 : (cc.coloredFrame ? root.iifFrameBgColor(cc) : "#007acc")
                                        }
                                    }
                                    // 手柄：小圆点（默认过大），按 value 的 visualPosition 显式定位
                                    handle: Rectangle {
                                        x: sliderCtrl.leftPadding + sliderCtrl.visualPosition
                                           * (sliderCtrl.availableWidth - width)
                                        y: sliderCtrl.topPadding + sliderCtrl.availableHeight / 2 - height / 2
                                        implicitWidth: root.fontBody * 1.6
                                        implicitHeight: root.fontBody * 1.6
                                        radius: width / 2
                                        color: cc.disabled ? "#6e6e6e" : "#d4d4d4"
                                        border.color: cc.disabled ? "#3a5a6e" : "#007acc"
                                        border.width: 1
                                    }
                                    // 写回仅松手时提交一次：onMoved 每步都写会触发模型刷新/组件重建，
                                    // 导致拖动中途被中断（拖一下就停）。拖动中本地实时值由右侧 Text 显示。
                                    onPressedChanged: {
                                        if (!pressed && root.lineModel) {
                                            var val = cc.logarithmic
                                                ? root.iifLogValue(sliderCtrl.value, cc.min || 0, cc.max || 100)
                                                : parseInt(sliderCtrl.value, 10)
                                            root.lineModel.setIifComponentValue(
                                                root.rowIndex, cc.idx || cc.compIdx, "" + val)
                                        }
                                    }
                                }
                                Text {
                                    text: root.iifSliderFormat(
                                        cc.logarithmic
                                            ? root.iifLogValue(sliderCtrl.value, cc.min || 0, cc.max || 100)
                                            : parseInt(sliderCtrl.value, 10),
                                        cc.slideFormat)
                                    color: cc.disabled ? "#6e6e6e" : root.iifFrameTextColor(cc)
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

                    // 链接节点：右端节点（对应 imgui RenderUI_Node；hint 标签已移到行首靠左）
                    LinkNodePoint {
                        property var cc: rowData && rowData.node ? rowData.node : null
                        visible: cc != null
                        flowNode: true
                        // 使分量节点中心与普通行节点对齐（普通行中心 = 行宽 - 1.5*fontSmall；
                        // 本节点右缘 = RowLayout 右缘(=行宽-4) - margin，即 center = 行宽-4-margin-0.75*fontSmall
                        // → 令两者相等得 margin = 0.75*fontSmall - 4，保持非负）
                        Layout.rightMargin: Math.max(0, root.fontSmall * 0.75 - 4)
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
                        // 双击该分量 → 只切该分量输入框/节点（对应 ImGui 双击分量切该分量状态，按 compIdx）
                        onDoubleClicked: {
                            if (root.lineModel) root.lineModel.toggleComponentInputMode(root.rowIndex, compIdx)
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
        // 同一组键只有一个 "+"：仅第一个值行（lineMult===0）显示，追加的新值行不再显示
        visible: root.isMultiple && !root.inputOnShow && root.lineMult === 0
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
            border.color: parent.activeFocus ? "#007acc" : "#3c3c3c"
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

    // IIF 分量节点坐标强制回写（模块移动/拖拽/缩放/平移结束后由 SectionNode.updateAllCenters
    // 级联调用）。分量节点的 onX/onY 是相对 IIF Flow 的（父移动不触发），若只靠它回写，
    // 模块移动到位后 LastCenter 停留在布局初期旧坐标 → 连线跑到画布外。
    function pushCompNodesRecursive(obj, force) {
        if (!obj || !obj.children) return
        for (var i = 0; i < obj.children.length; ++i) {
            var child = obj.children[i]
            if (!child) continue
            if (child.iifNode && child.pushCompCenter) {
                child.pushCompCenter(force)
            } else {
                pushCompNodesRecursive(child, force)
            }
        }
    }

    function updateIifCompCenters(force) {
        if (!force && (workspaceController.inputState === 1 || workspaceController.zoomPending)) return
        if (!iifRowsRepeater) return
        var rep = iifRowsRepeater
        for (var r = 0; r < rep.count; ++r) {
            // LinkNodePoint 在 RowLayout 内（iifRow 深层），递归查找
            pushCompNodesRecursive(rep.itemAt(r), force)
        }
    }

    // 行为A：向外部（SectionNode/WorkspaceView 命中）暴露 acceptor 方形接收点中心（workspaceView 坐标）。
    // 不能直接暴露 QML id（acceptorNode）——id 只在本组件作用域可见，外部访问 item.acceptorNode 为 undefined。
    function acceptorCenterWs() {
        if (visible && root.isAcceptor && acceptorNode && acceptorNode.visible) {
            return acceptorNode.mapToItem(workspaceView, acceptorNode.width / 2, acceptorNode.height / 2)
        }
        return null
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
        // accept 目标键（Collector/Armor）：左侧方形接收点坐标回写，供连线终点连到方形
        if (acceptorNode.visible) {
            var aPos = acceptorNode.mapToItem(workspaceView, acceptorNode.width / 2, acceptorNode.height / 2)
            root.lineModel.setAcceptorCenter(root.keyName, root.lineMult, aPos.x - dx, aPos.y - dy)
        }
    }
}
