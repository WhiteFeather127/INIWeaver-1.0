// SearchModuleDialog.qml
// 阶段 13.2：模块搜索弹窗，对应 SearchModuleAlt::RenderUI（IBR_WorkSpace.cpp:138-178）
// 由 WorkspaceController.moduleSearchRequested(x, y) 信号触发显示
// 布局：三个 Consider 复选框 + 搜索输入框（回车触发） + 结果列表（点击添加模块）
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Dialog {
    id: root
    modal: true
    anchors.centerIn: parent
    width: 480
    height: 420
    padding: 0
    closePolicy: Dialog.CloseOnEscape | Dialog.CloseOnPressOutside
    title: (i18n.rev, i18n.tr("GUI_Search_Title"))

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

        // 关闭按钮
        Rectangle {
            anchors.right: parent.right
            anchors.rightMargin: 8
            anchors.verticalCenter: parent.verticalCenter
            width: 20; height: 20
            radius: 3
            color: closeMouse.containsMouse ? "#3c3c3c" : "transparent"
            Text {
                anchors.centerIn: parent
                text: "×"
                color: "#cccccc"
                font.pixelSize: 16
            }
            MouseArea {
                id: closeMouse
                anchors.fill: parent
                hoverEnabled: true
                onClicked: root.reject()
            }
        }
    }

    contentItem: ColumnLayout {
        anchors.fill: parent
        anchors.margins: 12
        spacing: 8

        // 阶段 13.2：三个 Consider 复选框（对应 IBR_WorkSpace.cpp:149-153）
        // ConsiderDescName / ConsiderRegName / ConsiderDesc 初始全为 true
        RowLayout {
            Layout.fillWidth: true
            spacing: 16

            // ConsiderDescName（GUI_Search_Name）
            CheckBox {
                id: considerDescName
                checked: true
                text: (i18n.rev, i18n.tr("GUI_Search_Name"))
                onToggled: doSearch()

                contentItem: Text {
                    text: parent.text
                    color: parent.checked ? "#007acc" : "#858585"
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
                        text: "✓"
                        color: "#ffffff"
                        font.pixelSize: 10
                    }
                }
            }

            // ConsiderRegName（GUI_Search_RegName）
            CheckBox {
                id: considerRegName
                checked: true
                text: (i18n.rev, i18n.tr("GUI_Search_RegName"))
                onToggled: doSearch()

                contentItem: Text {
                    text: parent.text
                    color: parent.checked ? "#007acc" : "#858585"
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
                        text: "✓"
                        color: "#ffffff"
                        font.pixelSize: 10
                    }
                }
            }

            // ConsiderDesc（GUI_Search_Desc）
            CheckBox {
                id: considerDesc
                checked: true
                text: (i18n.rev, i18n.tr("GUI_Search_Desc"))
                onToggled: doSearch()

                contentItem: Text {
                    text: parent.text
                    color: parent.checked ? "#007acc" : "#858585"
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
                        text: "✓"
                        color: "#ffffff"
                        font.pixelSize: 10
                    }
                }
            }

            Item { Layout.fillWidth: true }
        }

        // 搜索输入框（对应 ImGui InputText with EnterReturnsTrue + SearchIconBg）
        RowLayout {
            Layout.fillWidth: true
            spacing: 4

            // 搜索图标
            Text {
                text: "🔍"
                font.pixelSize: 12
                color: "#858585"
                Layout.alignment: Qt.AlignVCenter
            }

            TextField {
                id: searchInput
                Layout.fillWidth: true
                Layout.preferredHeight: 26
                placeholderText: (i18n.rev, i18n.tr("GUI_Search_Tip") + "...")
                color: "#d4d4d4"
                font.pixelSize: 12
                selectByMouse: true
                focus: true

                background: Rectangle {
                    color: "#1e1e1e"
                    border.color: searchInput.activeFocus ? "#007acc" : "#3c3c3c"
                    border.width: 1
                    radius: 3
                }

                onAccepted: doSearch()
            }
        }

        // 搜索提示（对齐 ImGui GUI_Search_Tip，值为"按回车搜索"）
        Text {
            Layout.fillWidth: true
            text: (i18n.rev, i18n.tr("GUI_Search_Tip"))
            color: "#858585"
            font.pixelSize: 11
            wrapMode: Text.Wrap
        }

        // 分隔线
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 1
            color: "#1e1e1e"
        }

        // 结果列表（对应 DoubleClickTable.RenderUI）
        ScrollView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true

            ListView {
                id: resultListView
                model: resultModel
                spacing: 1

                delegate: Rectangle {
                    width: resultListView.width
                    height: 32
                    color: delegateMouse.containsMouse ? "#3c3c3c" : "transparent"

                    Text {
                        anchors.left: parent.left
                        anchors.leftMargin: 12
                        anchors.verticalCenter: parent.verticalCenter
                        text: model.name
                        color: "#d4d4d4"
                        font.pixelSize: 12
                        elide: Text.ElideRight
                        width: parent.width - 24
                    }

                    MouseArea {
                        id: delegateMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        // DescLong 悬停提示（统一用全局 appToolTip，对应 IBR_ToolTip(pModule->DescLong)）
                        onContainsMouseChanged: {
                            if (delegateMouse.containsMouse && model.descLong && model.descLong.length > 0) {
                                var g = delegateMouse.mapToGlobal(delegateMouse.width / 2, delegateMouse.height + 4)
                                appToolTip.show(model.descLong, g.x, g.y)
                            } else {
                                appToolTip.hide()
                            }
                        }
                        onClicked: {
                            // 对应 RenderModuleAltSelect 中点击添加模块
                            if (model.moduleKey && model.moduleKey.length > 0) {
                                moduleTreeModel.addModuleByKey(model.moduleKey)
                            }
                        }
                    }
                }
            }
        }

        // 结果计数
        Text {
            Layout.fillWidth: true
            Layout.preferredHeight: 16
            text: resultModel.count > 0
                  ? (i18n.rev, i18n.trF("GUI_SearchResultCount", [resultModel.count]))
                  : (i18n.rev, i18n.tr("GUI_SearchNoResult"))
            color: "#858585"
            font.pixelSize: 11
        }
    }

    // 搜索结果数据模型
    ListModel {
        id: resultModel
    }

    // 执行搜索（对应 ImGui 中 ToUpdate=true 时调用 Update(Search(...))）
    // 搜索框为空时也执行搜索：底层 Search("") 的 find 恒匹配，返回所有模块（对应 ImGui）
    function doSearch() {
        var text = searchInput.text.trim()
        resultModel.clear()

        var results = moduleTreeModel.searchWithConsider(
            text, considerRegName.checked, considerDescName.checked, considerDesc.checked)
        for (var i = 0; i < results.length; i++) {
            resultModel.append(results[i])
        }
    }

    // 弹窗打开时自动聚焦搜索框
    onOpened: {
        searchInput.forceActiveFocus()
    }
}
