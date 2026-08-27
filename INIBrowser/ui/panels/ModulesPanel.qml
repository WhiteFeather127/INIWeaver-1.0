// ModulesPanel.qml
// 模块菜单面板，对应 IBR_Panel.cpp ControlPanel_Modules() (line 165-187)
// 布局：标题 + 搜索框 + 包含特殊模块开关 + 折叠全部按钮 + 分隔线 + 模块树
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../components"

Item {
    id: root

    // 顶部标题栏 + 折叠全部按钮
    Rectangle {
        id: header
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        height: 32
        color: "#2d2d2d"

        Text {
            anchors.left: parent.left
            anchors.leftMargin: 8
            anchors.verticalCenter: parent.verticalCenter
            text: (i18n.rev, i18n.tr("GUI_MenuItem_Modules"))
            color: "#cccccc"
            font.pixelSize: 13
            font.bold: true
        }

        StyledButton {
            anchors.right: parent.right
            anchors.rightMargin: 8
            anchors.verticalCenter: parent.verticalCenter
            text: (i18n.rev, i18n.tr("GUI_FoldAllModules"))
            width: 64
            height: 22
            enabled: !moduleTreeModel.isEmpty
            font.pixelSize: 11
            onClicked: moduleTreeModel.collapseAll()
        }

        // 底部分隔线
        Rectangle {
            anchors.bottom: parent.bottom
            anchors.left: parent.left
            anchors.right: parent.right
            height: 1
            color: "#1e1e1e"
        }
    }

    // 阶段 6.1：搜索框 + 包含特殊模块开关（对应 SearchModuleAlt::RenderUI）
    Rectangle {
        id: searchBox
        anchors.top: header.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        height: 56
        color: "#252525"

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 6
            spacing: 4

            // 搜索框（对应 ImGui InputText with SearchIconBg）
            // 单向同步策略：用户输入时 QML -> C++ 同步 filter
            // C++ 端 filter 被外部改变时（如清空按钮），只在文本不一致且非聚焦时回写
            // 避免 IME 组合期间 C++ setFilter 后回写 text 打断中文输入
            TextField {
                id: searchField
                Layout.fillWidth: true
                Layout.preferredHeight: 24
                placeholderText: (i18n.rev, i18n.tr("GUI_SearchSection") + "...")
                color: "#d4d4d4"
                font.pixelSize: 12
                selectByMouse: true
                onTextChanged: moduleTreeModel.filter = text

                background: Rectangle {
                    color: "#1e1e1e"
                    border.color: searchField.activeFocus ? "#007acc" : "#3c3c3c"
                    border.width: 1
                    radius: 3
                }

                // 仅在非聚焦（用户未在输入）且文本不一致时回写，避免打断 IME
                Connections {
                    target: moduleTreeModel
                    function onFilterChanged() {
                        if (!searchField.activeFocus
                            && searchField.text !== moduleTreeModel.filter) {
                            searchField.text = moduleTreeModel.filter
                        }
                    }
                }
            }

            // 包含特殊模块开关（对应 ModuleTreeModel.includeSpecial）
            RowLayout {
                Layout.fillWidth: true
                spacing: 6

                StyledCheckBox {
                    checked: moduleTreeModel.includeSpecial
                    onToggled: moduleTreeModel.includeSpecial = checked
                    text: (i18n.rev, i18n.tr("GUI_IncludeSpecial"))
                    font.pixelSize: 11
                }

                Item { Layout.fillWidth: true }
            }
        }

        // 底部分隔线
        Rectangle {
            anchors.bottom: parent.bottom
            anchors.left: parent.left
            anchors.right: parent.right
            height: 1
            color: "#1e1e1e"
        }
    }

    // 空状态提示（对应 ImGui 行 174-178）
    Text {
        anchors.centerIn: parent
        anchors.verticalCenterOffset: -16
        visible: moduleTreeModel.isEmpty
        color: "#5a5a5a"
        font.pixelSize: 13
        text: (i18n.rev, i18n.tr("GUI_NoModuleAvailable"))
    }

    // 模块树列表
    ListView {
        id: treeView
        anchors.top: searchBox.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        visible: !moduleTreeModel.isEmpty
        clip: true
        model: moduleTreeModel
        property real _savedY: 0

        // 展开/折叠/过滤等 rebuild 会 beginResetModel 导致列表回顶；重建前保存 contentY、
        // modelReset 后恢复，避免浏览到中部时点一下文件夹就跳到顶端。同步设置防"回弹一帧顶端"。
        Connections {
            target: moduleTreeModel
            function onBeforeReset() {
                treeView._savedY = treeView.contentY
            }
            function onModelReset() {
                if (treeView._savedY > 0) {
                    treeView.contentY = treeView._savedY
                    Qt.callLater(() => treeView.contentY = treeView._savedY)
                }
            }
        }

        delegate: Item {
            width: treeView.width
            height: 28

            // 缩进（对应 ImGui TreeNodeEx 的深度）
            property real indent: depth * 16

            Rectangle {
                anchors.fill: parent
                color: mouseArea.containsMouse ? "#3c3c3c" : "transparent"

                // 文件夹/模块图标
                Text {
                    anchors.left: parent.left
                    anchors.leftMargin: 8 + indent
                    anchors.verticalCenter: parent.verticalCenter
                    text: isFolder ? (expanded ? "📂" : "📁") : "📄"
                    font.pixelSize: 14
                }

                // 名称
                Text {
                    anchors.left: parent.left
                    anchors.leftMargin: 28 + indent
                    anchors.right: parent.right
                    anchors.rightMargin: 8
                    anchors.verticalCenter: parent.verticalCenter
                    text: name
                    color: isFolder ? "#cccccc" : "#d4d4d4"
                    font.pixelSize: 13
                    elide: Text.ElideRight
                }

                // 子节点数（仅目录显示）
                Text {
                    anchors.right: parent.right
                    anchors.rightMargin: 8
                    anchors.verticalCenter: parent.verticalCenter
                    visible: isFolder && hasChildren
                    text: childCount
                    color: "#858585"
                    font.pixelSize: 11
                }

                MouseArea {
                    id: mouseArea
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: {
                        // 点击即收起悬停提示（对应 imgui 每帧 IsItemHovered 判断：点击后 hover 变 false 提示消失；
                        // dismiss 立即隐藏并短暂抑制，防止条目重建 onEntered 立刻又 show）
                        appToolTip.dismiss(mouseArea)
                        if (isFolder) {
                            moduleTreeModel.toggleExpanded(index)
                        } else {
                            // 对应 Tree_RenderUISidebar: IsItemClicked -> AddModule
                            moduleTreeModel.addModule(index)
                        }
                    }

                    // ===== Hover 提示（统一用全局 appToolTip，对应 ImGui SetTooltip） =====
                    // 用 onEntered/onExited：相邻 delegate 切换时 onContainsMouseChanged 可能
                    // 不重新触发导致新模块提示不显示（旧 hide 后新 show 丢失）
                    // 传 mouseArea 作为 source，抵御切换时滞后的 onExited 误关提示
                    onEntered: {
                        if (!isFolder && descLong.length > 0) {
                            var g = mapToGlobal(width / 2, height + 4)
                            appToolTip.show(descLong, g.x, g.y, mouseArea)
                        }
                    }
                    onExited: {
                        appToolTip.hide(mouseArea)
                    }

                    // 拖拽支持：左键按住模块节点拖动到工作区
                    drag.target: !isFolder ? dragHelper : null
                    drag.threshold: 8

                    onPressed: {
                        if (!isFolder) {
                            dragHelper.dragModuleKey = moduleKey
                            dragHelper.dragModuleName = name
                        }
                    }
                    onReleased: (mouse) => {
                        // 手动拖放：Qt Drag/DropArea 在此环境不可靠（拖拽源被列表 clip，装配不投递到 DropArea），
                        // 改在松手时若确认发生拖拽且落点在工作区视口内，则在该坐标放置模块。
                        if (drag.active && !isFolder) {
                            var gp = mouseArea.mapToGlobal(mouse.x, mouse.y)
                            if (workspaceController.viewportContainsPoint(gp.x, gp.y)) {
                                var vp = workspaceController.globalToViewport(gp.x, gp.y)
                                moduleTreeModel.placeModuleByKey(moduleKey, vp.x, vp.y)
                                workspaceController.onDrop(vp.x, vp.y, moduleKey)
                            }
                        }
                    }
                }

                // 拖拽视觉辅助（跟随鼠标的小预览）
                Item {
                    id: dragHelper
                    // 始终可见：Drag.Automatic 启动真正拖拽要求拖拽源 Item 可见，
                    // 若初始 visible=false，拖拽不启动、DropArea 收不到投放（此前现象：无预览框、模块未出现）。
                    // 预览外观拿到内层 Rectangle 上按需显示即可。
                    visible: true
                    width: 120
                    height: 24
                    x: mouseArea.mouseX - width / 2
                    y: mouseArea.mouseY - height / 2
                    Drag.active: mouseArea.drag.active
                    Drag.dragType: Drag.Automatic
                    Drag.mimeData: {
                        "text/plain": moduleKey
                    }
                    Drag.supportedActions: Qt.CopyAction
                    Drag.proposedAction: Qt.CopyAction

                    Rectangle {
                        anchors.fill: parent
                        // 仅拖拽激活时显示蓝色预览框
                        visible: mouseArea.drag.active && !isFolder
                        color: "#007acc"
                        border.color: "#1e1e1e"
                        border.width: 1
                        radius: 3
                        opacity: 0.9

                        Text {
                            anchors.centerIn: parent
                            text: name
                            color: "#ffffff"
                            font.pixelSize: 11
                            elide: Text.ElideRight
                            width: parent.width - 8
                            horizontalAlignment: Text.AlignHCenter
                        }
                    }

                    property string dragModuleKey: ""
                    property string dragModuleName: ""
                }
            }
        }
    }
}
