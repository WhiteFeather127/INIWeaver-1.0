// ImportPreviewDialog.qml
// 阶段 11.1：导入 INI 预览弹窗，对应 IBR_ImportIni.cpp:68-456 RenderUI
// 由 dialogController.importPreviewRequested 信号触发
// 提供：摘要统计 / 全局全选 / 过滤搜索 / IniType 下拉 / 冲突检测 /
//       Matched/LinkMatched/Unmatched 分组列表 / per-section 复选框 /
//       per-section RegType 下拉（未匹配组）/ KV 展开视图
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Dialog {
    id: root
    modal: true
    anchors.centerIn: parent
    width: 900
    height: 680
    padding: 0
    closePolicy: Dialog.CloseOnEscape | Dialog.CloseOnPressOutside
    title: qsTr("导入 INI 预览")

    // 由 onImportPreviewRequested 信号触发后加载
    property variant previewData: ({})
    property string iniType: ""
    property string filterText: ""
    // 选中状态：用对象当 Set 用（key=section index 字符串）
    property var selectedSet: ({})
    // 未匹配 section 的 RegType 选择（key=section index 字符串, value=RegType 名）
    property var unmatchedRegTypes: ({})

    background: Rectangle {
        color: "#252526"
        border.color: "#3c3c3c"
        border.width: 1
        radius: 4
    }

    header: Rectangle {
        height: 36
        color: "#2d2d2d"
        radius: 4

        Text {
            anchors.left: parent.left
            anchors.leftMargin: 12
            anchors.verticalCenter: parent.verticalCenter
            text: root.title
            color: "#cccccc"
            font.pixelSize: 13
            font.bold: true
        }
    }

    // 打开时加载数据并初始化状态
    onOpened: {
        previewData = dialogController.importPreviewData()
        iniType = previewData.defaultIniType || ""
        filterText = ""
        selectedSet = ({})
        unmatchedRegTypes = ({})
        // 初始化：所有非 registry list 的 section 默认选中
        // 未匹配 section 默认选第一个 RegType（对应 IBR_ImportIni.cpp:361-362）
        var selSet = {}
        var regMap = {}
        var sections = previewData.sections || []
        for (var i = 0; i < sections.length; i++) {
            var sec = sections[i]
            if (!sec.isRegistryList) {
                selSet[sec.index] = true
            }
            if (sec.matchStatus === 2 && previewData.availableRegTypes.length > 0) {
                regMap[sec.index] = previewData.availableRegTypes[0]
            }
        }
        selectedSet = selSet
        unmatchedRegTypes = regMap
    }

    // ===== 辅助函数 =====

    // 不区分大小写的包含检查（对应 IBR_ImportIni.cpp:57 contains_ignore_case）
    function matchesFilter(name, filter) {
        if (!filter || filter.length === 0) return true
        return name.toLowerCase().indexOf(filter.toLowerCase()) >= 0
    }

    // 计算统计信息（对应 IBR_ImportIni.cpp:73-100）
    function computeStats() {
        var sections = previewData.sections || []
        var matched = 0, linkMatched = 0, unmatched = 0
        var matchedSearch = 0, linkMatchedSearch = 0, unmatchedSearch = 0
        var registry = 0
        for (var i = 0; i < sections.length; i++) {
            var sec = sections[i]
            if (sec.isRegistryList) { registry++; continue }
            if (sec.matchStatus === 0) matched++
            else if (sec.matchStatus === 1) linkMatched++
            else unmatched++
            if (matchesFilter(sec.sectionName, filterText)) {
                if (sec.matchStatus === 0) matchedSearch++
                else if (sec.matchStatus === 1) linkMatchedSearch++
                else unmatchedSearch++
            }
        }
        return {
            total: sections.length,
            nonRegistry: sections.length - registry,
            registry: registry,
            matched: matched,
            linkMatched: linkMatched,
            unmatched: unmatched,
            matchedSearch: matchedSearch,
            linkMatchedSearch: linkMatchedSearch,
            unmatchedSearch: unmatchedSearch
        }
    }

    // 获取过滤后的分组数据
    // 返回 { matchedGroups: [{typeName, sections: [...]}], linkMatchedGroups: [...], unmatchedSections: [...] }
    function computeGroups() {
        var sections = previewData.sections || []
        var matchedGroups = {}
        var linkMatchedGroups = {}
        var unmatchedSections = []

        for (var i = 0; i < sections.length; i++) {
            var sec = sections[i]
            if (sec.isRegistryList) continue
            if (!matchesFilter(sec.sectionName, filterText)) continue

            if (sec.matchStatus === 0) {
                // Matched
                var mType = sec.matchedRegType || ""
                if (!matchedGroups[mType]) matchedGroups[mType] = []
                matchedGroups[mType].push(sec)
            } else if (sec.matchStatus === 1) {
                // LinkMatched
                var lType = sec.matchedRegType || ""
                if (!linkMatchedGroups[lType]) linkMatchedGroups[lType] = []
                linkMatchedGroups[lType].push(sec)
            } else {
                // Unmatched
                unmatchedSections.push(sec)
            }
        }

        // 转为数组
        var mg = []
        for (var mk in matchedGroups) mg.push({ typeName: mk, sections: matchedGroups[mk] })
        var lg = []
        for (var lk in linkMatchedGroups) lg.push({ typeName: lk, sections: linkMatchedGroups[lk] })

        return { matchedGroups: mg, linkMatchedGroups: lg, unmatchedSections: unmatchedSections }
    }

    // 全局全选状态（对应 IBR_ImportIni.cpp:107）
    function isAllSelected() {
        var sections = previewData.sections || []
        for (var i = 0; i < sections.length; i++) {
            var sec = sections[i]
            if (sec.isRegistryList) continue
            if (!selectedSet[sec.index]) return false
        }
        return true
    }

    // 切换全局全选（对应 IBR_ImportIni.cpp:108-113）
    function toggleSelectAll() {
        var newSel = !isAllSelected()
        var sections = previewData.sections || []
        var selSet = {}
        for (var key in selectedSet) selSet[key] = selectedSet[key]
        for (var i = 0; i < sections.length; i++) {
            var sec = sections[i]
            if (sec.isRegistryList) continue
            if (newSel) selSet[sec.index] = true
            else delete selSet[sec.index]
        }
        selectedSet = selSet
    }

    // 切换单个 section 选中
    function toggleSection(index) {
        var selSet = {}
        for (var key in selectedSet) selSet[key] = selectedSet[key]
        if (selSet[index]) delete selSet[index]
        else selSet[index] = true
        selectedSet = selSet
    }

    // 检查某组是否全选
    function isGroupAllSelected(sections) {
        for (var i = 0; i < sections.length; i++) {
            if (!selectedSet[sections[i].index]) return false
        }
        return true
    }

    // 切换组全选（对应 IBR_ImportIni.cpp:164-168, 193-197）
    function toggleGroupSelect(sections) {
        var newSel = !isGroupAllSelected(sections)
        var selSet = {}
        for (var key in selectedSet) selSet[key] = selectedSet[key]
        for (var i = 0; i < sections.length; i++) {
            if (newSel) selSet[sections[i].index] = true
            else delete selSet[sections[i].index]
        }
        selectedSet = selSet
    }

    // 设置未匹配 section 的 RegType
    function setUnmatchedRegType(index, regType) {
        var regMap = {}
        for (var key in unmatchedRegTypes) regMap[key] = unmatchedRegTypes[key]
        regMap[index] = regType
        unmatchedRegTypes = regMap
    }

    // 确认导入（对应 IBR_ImportIni.cpp:408-440）
    function confirmImport() {
        // 将 selectedSet 转为 QVariantList
        var selectedList = []
        for (var key in selectedSet) {
            selectedList.push(parseInt(key))
        }
        dialogController.confirmImportPreview(iniType, selectedList, unmatchedRegTypes)
        root.close()
    }

    // 取消导入（对应 IBR_ImportIni.cpp:444-456）
    function cancelImport() {
        dialogController.cancelImportPreview()
        root.close()
    }

    contentItem: ColumnLayout {
        anchors.fill: parent
        anchors.margins: 16
        spacing: 8

        // ===== 摘要统计（对应 IBR_ImportIni.cpp:103-104） =====
        Text {
            Layout.fillWidth: true
            text: {
                if (!previewData.valid) return ""
                var s = computeStats()
                // locc("GUI_ImportIni_Summary") 格式：section 数 / registry 数 / Matched / LinkMatched / Unmatched
                return qsTr("Section 数: %1  |  注册表列表: %2  |  已匹配: %3  |  链接匹配: %4  |  未匹配: %5")
                    .arg(s.nonRegistry).arg(s.registry).arg(s.matched).arg(s.linkMatched).arg(s.unmatched)
            }
            color: "#d4d4d4"
            font.pixelSize: 13
            wrapMode: Text.Wrap
        }

        // ===== 全局全选 + 过滤搜索 + IniType 下拉（对应 IBR_ImportIni.cpp:107-130） =====
        RowLayout {
            Layout.fillWidth: true
            spacing: 12

            // 全局全选/取消（对应 IBR_ImportIni.cpp:107-113）
            CheckBox {
                checked: isAllSelected()
                onClicked: toggleSelectAll()
                text: qsTr("全选")

                contentItem: Text {
                    text: parent.text
                    color: "#d4d4d4"
                    font.pixelSize: 12
                    verticalAlignment: Text.AlignVCenter
                    leftPadding: parent.indicator.width + parent.spacing
                }
            }

            // 过滤搜索框（对应 IBR_ImportIni.cpp:117-120 InputTextStdString + RegenMatch）
            TextField {
                id: filterField
                Layout.fillWidth: true
                Layout.preferredHeight: 30
                text: filterText
                color: "#d4d4d4"
                font.pixelSize: 12
                placeholderText: qsTr("搜索 Section...")
                onTextChanged: filterText = text

                leftPadding: 28
                Text {
                    text: "\uD83D\uDD0D"  // 🔍
                    anchors.left: parent.left
                    anchors.leftMargin: 8
                    anchors.verticalCenter: parent.verticalCenter
                    font.pixelSize: 12
                    color: "#858585"
                }

                background: Rectangle {
                    color: "#1e1e1e"
                    border.color: filterField.activeFocus ? "#007acc" : "#3c3c3c"
                    border.width: 1
                    radius: 3
                }
            }

            // IniType 下拉（对应 IBR_ImportIni.cpp:122-130 IBR_Combo）
            ComboBox {
                id: iniTypeCombo
                Layout.preferredWidth: 160
                Layout.preferredHeight: 30
                model: previewData.iniTypeOptions || []
                currentIndex: {
                    var idx = model.indexOf(iniType)
                    return idx >= 0 ? idx : 0
                }
                onActivated: iniType = currentText

                background: Rectangle {
                    color: "#1e1e1e"
                    border.color: iniTypeCombo.activeFocus ? "#007acc" : "#3c3c3c"
                    border.width: 1
                    radius: 3
                }
                contentItem: Text {
                    text: iniTypeCombo.displayText
                    color: "#d4d4d4"
                    font.pixelSize: 12
                    verticalAlignment: Text.AlignVCenter
                    leftPadding: 8
                }
            }
        }

        // ===== 冲突检测（对应 IBR_ImportIni.cpp:136-157） =====
        Rectangle {
            Layout.fillWidth: true
            visible: previewData.conflictSections && previewData.conflictSections.length > 0
            color: "#3d2a00"
            border.color: "#cc7700"
            border.width: 1
            radius: 3
            implicitHeight: conflictColumn.implicitHeight + 16

            ColumnLayout {
                id: conflictColumn
                anchors.fill: parent
                anchors.margins: 8
                spacing: 4

                Text {
                    Layout.fillWidth: true
                    text: qsTr("以下 Section 与当前项目同名（将覆盖已有数据）：")
                    color: "#e6a030"
                    font.pixelSize: 12
                    font.bold: true
                    wrapMode: Text.Wrap
                }

                Text {
                    Layout.fillWidth: true
                    text: (previewData.conflictSections || []).join(", ")
                    color: "#e6a030"
                    font.pixelSize: 12
                    wrapMode: Text.Wrap
                }
            }
        }

        // ===== 滚动区域：分组列表（对应 IBR_ImportIni.cpp:159-398） =====
        ScrollView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            ScrollBar.vertical.policy: ScrollBar.AsNeeded

            ColumnLayout {
                width: parent ? parent.width : 0
                spacing: 4

                // --- Matched 组（绿色，对应 IBR_ImportIni.cpp:160-237） ---
                GroupBox {
                    Layout.fillWidth: true
                    visible: {
                        var g = computeGroups()
                        return g.matchedGroups.length > 0
                    }

                    label: RowLayout {
                        spacing: 8

                        // 组级全选框（对应 IBR_ImportIni.cpp:162-169）
                        CheckBox {
                            checked: {
                                var g = computeGroups()
                                var allSecs = []
                                for (var i = 0; i < g.matchedGroups.length; i++)
                                    allSecs = allSecs.concat(g.matchedGroups[i].sections)
                                return isGroupAllSelected(allSecs)
                            }
                            onClicked: {
                                var g = computeGroups()
                                var allSecs = []
                                for (var i = 0; i < g.matchedGroups.length; i++)
                                    allSecs = allSecs.concat(g.matchedGroups[i].sections)
                                toggleGroupSelect(allSecs)
                            }
                        }

                        Text {
                            text: {
                                var s = computeStats()
                                if (filterText.length > 0)
                                    return qsTr("已匹配 (%1/%2)").arg(s.matchedSearch).arg(s.matched)
                                return qsTr("已匹配 (%1)").arg(s.matched)
                            }
                            color: "#33cc33"
                            font.pixelSize: 13
                            font.bold: true
                        }
                    }

                    background: Rectangle {
                        color: "transparent"
                        border.color: "#2a4a2a"
                        border.width: 1
                        radius: 3
                    }

                    ColumnLayout {
                        anchors.fill: parent
                        spacing: 4

                        // 按类型分组
                        Repeater {
                            model: computeGroups().matchedGroups

                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: 2

                                // 类型行：全选框 + 类型名 + 计数
                                RowLayout {
                                    spacing: 8

                                    CheckBox {
                                        checked: isGroupAllSelected(modelData.sections)
                                        onClicked: toggleGroupSelect(modelData.sections)
                                    }

                                    Text {
                                        text: "%1  (%2)".arg(modelData.typeName || qsTr("(无类型)")).arg(modelData.sections.length)
                                        color: "#88cc88"
                                        font.pixelSize: 12
                                        font.bold: true
                                    }
                                }

                                // 该类型下的每个 section
                                Repeater {
                                    model: modelData.sections

                                    ColumnLayout {
                                        Layout.fillWidth: true
                                        Layout.leftMargin: 24
                                        spacing: 2

                                        // Section 行
                                        RowLayout {
                                            spacing: 8

                                            CheckBox {
                                                checked: selectedSet[modelData.index] === true
                                                onClicked: toggleSection(modelData.index)
                                            }

                                            Text {
                                                text: "%1  (%2 %3)".arg(modelData.sectionName)
                                                    .arg(modelData.keyCount).arg(qsTr("个键"))
                                                color: "#d4d4d4"
                                                font.pixelSize: 12
                                            }

                                            // KV 展开按钮
                                            Button {
                                                text: modelData.kvExpanded ? "▼" : "▶"
                                                implicitWidth: 24
                                                implicitHeight: 24
                                                flat: true
                                                onClicked: {
                                                    modelData.kvExpanded = !modelData.kvExpanded
                                                }
                                                background: Rectangle { color: "transparent" }
                                                contentItem: Text {
                                                    text: parent.text
                                                    color: "#858585"
                                                    font.pixelSize: 10
                                                    horizontalAlignment: Text.AlignHCenter
                                                    verticalAlignment: Text.AlignVCenter
                                                }
                                            }
                                        }

                                        // KV 展开视图（对应 IBR_ImportIni.cpp:214-225）
                                        ColumnLayout {
                                            Layout.fillWidth: true
                                            Layout.leftMargin: 32
                                            visible: modelData.kvExpanded === true
                                            spacing: 1

                                            Repeater {
                                                model: modelData.keyValues

                                                Text {
                                                    text: "  %1 = %2".arg(modelData.key).arg(modelData.value)
                                                    color: "#9ab8d4"
                                                    font.pixelSize: 11
                                                    wrapMode: Text.Wrap
                                                    Layout.fillWidth: true
                                                }
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }
                }

                // --- LinkMatched 组（蓝色，对应 IBR_ImportIni.cpp:240-318） ---
                GroupBox {
                    Layout.fillWidth: true
                    visible: {
                        var g = computeGroups()
                        return g.linkMatchedGroups.length > 0
                    }

                    label: RowLayout {
                        spacing: 8

                        CheckBox {
                            checked: {
                                var g = computeGroups()
                                var allSecs = []
                                for (var i = 0; i < g.linkMatchedGroups.length; i++)
                                    allSecs = allSecs.concat(g.linkMatchedGroups[i].sections)
                                return isGroupAllSelected(allSecs)
                            }
                            onClicked: {
                                var g = computeGroups()
                                var allSecs = []
                                for (var i = 0; i < g.linkMatchedGroups.length; i++)
                                    allSecs = allSecs.concat(g.linkMatchedGroups[i].sections)
                                toggleGroupSelect(allSecs)
                            }
                        }

                        Text {
                            text: {
                                var s = computeStats()
                                if (filterText.length > 0)
                                    return qsTr("链接匹配 (%1/%2)").arg(s.linkMatchedSearch).arg(s.linkMatched)
                                return qsTr("链接匹配 (%1)").arg(s.linkMatched)
                            }
                            color: "#0099e6"
                            font.pixelSize: 13
                            font.bold: true
                        }
                    }

                    background: Rectangle {
                        color: "transparent"
                        border.color: "#1a3a4a"
                        border.width: 1
                        radius: 3
                    }

                    ColumnLayout {
                        anchors.fill: parent
                        spacing: 4

                        Repeater {
                            model: computeGroups().linkMatchedGroups

                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: 2

                                RowLayout {
                                    spacing: 8

                                    CheckBox {
                                        checked: isGroupAllSelected(modelData.sections)
                                        onClicked: toggleGroupSelect(modelData.sections)
                                    }

                                    Text {
                                        text: "%1  (%2)".arg(modelData.typeName || qsTr("(无类型)")).arg(modelData.sections.length)
                                        color: "#66aacc"
                                        font.pixelSize: 12
                                        font.bold: true
                                    }
                                }

                                Repeater {
                                    model: modelData.sections

                                    ColumnLayout {
                                        Layout.fillWidth: true
                                        Layout.leftMargin: 24
                                        spacing: 2

                                        RowLayout {
                                            spacing: 8

                                            CheckBox {
                                                checked: selectedSet[modelData.index] === true
                                                onClicked: toggleSection(modelData.index)
                                            }

                                            Text {
                                                text: "%1  (%2 %3)".arg(modelData.sectionName)
                                                    .arg(modelData.keyCount).arg(qsTr("个键"))
                                                color: "#d4d4d4"
                                                font.pixelSize: 12
                                            }

                                            // 链接来源（对应 IBR_ImportIni.cpp:292）
                                            Text {
                                                text: "<- %1".arg(modelData.linkMatchSource)
                                                color: "#888888"
                                                font.pixelSize: 11
                                            }

                                            Button {
                                                text: modelData.kvExpanded ? "▼" : "▶"
                                                implicitWidth: 24
                                                implicitHeight: 24
                                                flat: true
                                                onClicked: modelData.kvExpanded = !modelData.kvExpanded
                                                background: Rectangle { color: "transparent" }
                                                contentItem: Text {
                                                    text: parent.text
                                                    color: "#858585"
                                                    font.pixelSize: 10
                                                    horizontalAlignment: Text.AlignHCenter
                                                    verticalAlignment: Text.AlignVCenter
                                                }
                                            }
                                        }

                                        ColumnLayout {
                                            Layout.fillWidth: true
                                            Layout.leftMargin: 32
                                            visible: modelData.kvExpanded === true
                                            spacing: 1

                                            Repeater {
                                                model: modelData.keyValues

                                                Text {
                                                    text: "  %1 = %2".arg(modelData.key).arg(modelData.value)
                                                    color: "#9ab8d4"
                                                    font.pixelSize: 11
                                                    wrapMode: Text.Wrap
                                                    Layout.fillWidth: true
                                                }
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }
                }

                // --- Unmatched 组（红色，对应 IBR_ImportIni.cpp:321-398） ---
                GroupBox {
                    Layout.fillWidth: true
                    visible: computeGroups().unmatchedSections.length > 0

                    label: RowLayout {
                        spacing: 8

                        CheckBox {
                            checked: {
                                var g = computeGroups()
                                return isGroupAllSelected(g.unmatchedSections)
                            }
                            onClicked: {
                                var g = computeGroups()
                                toggleGroupSelect(g.unmatchedSections)
                            }
                        }

                        Text {
                            text: {
                                var s = computeStats()
                                if (filterText.length > 0)
                                    return qsTr("未匹配 (%1/%2)").arg(s.unmatchedSearch).arg(s.unmatched)
                                return qsTr("未匹配 (%1)").arg(s.unmatched)
                            }
                            color: "#cc3300"
                            font.pixelSize: 13
                            font.bold: true
                        }
                    }

                    background: Rectangle {
                        color: "transparent"
                        border.color: "#4a2a1a"
                        border.width: 1
                        radius: 3
                    }

                    ColumnLayout {
                        anchors.fill: parent
                        spacing: 4

                        Repeater {
                            model: computeGroups().unmatchedSections

                            ColumnLayout {
                                Layout.fillWidth: true
                                Layout.leftMargin: 24
                                spacing: 2

                                RowLayout {
                                    spacing: 8

                                    CheckBox {
                                        checked: selectedSet[modelData.index] === true
                                        onClicked: toggleSection(modelData.index)
                                    }

                                    Text {
                                        text: "%1  (%2 %3)".arg(modelData.sectionName)
                                            .arg(modelData.keyCount).arg(qsTr("个键"))
                                        color: "#d4d4d4"
                                        font.pixelSize: 12
                                    }

                                    // RegType 下拉（对应 IBR_ImportIni.cpp:357-375）
                                    ComboBox {
                                        id: regTypeCombo
                                        Layout.preferredWidth: 140
                                        Layout.preferredHeight: 26
                                        model: previewData.availableRegTypes || []
                                        currentIndex: {
                                            var cur = unmatchedRegTypes[modelData.index]
                                            if (!cur) return 0
                                            var idx = model.indexOf(cur)
                                            return idx >= 0 ? idx : 0
                                        }
                                        onActivated: {
                                            setUnmatchedRegType(modelData.index, currentText)
                                        }

                                        background: Rectangle {
                                            color: "#1e1e1e"
                                            border.color: regTypeCombo.activeFocus ? "#007acc" : "#3c3c3c"
                                            border.width: 1
                                            radius: 3
                                        }
                                        contentItem: Text {
                                            text: regTypeCombo.displayText
                                            color: "#d4d4d4"
                                            font.pixelSize: 11
                                            verticalAlignment: Text.AlignVCenter
                                            leftPadding: 6
                                        }
                                    }

                                    Button {
                                        text: modelData.kvExpanded ? "▼" : "▶"
                                        implicitWidth: 24
                                        implicitHeight: 24
                                        flat: true
                                        onClicked: modelData.kvExpanded = !modelData.kvExpanded
                                        background: Rectangle { color: "transparent" }
                                        contentItem: Text {
                                            text: parent.text
                                            color: "#858585"
                                            font.pixelSize: 10
                                            horizontalAlignment: Text.AlignHCenter
                                            verticalAlignment: Text.AlignVCenter
                                        }
                                    }
                                }

                                ColumnLayout {
                                    Layout.fillWidth: true
                                    Layout.leftMargin: 32
                                    visible: modelData.kvExpanded === true
                                    spacing: 1

                                    Repeater {
                                        model: modelData.keyValues

                                        Text {
                                            text: "  %1 = %2".arg(modelData.key).arg(modelData.value)
                                            color: "#9ab8d4"
                                            font.pixelSize: 11
                                            wrapMode: Text.Wrap
                                            Layout.fillWidth: true
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }

        // ===== 底部按钮（对应 IBR_ImportIni.cpp:404-456） =====
        Row {
            Layout.alignment: Qt.AlignRight
            spacing: 12

            Button {
                width: 96; height: 32
                text: qsTr("确认导入")
                enabled: previewData.valid === true
                onClicked: confirmImport()

                background: Rectangle {
                    color: parent.enabled ? (parent.hovered ? "#007acc" : "#3c3c3c")
                                           : "#252525"
                    border.color: parent.enabled ? "#007acc" : "transparent"
                    border.width: 1
                    radius: 3
                }
                contentItem: Text {
                    text: parent.text
                    color: parent.enabled ? "#ffffff" : "#5a5a5a"
                    font.pixelSize: 13
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
            }

            Button {
                width: 96; height: 32
                text: qsTr("取消")
                onClicked: cancelImport()

                background: Rectangle {
                    color: parent.hovered ? "#3c3c3c" : "#2d2d2d"
                    border.color: "#3c3c3c"
                    border.width: 1
                    radius: 3
                }
                contentItem: Text {
                    text: parent.text
                    color: "#cccccc"
                    font.pixelSize: 13
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
            }
        }
    }

    // ESC 键关闭时视为取消
    onRejected: cancelImport()
}
