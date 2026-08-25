// EditPanel.qml
// 编辑菜单面板，对应 IBR_Panel.cpp ControlPanel_Edit() (line 147-163)
// 完整移植 IBR_EditFrame 命名空间（IBR_Misc.cpp:587-940）：
// - 顶部：切换文本编辑按钮 + 粘贴刷新注册名复选框
// - 新增行：Key = Value + "＋"按钮
// - 键值列表：OnShow开关 + Key名 + 值编辑 + 描述编辑 + 删除行
// - 文本编辑模式：多行文本编辑 + 保存/不保存退出
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs
import "../components"

Item {
    id: editPanel

    // 主容器
    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 4
        visible: !editPanelController.isEmpty
        spacing: 4

        // ===== 文本编辑模式 =====
        // 对应 IBR_EditFrame::RenderUI_TextEdit（IBR_Misc.cpp:795-822）
        ColumnLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            visible: editPanelController.onTextEdit
            spacing: 4

            RowLayout {
                Layout.fillWidth: true
                spacing: 4

                Text {
                    text: (i18n.rev, i18n.tr("GUI_TextEditModeTitle"))
                    color: "#cccccc"
                    font.pixelSize: 13
                }
                Item { Layout.fillWidth: true }
                Button {
                    text: (i18n.rev, i18n.tr("GUI_ExitAndSave"))
                    onClicked: editPanelController.exitTextEdit(true)
                    background: Rectangle {
                        color: parent.hovered ? "#3c3c3c" : "#2d2d2d"
                        border.color: "#3c3c3c"; border.width: 1; radius: 3
                    }
                    contentItem: Text {
                        text: parent.text; color: "#cccccc"; font.pixelSize: 13
                        horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter
                    }
                }
                Button {
                    text: (i18n.rev, i18n.tr("GUI_ExitNoSave"))
                    onClicked: editPanelController.exitTextEdit(false)
                    background: Rectangle {
                        color: parent.hovered ? "#3c3c3c" : "#2d2d2d"
                        border.color: "#3c3c3c"; border.width: 1; radius: 3
                    }
                    contentItem: Text {
                        text: parent.text; color: "#cccccc"; font.pixelSize: 13
                        horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter
                    }
                }
            }

            ScrollView {
                Layout.fillWidth: true
                Layout.fillHeight: true
                TextArea {
                    text: editPanelController.textEditContent
                    color: "#e0e0e0"
                    placeholderTextColor: "#909090"
                    font.family: "Consolas, Microsoft YaHei"
                    font.pixelSize: 13
                    background: Rectangle { color: "#1e1e1e"; border.color: "#3c3c3c"; border.width: 1 }
                    onTextChanged: editPanelController.textEditContent = text
                    selectByMouse: true
                    wrapMode: TextArea.NoWrap
                }
            }
        }

        // ===== 正常编辑模式 =====
        // 对应 IBR_EditFrame::RenderUI（IBR_Misc.cpp:900-933）
        ColumnLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            visible: !editPanelController.onTextEdit
            spacing: 4

            // 模块名标题
            Text {
                Layout.fillWidth: true
                text: editPanelController.displayName
                color: "#cccccc"
                font.pixelSize: 14
                font.bold: true
                elide: Text.ElideRight
            }

            // 顶部工具栏：切换文本编辑 + 粘贴刷新注册名
            // 对应 RenderUI_SwitchToText + RenderUI_UseOwnName
            RowLayout {
                Layout.fillWidth: true
                spacing: 4

                Button {
                    text: (i18n.rev, i18n.tr("GUI_SwitchToTextEdit"))
                    onClicked: editPanelController.switchToText()
                    background: Rectangle {
                        color: parent.hovered ? "#3c3c3c" : "#2d2d2d"
                        border.color: "#3c3c3c"; border.width: 1; radius: 3
                    }
                    contentItem: Text {
                        text: parent.text; color: "#cccccc"; font.pixelSize: 13
                        horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter
                    }
                }
                Item { Layout.fillWidth: true }
                CheckBox {
                    text: (i18n.rev, i18n.tr("GUI_RefreshRegisterOnPaste"))
                    checked: editPanelController.needtoMangle
                    onToggled: editPanelController.toggleUseOwnName()
                    contentItem: Text {
                        text: parent.text
                        color: "#cccccc"
                        font.pixelSize: 13
                        verticalAlignment: Text.AlignVCenter
                        leftPadding: parent.indicator.width + parent.spacing
                    }
                }
            }

            // 新增行区域（对应 RenderUI_NewLine，IBR_Misc.cpp:687-744）
            RowLayout {
                Layout.fillWidth: true
                spacing: 4

                TextField {
                    id: newKeyField
                    Layout.fillWidth: true
                    color: "#e0e0e0"
                    font.pixelSize: 13
                    background: Rectangle { color: "#2d2d2d"; border.color: newKeyField.activeFocus ? "#007acc" : "#3c3c3c"; border.width: 1; radius: 2 }
                }
                Text { text: "="; color: "#cccccc"; font.pixelSize: 13 }
                TextField {
                    id: newValueField
                    Layout.fillWidth: true
                    color: "#e0e0e0"
                    font.pixelSize: 13
                    background: Rectangle { color: "#2d2d2d"; border.color: newValueField.activeFocus ? "#007acc" : "#3c3c3c"; border.width: 1; radius: 2 }
                    onAccepted: addNewLine()
                }
                Button {
                    text: "＋"
                    onClicked: addNewLine()
                    background: Rectangle {
                        color: parent.hovered ? "#3c3c3c" : "#2d2d2d"
                        border.color: "#3c3c3c"; border.width: 1; radius: 3
                    }
                    contentItem: Text {
                        text: parent.text; color: "#cccccc"; font.pixelSize: 13
                        horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter
                    }
                }
            }

            // 初始值提示（对应 RenderUI_NewLine 的 TextDisabled，IBR_Misc.cpp:737-748）
            // 当 Value 为空且 Key 非空时，显示该 Key 的默认初始值
            Text {
                Layout.fillWidth: true
                visible: newValueField.text.length === 0 && newKeyField.text.length > 0
                text: {
                    var v = editPanelController.getInitialValue(newKeyField.text)
                    return v.length > 0 ? (i18n.rev, i18n.trF("GUI_UseInitialValue", [v])) : ""
                }
                color: "#909090"
                font.pixelSize: 12
                elide: Text.ElideRight
            }

            // 分隔线
            Rectangle {
                Layout.fillWidth: true
                height: 1
                color: "#3c3c3c"
            }

            // 键值列表（对应 RenderUI_Lines，IBR_Misc.cpp:872-898）
            ScrollView {
                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true

                ListView {
                    id: linesListView
                    model: editPanelController.editLines
                    spacing: 2

                    delegate: Rectangle {
                        width: linesListView.width
                        height: lineColumn.implicitHeight + 8
                        color: "transparent"

                        property bool showDescEdit: false

                        // ===== IIF 分量布局辅助（对齐 imgui IBG_InputForm::RenderUI 的 newl/samel/sep 语义）=====
                        // 把某值(ventry)的分量按布局标记分成"行"：{isSep, comps[]}
                        function iifRowsForEntry(ventry) {
                            var comps = (ventry && ventry.comps) ? ventry.comps : []
                            var rows = []
                            var cur = null
                            var same = false
                            for (var i = 0; i < comps.length; i++) {
                                var c = comps[i]
                                var t = c.type
                                if (t === "samel") { same = true; continue }
                                if (t === "newl") {
                                    // NewLine 标记：在两个分量之间插入一个空行（对齐用户要求"写了 newline 就空一行"）。
                                    // 合并连续 newl（只产生一个空行）；行首/行尾的 newl 不产生空行。
                                    cur = null; same = false
                                    if (rows.length > 0 && !rows[rows.length - 1].isBlank)
                                        rows.push({ isSep: false, isBlank: true, comps: [] })
                                    continue
                                }
                                if (t === "sep") { rows.push({ isSep: true, comps: [] }); cur = null; same = false; continue }
                                // SetValue（setter）：只设置值，不渲染可见控件、不占空间（对齐 ImGui IIC_Setter_String::RenderUI 无控件）
                                if (t === "setter") { continue }
                                if (!same) cur = null
                                if (!cur) { cur = { isSep: false, comps: [] }; rows.push(cur) }
                                cur.comps.push(c)
                                same = false
                            }
                            // 移除末尾空行（行尾 NewLine 不产生空行）
                            if (rows.length > 0 && rows[rows.length - 1].isBlank) rows.pop()
                            return rows
                        }
                        // 所有值(mult)的分量拍平成行（外层 Repeater 用，逐行走 index*rowH 定位）
                        function iifFlattenRows() {
                            var vals = (modelData && modelData.iifValues) ? modelData.iifValues : []
                            var out = []
                            for (var v = 0; v < vals.length; v++) {
                                var ventry = vals[v]
                                var rows = iifRowsForEntry(ventry)
                                for (var g = 0; g < rows.length; g++)
                                    out.push({ mult: ventry.mult, isSep: rows[g].isSep, comps: rows[g].comps })
                            }
                            return out
                        }
                        // 选项数组
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
                            var parts = v.split(d)
                            for (var i = 0; i < parts.length; i++) set[parts[i]] = true
                            return set
                        }
                        // choice 勾选切换写回：重算 delim 连接串
                        function iifSetChoice(comp, key, on, mult) {
                            var d = (comp && comp.delim) || ","
                            var set = iifChoiceSelected(comp)
                            if (on) set[key] = true; else delete set[key]
                            var out = []
                            for (var k in set) if (set[k] === true) out.push(k)
                            editPanelController.setIifValue(modelData.keyName, mult, comp.idx, out.join(d))
                        }
                        // color 值 → QML 色串：#RRGGBB（支持 "255,0,0" Int 模式与 "#xxx" 十六进制；
                        // 对齐 ImGui IIC_ColorPanel：vMode 决定通道序（rgb/bgr/hsv），fMode 决定值格式）
                        function iifColorToHex(v, comp) {
                            if (!v) return "#000000"
                            var s = "" + v
                            s = s.trim()
                            if (s.charAt(0) === "#") return s
                            var vMode = (comp && comp.vMode) || "rgb"
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
                        function iifColorFormat(v, comp) {
                            var fMode = (comp && comp.fMode) || ""
                            if (fMode) return fMode
                            var s = (v || "").trim()
                            if (!s) return "int"
                            if (s.charAt(0) === "#") return "hashhex"
                            if (s.indexOf(",") >= 0) return (s.indexOf(".") >= 0) ? "float" : "int"
                            return "hex"
                        }
                        // 取色器选中后按分量 fMode/vMode 写回，避免破坏原有 Hex/Hash_Hex/Int/Float 形式与通道序
                        function iifColorWrite(comp, color, mult) {
                            var fmt = iifColorFormat(comp.value, comp)
                            var vMode = (comp && comp.vMode) || "rgb"
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
                            editPanelController.setIifValue(modelData.keyName, mult, comp.idx, val)
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
                        // ===== 文本真实度量（对齐字体大小：中文字符/字体变化时 length*0.6 估算会失真 → 重叠） =====
                        // 用 FontMetrics.advanceWidth()（纯方法调用）：不写属性、不建立绑定依赖，
                        // 避免 TextMetrics.text 赋值在绑定求值中修改自身依赖属性导致绑定循环。
                        FontMetrics {
                            id: iifFontM
                            font.pixelSize: 13
                        }
                        function iifTextWidth(str) {
                            if (!str) return 0
                            return iifFontM.advanceWidth(str)
                        }
                        // radio/choice 单个选项项宽（圆点/勾框 + spacing + 标签真实宽）
                        function iifOptItemWidth(opt) {
                            return 17 + iifTextWidth(opt.label || opt.key || "")
                        }
                        // radio/choice 选项按真实宽度折行分组（对齐 ImGui SameLine 流式：宽度不足自动换行；
                        // MaxInOneLine>0 时每行最多 MaxInOneLine 个；SameLine=false 每项单独一行）
                        // limitW<=0 不限宽；返回 rows[行][选项下标]
                        function iifOptRows(comp, limitW) {
                            var arr = iifOptArr(comp)
                            var rows = []
                            var cur = []
                            var curW = 0
                            var maxPer = (comp.maxInOneLine && comp.maxInOneLine > 0) ? comp.maxInOneLine : -1
                            for (var i = 0; i < arr.length; i++) {
                                var itemW = iifOptItemWidth(arr[i])
                                if (comp.sameLine === false) { rows.push([i]); continue }
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
                        // radio/choice 折行可用宽：侧边栏可用宽（iifArea 宽度）留边距
                        function iifOptLimitWidth() {
                            return Math.max(120, iifArea.width - 8)
                        }

                        ColumnLayout {
                            id: lineColumn
                            anchors.fill: parent
                            anchors.margins: 2
                            spacing: 2

                            // 主行：OnShow开关 + Key名 + 值编辑
                            RowLayout {
                                Layout.fillWidth: true
                                spacing: 4

                                // OnShow 开关（对应 RenderUI_OnShow 的 RadioButton）
                                // 注意：Qt 的 RadioButton 选中后无法点击取消，且 onToggled+onClicked
                                // 会双次触发 toggleOnShow 导致状态抵消，改用 CheckBox 支持双向切换
                                StyledCheckBox {
                                    checked: modelData.onShow || false
                                    onToggled: editPanelController.toggleOnShow(modelData.keyName)
                                    rightPadding: 0
                                    // 固定尺寸：避免 CheckBox 默认 padding 撑高行导致与 Key 名垂直错位
                                    width: 14
                                    height: 14
                                }

                                // Key 名（不省略号，按自然宽度完整显示，避免长键名被截断）
                                Text {
                                    Layout.fillWidth: false
                                    Layout.preferredWidth: implicitWidth
                                    text: modelData.keyName
                                    color: modelData.missing ? "#f48771" : "#e0e0e0"
                                    font.pixelSize: 13
                                    elide: Text.ElideNone
                                    HoverHandler {
                                        id: hoverHandler
                                        // 悬停提示（统一用全局 appToolTip，暗色方角立即显示）
                                        onHoveredChanged: {
                                            if (hovered && modelData.hint && modelData.hint.length > 0) {
                                                var g = parent.mapToGlobal(parent.width / 2, parent.height + 4)
                                                appToolTip.show(modelData.hint, g.x, g.y)
                                            } else {
                                                appToolTip.hide()
                                            }
                                        }
                                    }
                                    MouseArea {
                                        anchors.fill: parent
                                        acceptedButtons: Qt.RightButton
                                        onClicked: showDescEdit = !showDescEdit
                                    }
                                }

                                // 缺失行数据红色提示（对应 TextColored IllegalLineColor，IBR_Misc.cpp:903）
                                Text {
                                    Layout.fillWidth: true
                                    visible: modelData.missing || false
                                    text: (i18n.rev, i18n.tr("GUI_MissingLineData"))
                                    color: "#f48771"
                                    font.pixelSize: 13
                                }

                                // 值编辑控件（缺失行、多值键不显示；同名多值键由下方 valueRows 逐值渲染）
                                // 互斥规则：主值框 与 多值 Column(valueRows) 严格二选一，绝不并存
                                //  - 非 Multiple 键：主值框显示（valueRows 恒 0）
                                //  - Multiple 键：无主值框概念，每个同名键都由 valueRows 自己的框渲染
                                TextField {
                                    id: mainValueField
                                    Layout.fillWidth: true
                                    visible: !(modelData.missing || false)
                                             && ((modelData.keyType || 0) !== 2)   // IIF 键改由下方分量区逐分量渲染
                                             && !(modelData.isMultiple || false)
                                    text: modelData.value || ""
                                    color: "#e0e0e0"
                                    placeholderTextColor: "#909090"
                                    font.pixelSize: 13
                                    background: Rectangle {
                                        color: "#2d2d2d"
                                        border.color: parent.activeFocus ? "#007acc" : "#3c3c3c"
                                        border.width: 1
                                        radius: 2
                                    }
                                    onEditingFinished: editPanelController.setLineValue(modelData.keyName, text)
                                    onActiveFocusChanged: if (activeFocus) console.log("[SIDEBAR-DIAG] focus=mainValue key='" + modelData.keyName + "' isMultiple=" + (modelData.isMultiple||false) + " value=['" + text + "']")
                                    // 悬停提示（统一用全局 appToolTip，暗色方角立即显示）
                                    onHoveredChanged: {
                                        if (hovered && modelData.hint && modelData.hint.length > 0) {
                                            var g = mapToGlobal(width / 2, height + 4)
                                            appToolTip.show(modelData.hint, g.x, g.y)
                                        } else {
                                            appToolTip.hide()
                                        }
                                    }
                                }
}

                            // 同名多值键（isMultiple）：无主值框概念，每个同名键都有自己的值框
                            // 每值一个输入框，按值索引写回（对应 imgui ForEachWithIdx 逐行渲染）
                            // 只要 isMultiple 就逐值显示（含仅 1 个值的情形）；非 Multiple 键 values 为空 → model 恒 0
                            // 高度由 Column 天然堆叠自适应（不引 mainValueField——Multiple 键主值框已隐藏，不可作为参考）
                            Column {
                                id: valueRows
                                Layout.fillWidth: true
                                spacing: 2
                                // 捕获外层 ListView 键条目（Repeater delegate 内 modelData 会被整型 model 遮蔽）
                                property var entry: modelData
                                visible: (entry.isMultiple || false)
                                         && ((entry.keyType || 0) !== 2)   // IIF 键改由下方逐分量渲染
                                         && !(entry.missing || false)
                                Repeater {
                                    model: entry.values ? entry.values.length : 0
                                    delegate: TextField {
                                        // 每行一个值，高度走 TextField 默认 implicitHeight，与普通单值框一致
                                        width: linesListView.width - 4
                                        text: valueRows.entry.values[index] || ""
                                        color: "#e0e0e0"
                                        placeholderTextColor: "#909090"
                                        font.pixelSize: 13
                                        background: Rectangle {
                                            color: "#2d2d2d"
                                            border.color: parent.activeFocus ? "#007acc" : "#3c3c3c"
                                            border.width: 1
                                            radius: 2
                                        }
                                        onEditingFinished: editPanelController.setLineValueAt(valueRows.entry.keyName, index, text)
                                        onActiveFocusChanged: if (activeFocus) console.log("[SIDEBAR-DIAG] focus=multiValue key='" + valueRows.entry.keyName + "' index=" + index + " value=['" + text + "']")
                                        onHoveredChanged: {
                                            if (hovered && valueRows.entry.hint && valueRows.entry.hint.length > 0) {
                                                var g = mapToGlobal(width / 2, height + 4)
                                                appToolTip.show(valueRows.entry.hint, g.x, g.y)
                                            } else {
                                                appToolTip.hide()
                                            }
                                        }
                                    }
                                }
                            }

                            // ===== IIF 分量编辑区（keyType===2）=====
                            // 替代 mainValueField+valueRows：按值(mult)拍平成行，各分量渲染完整控件
                            // 对齐 imgui IIC_*::RenderUI；写回复用 EditPanelController::setIifValue/setIifBool
                            Column {
                                id: iifArea
                                Layout.fillWidth: true
                                visible: ((modelData.keyType || 0) === 2) && !(modelData.missing || false)
                                property var kentry: modelData
                                property string iifKey: kentry ? (kentry.keyName || "") : ""
                                property int rowH: 30
                                // 单行实际高度：radio/choice 内部选项按 MaxInOneLine 换行到多行时高度随之增高，
                                // 使多行选项不向下溢出压到下一行/下一个控件（对齐 ImGui 流式布局）
                                function sRowHeight(r) {
                                    if (!r || r.isSep) return rowH
                                    var h = rowH
                                    var comps = r.comps || []
                                    for (var i = 0; i < comps.length; i++) {
                                        var c = comps[i]
                                        var t = c.type
                                        if (t !== "radio" && t !== "choice") continue
                                        // 行数按真实宽度折行（与渲染处 iifOptRows 同一 limitW），
                                        // 避免多行选项向下溢出压到下一行/下一个控件
                                        var rows = iifOptRows(c, iifOptLimitWidth()).length
                                        var itemH = 20            // 选项高（覆盖 radio/choice 勾选框）
                                        var sp = 4                // Column spacing
                                        var rh = rows * itemH + Math.max(0, rows - 1) * sp
                                        // hint(Short) 独占第一行（对齐 imgui TextWrapped(Hint.Short)）
                                        if (c.label && c.label.length > 0) rh += rowH + sp
                                        if (rh > h) h = rh
                                    }
                                    return h
                                }
                                // 第 i 行顶部 y = 前序各行高累加（手动布局用，替代固定 index*rowH）
                                function sRowY(i) {
                                    var rows = iifFlattenRows()
                                    var y = 0
                                    for (var k = 0; k < i && k < rows.length; k++) y += sRowHeight(rows[k])
                                    return y
                                }
                                height: modelData ? (function () {
                                    var rows = iifFlattenRows()
                                    var total = 0
                                    for (var i = 0; i < rows.length; i++) total += sRowHeight(rows[i])
                                    return total
                                })() : 0
                                Layout.preferredHeight: height

                                Repeater {
                                    model: (modelData && modelData.iifValues) ? iifFlattenRows() : []
                                    delegate: Item {
                                        id: iifRowItem
                                        property var rdata: modelData
                                        width: iifArea.width
                                        y: iifArea.sRowY(index)
                                        height: iifArea.sRowHeight(rdata)

                                        // sep → 分隔线
                                        Rectangle {
                                            visible: rdata.isSep
                                            anchors.left: parent.left
                                            anchors.right: parent.right
                                            anchors.verticalCenter: parent.verticalCenter
                                            height: 1
                                            color: "#3a3a3a"
                                        }

                                        // 普通行：各分量
                                        Row {
                                            id: iifRow
                                            visible: !rdata.isSep
                                            anchors.fill: parent
                                            spacing: 4

                                            Repeater {
                                                model: rdata.comps || []
                                                delegate: Item {
                                                    id: iifCell
                                                    property var comp: modelData
                                                    width: cellComp.implicitWidth
                                                    height: iifArea.sRowHeight(iifRowItem.rdata)

                                                    Row {
                                                        id: cellComp
                                                        anchors.left: parent.left
                                                        anchors.top: parent.top
                                                        anchors.bottom: parent.bottom
                                                        spacing: 4

                                                        // 分量 Short 标签（radio/choice 的 hint 由组件内部独占第一行，对齐 imgui TextWrapped(Hint.Short)）
                                                        Text {
                                                            visible: (iifCell.comp.label || "").length > 0
                                                                    && iifCell.comp.type !== "radio" && iifCell.comp.type !== "choice"
                                                            text: iifCell.comp.label || ""
                                                            color: "#9cdcfe"
                                                            font.pixelSize: 13
                                                            verticalAlignment: Text.AlignVCenter
                                                        }

                                                        // 只读文本类：text/locale/setter
                                                        // 对齐 ImGui IIC_PureText/LocalizedText：Colored 用分量 Color，Wrapped 自动换行
                                                        Text {
                                                            visible: iifCell.comp.type === "text" || iifCell.comp.type === "locale" || iifCell.comp.type === "setter"
                                                            text: iifCell.comp.value || ""
                                                            color: iifCell.comp.colored ? (iifCell.comp.color || "#c586c0") : "#c586c0"
                                                            font.pixelSize: 13
                                                            wrapMode: iifCell.comp.wrapped ? Text.Wrap : Text.NoWrap
                                                            verticalAlignment: Text.AlignVCenter
                                                        }

                                                        // 链接分量：只读目标文本（侧边栏不做拖拽建链）
                                                        Text {
                                                            visible: iifCell.comp.type === "link"
                                                            text: iifCell.comp.value || ""
                                                            color: "#9cdcfe"
                                                            font.pixelSize: 13
                                                            verticalAlignment: Text.AlignVCenter
                                                        }

                                                        // bool 勾选
                                                        StyledCheckBox {
                                                            visible: iifCell.comp.type === "bool"
                                                            checked: iifCell.comp.boolVal === true
                                                            rightPadding: 0
                                                            onToggled: editPanelController.setIifBool(iifArea.iifKey, iifRowItem.rdata.mult, iifCell.comp.idx, checked)
                                                        }

                                                        // input/int 输入框
                                                        TextField {
                                                            visible: iifCell.comp.type === "input" || iifCell.comp.type === "int"
                                                            text: iifCell.comp.value || ""
                                                            color: "#e0e0e0"
                                                            font.pixelSize: 13
                                                            width: 140
                                                            height: 24
                                                            verticalAlignment: Text.AlignVCenter
                                                            background: Rectangle {
                                                                color: "#2d2d2d"
                                                                border.color: parent.activeFocus ? "#007acc" : "#3c3c3c"
                                                                border.width: 1; radius: 2
                                                            }
                                                            onEditingFinished: editPanelController.setIifValue(iifArea.iifKey, iifRowItem.rdata.mult, iifCell.comp.idx, text)
                                                            onActiveFocusChanged: if (activeFocus) console.log("[SIDEBAR-DIAG] focus=iifInput key='" + iifArea.iifKey + "' mult=" + iifRowItem.rdata.mult + " idx=" + iifCell.comp.idx + " val='" + text + "'")
                                                        }

                                                        // combo 下拉（方角 + 箭头 + 深色弹层，对齐画布）
                                                        ComboBox {
                                                            id: comboCtrl
                                                            visible: iifCell.comp.type === "combo"
                                                            model: iifOptArr(iifCell.comp)
                                                            textRole: "label"
                                                            currentIndex: iifComboIndex(iifCell.comp)
                                                            width: 150
                                                            height: 24
                                                            background: Rectangle {
                                                                color: "#2d2d2d"
                                                                border.color: comboCtrl.popup.visible ? "#007acc" : "#3c3c3c"
                                                                border.width: 1
                                                                radius: 0
                                                            }
                                                            contentItem: Text {
                                                                text: comboCtrl.currentText
                                                                color: "#e0e0e0"
                                                                font.pixelSize: 13
                                                                verticalAlignment: Text.AlignVCenter
                                                                leftPadding: 6
                                                                rightPadding: 16
                                                                elide: Text.ElideRight
                                                            }
                                                            indicator: Text {
                                                                anchors.verticalCenter: parent.verticalCenter
                                                                anchors.right: parent.right
                                                                anchors.rightMargin: 6
                                                                text: "▾"
                                                                color: "#9a9a9a"
                                                                font.pixelSize: 13
                                                            }
                                                            popup: Popup {
                                                                y: comboCtrl.height
                                                                width: comboCtrl.width
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
                                                                    // 数值 count 作 model，delegate 经 index 直接取 opts 元素
                                                                    model: iifOptArr(iifCell.comp).length
                                                                    width: comboCtrl.width
                                                                    implicitHeight: Math.min(contentHeight, (comboCtrl.height + 2) * 7)
                                                                    delegate: Rectangle {
                                                                        required property int index
                                                                        readonly property var opt: {
                                                                            var arr = iifOptArr(iifCell.comp)
                                                                            return (index >= 0 && index < arr.length) ? arr[index] : null
                                                                        }
                                                                        width: comboList.width
                                                                        height: comboCtrl.height
                                                                        color: (index === comboCtrl.currentIndex || hoverArea.containsMouse)
                                                                            ? "#3e3e3e" : "transparent"
                                                                        Text {
                                                                            text: (opt && opt.label) || (opt && opt.key) || ""
                                                                            color: "#d4d4d4"
                                                                            font.pixelSize: 13
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
                                                                                if (opt) editPanelController.setIifValue(
                                                                                    iifArea.iifKey, iifRowItem.rdata.mult, iifCell.comp.idx, opt.key)
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

                                                        // radio 单选组：对齐 ImGui IIC_EnumRadio——hint(Short) 独占第一行，
                                                        // 选项从下一行起每 MaxInOneLine 个折一行换行
                                                        Column {
                                                            visible: iifCell.comp.type === "radio"
                                                            spacing: 4
                                                            // hint 第一行（imgui TextWrapped(Hint.Short) 后自动换行）
                                                            Text {
                                                                visible: (iifCell.comp.label || "").length > 0
                                                                text: iifCell.comp.label || ""
                                                                color: "#9cdcfe"
                                                                font.pixelSize: 13
                                                            }
                                                            Repeater {
                                                                model: iifOptRows(iifCell.comp, iifOptLimitWidth())
                                                                delegate: Row {
                                                                    required property var modelData   // 该行选项下标数组
                                                                    readonly property var rowIdxArr: modelData
                                                                    spacing: 6
                                                                    Repeater {
                                                                        model: rowIdxArr
                                                                        delegate: Item {
                                                                            required property int index
                                                                            readonly property var opt: iifOptArr(iifCell.comp)[rowIdxArr[index]]
                                                                            width: iifOptItemWidth(opt)
                                                                            height: 20
                                                                            // 圆点与文本都垂直居中（Row 顶部对齐会导致错位）
                                                                            Rectangle {
                                                                                anchors.left: parent.left
                                                                                anchors.verticalCenter: parent.verticalCenter
                                                                                width: 14; height: 14; radius: 7
                                                                                color: (opt.key === iifCell.comp.value) ? "#2d6db5" : "#1e1e1e"
                                                                                border.color: (opt.key === iifCell.comp.value) ? "#007acc" : "#5a5a5a"
                                                                                border.width: 1
                                                                                MouseArea {
                                                                                    anchors.fill: parent
                                                                                    onClicked: editPanelController.setIifValue(iifArea.iifKey, iifRowItem.rdata.mult, iifCell.comp.idx, opt.key)
                                                                                }
                                                                            }
                                                                            Text {
                                                                                anchors.left: parent.left
                                                                                anchors.leftMargin: 17
                                                                                anchors.verticalCenter: parent.verticalCenter
                                                                                text: opt.label || opt.key || ""
                                                                                color: "#e0e0e0"
                                                                                font.pixelSize: 13
                                                                                verticalAlignment: Text.AlignVCenter
                                                                                MouseArea {
                                                                                    anchors.fill: parent
                                                                                    acceptedButtons: Qt.NoButton
                                                                                    hoverEnabled: true
                                                                                    onContainsMouseChanged: {
                                                                                        if (containsMouse && opt.desc && opt.desc.length > 0) {
                                                                                            var gr = mapToGlobal(width / 2, height + 4)
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

                                                        // choice 多选组：对齐 ImGui IIC_MultipleChoice——hint(Short) 独占第一行，
                                                        // 选项从下一行起每 MaxInOneLine 个折一行换行
                                                        Column {
                                                            visible: iifCell.comp.type === "choice"
                                                            spacing: 4
                                                            // hint 第一行（imgui TextWrapped(Hint.Short) 后自动换行）
                                                            Text {
                                                                visible: (iifCell.comp.label || "").length > 0
                                                                text: iifCell.comp.label || ""
                                                                color: "#9cdcfe"
                                                                font.pixelSize: 13
                                                            }
                                                            Repeater {
                                                                model: iifOptRows(iifCell.comp, iifOptLimitWidth())
                                                                delegate: Row {
                                                                    required property var modelData   // 该行选项下标数组
                                                                    readonly property var rowIdxArr: modelData
                                                                    spacing: 6
                                                                    Repeater {
                                                                        model: rowIdxArr
                                                                        delegate: Item {
                                                                            required property int index   // 行内序号
                                                                            readonly property var opt: iifOptArr(iifCell.comp)[rowIdxArr[index]]
                                                                            readonly property var sel: iifChoiceSelected(iifCell.comp)
                                                                            width: iifOptItemWidth(opt)
                                                                            height: 20
                                                                            // 勾选框与文本都垂直居中（Row 顶部对齐会导致 StyledCheckBox 与文本错位）
                                                                            StyledCheckBox {
                                                                                anchors.left: parent.left
                                                                                anchors.verticalCenter: parent.verticalCenter
                                                                                width: 14
                                                                                height: 14
                                                                                checked: { var s = sel; return s[opt.key] === true }
                                                                                rightPadding: 0
                                                                                onToggled: iifSetChoice(iifCell.comp, opt.key, checked, iifRowItem.rdata.mult)
                                                                            }
                                                                            Text {
                                                                                anchors.left: parent.left
                                                                                anchors.leftMargin: 17
                                                                                anchors.verticalCenter: parent.verticalCenter
                                                                                text: opt.label || opt.key || ""
                                                                                color: "#e0e0e0"
                                                                                font.pixelSize: 13
                                                                                verticalAlignment: Text.AlignVCenter
                                                                                MouseArea {
                                                                                    anchors.fill: parent
                                                                                    acceptedButtons: Qt.NoButton
                                                                                    hoverEnabled: true
                                                                                    onContainsMouseChanged: {
                                                                                        if (containsMouse && opt.desc && opt.desc.length > 0) {
                                                                                            var gch = mapToGlobal(width / 2, height + 4)
                                                                                            appToolTip.show(opt.desc, gch.x, gch.y)
                                                                                        } else appToolTip.hide()
                                                                                    }
                                                                                }
                                                                            }
                                                                        }
                                                                    }
                                                                }
                                                            }
                                                        }

                                                        // 取色器（color 分量点击色块弹出，对应 ImGui IIC_ColorPanel 的 ColorPicker3）
                                                        // 用 Dialog 通用 onAccepted + selectedColor：当前 Qt 版本 ColorDialog 无 onColorSelected 信号
                                                        ColorDialog {
                                                            id: colorDlg
                                                            title: "选择颜色"
                                                            onAccepted: { iifColorWrite(iifCell.comp, colorDlg.selectedColor, iifRowItem.rdata.mult) }
                                                        }

                                                        // color 色块（可点击取色）+ 值输入
                                                        Row {
                                                            visible: iifCell.comp.type === "color"
                                                            spacing: 4
                                                            Rectangle {
                                                                width: 22; height: 22; radius: 3
                                                                color: iifColorToHex(iifCell.comp.value, iifCell.comp)
                                                                border.color: "#5a5a5a"; border.width: 1
                                                                // 点击弹出取色器
                                                                MouseArea {
                                                                    anchors.fill: parent
                                                                    hoverEnabled: true
                                                                    cursorShape: Qt.PointingHandCursor
                                                                    onClicked: {
                                                                        colorDlg.selectedColor = iifColorToHex(iifCell.comp.value, iifCell.comp)
                                                                        colorDlg.open()
                                                                    }
                                                                }
                                                            }
                                                            TextField {
                                                                text: iifCell.comp.value || ""
                                                                width: 110
                                                                height: 24
                                                                font.pixelSize: 13
                                                                color: "#e0e0e0"
                                                                verticalAlignment: Text.AlignVCenter
                                                                background: Rectangle {
                                                                    color: "#2d2d2d"
                                                                    border.color: parent.activeFocus ? "#007acc" : "#3c3c3c"
                                                                    border.width: 1; radius: 2
                                                                }
                                                                onEditingFinished: editPanelController.setIifValue(iifArea.iifKey, iifRowItem.rdata.mult, iifCell.comp.idx, text)
                                                            }
                                                        }

                                                        // slider 滑条 + 值
                                                        Row {
                                                            visible: iifCell.comp.type === "slider"
                                                            spacing: 6
                                                            Slider {
                                                                id: sliderCtrl
                                                                from: iifCell.comp.min || 0
                                                                to: iifCell.comp.max || 100
                                                                value: parseInt(iifCell.comp.value || "0", 10)
                                                                stepSize: 1
                                                                width: 140
                                                                height: 24
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
                                                                    // 已取值部分用蓝填充
                                                                    Rectangle {
                                                                        width: sliderCtrl.visualPosition * parent.width
                                                                        height: parent.height
                                                                        radius: 1
                                                                        color: "#007acc"
                                                                    }
                                                                }
                                                                // 手柄：小圆点，按 value 的 visualPosition 显式定位
                                                                handle: Rectangle {
                                                                    x: sliderCtrl.leftPadding + sliderCtrl.visualPosition
                                                                       * (sliderCtrl.availableWidth - width)
                                                                    y: sliderCtrl.topPadding + sliderCtrl.availableHeight / 2 - height / 2
                                                                    implicitWidth: 13 * 1.6
                                                                    implicitHeight: 13 * 1.6
                                                                    radius: width / 2
                                                                    color: "#d4d4d4"
                                                                    border.color: "#007acc"
                                                                    border.width: 1
                                                                }
                                                                // 写回仅松手时提交一次，避免拖动中刷新中断；拖动中数值由右侧 Text 实时显示
                                                                onPressedChanged: {
                                                                    if (!pressed)
                                                                        editPanelController.setIifValue(iifArea.iifKey, iifRowItem.rdata.mult, iifCell.comp.idx, "" + parseInt(sliderCtrl.value, 10))
                                                                }
                                                            }
                                                            Text {
                                                                text: iifSliderFormat(parseInt(sliderCtrl.value, 10), iifCell.comp.slideFormat)
                                                                color: "#e0e0e0"
                                                                font.pixelSize: 13
                                                                verticalAlignment: Text.AlignVCenter
                                                            }
                                                        }
                                                    }

                                                    // 分量悬停 hint（Long||Short，支持换行）
                                                    MouseArea {
                                                        anchors.fill: parent
                                                        acceptedButtons: Qt.NoButton
                                                        hoverEnabled: true
                                                        onContainsMouseChanged: {
                                                            if (containsMouse && iifCell.comp.tooltip && iifCell.comp.tooltip.length > 0) {
                                                                var gt = parent.mapToGlobal(parent.width / 2, parent.height + 4)
                                                                appToolTip.show(iifCell.comp.tooltip, gt.x, gt.y)
                                                            } else appToolTip.hide()
                                                        }
                                                    }
                                                }
                                            }
                                        }
                                    }
                                }
                            }

                            // OnShow 描述编辑区域（右键 Key 名切换显示）
                            // 对应 RenderUI_OnShow 的 InputOnShow 分支（IBR_Misc.cpp:842-869）
                            RowLayout {
                                Layout.fillWidth: true
                                visible: showDescEdit
                                spacing: 4

                                TextField {
                                    Layout.fillWidth: true
                                    text: modelData.onShowDesc || ""
                                    placeholderText: (i18n.rev, i18n.tr("GUI_EditDesc"))
                                    color: "#e0e0e0"
                                    placeholderTextColor: "#909090"
                                    font.pixelSize: 12
                                    background: Rectangle {
                                        color: "#2d2d2d"
                                        border.color: parent.activeFocus ? "#007acc" : "#3c3c3c"
                                        border.width: 1
                                        radius: 2
                                    }
                                    onEditingFinished: editPanelController.setOnShowDesc(modelData.keyName, text)
                                    onActiveFocusChanged: if (activeFocus) console.log("[SIDEBAR-DIAG] focus=descEdit key='" + modelData.keyName + "' text=['" + text + "']")
                                }
                                Button {
                                    text: (i18n.rev, i18n.tr("GUI_RemoveLine"))
                                    onClicked: editPanelController.removeLine(modelData.keyName)
                                    background: Rectangle {
                                        color: parent.hovered ? "#3c3c3c" : "#2d2d2d"
                                        border.color: "#3c3c3c"; border.width: 1; radius: 3
                                    }
                                    contentItem: Text {
                                        text: parent.text; color: "#cccccc"; font.pixelSize: 12
                                        horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    function addNewLine() {
        if (newKeyField.text.length > 0) {
            editPanelController.addLine(newKeyField.text, newValueField.text)
            newKeyField.clear()
            newValueField.clear()
        }
    }
}
