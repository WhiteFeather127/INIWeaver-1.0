// FilePanel.qml
// 文件菜单面板，对应 IBR_Panel.cpp ControlPanel_File() (line 70-121)
// 按钮：保存/另存为/导出/关闭项目/打开项目/文件关联/导入INI + 最近文件列表
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

        // 第一行：保存 / 另存为 / 导出（各占 1/3）
        RowLayout {
            Layout.fillWidth: true
            Layout.leftMargin: 8
            Layout.rightMargin: 8
            spacing: 4

            StyledButton {
                Layout.fillWidth: true
                Layout.preferredHeight: 48
                text: qsTr("保存")
                enabled: projectController.isOpen
                font.pixelSize: 13
                onClicked: projectController.saveProject()
            }

            StyledButton {
                Layout.fillWidth: true
                Layout.preferredHeight: 48
                text: qsTr("另存为")
                enabled: projectController.isOpen
                font.pixelSize: 13
                onClicked: projectController.saveAsProject()
            }

            // 导出按钮：仅当 IBR_SectionMap 非空时启用（对应 ImGui 行 80-86）
            // 通过 dialogController.showExportDialog() 触发 QML 弹窗（替代原 ImGui 弹窗）
            StyledButton {
                Layout.fillWidth: true
                Layout.preferredHeight: 48
                text: qsTr("导出")
                enabled: projectController.canExport
                font.pixelSize: 13
                onClicked: dialogController.showExportDialog()
            }
        }

        // 第二行：关闭项目 / 打开项目（各占 1/2）
        RowLayout {
            Layout.fillWidth: true
            Layout.leftMargin: 8
            Layout.rightMargin: 8
            spacing: 4

            StyledButton {
                Layout.fillWidth: true
                Layout.preferredHeight: 48
                text: qsTr("关闭项目")
                enabled: projectController.isOpen
                font.pixelSize: 13
                onClicked: projectController.requestClose()
            }

            StyledButton {
                Layout.fillWidth: true
                Layout.preferredHeight: 48
                text: qsTr("新建项目")
                font.pixelSize: 13
                onClicked: projectController.newProject()
            }

            StyledButton {
                Layout.fillWidth: true
                Layout.preferredHeight: 48
                text: qsTr("打开项目")
                font.pixelSize: 13
                onClicked: projectController.openProject()
            }
        }

        // 文件关联按钮（占满整行）
        StyledButton {
            Layout.fillWidth: true
            Layout.leftMargin: 8
            Layout.rightMargin: 8
            Layout.preferredHeight: 36
            text: qsTr("设置文件关联")
            font.pixelSize: 13
            onClicked: projectController.setFileAssociation()
        }

        // 导入 INI 按钮（占满整行）
        // 通过 dialogController.showImportDialog() 触发 QML 弹窗（替代原 ImGui 弹窗）
        StyledButton {
            Layout.fillWidth: true
            Layout.leftMargin: 8
            Layout.rightMargin: 8
            Layout.preferredHeight: 36
            text: qsTr("导入 INI")
            enabled: projectController.isOpen
            font.pixelSize: 13
            onClicked: dialogController.showImportDialog("")
        }

        // 分隔线
        Rectangle {
            Layout.fillWidth: true
            Layout.leftMargin: 8
            Layout.rightMargin: 8
            Layout.preferredHeight: 1
            color: "#1e1e1e"
        }

        // 最近文件区域
        Text {
            Layout.leftMargin: 8
            Layout.rightMargin: 8
            text: qsTr("最近文件")
            color: "#cccccc"
            font.pixelSize: 13
            font.bold: true
        }

        // 清空最近文件按钮（列表为空时禁用）
        StyledButton {
            Layout.fillWidth: true
            Layout.leftMargin: 8
            Layout.rightMargin: 8
            Layout.preferredHeight: 28
            text: qsTr("清空最近文件")
            enabled: projectController.recentFiles.length > 0
            font.pixelSize: 12
            onClicked: projectController.clearRecentFiles()
        }

        // 最近文件列表
        // 高度由 model 数量 × delegate 高度直接计算，避免 contentItem.height
        // 在 delegate 异步创建时暂时为 0 导致列表塌缩
        ListView {
            id: recentList
            Layout.fillWidth: true
            Layout.leftMargin: 8
            Layout.rightMargin: 8
            Layout.preferredHeight: projectController.recentFiles.length * 36
            interactive: false
            model: projectController.recentFiles
            delegate: ItemDelegate {
                width: recentList.width
                height: 36

                StyledButton {
                    anchors.left: parent.left
                    anchors.verticalCenter: parent.verticalCenter
                    width: 48
                    height: 24
                    text: qsTr("打开")
                    font.pixelSize: 12
                    onClicked: projectController.openRecentProject(modelData)
                }

                Text {
                    anchors.left: parent.left
                    anchors.leftMargin: 56
                    anchors.right: parent.right
                    anchors.verticalCenter: parent.verticalCenter
                    text: modelData
                    color: "#cccccc"
                    font.pixelSize: 12
                    elide: Text.ElideMiddle
                }

                Rectangle {
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.bottom: parent.bottom
                    height: 1
                    color: "#1e1e1e"
                }
            }
        }

        Item { Layout.fillHeight: true }
    }
}
