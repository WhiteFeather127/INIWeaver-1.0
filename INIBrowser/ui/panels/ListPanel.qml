// ListPanel.qml
// 列表菜单面板，对应 IBR_Panel.cpp ControlPanel_ListView() (line 135-143)
// 布局：标题栏 + 操作按钮（含 Duplicate/Freeze/Hide 智能切换）+ 排序/筛选区（含 Full/Case/Regex/ShowRegName）+ Section 列表
// 功能：排序/筛选/全选/反选/复制/智能冻结/智能隐藏/双击跳转/状态颜色区分/整行染色/显式 Checkbox（对齐 IBR_ListView.cpp）
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../components"

Item {
    id: root

    // 项目未打开时显示提示（对应 ControlPanel_WaitOpen）
    Text {
        anchors.centerIn: parent
        anchors.verticalCenterOffset: -16
        visible: !projectController.isOpen
        color: "#5a5a5a"
        font.pixelSize: 13
        text: qsTr("请先打开项目")
    }

    // 项目打开后显示 Section 列表
    ColumnLayout {
        anchors.fill: parent
        spacing: 0
        visible: projectController.isOpen

        // ===== 顶部标题栏 + 操作按钮 =====
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 32
            color: "#2d2d2d"

            Text {
                anchors.left: parent.left
                anchors.leftMargin: 8
                anchors.verticalCenter: parent.verticalCenter
                // 对应 IBR_ListView.cpp:155-157 选中数显示
                text: qsTr("Section 列表 (") + sectionListModel.selectedCount()
                      + "/" + sectionListModel.rowCount() + ")"
                color: "#cccccc"
                font.pixelSize: 13
                font.bold: true
            }

            // 操作按钮组（对应 IBR_ListView.cpp:127-152）
            // 阶段 9.2 新增：Duplicate/Freeze/Hide 三个智能切换按钮
            Row {
                anchors.right: parent.right
                anchors.rightMargin: 4
                anchors.verticalCenter: parent.verticalCenter
                spacing: 2

                // 全选/取消全选（智能切换文字）
                // 阶段 9.3：对齐 ImGui IBR_ListView.cpp:127-132 禁用逻辑
                // FullSelected && SelectN==0（空列表）时显示"全选"但禁用
                StyledButton {
                    width: 48; height: 22
                    enabled: sectionListModel.rowCount() > 0
                    text: (sectionListModel.rowCount() > 0
                           && sectionListModel.selectedCount() === sectionListModel.rowCount())
                          ? qsTr("全不选") : qsTr("全选")
                    font.pixelSize: 11
                    onClicked: {
                        if (sectionListModel.selectedCount() === sectionListModel.rowCount())
                            sectionListModel.clearSelection()
                        else
                            sectionListModel.selectAll()
                    }
                }

                // 反选
                StyledButton {
                    width: 48; height: 22
                    text: qsTr("反选")
                    font.pixelSize: 11
                    onClicked: sectionListModel.selectInvert()
                }

                // 刷新
                StyledButton {
                    width: 48; height: 22
                    text: qsTr("刷新")
                    font.pixelSize: 11
                    onClicked: sectionListModel.refresh()
                }

                // 阶段 9.2 新增：Duplicate 按钮（对应 IBR_ListView.cpp:137, 144）
                StyledButton {
                    width: 56; height: 22
                    enabled: sectionListModel.selectedCount() > 0
                    text: qsTr("复制副本")
                    font.pixelSize: 11
                    onClicked: sectionListModel.duplicate()
                }

                // 阶段 9.2 新增：Freeze/Unfreeze 智能切换按钮（对应 IBR_ListView.cpp:109, 146-147）
                // 当所有选中项都已冻结时显示"解冻"，否则显示"冻结"
                StyledButton {
                    width: 56; height: 22
                    enabled: sectionListModel.selectedCount() > 0
                    // UseUnfreeze = SelectN && (SelAndFrozenN == SelectN)
                    text: (sectionListModel.selectedCount() > 0
                           && sectionListModel.selAndFrozenN === sectionListModel.selectedCount())
                          ? qsTr("解冻") : qsTr("冻结")
                    font.pixelSize: 11
                    onClicked: {
                        if (sectionListModel.selAndFrozenN === sectionListModel.selectedCount()
                            && sectionListModel.selectedCount() > 0)
                            sectionListModel.freezeAll(false)  // 全冻结时解冻
                        else
                            sectionListModel.freezeAll(true)   // 否则冻结所有
                    }
                }

                // 阶段 9.2 新增：Hide/Show 智能切换按钮（对应 IBR_ListView.cpp:110, 149-150）
                // 当所有选中项都已隐藏时显示"显示"，否则显示"隐藏"
                StyledButton {
                    width: 56; height: 22
                    enabled: sectionListModel.selectedCount() > 0
                    // UseShow = SelectN && (SelAndHiddenN == SelectN)
                    text: (sectionListModel.selectedCount() > 0
                           && sectionListModel.selAndHiddenN === sectionListModel.selectedCount())
                          ? qsTr("显示") : qsTr("隐藏")
                    font.pixelSize: 11
                    onClicked: {
                        if (sectionListModel.selAndHiddenN === sectionListModel.selectedCount()
                            && sectionListModel.selectedCount() > 0)
                            sectionListModel.hideAll(false)  // 全隐藏时显示
                        else
                            sectionListModel.hideAll(true)   // 否则隐藏所有
                    }
                }

                // 删除
                StyledButton {
                    width: 48; height: 22
                    enabled: sectionListModel.selectedCount() > 0
                    text: qsTr("删除")
                    font.pixelSize: 11
                    onClicked: sectionListModel.deleteSelected()
                }
            }

            Rectangle {
                anchors.bottom: parent.bottom
                anchors.left: parent.left; anchors.right: parent.right
                height: 1; color: "#1e1e1e"
            }
        }

        // ===== 排序/筛选区 =====
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 116
            color: "#252525"

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 4
                spacing: 4

                // 第一行：排序 ComboBox + 升降序切换
                RowLayout {
                    Layout.fillWidth: true
                    spacing: 4

                    Text {
                        text: qsTr("排序:")
                        color: "#858585"
                        font.pixelSize: 11
                    }

                    // 排序方式下拉框（对应 IBR_ListView.cpp:161-177 IBR_Combo）
                    ComboBox {
                        id: sortCombo
                        Layout.preferredWidth: 120
                        Layout.preferredHeight: 22
                        model: [
                            qsTr("默认"),
                            qsTr("寄存器名"),
                            qsTr("显示名"),
                            qsTr("寄存器类型")
                        ]
                        currentIndex: sectionListModel.sortKey
                        onActivated: sectionListModel.sortKey = index

                        background: Rectangle {
                            color: parent.pressed ? "#3c3c3c" : "#2d2d2d"
                            border.color: "#3c3c3c"; border.width: 1; radius: 3
                        }
                        contentItem: Text {
                            text: parent.displayText
                            color: "#cccccc"; font.pixelSize: 11
                            verticalAlignment: Text.AlignVCenter
                            leftPadding: 6
                        }
                    }

                    // 升降序切换按钮（对应 ArrowButton）
                    Button {
                        Layout.preferredWidth: 28
                        Layout.preferredHeight: 22
                        text: sectionListModel.sortReverse ? "↓" : "↑"
                        onClicked: sectionListModel.sortReverse = !sectionListModel.sortReverse
                        background: Rectangle {
                            color: parent.hovered ? "#3c3c3c" : "#2d2d2d"
                            border.color: "#3c3c3c"; border.width: 1; radius: 3
                        }
                        contentItem: Text {
                            text: parent.text; color: "#cccccc"; font.pixelSize: 12
                            horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter
                        }
                    }

                    Item { Layout.fillWidth: true }

                    // 阶段 9.2 新增：ShowRegName 切换按钮（对应 IBR_ListView.cpp:294）
                    StyledButton {
                        Layout.preferredWidth: 92
                        Layout.preferredHeight: 22
                        text: sectionListModel.showRegName ? qsTr("显示: 寄存器名") : qsTr("显示: 显示名")
                        accent: sectionListModel.showRegName
                        font.pixelSize: 10
                        onClicked: sectionListModel.showRegName = !sectionListModel.showRegName
                    }
                }

                // 第二行：搜索框（对应 IBR_ListView.cpp:180 InputText + SearchIconBg）
                // 阶段 9.3：增加搜索图标（对应 ImGuiInputTextFlags_SearchIconBg）
                // 单向同步：用户输入时 QML -> C++；C++ 端 filterText 外部改变时
                // 仅在非聚焦且文本不一致时回写，避免打断 IME 中文输入组合
                TextField {
                    id: searchField
                    Layout.fillWidth: true
                    Layout.preferredHeight: 24
                    placeholderText: qsTr("搜索 Section...")
                    color: "#d4d4d4"
                    font.pixelSize: 12
                    selectByMouse: true
                    leftPadding: 26  // 留出搜索图标空间
                    onTextChanged: sectionListModel.filterText = text

                    background: Rectangle {
                        color: "#1e1e1e"
                        border.color: searchField.activeFocus ? "#007acc" : "#3c3c3c"
                        border.width: 1; radius: 3

                        // 搜索图标（对应 ImGuiInputTextFlags_SearchIconBg）
                        Text {
                            anchors.left: parent.left
                            anchors.leftMargin: 6
                            anchors.verticalCenter: parent.verticalCenter
                            text: "\uD83D\uDD0D"  // 🔍
                            color: "#858585"
                            font.pixelSize: 13
                        }
                    }

                    Connections {
                        target: sectionListModel
                        function onFilterTextChanged() {
                            if (!searchField.activeFocus
                                && searchField.text !== sectionListModel.filterText) {
                                searchField.text = sectionListModel.filterText
                            }
                        }
                    }
                }

                // 第三行：5 个筛选开关（对应 IBR_ListView.cpp:183-194）
                // 阶段 9.2 新增：Full / CaseSensitive / Regex 三个 CheckBox
                RowLayout {
                    Layout.fillWidth: true
                    spacing: 8

                    // 按寄存器名/显示名搜索切换（对应 IBR_ListView.cpp:184-185 ImGui::Button）
                    // 阶段 9.3：由 CheckBox 改为 Button，对齐 ImGui 的 Button 切换语义
                    StyledButton {
                        Layout.preferredHeight: 22
                        text: sectionListModel.filterByRegistry
                              ? qsTr("搜索: 寄存器名") : qsTr("搜索: 显示名")
                        accent: sectionListModel.filterByRegistry
                        font.pixelSize: 10
                        onClicked: sectionListModel.filterByRegistry = !sectionListModel.filterByRegistry
                    }

                    // Full 全字匹配（对应 IBR_ListView.cpp:188）
                    StyledCheckBox {
                        text: qsTr("全字匹配")
                        checked: sectionListModel.filterFull
                        onToggled: sectionListModel.filterFull = checked
                        font.pixelSize: 11
                    }

                    // CaseSensitive 大小写敏感（对应 IBR_ListView.cpp:191）
                    StyledCheckBox {
                        text: qsTr("区分大小写")
                        checked: sectionListModel.filterCaseSensitive
                        onToggled: sectionListModel.filterCaseSensitive = checked
                        font.pixelSize: 11
                    }

                    // Regex 正则匹配（对应 IBR_ListView.cpp:193）
                    StyledCheckBox {
                        text: qsTr("正则表达式")
                        checked: sectionListModel.filterRegex
                        onToggled: sectionListModel.filterRegex = checked
                        font.pixelSize: 11
                    }

                    Item { Layout.fillWidth: true }
                }
            }

            Rectangle {
                anchors.bottom: parent.bottom
                anchors.left: parent.left; anchors.right: parent.right
                height: 1; color: "#1e1e1e"
            }
        }

        // ===== Section 列表 =====
        ListView {
            id: listView
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            model: sectionListModel

            // 监听 jumpRequested 信号（对应 IBR_ListView.cpp:303-307 跳转）
            Connections {
                target: sectionListModel
                function onJumpRequested(eqX, eqY, sectionId) {
                    workspaceController.centerViewTo(eqX, eqY)
                    workspaceController.activateAndEdit(sectionId)
                }
            }

            delegate: Item {
                width: listView.width
                height: 32

                Rectangle {
                    anchors.fill: parent
                    // 阶段 9.2：整行背景染色（对应 IBR_ListView.cpp:285-288 PushStyleColor FrameBg）
                    // 用 registerColor 染色，alpha 降低到 0.15 避免文字不可读
                    // hover/selected 在染色基础上叠加深色覆盖
                    color: {
                        if (selected) return "#37373d"
                        if (mouseArea.containsMouse) return "#2a2d2e"
                        if (!isComment && registerColor.a > 0) {
                            // registerColor 与背景混合，alpha=0.15
                            return Qt.rgba(registerColor.r, registerColor.g, registerColor.b, 0.15)
                        }
                        return "transparent"
                    }

                    // 阶段 9.2：显式 Checkbox 选择控件（对应 IBR_ListView.cpp:296 ImGui::Checkbox）
                    // 阶段 9.3：选中色跟随 registerColor（对应 IBR_ListView.cpp:288 PushStyleColor CheckMark）
                    Rectangle {
                        id: selectCheckbox
                        anchors.left: parent.left
                        anchors.leftMargin: 6
                        anchors.verticalCenter: parent.verticalCenter
                        width: 14; height: 14
                        radius: 2
                        // 有寄存器类型颜色时用 registerColor，否则回退 #007acc
                        color: selected ? (registerColor.a > 0 ? registerColor : "#007acc") : "#1e1e1e"
                        border.color: selected ? (registerColor.a > 0 ? registerColor : "#007acc") : "#3c3c3c"
                        border.width: 1
                        visible: !isComment

                        Text {
                            anchors.centerIn: parent
                            visible: selected
                            text: "✓"
                            color: "#ffffff"
                            font.pixelSize: 10
                        }

                        MouseArea {
                            anchors.fill: parent
                            onClicked: sectionListModel.select(index, false)
                        }
                    }

                    // 寄存器类型颜色色块
                    Rectangle {
                        anchors.left: selectCheckbox.right
                        anchors.leftMargin: 4
                        anchors.verticalCenter: parent.verticalCenter
                        width: 4; height: 20
                        color: registerColor
                        visible: !isComment
                    }

                    // 显示名（根据 showRegName 在 data() 中切换，对应 IBR_ListView.cpp:294）
                    Text {
                        anchors.left: parent.left
                        anchors.leftMargin: 30
                        anchors.right: statusIcons.left
                        anchors.rightMargin: 8
                        anchors.verticalCenter: parent.verticalCenter
                        text: displayName
                        // 状态文字色（对应 IBR_ListView.cpp:290-294）
                        color: isComment ? "#858585"
                              : hidden ? "#5a5a5a"
                              : ignored ? "#808080"
                              : frozen ? "#4fc3f7"
                              : "#d4d4d4"
                        font.pixelSize: 12
                        elide: Text.ElideRight
                    }

                    // 状态图标 + 寄存器类型 + 跳转按钮
                    Row {
                        id: statusIcons
                        anchors.right: parent.right
                        anchors.rightMargin: 4
                        anchors.verticalCenter: parent.verticalCenter
                        spacing: 2

                        Text {
                            visible: frozen
                            text: "❄"
                            color: "#4fc3f7"
                            font.pixelSize: 12
                        }
                        Text {
                            visible: hidden
                            text: "👁"
                            color: "#ce9178"
                            font.pixelSize: 12
                        }
                        Text {
                            visible: ignored
                            text: "⊘"
                            color: "#808080"
                            font.pixelSize: 12
                        }

                        // 阶段 9.2 新增：寄存器类型小字号显示（对应 ImGui 列表项右侧的 RegType 信息）
                        Text {
                            visible: !isComment && registerType.length > 0
                            text: registerType
                            color: "#858585"
                            font.pixelSize: 10
                            leftPadding: 4; rightPadding: 4
                        }

                        // 跳转按钮（对应 IBR_ListView.cpp:303-307 ArrowButton）
                        Button {
                            width: 22; height: 22
                            text: "→"
                            onClicked: sectionListModel.jumpToSection(index)
                            background: Rectangle {
                                color: parent.hovered ? "#3c3c3c" : "transparent"
                                radius: 2
                            }
                            contentItem: Text {
                                text: parent.text
                                color: parent.hovered ? "#007acc" : "#858585"
                                font.pixelSize: 12
                                horizontalAlignment: Text.AlignHCenter
                                verticalAlignment: Text.AlignVCenter
                            }
                        }
                    }

                    MouseArea {
                        id: mouseArea
                        anchors.fill: parent
                        hoverEnabled: true
                        acceptedButtons: Qt.LeftButton | Qt.RightButton
                        onClicked: {
                            if (mouse.button === Qt.LeftButton) {
                                sectionListModel.select(index, true)
                            } else {
                                // 右键菜单：先选中再弹出
                                sectionListModel.select(index, true)
                                listContextMenu.popup(mouseArea, mouseX, mouseY)
                            }
                        }
                        onDoubleClicked: {
                            if (mouse.button === Qt.LeftButton) {
                                // 双击跳转工作区（对应 IBR_ListView.cpp:303-307）
                                sectionListModel.jumpToSection(index)
                            }
                        }
                    }

                    // 右键菜单（对应 ImGui 列表项右键菜单）
                    Menu {
                        id: listContextMenu

                        MenuItem {
                            text: qsTr("冻结/解冻")
                            onTriggered: sectionListModel.freeze(index, !frozen)
                        }
                        MenuItem {
                            text: qsTr("隐藏/显示")
                            onTriggered: sectionListModel.hide(index, !hidden)
                        }
                        MenuItem {
                            text: qsTr("忽略/取消忽略")
                            onTriggered: sectionListModel.ignore(index, !ignored)
                        }

                        MenuSeparator {}

                        MenuItem {
                            text: qsTr("删除")
                            onTriggered: sectionListModel.deleteSection(index)
                        }
                    }
                }
            }
        }
    }
}
