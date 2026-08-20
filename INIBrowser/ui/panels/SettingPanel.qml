// SettingPanel.qml
// 设置菜单面板，对应 IBR_Inst_Setting.RenderUI()
// 布局：设置项列表（按 type 渲染可编辑控件）
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

        // 阶段 6.2：设置项列表（按 type 渲染可编辑控件，对应 IBR_Setting.cpp:86-93）
        Repeater {
            model: root._settingTypes
            delegate: Rectangle {
                Layout.fillWidth: true
                Layout.leftMargin: 8; Layout.rightMargin: 8
                // 紧凑单行高度（项内不显示说明小字，对应 ImGui 原版每项一行）
                Layout.preferredHeight: modelData.type === 3 ? 36 : 40
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

                        // Bool 类型：CheckBox（对应 ImGui Checkbox）
                        CheckBox {
                            visible: modelData.type === 3
                            checked: modelData.value || false
                            onToggled: settingController.setSettingValue(modelData.name, checked)

                            indicator: Rectangle {
                                implicitWidth: 16; implicitHeight: 16
                                x: parent.leftPadding
                                y: parent.topPadding + (parent.availableHeight - height) / 2
                                radius: 2
                                color: parent.checked ? "#007acc" : "#3c3c3c"
                                border.color: parent.checked ? "#007acc" : "#1e1e1e"
                                border.width: 1
                                Rectangle {
                                    x: 4; y: 4; width: 6; height: 6; radius: 1
                                    visible: parent.parent.checked
                                    color: "#ffffff"
                                }
                            }
                        }

                        // IntA 类型：SpinBox（对应 ImGui InputInt，可手输，支持 -1 特殊值）
                        // Limit 布局：IntA = {Def, Min, Max, SpV?, ...}
                        // 有 SpV（如帧率/自动换行的 -1）时 from 取 SpV，否则取 Min
                        SpinBox {
                            visible: modelData.type === 1
                            value: modelData.value || 0
                            from: (modelData.limits && modelData.limits.length >= 2)
                                ? ((modelData.limits.length >= 4 && modelData.limits[3] < modelData.limits[1])
                                    ? modelData.limits[3] : modelData.limits[1])
                                : -9999
                            to: (modelData.limits && modelData.limits.length >= 3) ? modelData.limits[2] : 9999
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

                        // IntB 类型：Slider（对应 ImGui SliderInt）
                        // Limit 布局：IntB = {Min, Max, Format}
                        Slider {
                            visible: modelData.type === 2
                            from: (modelData.limits && modelData.limits.length >= 1) ? modelData.limits[0] : 0
                            to: (modelData.limits && modelData.limits.length >= 2) ? modelData.limits[1] : 100
                            stepSize: 1
                            value: modelData.value || 0
                            implicitWidth: 140
                            onMoved: settingController.setSettingValue(modelData.name, Math.round(value))

                            background: Rectangle {
                                x: parent.leftPadding
                                y: parent.topPadding + parent.availableHeight / 2 - height / 2
                                implicitWidth: 140; implicitHeight: 4
                                width: parent.availableWidth; height: implicitHeight
                                radius: 2; color: "#3c3c3c"
                                Rectangle {
                                    width: parent.parent.visualPosition * parent.width
                                    height: parent.height; radius: 2
                                    color: "#007acc"
                                }
                            }
                            handle: Rectangle {
                                x: parent.leftPadding + parent.visualPosition * (parent.availableWidth - width)
                                y: parent.topPadding + parent.availableHeight / 2 - height / 2
                                implicitWidth: 14; implicitHeight: 14; radius: 7
                                color: parent.pressed ? "#ffffff" : "#cccccc"
                                border.width: 1; border.color: "#007acc"
                            }
                        }

                        // 阶段 13.1：Lang 类型语言切换 ComboBox（对应 IBR_L10n::RenderUI 的 Combo）
                        ComboBox {
                            id: langComboBox
                            visible: modelData.type === 4
                            // 数据模型：settingController.availableLanguages() 返回 [{key, displayName}]
                            model: settingController.availableLanguages()
                            // 当前显示文本：当前语言的 LangName
                            displayText: settingController.currentLanguageName
                            // 选项显示文本
                            textRole: "displayName"
                            // 宽度
                            implicitWidth: 140
                            // 选中项时切换语言（popup 为自定义 ListView，ComboBox 默认 activate 机制不会触发，故此处在 delegate 点击中显式调用）
                            function applyLanguage(index) {
                                var langs = settingController.availableLanguages()
                                var lang = (index >= 0 && index < langs.length) ? langs[index] : null
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
                                // 手动触发语言切换并关闭弹出层（自定义 popup 需显式处理点击）
                                onClicked: {
                                    langComboBox.currentIndex = index
                                    langComboBox.popup.close()
                                    langComboBox.applyLanguage(index)
                                }
                            }
                            // 弹出层样式（显式引用 langComboBox 的 model/delegate；此前误用 parent.parent 取到 overlay，导致 model 为 null 下拉无法打开）
                            popup: Popup {
                                y: langComboBox.height
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
                                    model: langComboBox.model
                                    currentIndex: langComboBox.currentIndex
                                    delegate: langComboBox.delegate
                                }
                            }
                        }

                        // None 类型：无编辑控件，灰字显示（对应 ImGui TextDisabled）
                        Text {
                            visible: modelData.type === 0
                            text: modelData.descShort
                            color: "#5a5a5a"; font.pixelSize: 11
                        }
                    }
                }
            }
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
