// StyledButton.qml
// 通用按钮组件：统一 VS Code 暗色主题样式
// 替代各 Panel 中重复的 background/contentItem 样式定义
// 颜色规则（对齐现有实现）：
//   enabled:  hover=#3c3c3c, normal=#2d2d2d, border=#3c3c3c
//   disabled: bg=#252525, border=transparent, text=#5a5a5a
//   text enabled: #cccccc
import QtQuick
import QtQuick.Controls

Button {
    id: control

    // 可选自定义属性：通过 accentColor 切换强调色（用于 ShowRegName 等切换按钮）
    property color accentColor: "#007acc"
    property bool accent: false  // accent=true 时启用强调色边框/文字
    property color textColor: accent ? accentColor : "#cccccc"

    // 横向左右 padding：使没显式设置宽度的按钮 implicitWidth 按文字内容自适应
    // （implicitWidth = 文本宽 + 左右 padding），文字时长时不短、短时不空。
    // 仅设置水平方向，避免改动垂直 default padding 影响高度（各列表按钮高度固定 22）
    leftPadding: 8
    rightPadding: 8

    background: Rectangle {
        color: control.enabled
               ? (control.hovered ? "#3c3c3c" : "#2d2d2d")
               : "#252525"
        border.color: control.enabled
                      ? (control.accent ? control.accentColor : "#3c3c3c")
                      : "transparent"
        border.width: 1
        radius: 3
    }

    contentItem: Text {
        text: control.text
        color: control.enabled ? control.textColor : "#5a5a5a"
        font: control.font
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
    }
}
