// FolderIcon.qml
// 黄色文件夹图标（Canvas 绘制，模仿 ImGui DrawFolderIcon / DrawOpenFolderIcon，IBR_ModuleTree.cpp:35-76）
// 关闭态：黄色主体 + 顶部短曲线；打开态：黄色主体 + 展开曲线
import QtQuick

Canvas {
    id: root
    implicitWidth: 14
    implicitHeight: 14
    // 是否展开（打开态图标）
    property bool open: false

    onPaint: {
        var _perf = workspaceController.diagLogEnabled()
        if (_perf) workspaceController.perfBegin("QML.FolderIcon.onPaint")
        var ctx = getContext("2d")
        ctx.clearRect(0, 0, width, height)
        var s = Math.min(width, height)
        // 文件夹主体（黄色 255,215,0，圆角 0.05s）
        ctx.fillStyle = "#ffd700"
        ctx.beginPath()
        var r = s * 0.05
        ctx.moveTo(r, s * 0.05)
        ctx.lineTo(s - r, s * 0.05)
        ctx.quadraticCurveTo(s, s * 0.05, s, s * 0.05 + r)
        ctx.lineTo(s, s * 0.8 - r)
        ctx.quadraticCurveTo(s, s * 0.8, s - r, s * 0.8)
        ctx.lineTo(r, s * 0.8)
        ctx.quadraticCurveTo(0, s * 0.8, 0, s * 0.8 - r)
        ctx.lineTo(0, s * 0.05 + r)
        ctx.quadraticCurveTo(0, s * 0.05, r, s * 0.05)
        ctx.fill()

        // 顶部曲线（棕色 227,161,50，线宽 0.1s）
        ctx.strokeStyle = "#e3a132"
        ctx.lineWidth = Math.max(1, s * 0.1)
        ctx.lineCap = "butt"
        if (root.open) {
            ctx.beginPath()
            ctx.moveTo(0, s * 0.65)
            ctx.lineTo(s * 0.05, s * 0.15)
            ctx.lineTo(s * 0.35, s * 0.15)
            ctx.lineTo(s * 0.55, 0)
            ctx.lineTo(s * 1.05, 0)
            ctx.lineTo(s, s * 0.65)
            ctx.stroke()
        } else {
            ctx.beginPath()
            ctx.moveTo(0, s * 0.15)
            ctx.lineTo(s * 0.3, s * 0.15)
            ctx.lineTo(s * 0.5, 0)
            ctx.lineTo(s, 0)
            ctx.stroke()
        }
        if (_perf) workspaceController.perfEnd()
    }

    onOpenChanged: requestPaint()
}
