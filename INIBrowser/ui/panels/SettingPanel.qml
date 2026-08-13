// SettingPanel.qml
// 设置菜单面板，对应 IBR_Inst_Setting.RenderUI()
// 布局：常用设置（深色模式/帧率限制/自动换行阈值/保存时导出/导出后打开文件夹）
//       + 设置项列表（按 type 渲染可编辑控件）
//       + 底部 DescLong 显示区（对应 IBR_Setting.cpp:112-114 BeginChildFrame(113003)）
//       + 自动保存定时器（对应 IBR_Setting.cpp:55-60 每 5 秒 CallSaveSetting）
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ScrollView {
    id: root
    clip: true
    contentWidth: availableWidth

    // 阶段 6.2：当前 hover 的设置项 DescLong（对应 IBR_Setting.cpp:108 DescLong）
    property string currentDescLong: ""

    // 缓存 settingTypes() 返回值，避免 Repeater 重建时重复调用
    // 仅在 settingsChanged/languagesChanged 信号触发时重新拉取
    property var _settingTypes: settingController.settingTypes()

    Connections {
        target: settingController
        function onSettingsChanged() { root._settingTypes = settingController.settingTypes() }
        function onLanguagesChanged() { root._settingTypes = settingController.settingTypes() }
    }

    // 阶段 6.2：自动保存定时器（对应 IBR_Setting.cpp:55-60 每 5 秒 CallSaveSetting）
    Timer {
        interval: 5000
        repeat: true
        running: true
        onTriggered: settingController.saveSettings()
    }

    ColumnLayout {
        width: root.availableWidth
        spacing: 8

        // 标题
        Text {
            Layout.leftMargin: 8
            Layout.topMargin: 8
            text: qsTr("常用设置")
            color: "#cccccc"
            font.pixelSize: 13
            font.bold: true
        }

        // 深色模式开关
        Rectangle {
            Layout.fillWidth: true
            Layout.leftMargin: 8
            Layout.rightMargin: 8
            Layout.preferredHeight: 36
            color: "#2d2d2d"
            radius: 3

            Text {
                anchors.left: parent.left
                anchors.leftMargin: 12
                anchors.verticalCenter: parent.verticalCenter
                text: qsTr("深色模式")
                color: "#cccccc"
                font.pixelSize: 13
            }

            Switch {
                id: darkModeSwitch
                anchors.right: parent.right
                anchors.rightMargin: 8
                anchors.verticalCenter: parent.verticalCenter
                checked: settingController.darkMode
                onToggled: settingController.setDarkMode(checked)

                indicator: Rectangle {
                    implicitWidth: 36
                    implicitHeight: 18
                    x: darkModeSwitch.leftPadding
                    y: darkModeSwitch.topPadding + darkModeSwitch.availableHeight / 2 - height / 2
                    radius: 9
                    color: darkModeSwitch.checked ? "#007acc" : "#3c3c3c"
                    border.color: "#1e1e1e"

                    Rectangle {
                        x: darkModeSwitch.checked ? parent.width - width - 3 : 3
                        y: parent.height / 2 - height / 2
                        width: 12; height: 12
                        radius: 6
                        color: "#ffffff"
                    }
                }
            }
        }

        // 帧率限制
        Rectangle {
            Layout.fillWidth: true
            Layout.leftMargin: 8
            Layout.rightMargin: 8
            Layout.preferredHeight: 52
            color: "#2d2d2d"
            radius: 3

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 8
                spacing: 2

                RowLayout {
                    Layout.fillWidth: true
                    Text {
                        text: qsTr("帧率限制")
                        color: "#cccccc"
                        font.pixelSize: 12
                    }
                    Item { Layout.fillWidth: true }
                    Text {
                        text: settingController.frameRateLimit + " FPS"
                        color: "#858585"
                        font.pixelSize: 12
                    }
                }

                Slider {
                    id: fpsSlider
                    Layout.fillWidth: true
                    from: 15; to: 200
                    value: settingController.frameRateLimit
                    stepSize: 5
                    onMoved: settingController.frameRateLimit = Math.round(value)

                    background: Rectangle {
                        x: fpsSlider.leftPadding
                        y: fpsSlider.topPadding + fpsSlider.availableHeight / 2 - height / 2
                        implicitWidth: 200; implicitHeight: 3
                        width: fpsSlider.availableWidth; height: implicitHeight
                        radius: 2; color: "#3c3c3c"
                        Rectangle {
                            width: fpsSlider.visualPosition * parent.width
                            height: parent.height; radius: 2; color: "#007acc"
                        }
                    }
                    handle: Rectangle {
                        x: fpsSlider.leftPadding + fpsSlider.visualPosition * (fpsSlider.availableWidth - width)
                        y: fpsSlider.topPadding + fpsSlider.availableHeight / 2 - height / 2
                        implicitWidth: 12; implicitHeight: 12; radius: 6
                        color: fpsSlider.pressed ? "#ffffff" : "#cccccc"
                        border.width: 1; border.color: "#007acc"
                    }
                }
            }
        }

        // 保存时导出
        Rectangle {
            Layout.fillWidth: true
            Layout.leftMargin: 8
            Layout.rightMargin: 8
            Layout.preferredHeight: 36
            color: "#2d2d2d"
            radius: 3

            Text {
                anchors.left: parent.left; anchors.leftMargin: 12
                anchors.verticalCenter: parent.verticalCenter
                text: qsTr("保存时导出")
                color: "#cccccc"; font.pixelSize: 13
            }

            Switch {
                id: outputOnSaveSwitch
                anchors.right: parent.right; anchors.rightMargin: 8
                anchors.verticalCenter: parent.verticalCenter
                checked: settingController.outputOnSave
                onToggled: settingController.outputOnSave = checked

                indicator: Rectangle {
                    implicitWidth: 36; implicitHeight: 18
                    x: outputOnSaveSwitch.leftPadding
                    y: outputOnSaveSwitch.topPadding + outputOnSaveSwitch.availableHeight / 2 - height / 2
                    radius: 9
                    color: outputOnSaveSwitch.checked ? "#007acc" : "#3c3c3c"
                    border.color: "#1e1e1e"
                    Rectangle {
                        x: outputOnSaveSwitch.checked ? parent.width - width - 3 : 3
                        y: parent.height / 2 - height / 2
                        width: 12; height: 12; radius: 6; color: "#ffffff"
                    }
                }
            }
        }

        // 导出后打开文件夹
        Rectangle {
            Layout.fillWidth: true
            Layout.leftMargin: 8
            Layout.rightMargin: 8
            Layout.preferredHeight: 36
            color: "#2d2d2d"
            radius: 3

            Text {
                anchors.left: parent.left; anchors.leftMargin: 12
                anchors.verticalCenter: parent.verticalCenter
                text: qsTr("导出后打开文件夹")
                color: "#cccccc"; font.pixelSize: 13
            }

            Switch {
                id: openFolderSwitch
                anchors.right: parent.right; anchors.rightMargin: 8
                anchors.verticalCenter: parent.verticalCenter
                checked: settingController.openFolderOnOutput
                onToggled: settingController.openFolderOnOutput = checked

                indicator: Rectangle {
                    implicitWidth: 36; implicitHeight: 18
                    x: openFolderSwitch.leftPadding
                    y: openFolderSwitch.topPadding + openFolderSwitch.availableHeight / 2 - height / 2
                    radius: 9
                    color: openFolderSwitch.checked ? "#007acc" : "#3c3c3c"
                    border.color: "#1e1e1e"
                    Rectangle {
                        x: openFolderSwitch.checked ? parent.width - width - 3 : 3
                        y: parent.height / 2 - height / 2
                        width: 12; height: 12; radius: 6; color: "#ffffff"
                    }
                }
            }
        }

        // 自动换行阈值
        Rectangle {
            Layout.fillWidth: true
            Layout.leftMargin: 8; Layout.rightMargin: 8
            Layout.preferredHeight: 52
            color: "#2d2d2d"; radius: 3

            ColumnLayout {
                anchors.fill: parent; anchors.margins: 8; spacing: 2

                RowLayout {
                    Layout.fillWidth: true
                    Text {
                        text: qsTr("自动换行阈值")
                        color: "#cccccc"; font.pixelSize: 12
                    }
                    Item { Layout.fillWidth: true }
                    Text {
                        text: settingController.autoWrapThreshold
                        color: "#858585"; font.pixelSize: 12
                    }
                }

                Slider {
                    id: wrapSlider
                    Layout.fillWidth: true
                    from: 10; to: 200
                    value: settingController.autoWrapThreshold
                    stepSize: 1
                    onMoved: settingController.autoWrapThreshold = Math.round(value)

                    background: Rectangle {
                        x: wrapSlider.leftPadding
                        y: wrapSlider.topPadding + wrapSlider.availableHeight / 2 - height / 2
                        implicitWidth: 200; implicitHeight: 3
                        width: wrapSlider.availableWidth; height: implicitHeight
                        radius: 2; color: "#3c3c3c"
                        Rectangle {
                            width: wrapSlider.visualPosition * parent.width
                            height: parent.height; radius: 2; color: "#007acc"
                        }
                    }
                    handle: Rectangle {
                        x: wrapSlider.leftPadding + wrapSlider.visualPosition * (wrapSlider.availableWidth - width)
                        y: wrapSlider.topPadding + wrapSlider.availableHeight / 2 - height / 2
                        implicitWidth: 12; implicitHeight: 12; radius: 6
                        color: wrapSlider.pressed ? "#ffffff" : "#cccccc"
                        border.width: 1; border.color: "#007acc"
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

        // 设置项列表标题
        Text {
            Layout.leftMargin: 8
            text: qsTr("所有设置项")
            color: "#cccccc"; font.pixelSize: 13; font.bold: true
        }

        // 阶段 6.2：设置项列表（按 type 渲染可编辑控件，对应 IBR_Setting.cpp:86-93）
        Repeater {
            model: root._settingTypes
            delegate: Rectangle {
                Layout.fillWidth: true
                Layout.leftMargin: 8; Layout.rightMargin: 8
                // Bool/Int 类型高度更大，其余紧凑
                Layout.preferredHeight: modelData.type === 3 ? 36 : (modelData.type === 1 || modelData.type === 2 ? 52 : 40)
                color: "#2d2d2d"; radius: 3

                // hover 时更新底部 DescLong（对应 IBR_Setting.cpp:90-91 DescLong = Li.DescLong）
                MouseArea {
                    anchors.fill: parent
                    hoverEnabled: true
                    onContainsMouseChanged: {
                        if (containsMouse) root.currentDescLong = modelData.descLong || ""
                        else root.currentDescLong = ""
                    }
                    // 不拦截点击事件，传递给子控件
                    propagateComposedEvents: true
                }

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 8
                    spacing: 2

                    // 描述行
                    RowLayout {
                        Layout.fillWidth: true
                        Text {
                            text: modelData.descShort
                            color: "#cccccc"; font.pixelSize: 12
                            Layout.fillWidth: true
                        }

                        // 阶段 6.2：按 type 渲染编辑控件
                        // type: 0=None, 1=IntA, 2=IntB, 3=Bool, 4=Lang

                        // Bool 类型：Switch
                        Switch {
                            visible: modelData.type === 3
                            checked: modelData.value || false
                            onToggled: settingController.setSettingValue(modelData.name, checked)

                            indicator: Rectangle {
                                implicitWidth: 32; implicitHeight: 16
                                x: parent.leftPadding
                                y: parent.topPadding + parent.availableHeight / 2 - height / 2
                                radius: 8
                                color: parent.checked ? "#007acc" : "#3c3c3c"
                                border.color: "#1e1e1e"
                                Rectangle {
                                    x: parent.checked ? parent.width - width - 2 : 2
                                    y: parent.height / 2 - height / 2
                                    width: 10; height: 10; radius: 5
                                    color: "#ffffff"
                                }
                            }
                        }

                        // Int 类型：SpinBox（对应 ImGui InputInt）
                        SpinBox {
                            visible: modelData.type === 1 || modelData.type === 2
                            value: modelData.value || 0
                            from: (modelData.limits && modelData.limits.length >= 1) ? modelData.limits[0] : -9999
                            to: (modelData.limits && modelData.limits.length >= 2) ? modelData.limits[1] : 9999
                            onValueModified: settingController.setSettingValue(modelData.name, value)
                            editable: true

                            contentItem: TextField {
                                text: parent.textFromValue(parent.value, parent.locale)
                                color: "#d4d4d4"
                                font.pixelSize: 12
                                horizontalAlignment: Text.AlignHCenter
                                verticalAlignment: Text.AlignVCenter
                                selectByMouse: true
                                readOnly: !parent.editable
                                background: Rectangle {
                                    color: "#1e1e1e"
                                    border.color: parent.activeFocus ? "#007acc" : "#3c3c3c"
                                    border.width: 1; radius: 2
                                }
                            }

                            up.indicator: Rectangle {
                                x: parent.mirrored ? 0 : parent.width - width
                                height: parent.height
                                implicitWidth: 20
                                color: parent.up.pressed ? "#3c3c3c" : "#2d2d2d"
                                border.color: "#3c3c3c"; border.width: 1
                                Text {
                                    anchors.centerIn: parent
                                    text: "+"
                                    color: "#cccccc"; font.pixelSize: 12
                                }
                            }
                            down.indicator: Rectangle {
                                x: parent.mirrored ? parent.width - width : 0
                                height: parent.height
                                implicitWidth: 20
                                color: parent.down.pressed ? "#3c3c3c" : "#2d2d2d"
                                border.color: "#3c3c3c"; border.width: 1
                                Text {
                                    anchors.centerIn: parent
                                    text: "−"
                                    color: "#cccccc"; font.pixelSize: 12
                                }
                            }
                            background: Rectangle {
                                implicitWidth: 80; implicitHeight: 24
                                color: "#1e1e1e"
                                border.color: "#3c3c3c"; border.width: 1; radius: 2
                            }
                        }

                        // 阶段 13.1：Lang 类型语言切换 ComboBox（对应 IBR_L10n::RenderUI 的 Combo）
                        ComboBox {
                            visible: modelData.type === 4
                            // 数据模型：settingController.availableLanguages() 返回 [{key, displayName}]
                            model: settingController.availableLanguages()
                            // 当前显示文本：当前语言的 LangName
                            displayText: settingController.currentLanguageName
                            // 选项显示文本
                            textRole: "displayName"
                            // 宽度
                            implicitWidth: 140
                            // 选中时切换语言
                            onActivated: function(index) {
                                var lang = model[index]
                                if (lang && lang.key)
                                    settingController.setLanguage(lang.key)
                            }

                            // 自定义样式（VS Code 暗色主题）
                            contentItem: Text {
                                leftPadding: 8
                                rightPadding: indicatorArrow.width + 12
                                text: parent.displayText
                                color: "#d4d4d4"
                                font.pixelSize: 12
                                verticalAlignment: Text.AlignVCenter
                                elide: Text.ElideRight
                            }
                            background: Rectangle {
                                implicitWidth: 140
                                implicitHeight: 24
                                color: parent.pressed ? "#3c3c3c" : "#1e1e1e"
                                border.color: parent.pressed ? "#007acc" : "#3c3c3c"
                                border.width: 1
                                radius: 2
                            }
                            indicator: Text {
                                id: indicatorArrow
                                x: parent.width - width - 8
                                y: parent.topPadding + (parent.availableHeight - height) / 2
                                text: "▾"
                                color: "#cccccc"
                                font.pixelSize: 10
                            }
                            // 下拉项样式
                            delegate: ItemDelegate {
                                width: parent ? parent.width : 140
                                height: 28
                                contentItem: Text {
                                    text: modelData.displayName
                                    color: parent.highlighted ? "#ffffff" : "#d4d4d4"
                                    font.pixelSize: 12
                                    verticalAlignment: Text.AlignVCenter
                                    leftPadding: 8
                                }
                                background: Rectangle {
                                    color: parent.highlighted ? "#007acc" : "transparent"
                                }
                            }
                            // 弹出层样式
                            popup: Popup {
                                y: parent.height
                                width: 160
                                implicitHeight: contentItem.implicitHeight
                                padding: 1
                                background: Rectangle {
                                    color: "#252526"
                                    border.color: "#3c3c3c"
                                    border.width: 1
                                }
                                contentItem: ListView {
                                    clip: true
                                    implicitHeight: contentHeight
                                    model: parent.parent ? parent.parent.model : null
                                    currentIndex: parent.parent ? parent.parent.currentIndex : -1
                                    delegate: parent.parent ? parent.parent.delegate : null
                                }
                            }
                        }

                        // None 类型：无编辑控件
                        Text {
                            visible: modelData.type === 0
                            text: qsTr("(只读)")
                            color: "#5a5a5a"; font.pixelSize: 11
                        }
                    }

                    // DescLong（Int 类型在第二行显示）
                    Text {
                        visible: (modelData.type === 1 || modelData.type === 2) && modelData.descLong
                        text: modelData.descLong
                        color: "#858585"; font.pixelSize: 11
                        elide: Text.ElideRight
                        Layout.fillWidth: true
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

        // 阶段 6.2：底部 DescLong 显示区（对应 IBR_Setting.cpp:112-114 BeginChildFrame(113003)）
        Rectangle {
            Layout.fillWidth: true
            Layout.leftMargin: 8; Layout.rightMargin: 8
            Layout.preferredHeight: Math.max(48, descLongText.implicitHeight + 16)
            color: "#1e1e1e"; radius: 3; border.color: "#3c3c3c"; border.width: 1

            Text {
                id: descLongText
                anchors.fill: parent
                anchors.margins: 8
                text: root.currentDescLong.length > 0 ? root.currentDescLong : qsTr("将鼠标悬停在设置项上查看详细说明")
                color: root.currentDescLong.length > 0 ? "#d4d4d4" : "#5a5a5a"
                font.pixelSize: 11
                wrapMode: Text.Wrap
                verticalAlignment: Text.AlignTop
            }
        }

        Item { Layout.fillHeight: true; Layout.preferredHeight: 16 }
    }
}
