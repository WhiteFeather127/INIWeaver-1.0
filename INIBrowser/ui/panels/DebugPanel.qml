// DebugPanel.qml
// 调试菜单面板，对应 IBR_Debug.cpp:82-200 RenderUI()
// 仅 -debugmenu 启动时可见。布局：6 个可编辑复选框 + PoolQueryBuf 输入 + 8 个按钮 + 3 个 TreeNode + 撤销栈
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../components"

ScrollView {
    id: root
    clip: true
    contentWidth: availableWidth

    ColumnLayout {
        width: root.availableWidth
        spacing: 8

        // 标题（对应 ImGui Text GUI_DebugTitle）
        Text {
            Layout.leftMargin: 8; Layout.topMargin: 8
            text: (i18n.rev, i18n.tr("GUI_DebugTitle"))
            color: "#cccccc"; font.pixelSize: 13; font.bold: true
        }

        // ===== 6 个可编辑调试复选框（对应 IBR_Debug.cpp:86, 94-98） =====
        Rectangle {
            Layout.fillWidth: true
            Layout.leftMargin: 8; Layout.rightMargin: 8
            Layout.preferredHeight: debugFlagsCol.implicitHeight + 16
            color: "#1e1e1e"; radius: 3; border.color: "#3c3c3c"; border.width: 1

            ColumnLayout {
                id: debugFlagsCol
                anchors.fill: parent
                anchors.margins: 8
                spacing: 6

                // DebugOutputExtra / Ext（对应 IBR_Debug.cpp:85-86 static bool Ext）
                CheckBox {
                    text: (i18n.rev, i18n.tr("GUI_DebugOutputExtra") + " (Ext)")
                    checked: projectController.debugOutputExtra
                    onClicked: projectController.debugOutputExtra = checked
                    contentItem: Text {
                        text: parent.text
                        color: parent.checked ? "#007acc" : "#d4d4d4"
                        font.pixelSize: 12
                        verticalAlignment: Text.AlignVCenter
                        leftPadding: parent.indicator.width + parent.spacing
                    }
                    indicator: Rectangle {
                        implicitWidth: 14; implicitHeight: 14
                        radius: 2
                        color: parent.checked ? "#007acc" : "#1e1e1e"
                        border.color: parent.checked ? "#007acc" : "#3c3c3c"
                        border.width: 1
                        Text {
                            anchors.centerIn: parent
                            visible: parent.parent.checked
                            text: "✓"; color: "#ffffff"; font.pixelSize: 10
                        }
                    }
                }

                // UseModuleProperties（对应 IBR_Debug.cpp:94）
                CheckBox {
                    text: (i18n.rev, i18n.tr("GUI_DebugModuleProperties") + " (UseModuleProperties)")
                    checked: projectController.useModuleProperties
                    onClicked: projectController.useModuleProperties = checked
                    contentItem: Text {
                        text: parent.text
                        color: parent.checked ? "#007acc" : "#d4d4d4"
                        font.pixelSize: 12
                        verticalAlignment: Text.AlignVCenter
                        leftPadding: parent.indicator.width + parent.spacing
                    }
                    indicator: Rectangle {
                        implicitWidth: 14; implicitHeight: 14
                        radius: 2
                        color: parent.checked ? "#007acc" : "#1e1e1e"
                        border.color: parent.checked ? "#007acc" : "#3c3c3c"
                        border.width: 1
                        Text {
                            anchors.centerIn: parent
                            visible: parent.parent.checked
                            text: "✓"; color: "#ffffff"; font.pixelSize: 10
                        }
                    }
                }

                // ShowWorkspaceWindowFrame（对应 IBR_Debug.cpp:95）
                CheckBox {
                    text: (i18n.rev, i18n.tr("GUI_DebugShowDecisionBox") + " (ShowWorkspaceWindowFrame)")
                    checked: projectController.showWorkspaceWindowFrame
                    onClicked: projectController.showWorkspaceWindowFrame = checked
                    contentItem: Text {
                        text: parent.text
                        color: parent.checked ? "#007acc" : "#d4d4d4"
                        font.pixelSize: 12
                        verticalAlignment: Text.AlignVCenter
                        leftPadding: parent.indicator.width + parent.spacing
                    }
                    indicator: Rectangle {
                        implicitWidth: 14; implicitHeight: 14
                        radius: 2
                        color: parent.checked ? "#007acc" : "#1e1e1e"
                        border.color: parent.checked ? "#007acc" : "#3c3c3c"
                        border.width: 1
                        Text {
                            anchors.centerIn: parent
                            visible: parent.parent.checked
                            text: "✓"; color: "#ffffff"; font.pixelSize: 10
                        }
                    }
                }

                // DontGoToEdit（对应 IBR_Debug.cpp:96）
                CheckBox {
                    text: (i18n.rev, i18n.tr("GUI_DebugNoEnterEdit") + " (DontGoToEdit)")
                    checked: projectController.dontGoToEdit
                    onClicked: projectController.dontGoToEdit = checked
                    contentItem: Text {
                        text: parent.text
                        color: parent.checked ? "#007acc" : "#d4d4d4"
                        font.pixelSize: 12
                        verticalAlignment: Text.AlignVCenter
                        leftPadding: parent.indicator.width + parent.spacing
                    }
                    indicator: Rectangle {
                        implicitWidth: 14; implicitHeight: 14
                        radius: 2
                        color: parent.checked ? "#007acc" : "#1e1e1e"
                        border.color: parent.checked ? "#007acc" : "#3c3c3c"
                        border.width: 1
                        Text {
                            anchors.centerIn: parent
                            visible: parent.parent.checked
                            text: "✓"; color: "#ffffff"; font.pixelSize: 10
                        }
                    }
                }

                // DontDrawBg（对应 IBR_Debug.cpp:97）
                CheckBox {
                    text: (i18n.rev, i18n.tr("GUI_DebugCrazyRendering") + " (DontDrawBg)")
                    checked: projectController.dontDrawBg
                    onClicked: projectController.dontDrawBg = checked
                    contentItem: Text {
                        text: parent.text
                        color: parent.checked ? "#007acc" : "#d4d4d4"
                        font.pixelSize: 12
                        verticalAlignment: Text.AlignVCenter
                        leftPadding: parent.indicator.width + parent.spacing
                    }
                    indicator: Rectangle {
                        implicitWidth: 14; implicitHeight: 14
                        radius: 2
                        color: parent.checked ? "#007acc" : "#1e1e1e"
                        border.color: parent.checked ? "#007acc" : "#3c3c3c"
                        border.width: 1
                        Text {
                            anchors.centerIn: parent
                            visible: parent.parent.checked
                            text: "✓"; color: "#ffffff"; font.pixelSize: 10
                        }
                    }
                }

                // LinkDebugMode（对应 IBR_Debug.cpp:98）
                CheckBox {
                    text: (i18n.rev, i18n.tr("GUI_DebugLinkInspect") + " (LinkDebugMode)")
                    checked: projectController.linkDebugMode
                    onClicked: projectController.linkDebugMode = checked
                    contentItem: Text {
                        text: parent.text
                        color: parent.checked ? "#007acc" : "#d4d4d4"
                        font.pixelSize: 12
                        verticalAlignment: Text.AlignVCenter
                        leftPadding: parent.indicator.width + parent.spacing
                    }
                    indicator: Rectangle {
                        implicitWidth: 14; implicitHeight: 14
                        radius: 2
                        color: parent.checked ? "#007acc" : "#1e1e1e"
                        border.color: parent.checked ? "#007acc" : "#3c3c3c"
                        border.width: 1
                        Text {
                            anchors.centerIn: parent
                            visible: parent.parent.checked
                            text: "✓"; color: "#ffffff"; font.pixelSize: 10
                        }
                    }
                }
            }
        }

        // ===== 按钮区（对应 IBR_Debug.cpp:87-153, 194） =====
        Rectangle {
            Layout.fillWidth: true
            Layout.leftMargin: 8; Layout.rightMargin: 8
            Layout.preferredHeight: btnCol.implicitHeight + 16
            color: "#1e1e1e"; radius: 3; border.color: "#3c3c3c"; border.width: 1

            ColumnLayout {
                id: btnCol
                anchors.fill: parent
                anchors.margins: 8
                spacing: 4

                // 第一行：CopyOutput + Clipboard2Json（对应 IBR_Debug.cpp:87-92, 117-128）
                RowLayout {
                    Layout.fillWidth: true
                    spacing: 4

                    StyledButton {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 26
                        text: (i18n.rev, i18n.tr("GUI_DebugCopyOutput") + " (CopyOutput)")
                        font.pixelSize: 11
                        onClicked: projectController.copyOutput()
                    }

                    StyledButton {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 26
                        text: (i18n.rev, i18n.tr("GUI_DebugClipboard2Json") + " (Clipboard2Json)")
                        font.pixelSize: 11
                        onClicked: projectController.clipboard2Json()
                    }
                }

                // 第二行：ITDOpenInputForm + ITDCloseInputForm（对应 IBR_Debug.cpp:100-115）
                // 阶段 8.3：两按钮互斥显示，对应 ImGui if(MenuMatchesSource) ShowClose else ShowOpen
                RowLayout {
                    Layout.fillWidth: true
                    spacing: 4

                    StyledButton {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 26
                        visible: !projectController.itdFormOpen
                        text: (i18n.rev, i18n.tr("GUI_ITDOpenInputForm") + " (ITDOpen)")
                        font.pixelSize: 11
                        onClicked: projectController.itdOpenInputForm()
                    }

                    StyledButton {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 26
                        visible: projectController.itdFormOpen
                        text: (i18n.rev, i18n.tr("GUI_ITDCloseInputForm") + " (ITDClose)")
                        font.pixelSize: 11
                        onClicked: projectController.itdCloseInputForm()
                    }
                }

                // 第三行：TriggerRefreshLink + ClearOnceInfo（对应 IBR_Debug.cpp:150-153, 194）
                RowLayout {
                    Layout.fillWidth: true
                    spacing: 4

                    StyledButton {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 26
                        text: (i18n.rev, i18n.tr("GUI_DebugTriggerRefreshLink") + " (RefreshLink)")
                        font.pixelSize: 11
                        onClicked: projectController.triggerRefreshLink()
                    }

                    StyledButton {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 26
                        text: (i18n.rev, i18n.tr("GUI_DebugClearOnceInfo") + " (ClearOnce)")
                        font.pixelSize: 11
                        onClicked: projectController.clearOnceInfo()
                    }
                }
            }
        }

        // ===== PoolQueryBuf 输入框（对应 IBR_Debug.cpp:130 InputTextStdString） =====
        Rectangle {
            Layout.fillWidth: true
            Layout.leftMargin: 8; Layout.rightMargin: 8
            Layout.preferredHeight: queryCol.implicitHeight + 16
            color: "#1e1e1e"; radius: 3; border.color: "#3c3c3c"; border.width: 1

            ColumnLayout {
                id: queryCol
                anchors.fill: parent
                anchors.margins: 8
                spacing: 4

                Text {
                    text: (i18n.rev, i18n.tr("GUI_DebugStringPoolQuery") + " (PoolQueryBuf)")
                    color: "#858585"; font.pixelSize: 11
                }

                TextField {
                    id: poolQueryField
                    Layout.fillWidth: true
                    Layout.preferredHeight: 24
                    color: "#d4d4d4"
                    font.pixelSize: 12
                    selectByMouse: true
                    onTextChanged: projectController.poolQueryBuf = text

                    background: Rectangle {
                        color: "#1e1e1e"
                        border.color: poolQueryField.activeFocus ? "#007acc" : "#3c3c3c"
                        border.width: 1; radius: 3
                    }

                    // 单向同步：避免 IME 组合期间回写打断中文输入
                    // poolQueryBuf 的 NOTIFY 是 debugInfoChanged
                    Connections {
                        target: projectController
                        function onDebugInfoChanged() {
                            if (!poolQueryField.activeFocus
                                && poolQueryField.text !== projectController.poolQueryBuf) {
                                poolQueryField.text = projectController.poolQueryBuf
                            }
                        }
                    }
                }

                // String To ID + ID To String 按钮（对应 IBR_Debug.cpp:131-147）
                RowLayout {
                    Layout.fillWidth: true
                    spacing: 4

                    StyledButton {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 24
                        text: (i18n.rev, i18n.tr("GUI_DebugStringToID"))
                        font.pixelSize: 11
                        onClicked: projectController.queryStringToId()
                    }

                    StyledButton {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 24
                        text: (i18n.rev, i18n.tr("GUI_DebugIDToString"))
                        font.pixelSize: 11
                        onClicked: projectController.queryIdToString()
                    }
                }

                // 查询结果显示（对应 IBR_Debug.cpp:148 TextWrappedEx LastQueryResult）
                Text {
                    Layout.fillWidth: true
                    text: (i18n.rev, i18n.trF("GUI_DebugResult", [projectController.lastQueryResult || (i18n.rev, i18n.tr("GUI_NoResult"))]))
                    color: "#ce9178"; font.pixelSize: 12
                    wrapMode: Text.WrapAnywhere
                    maximumLineCount: 3
                    elide: Text.ElideRight
                }
            }
        }

        // 分隔线
        Rectangle {
            Layout.fillWidth: true
            Layout.leftMargin: 8; Layout.rightMargin: 8
            Layout.preferredHeight: 1; color: "#1e1e1e"
        }

        // ===== 3 个 TreeNode（对应 IBR_Debug.cpp:159-199） =====
        // GUI_DebugUIState（对应 IBR_Debug.cpp:159-186）
        Rectangle {
            Layout.fillWidth: true
            Layout.leftMargin: 8; Layout.rightMargin: 8
            Layout.preferredHeight: uiStateHeader.height + (uiStateExpanded ? uiStateContent.implicitHeight + 8 : 0)
            color: "#1e1e1e"; radius: 3; border.color: "#3c3c3c"; border.width: 1
            clip: true

            property bool uiStateExpanded: false

            // 阶段 8.3：UIState 实时刷新（对应 ImGui 每帧刷新）
            // 展开时 100ms 定时拉取最新状态，折叠时停止
            Timer {
                id: uiStateRefreshTimer
                interval: 100
                repeat: true
                running: parent.uiStateExpanded
                onTriggered: uiStateContent.uiState = projectController.debugUIState()
            }

            Button {
                id: uiStateHeader
                anchors.left: parent.left; anchors.right: parent.right
                anchors.top: parent.top
                height: 26
                flat: true
                text: (parent.uiStateExpanded ? "▼" : "▶") + " " + (i18n.rev, i18n.tr("GUI_DebugUIState"))
                onClicked: parent.uiStateExpanded = !parent.uiStateExpanded
                background: Rectangle { color: "transparent" }
                contentItem: Text {
                    text: parent.text; color: "#cccccc"; font.pixelSize: 12
                    horizontalAlignment: Text.AlignLeft; verticalAlignment: Text.AlignVCenter
                    leftPadding: 8
                }
            }

            ColumnLayout {
                id: uiStateContent
                anchors.left: parent.left; anchors.right: parent.right
                anchors.top: uiStateHeader.bottom
                anchors.margins: 8
                visible: parent.uiStateExpanded

                property var uiState: projectController.debugUIState()

                // 手动刷新按钮（保留，对应原快照模式）
                StyledButton {
                    Layout.preferredHeight: 22
                    Layout.preferredWidth: 80
                    text: (i18n.rev, i18n.tr("GUI_Refresh"))
                    font.pixelSize: 11
                    onClicked: {
                        uiStateContent.uiState = projectController.debugUIState()
                    }
                }

                Repeater {
                    model: [
                        { label: "MousePos", key: "mousePos" },
                        { label: "MassAfter_RightDownPos", key: "massAfterRightDownPos" },
                        { label: "Current EqMax", key: "currentEqMax" },
                        { label: "ScreenSize", key: "screenSize" },
                        { label: "IsBgDragging", key: "isBgDragging" },
                        { label: "HoldingModules", key: "holdingModules" },
                        { label: "IsMassSelecting", key: "isMassSelecting" },
                        { label: "IsMassAfter", key: "isMassAfter" },
                        { label: "HasRightDownToWait", key: "hasRightDownToWait" },
                        { label: "HasLeftDownToWait", key: "hasLeftDownToWait" },
                        { label: "MoveAfterMass", key: "moveAfterMass" },
                        { label: "LastClickable", key: "lastClickable" },
                        { label: "LastOnWindow", key: "lastOnWindow" },
                        { label: "LastCont", key: "lastCont" },
                        { label: "OnCombo", key: "onCombo" },
                        { label: "OnPopupMenu", key: "onPopupMenu" },
                        { label: "HasDragNow", key: "hasDragNow" },
                        // 阶段 8.3：补齐 5 项键盘/鼠标状态（对应 IBR_Debug.cpp:179-183）
                        { label: "DblClickLeft", key: "dblClickLeft" },
                        { label: "CTRL", key: "ctrl" },
                        { label: "SHIFT", key: "shift" },
                        { label: "ALT", key: "alt" },
                        { label: "SUPER", key: "super" }
                    ]

                    Text {
                        Layout.fillWidth: true
                        text: {
                            // 严格判断 undefined/null，避免 0/false/"" 被 || 吞掉
                            var v = uiStateContent.uiState[modelData.key]
                            var display = (v === undefined || v === null) ? "(unknown)" : v
                            return modelData.label + " = " + display
                        }
                        color: "#d4d4d4"; font.pixelSize: 11
                        font.family: "Consolas, 'Courier New', monospace"
                    }
                }
            }
        }

        // GUI_DebugRealTimeInfo（对应 IBR_Debug.cpp:188-192）
        Rectangle {
            Layout.fillWidth: true
            Layout.leftMargin: 8; Layout.rightMargin: 8
            Layout.preferredHeight: rtHeader.height + (rtExpanded ? rtContent.implicitHeight + 8 : 0)
            color: "#1e1e1e"; radius: 3; border.color: "#3c3c3c"; border.width: 1
            clip: true

            property bool rtExpanded: false

            Button {
                id: rtHeader
                anchors.left: parent.left; anchors.right: parent.right
                anchors.top: parent.top
                height: 26
                flat: true
                text: (parent.rtExpanded ? "▼" : "▶") + " " + (i18n.rev, i18n.tr("GUI_DebugRealTimeInfo"))
                onClicked: parent.rtExpanded = !parent.rtExpanded
                background: Rectangle { color: "transparent" }
                contentItem: Text {
                    text: parent.text; color: "#cccccc"; font.pixelSize: 12
                    horizontalAlignment: Text.AlignLeft; verticalAlignment: Text.AlignVCenter
                    leftPadding: 8
                }
            }

            ColumnLayout {
                id: rtContent
                anchors.left: parent.left; anchors.right: parent.right
                anchors.top: rtHeader.bottom
                anchors.margins: 8
                visible: parent.rtExpanded

                Repeater {
                    model: projectController.debugRealTimeMessages()
                    Text {
                        Layout.fillWidth: true
                        text: modelData
                        color: "#d4d4d4"; font.pixelSize: 11
                        font.family: "Consolas, 'Courier New', monospace"
                        wrapMode: Text.Wrap
                    }
                }
            }
        }

        // GUI_DebugOnceInfo（对应 IBR_Debug.cpp:195-199）
        Rectangle {
            Layout.fillWidth: true
            Layout.leftMargin: 8; Layout.rightMargin: 8
            Layout.preferredHeight: onceHeader.height + (onceExpanded ? onceContent.implicitHeight + 8 : 0)
            color: "#1e1e1e"; radius: 3; border.color: "#3c3c3c"; border.width: 1
            clip: true

            property bool onceExpanded: false

            Button {
                id: onceHeader
                anchors.left: parent.left; anchors.right: parent.right
                anchors.top: parent.top
                height: 26
                flat: true
                text: (parent.onceExpanded ? "▼" : "▶") + " " + (i18n.rev, i18n.tr("GUI_DebugOnceInfo"))
                onClicked: parent.onceExpanded = !parent.onceExpanded
                background: Rectangle { color: "transparent" }
                contentItem: Text {
                    text: parent.text; color: "#cccccc"; font.pixelSize: 12
                    horizontalAlignment: Text.AlignLeft; verticalAlignment: Text.AlignVCenter
                    leftPadding: 8
                }
            }

            ColumnLayout {
                id: onceContent
                anchors.left: parent.left; anchors.right: parent.right
                anchors.top: onceHeader.bottom
                anchors.margins: 8
                visible: parent.onceExpanded

                Repeater {
                    model: projectController.debugOnceMessages()
                    Text {
                        Layout.fillWidth: true
                        text: modelData
                        color: "#d4d4d4"; font.pixelSize: 11
                        font.family: "Consolas, 'Courier New', monospace"
                        wrapMode: Text.Wrap
                    }
                }
            }
        }

        // 分隔线
        Rectangle {
            Layout.fillWidth: true
            Layout.leftMargin: 8; Layout.rightMargin: 8
            Layout.preferredHeight: 1; color: "#1e1e1e"
        }

        // ===== 撤销栈信息（保留原有功能，对应 ImGui TreeNode GUI_DebugUndoStackInfo） =====
        RowLayout {
            Layout.fillWidth: true
            Layout.leftMargin: 8; Layout.rightMargin: 8
            spacing: 8

            Text {
                text: (i18n.rev, i18n.tr("GUI_UndoStackInfo"))
                color: "#cccccc"; font.pixelSize: 13; font.bold: true
            }
            Item { Layout.fillWidth: true }
            Text {
                text: (i18n.rev, i18n.trF("GUI_UndoStackSummary", [projectController.undoStackCount(), projectController.undoStackCursor()]))
                color: "#858585"; font.pixelSize: 12
            }
        }

        // 撤销栈列表
        Rectangle {
            Layout.fillWidth: true
            Layout.leftMargin: 8; Layout.rightMargin: 8
            Layout.fillHeight: true
            Layout.minimumHeight: 120
            color: "#1e1e1e"; radius: 3; border.color: "#3c3c3c"; border.width: 1

            ListView {
                id: undoList
                anchors.fill: parent
                anchors.margins: 4
                clip: true
                model: projectController.undoStackItems()

                delegate: Rectangle {
                    width: undoList.width
                    height: 22
                    color: index === projectController.undoStackCursor() ? "#3c3c3c" : "transparent"

                    Text {
                        anchors.left: parent.left
                        anchors.leftMargin: 8
                        anchors.verticalCenter: parent.verticalCenter
                        text: (index + 1) + ". " + (modelData.id || "(空)")
                        color: index === projectController.undoStackCursor() ? "#ffffff" : "#d4d4d4"
                        font.pixelSize: 11
                        elide: Text.ElideRight
                        width: parent.width - 12
                    }
                }
            }

            Text {
                anchors.centerIn: parent
                visible: undoList.count === 0
                color: "#5a5a5a"; font.pixelSize: 12
                text: (i18n.rev, i18n.tr("GUI_UndoStackEmpty"))
            }
        }

        // 刷新调试按钮
        StyledButton {
            Layout.fillWidth: true
            Layout.leftMargin: 8; Layout.rightMargin: 8
            Layout.preferredHeight: 28
            text: (i18n.rev, i18n.tr("GUI_RefreshDebug"))
            font.pixelSize: 12
            onClicked: projectController.refreshDebug()
        }

        Item { Layout.fillHeight: true; Layout.preferredHeight: 16 }
    }
}
