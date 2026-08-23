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
                                if (t === "newl") { cur = null; same = false; continue }
                                if (t === "sep") { rows.push({ isSep: true, comps: [] }); cur = null; same = false; continue }
                                if (!same) cur = null
                                if (!cur) { cur = { isSep: false, comps: [] }; rows.push(cur) }
                                cur.comps.push(c)
                                same = false
                            }
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
                                height: modelData ? (iifFlattenRows().length * rowH) : 0
                                Layout.preferredHeight: height

                                Repeater {
                                    model: (modelData && modelData.iifValues) ? iifFlattenRows() : []
                                    delegate: Item {
                                        id: iifRowItem
                                        property var rdata: modelData
                                        width: iifArea.width
                                        y: index * iifArea.rowH
                                        height: iifArea.rowH

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
                                                    height: iifArea.rowH

                                                    Row {
                                                        id: cellComp
                                                        anchors.left: parent.left
                                                        anchors.top: parent.top
                                                        anchors.bottom: parent.bottom
                                                        spacing: 4

                                                        // 分量 Short 标签
                                                        Text {
                                                            visible: (iifCell.comp.label || "").length > 0
                                                            text: iifCell.comp.label || ""
                                                            color: "#9cdcfe"
                                                            font.pixelSize: 13
                                                            verticalAlignment: Text.AlignVCenter
                                                        }

                                                        // 只读文本类：text/locale/setter
                                                        Text {
                                                            visible: iifCell.comp.type === "text" || iifCell.comp.type === "locale" || iifCell.comp.type === "setter"
                                                            text: iifCell.comp.value || ""
                                                            color: "#c586c0"
                                                            font.pixelSize: 13
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

                                                        // combo 下拉
                                                        ComboBox {
                                                            visible: iifCell.comp.type === "combo"
                                                            model: iifOptArr(iifCell.comp)
                                                            textRole: "label"
                                                            currentIndex: iifComboIndex(iifCell.comp)
                                                            width: 150
                                                            height: 24
                                                            onActivated: {
                                                                var arr = iifOptArr(iifCell.comp)
                                                                var o = (index >= 0 && index < arr.length) ? arr[index] : null
                                                                if (o) editPanelController.setIifValue(iifArea.iifKey, iifRowItem.rdata.mult, iifCell.comp.idx, o.key)
                                                            }
                                                        }

                                                        // radio 单选组：可点选按钮
                                                        Row {
                                                            visible: iifCell.comp.type === "radio"
                                                            spacing: 6
                                                            Repeater {
                                                                model: iifOptArr(iifCell.comp)
                                                                delegate: Row {
                                                                    property var opt: modelData
                                                                    spacing: 3
                                                                    Rectangle {
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

                                                        // choice 多选组：每选项一个勾选（当前值按 delim 切分）
                                                        Row {
                                                            visible: iifCell.comp.type === "choice"
                                                            spacing: 6
                                                            Repeater {
                                                                model: iifOptArr(iifCell.comp)
                                                                delegate: Row {
                                                                    property var opt: modelData
                                                                    property var sel: iifChoiceSelected(iifCell.comp)
                                                                    spacing: 3
                                                                    StyledCheckBox {
                                                                        checked: { var s = sel; return s[opt.key] === true }
                                                                        rightPadding: 0
                                                                        onToggled: iifSetChoice(iifCell.comp, opt.key, checked, iifRowItem.rdata.mult)
                                                                    }
                                                                    Text {
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

                                                        // color 色块 + 值输入
                                                        Row {
                                                            visible: iifCell.comp.type === "color"
                                                            spacing: 4
                                                            Rectangle {
                                                                width: 22; height: 22; radius: 3
                                                                color: iifCell.comp.value || "#000000"
                                                                border.color: "#5a5a5a"; border.width: 1
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
                                                                width: 140
                                                                height: 24
                                                                onValueChanged: if (!sliderCtrl.pressed && iifCell.comp.value !== "" + sliderCtrl.value)
                                                                              editPanelController.setIifValue(iifArea.iifKey, iifRowItem.rdata.mult, iifCell.comp.idx, "" + parseInt(sliderCtrl.value, 10))
                                                            }
                                                            Text {
                                                                text: iifCell.comp.value || "0"
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
