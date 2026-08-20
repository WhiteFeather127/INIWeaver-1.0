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
        text: (i18n.rev, i18n.tr("GUI_WaitOpen"))
    }

    // 项目打开后显示 Section 列表
    ColumnLayout {
        anchors.fill: parent
        spacing: 0
        visible: projectController.isOpen

        // ===== 顶部操作按钮（Flow 自动换行）=====
        Rectangle {
            Layout.fillWidth: true
            // 高度自适应：Flow.implicitHeight 为内容高度（1行22 / 2行46），+8 上下 margin
            Layout.preferredHeight: btnFlow.implicitHeight + 8
            color: "#2d2d2d"

            // 操作按钮组（对应 IBR_ListView.cpp:127-152）
            // Flow 布局：空间不够时自动换行到下一行
            // 注意：rowCount 是 QAbstractListModel::rowCount() override（非 Q_PROPERTY），
            // QML 属性访问返回 undefined → enabled 恒 false（全选一直灰），必须用 rowCount() 函数调用
            // 而 selectedCount 是 Q_PROPERTY(int)，用属性访问（无括号），加括号会报 not a function
            Flow {
                id: btnFlow
                anchors.fill: parent
                anchors.margins: 4
                spacing: 2

                // 全选/取消全选（智能切换文字）
                // 对齐 ImGui IBR_ListView.cpp:127-132：
                //   全选时显示"全不选"(enabled)，未全选显示"全选"(enabled)，空列表显示"全选"(disabled)
                StyledButton {
                    width: 60; height: 22
                    enabled: sectionListModel.rowCount() > 0
                    text: (sectionListModel.rowCount() > 0
                           && sectionListModel.selectedCount === sectionListModel.rowCount())
                          ? (i18n.rev, i18n.tr("GUI_SelectNone")) : (i18n.rev, i18n.tr("GUI_SelectAll"))
                    font.pixelSize: 11
                    onClicked: {
                        if (sectionListModel.selectedCount === sectionListModel.rowCount())
                            workspaceController.clearSelection()
                        else
                            workspaceController.selectAll()
                    }
                }

                // 反选（走 workspaceController.selectInvert 统一同步）
                StyledButton {
                    width: 48; height: 22
                    enabled: sectionListModel.rowCount() > 0
                    text: (i18n.rev, i18n.tr("GUI_InvertSelect"))
                    font.pixelSize: 11
                    onClicked: workspaceController.selectInvert()
                }

                // 刷新
                StyledButton {
                    width: 48; height: 22
                    text: (i18n.rev, i18n.tr("GUI_Refresh"))
                    font.pixelSize: 11
                    onClicked: sectionListModel.refresh()
                }

                // Duplicate 按钮（对应 IBR_ListView.cpp:137, 144）
                StyledButton {
                    width: 64; height: 22
                    enabled: sectionListModel.selectedCount > 0
                    text: (i18n.rev, i18n.tr("GUI_Duplicate"))
                    font.pixelSize: 11
                    onClicked: sectionListModel.duplicate()
                }

                // Freeze/Unfreeze 智能切换按钮（对应 IBR_ListView.cpp:109, 146-147）
                StyledButton {
                    width: 56; height: 22
                    enabled: sectionListModel.selectedCount > 0
                    text: (sectionListModel.selectedCount > 0
                           && sectionListModel.selAndFrozenN === sectionListModel.selectedCount)
                          ? (i18n.rev, i18n.tr("GUI_UnfreezeSec")) : (i18n.rev, i18n.tr("GUI_FreezeSec"))
                    font.pixelSize: 11
                    onClicked: {
                        if (sectionListModel.selAndFrozenN === sectionListModel.selectedCount
                            && sectionListModel.selectedCount > 0)
                            sectionListModel.freezeAll(false)
                        else
                            sectionListModel.freezeAll(true)
                    }
                }

                // Hide/Show 智能切换按钮（对应 IBR_ListView.cpp:110, 149-150）
                StyledButton {
                    width: 56; height: 22
                    enabled: sectionListModel.selectedCount > 0
                    text: (sectionListModel.selectedCount > 0
                           && sectionListModel.selAndHiddenN === sectionListModel.selectedCount)
                          ? (i18n.rev, i18n.tr("GUI_ShowSec")) : (i18n.rev, i18n.tr("GUI_HideSec"))
                    font.pixelSize: 11
                    onClicked: {
                        if (sectionListModel.selAndHiddenN === sectionListModel.selectedCount
                            && sectionListModel.selectedCount > 0)
                            sectionListModel.hideAll(false)
                        else
                            sectionListModel.hideAll(true)
                    }
                }

                // 删除
                StyledButton {
                    width: 48; height: 22
                    enabled: sectionListModel.selectedCount > 0
                    text: (i18n.rev, i18n.tr("GUI_Delete"))
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
            Layout.preferredHeight: 132
            color: "#252525"

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 4
                spacing: 4

                // 已选计数（对应 IBR_ListView.cpp:155-157 ImGui SelectedCount 显示）
                // 从顶部标题栏移到此处（按钮下面那行）
                Text {
                    text: (i18n.rev, i18n.tr("GUI_SelectedCount")) + " " + sectionListModel.selectedCount
                          + "/" + sectionListModel.rowCount()
                    color: "#cccccc"
                    font.pixelSize: 11
                }

                // 第一行：排序 ComboBox + 升降序切换
                RowLayout {
                    Layout.fillWidth: true
                    spacing: 4

                    Text {
                        text: (i18n.rev, i18n.tr("GUI_SortBy")) + ":"
                        color: "#858585"
                        font.pixelSize: 11
                    }

                    // 排序方式下拉框（对应 IBR_ListView.cpp:161-177 IBR_Combo）
                    ComboBox {
                        id: sortCombo
                        Layout.preferredWidth: 120
                        Layout.preferredHeight: 22
                        model: [
                            (i18n.rev, i18n.tr("GUI_SortByDefault")),
                            (i18n.rev, i18n.tr("GUI_SortByRegName")),
                            (i18n.rev, i18n.tr("GUI_SortByDisplayName")),
                            (i18n.rev, i18n.tr("GUI_SortByRegType"))
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
                        text: sectionListModel.showRegName ? (i18n.rev, i18n.tr("GUI_ShowRegMode")) : (i18n.rev, i18n.tr("GUI_ShowNameMode"))
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
                    placeholderText: (i18n.rev, i18n.tr("GUI_SearchSection") + "...")
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
                              ? (i18n.rev, i18n.tr("GUI_SearchSec_ByRegistry")) : (i18n.rev, i18n.tr("GUI_SearchSec_ByDisplayName"))
                        accent: sectionListModel.filterByRegistry
                        font.pixelSize: 10
                        onClicked: sectionListModel.filterByRegistry = !sectionListModel.filterByRegistry
                    }

                    // Full 全字匹配（对应 IBR_ListView.cpp:188）
                    StyledCheckBox {
                        text: (i18n.rev, i18n.tr("GUI_SearchSec_Full"))
                        checked: sectionListModel.filterFull
                        onToggled: sectionListModel.filterFull = checked
                        font.pixelSize: 11
                    }

                    // CaseSensitive 大小写敏感（对应 IBR_ListView.cpp:191）
                    StyledCheckBox {
                        text: (i18n.rev, i18n.tr("GUI_SearchSec_CaseSensitive"))
                        checked: sectionListModel.filterCaseSensitive
                        onToggled: sectionListModel.filterCaseSensitive = checked
                        font.pixelSize: 11
                    }

                    // Regex 正则匹配（对应 IBR_ListView.cpp:193）
                    StyledCheckBox {
                        text: (i18n.rev, i18n.tr("GUI_SearchSec_Regex"))
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

            // workspace→列表同步：画布框选/全选/点击节点后 selectedRevision 变化，
            // 列表需同步勾选显示（读 Dynamic.Selected），不重建行避免滚动跳变
            Connections {
                target: workspaceController
                function onSelectedRevisionChanged() {
                    sectionListModel.syncSelectionFromWorkspace()
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
                        if (mouseArea.containsMouse || jumpBtn.hovered) return "#2a2d2e"
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
                            onClicked: workspaceController.toggleSelectSection(sectionId)
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
                    }

                    // 跳转按钮（对应 IBR_ListView.cpp:303-307 ArrowButton）
                    // hover 该行时在最右端显示带边框按钮，点击判定区大（28x26）
                    // 独立于 statusIcons Row 用 anchors 定位，避免 hover 时布局抖动
                    Button {
                        id: jumpBtn
                        z: 10  // 浮在整行 mouseArea 之上，确保可点击（mouseArea 在其后声明 z 更高）
                        anchors.right: parent.right
                        anchors.rightMargin: 4
                        anchors.verticalCenter: parent.verticalCenter
                        width: 28; height: 26
                        visible: (mouseArea.containsMouse || jumpBtn.hovered) && !isComment
                        text: "→"
                        font.pixelSize: 13
                        background: Rectangle {
                            color: jumpBtn.hovered ? "#3c3c3c" : "#2d2d2d"
                            border.color: jumpBtn.hovered ? "#007acc" : "#3c3c3c"
                            border.width: 1
                            radius: 3
                        }
                        contentItem: Text {
                            text: jumpBtn.text
                            color: jumpBtn.hovered ? "#007acc" : "#cccccc"
                            font: jumpBtn.font
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                        }
                        onClicked: sectionListModel.jumpToSection(index)
                    }

                    MouseArea {
                        id: mouseArea
                        anchors.fill: parent
                        hoverEnabled: true
                        acceptedButtons: Qt.LeftButton | Qt.RightButton
                        onClicked: {
                            // 点跳转按钮区域不触发整行 toggle（按钮 z:10 在上层先处理跳转，
                            // 但整行 mouseArea 仍会收到事件，需排除按钮区域避免双重触发）
                            if (jumpBtn.visible
                                && mouseX >= jumpBtn.x && mouseX <= jumpBtn.x + jumpBtn.width
                                && mouseY >= jumpBtn.y && mouseY <= jumpBtn.y + jumpBtn.height)
                                return
                            if (mouse.button === Qt.LeftButton) {
                                // 整行左键单击=toggle 勾选（与 checkbox 一致，对齐 ImGui 多选语义）
                                workspaceController.toggleSelectSection(sectionId)
                            } else {
                                // 右键菜单：未选中则加入选中集合再弹出（QML 扩展，ImGui 列表无右键）
                                // 用 toggleSelectSection 保持多选语义（已选中则不动，未选中则加入）
                                if (!selected)
                                    workspaceController.toggleSelectSection(sectionId)
                                var gp = mouseArea.mapToGlobal(mouseX, mouseY)
                                contextMenuHost.show([
                                    { type: "item", text: (i18n.rev, i18n.tr("GUI_FreezeSec") + "/" + i18n.tr("GUI_UnfreezeSec")), action: "freeze" },
                                    { type: "item", text: (i18n.rev, i18n.tr("GUI_HideSec") + "/" + i18n.tr("GUI_ShowSec")), action: "hide" },
                                    { type: "item", text: (i18n.rev, i18n.tr("GUI_Ignore") + "/" + i18n.tr("GUI_NoIgnore")), action: "ignore" },
                                    { type: "separator" },
                                    { type: "item", text: (i18n.rev, i18n.tr("GUI_Delete")), action: "delete" }
                                ], gp.x, gp.y, (action) => {
                                    switch (action) {
                                    case "freeze": sectionListModel.freeze(index, !frozen); break
                                    case "hide":   sectionListModel.hide(index, !hidden); break
                                    case "ignore": sectionListModel.ignore(index, !ignored); break
                                    case "delete": sectionListModel.deleteSection(index); break
                                    }
                                    // 侧边栏操作与画布统一：立即刷新画布（对应 ImGui 立即模式
                                    // 每帧重读 SectionData 的 Frozen/Hidden/Ignore，两侧天然同步）
                                    workspaceController.refresh()
                                })
                            }
                        }
                    }
                }
            }
        }
    }
}
