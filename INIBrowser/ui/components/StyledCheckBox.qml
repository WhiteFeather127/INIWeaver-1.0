// StyledCheckBox.qml
// 通用复选框组件：统一 VS Code 暗色主题样式
// 替代各 Panel 中重复的 indicator/contentItem 样式定义
// 颜色规则（对齐现有实现）：
//   checked:  bg=#007acc, border=#007acc, mark=#ffffff
//   unchecked: bg=#1e1e1e, border=#3c3c3c
//   text checked: #007acc, unchecked: #858585
import QtQuick
import QtQuick.Controls

CheckBox {
    id: control

    // 可选自定义属性：通过 accentColor 切换选中色（如跟随 registerColor）
    property color accentColor: "#007acc"

    // 去掉默认 padding：indicator 位置由显式 anchors 控制，避免 padding 撑高/偏移导致
    // 勾选框与右侧文本/控件垂直错位（OnShow 开关、IIF bool、choice 选项共用本组件）
    padding: 0

    contentItem: Text {
        text: control.text
        color: control.checked ? control.accentColor : "#858585"
        font: control.font
        verticalAlignment: Text.AlignVCenter
        leftPadding: control.indicator.width + control.spacing
    }

    indicator: Rectangle {
        implicitWidth: 14
        implicitHeight: 14
        // 显式锚定：左侧 + 垂直居中，确保勾选框在控件内位置确定（不依赖样式默认值）
        anchors.left: parent.left
        anchors.verticalCenter: parent.verticalCenter
        radius: 2
        color: control.checked ? control.accentColor : "#1e1e1e"
        border.color: control.checked ? control.accentColor : "#3c3c3c"
        border.width: 1

        Text {
            anchors.centerIn: parent
            visible: control.checked
            text: "✓"
            color: "#ffffff"
            font.pixelSize: 10
        }
    }
}
