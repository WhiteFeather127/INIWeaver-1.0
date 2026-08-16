// Main.qml
// INIWeaver 主窗口：菜单栏 + 左侧面板区 + 中央工作区 + 底部提示栏
// 配色：VS Code 风格 (#1e1e1e/#2d2d2d/#3c3c3c/#007acc)
// 布局对应 ImGui MainStage.h ControlPanel()
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Window
import "./workspace"
import "./dialogs"
import "./components"

ApplicationWindow {
    id: window
    visible: true
    width: 1280
    height: 800
    minimumWidth: 960
    minimumHeight: 600
    // 设置全局字体：Windows 上默认字体不含中文字符
    // QtMain.cpp 中 QApplication::setFont 已设置全局字体，此处保留作为 QML 层的双保险
    font.family: "Microsoft YaHei"
    // 对应 IBR_Misc.cpp:506-516 UpdateWindowTitle：AppName + " - " + ProjName + "[*]"
    title: (projectController.projectName.length > 0
            ? projectController.projectName + " - INIWeaver"
            : "INIWeaver - INI 织网者")
           + (projectController.isModified ? " [*]" : "")
    color: "#1e1e1e"

    // 全局点击失焦由 C++ 事件过滤器实现（QtMain.cpp FocusBlurFilter）
    // QML TapHandler 无法捕获被 nodeMouseArea.preventStealing 拦截的画布点击事件

    // 帧率统计：对应 ImGui::GetIO().Framerate（MainStage.h:167）
    // QQuickWindow::afterRendering 在每帧渲染完成后发射，1 秒内累计帧数即 FPS
    property int fpsValue: 0
    property int _frameCount: 0
    property real _lastFrameTime: 0
    property real _maxFrameGap: 0
    onAfterRendering: {
        _frameCount++
        var now = Date.now()
        if (_lastFrameTime > 0) {
            var gap = now - _lastFrameTime
            if (gap > _maxFrameGap) _maxFrameGap = gap
        }
        _lastFrameTime = now
    }
    Timer {
        interval: 1000
        repeat: true
        running: true
        onTriggered: {
            window.fpsValue = window._frameCount
            window._frameCount = 0
            window._maxFrameGap = 0
        }
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        // 顶部菜单栏（使用本地 AppMenuBar.qml，避免与 QtQuick.Controls 的 MenuBar 类型冲突）
        AppMenuBar {
            id: menuBar
            Layout.fillWidth: true
            Layout.preferredHeight: 32
            fpsValue: window.fpsValue
        }

        // 主内容区：左侧面板 + 中央工作区
        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 0

            // 左侧面板区：根据 activeMenu 切换面板内容
            Rectangle {
                id: panelArea
                Layout.preferredWidth: 280
                Layout.minimumWidth: 200
                Layout.maximumWidth: 500
                Layout.fillHeight: true
                color: "#2d2d2d"

                // 面板宽度调整手柄（拖拽改变面板宽度，范围受 Layout 最小/最大宽度钳制）
                MouseArea {
                    id: panelResizeHandle
                    anchors.right: parent.right
                    anchors.top: parent.top
                    anchors.bottom: parent.bottom
                    width: 6
                    z: 2
                    cursorShape: Qt.SizeHorCursor
                    hoverEnabled: true

                    // 拖拽状态：按下时记录起始全局 X 与面板宽度
                    property real startGlobalX: 0
                    property real startWidth: 0

                    // 视觉分隔条（半透明，hover/拖拽时高亮提示可调整）
                    Rectangle {
                        anchors.right: parent.right
                        anchors.top: parent.top
                        anchors.bottom: parent.bottom
                        width: 2
                        color: panelResizeHandle.containsMouse ? "#4a4a4a" : "#1e1e1e"
                    }

                    onPressed: (mouse) => {
                        // 注意：不能用 mouse.globalX（Qt 6.8 实测为 undefined，赋值报
                        // "Cannot assign [undefined] to double"）。改用 mapToGlobal 换算：
                        // 拖拽期间热区跟随面板右缘移动，mapToGlobal(mouse.x) 恰好还原
                        // 鼠标真实全局坐标（热区右缘 + 相对偏移 = 鼠标位置）。
                        var gp = mapToGlobal(mouse.x, mouse.y)
                        startGlobalX = gp.x
                        startWidth = panelArea.Layout.preferredWidth
                    }
                    onPositionChanged: (mouse) => {
                        if (!pressed) return
                        var gp = mapToGlobal(mouse.x, mouse.y)
                        var newW = startWidth + (gp.x - startGlobalX)
                        panelArea.Layout.preferredWidth = Math.max(
                            panelArea.Layout.minimumWidth,
                            Math.min(panelArea.Layout.maximumWidth, newW))
                    }
                    onReleased: {
                        // 拖拽结束：全量同步一次，补齐 LinkRenderer 兜底坐标
                        // （拖拽期间 setViewportSize 只做轻量更新，见 WorkspaceController.cpp 注释）
                        workspaceController.refresh()
                    }
                }

                // 根据 activeMenu 加载对应面板
                Loader {
                    id: panelLoader
                    anchors.fill: parent
                    source: {
                        switch (menuController.activeMenu) {
                            case 0: return "panels/FilePanel.qml"
                            case 1: return "panels/ModulesPanel.qml"
                            case 2: return "panels/ViewPanel.qml"
                            case 3: return "panels/ListPanel.qml"
                            case 4: return "panels/EditPanel.qml"
                            case 5: return "panels/SettingPanel.qml"
                            case 6: return "panels/AboutPanel.qml"
                            case 7: return "panels/DebugPanel.qml"
                            default: return ""
                        }
                    }
                }

                // 未加载面板时的占位文本
                Text {
                    anchors.centerIn: parent
                    visible: panelLoader.status != Loader.Ready
                    color: "#858585"
                    font.pixelSize: 14
                    text: qsTr("加载中...")
                }
            }

            // 中央工作区：WorkspaceView 组件
            WorkspaceView {
                id: workspace
                Layout.fillWidth: true
                Layout.fillHeight: true
            }
        }

        // 底部提示栏（固定 26px）
        StatusBar {
            id: statusBar
            Layout.fillWidth: true
            Layout.preferredHeight: 26
            Layout.minimumHeight: 26
            Layout.maximumHeight: 26
        }
    }

    // 独立"视图窗口"：无边框圆角浮窗（圆角 + 边框样式与模块相近），
    // 自定义标题条拖动 + 右下角调整大小，迷你地图占满标题条以下空间。
    // 由视图面板的"视图窗口"按钮打开（见 ViewPanel.openMiniMapWindow）
    Window {
        id: miniMapWindow
        flags: Qt.FramelessWindowHint | Qt.NoDropShadowWindowHint | Qt.Window
        width: 380
        height: 320
        minimumWidth: 220
        minimumHeight: 180
        color: "transparent"  // 透明背景，由下方圆角容器绘制（避免系统矩形阴影）
        visible: false

        // 圆角 + 边框容器（与模块相近：圆角 3 + #3c3c3c 1px 边框）
        Rectangle {
            anchors.fill: parent
            radius: 3
            color: "#1e1e1e"
            border.color: "#3c3c3c"
            border.width: 1
            clip: true  // 标题条/迷你地图裁剪进圆角

            ColumnLayout {
                anchors.fill: parent
                spacing: 0

                // 无边框窗口的自定义标题条：拖动移动窗口 + 关闭按钮
                // 圆角与容器一致（顶部两角融入窗口圆角，clip 双重保障裁剪）
                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 22
                    color: "#2d2d2d"
                    radius: 3

                    // 按住标题条任意位置拖动窗口（Qt 6 无边框窗口标准 API）
                    MouseArea {
                        anchors.fill: parent
                        onPressed: (mouse) => miniMapWindow.startSystemMove()
                    }

                    Text {
                        anchors.left: parent.left
                        anchors.leftMargin: 8
                        anchors.verticalCenter: parent.verticalCenter
                        text: qsTr("视图窗口")
                        color: "#cccccc"
                        font.pixelSize: 12
                    }

                    // 关闭按钮
                    Rectangle {
                        anchors.right: parent.right
                        anchors.verticalCenter: parent.verticalCenter
                        anchors.rightMargin: 4
                        width: 18
                        height: 18
                        radius: 2
                        color: closeArea.containsMouse ? "#3c3c3c" : "transparent"
                        MouseArea {
                            id: closeArea
                            anchors.fill: parent
                            hoverEnabled: true
                            onClicked: miniMapWindow.close()
                        }
                        Text {
                            anchors.centerIn: parent
                            text: "✕"
                            color: "#cccccc"
                            font.pixelSize: 10
                        }
                    }
                }

                // 迷你地图填满剩余空间（无内边距，最大占用）
                MiniMap {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    sections: workspaceController.sections
                    // 视图窗口不画画布边缘外框（侧边栏内嵌保留，见 ViewPanel）
                    showWorldBorder: false
                    // 画布大小与侧边栏同步（含视口，includeViewportInWorld=true）：
                    // 视图框拖出模块范围时画布随之扩展，保证视图框始终在画布内，
                    // 拖到迷你地图显示框边缘即停，不会出框被裁剪
                    includeViewportInWorld: true
                    viewRect: {
                        // 与 ViewPanel 同款：依赖 eqCenter/ratio，viewportEqRect 的 NOTIFY 只有
                        // workspaceRectChanged，平移/缩放后视图框必须跟随更新
                        workspaceController.eqCenter
                        workspaceController.ratio
                        var r = workspaceController.viewportEqRect
                        return { x: r.x, y: r.y, w: r.width, h: r.height }
                    }
                    eqCenter: workspaceController.eqCenter
                    // 阶段 13.4：背景拖拽状态（inputState==1 为 BgDragging）
                    isBgDragging: workspaceController.inputState === 1
                    // 点击迷你地图定位到对应坐标（对应 IBR_FullView::ChangeOffsetPos）
                    onClicked: (pos) => {
                        workspaceController.centerViewTo(pos.x, pos.y)
                    }
                }
            }
        }

        // 右下角调整大小手柄（无边框窗口无系统 resize 边框，需手动实现）
        MouseArea {
            id: resizeHandle
            width: 16
            height: 16
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            cursorShape: Qt.SizeFDiagCursor

            property real startW: 0
            property real startH: 0
            property real startGX: 0
            property real startGY: 0

            onPressed: (mouse) => {
                // 注意：不能直接用 mouse.globalX（Qt 6.8 实测 undefined），用 mapToGlobal 换算
                var gp = resizeHandle.mapToGlobal(mouse.x, mouse.y)
                startW = miniMapWindow.width
                startH = miniMapWindow.height
                startGX = gp.x
                startGY = gp.y
            }
            onPositionChanged: (mouse) => {
                if (!pressed) return
                var gp = resizeHandle.mapToGlobal(mouse.x, mouse.y)
                miniMapWindow.width = Math.max(
                    miniMapWindow.minimumWidth, startW + (gp.x - startGX))
                miniMapWindow.height = Math.max(
                    miniMapWindow.minimumHeight, startH + (gp.y - startGY))
            }
        }
    }

    // 视图面板"视图窗口"按钮 → 打开浮窗
    Connections {
        target: panelLoader.item
        function onOpenMiniMapWindow() {
            miniMapWindow.show()
        }
    }

    // 绑定 ProjectController.hintMessage -> statusBar.setHint
    // 用户操作反馈（如"已复制"），使用 3000ms 倒计时（对应 ImGui HintStayTimeMillis）
    Connections {
        target: projectController
        function onHintMessage(text) {
            statusBar.setHint(text, 0, 3000)
        }
    }

    // 绑定 DialogController.hintSet -> statusBar.setHint
    // 阶段 10.3：对应 ImGui IBR_HintManager::SetHint(text, -1)，-1 表示永不倒计时
    Connections {
        target: dialogController
        function onHintSet(text) {
            statusBar.setHint(text, 0, -1)
        }
        // 阶段 5 新增：带类型的 hint（对应 IBR_Components.cpp:788-895）
        // 阶段 10.3：透传 durationMs（-1=永不倒计时, >0=倒计时毫秒）
        function onHintSetWithType(text, type, durationMs) {
            statusBar.setHint(text, type, durationMs)
        }
    }

    // 快捷键声明（对应 IBR_HotKey.cpp 22 个快捷键）
    Shortcuts {
        id: shortcuts
    }

    // 文件拖放（对应 IBR_ProjectManager::OnDropFile）
    DropArea {
        anchors.fill: parent
        onDropped: (drop) => {
            if (drop.hasUrls) {
                var paths = []
                for (var i = 0; i < drop.urls.length; i++) {
                    paths.push(drop.urls[i].toString())
                }
                projectController.onDropFiles(paths)
                drop.acceptProposedAction()
            }
        }
    }

    // ----------------------------------------------------------------
    // 弹窗实例（对应 DialogController 信号）
    // ----------------------------------------------------------------

    // 通用确认弹窗
    ConfirmDialog {
        id: confirmDialog
        parent: Overlay.overlay
        // 监听 DialogController.confirmRequested 信号
        Connections {
            target: dialogController
            function onConfirmRequested(title, message, actionId) {
                confirmDialog.title = title
                confirmDialog.messageText = message
                confirmDialog.actionId = actionId
                confirmDialog.open()
            }
        }
    }

    // 阶段 11.3：三态确认弹窗（保存/不保存/取消）
    // 对应 ImGui AskIfSave（IBR_ProjManager.cpp:221-249）
    ConfirmDialog3 {
        id: confirmDialog3
        parent: Overlay.overlay
        Connections {
            target: dialogController
            function onConfirm3Requested(title, message, actionId) {
                confirmDialog3.title = title
                confirmDialog3.messageText = message
                confirmDialog3.actionId = actionId
                confirmDialog3.open()
            }
        }
    }

    // 阶段 11.2：错误队列弹窗（对应 IBR_ErrorCollector.cpp:50-97 ShowErrorPopupImpl）
    // 由 currentErrorChanged 信号触发，内部自动 open/close
    ErrorQueueDialog {
        id: errorQueueDialog
        parent: Overlay.overlay
    }

    // 阶段 11.4：统一右键菜单单例（对齐 ImGui IBR_PopupManager 单例架构）
    // 全局唯一右键菜单，所有右键入口（画布空白/多选、单节点、圆点、行、列表项、通用 hook）
    // 统一经 contextMenuHost.show(descs, screenX, screenY, handler) 提交内容。
    // 挂在 ApplicationWindow 根级，全组件树经动态作用域共享该 id。
    ContextMenuHost {
        id: contextMenuHost
        parent: Overlay.overlay
    }

    // 通用右键菜单 hook（对应 IBR_PopupManager::SetRightClickMenu 转发）
    // 由 dialogController.rightClickMenuRequested(title, items, posX, posY) 信号触发（屏幕坐标）
    property bool rightClickMenuActive: false
    Connections {
        target: dialogController
        function onRightClickMenuRequested(title, items, posX, posY) {
            // items 字符串数组 → itemDescs（action = 下标，触发时回传 index）
            var descs = []
            for (var i = 0; i < items.length; ++i)
                descs.push({ type: "item", text: items[i], action: String(i) })
            window.rightClickMenuActive = true
            contextMenuHost.show(descs, posX, posY, (action) => {
                dialogController.notifyRightClickMenuResult(parseInt(action, 10))
            })
        }
        function onRightClickMenuCleared() {
            contextMenuHost.hide()
        }
    }

    // 通用 hook 菜单关闭：未触发时通知 -1=取消（对应 ImGui ClearRightClickMenu 后返回 -1）
    // 触发项后 hide 也会走这里，DialogController 侧做幂等处理
    Connections {
        target: contextMenuHost
        function onMenuClosed() {
            if (!window.rightClickMenuActive) return
            window.rightClickMenuActive = false
            if (dialogController.hasRightClickMenu) {
                dialogController.notifyRightClickMenuResult(-1)
            }
        }
    }

    // 等待/加载弹窗（监听 waitingShown / waitingHidden 信号）
    WaitingDialog {
        parent: Overlay.overlay
    }

    // 导出对话框
    ExportDialog {
        id: exportDialog
        parent: Overlay.overlay
        // 监听 DialogController.exportRequested 信号；打开前先从业务层拉取
        // 输出目录与按 INI 类型的默认文件名（对应 ImGui 版 OutputAction 的数据准备）
        Connections {
            target: dialogController
            function onExportRequested() {
                exportDialog.loadData()
                exportDialog.open()
            }
        }
    }

    // 导入 INI 对话框
    ImportIniDialog {
        id: importIniDialog
        parent: Overlay.overlay
        // 监听 DialogController.importRequested 信号
        Connections {
            target: dialogController
            function onImportRequested(path) {
                importIniDialog.initialPath = path
                importIniDialog.open()
            }
        }
    }

    // 阶段 11.1：导入 INI 预览弹窗（对应 IBR_ImportPreview::Open）
    // 由 dialogController.importPreviewRequested 信号触发
    // 用户在 ImportIniDialog 中选择路径后，C++ 解析 INI 文件，
    // 通过 IBR_ImportPreview::SetHook 转发到 DialogController.onImportPreviewRequested，
    // 再发出 importPreviewRequested 信号通知此处打开预览弹窗
    ImportPreviewDialog {
        id: importPreviewDialog
        parent: Overlay.overlay
        Connections {
            target: dialogController
            function onImportPreviewRequested() {
                importPreviewDialog.open()
            }
        }
    }

    // 通用消息弹窗（对应 IBR_PopupManager::MessageModal）
    // 由 popupRequested 信号触发，用于显示错误/环路检测/导出失败等弹窗
    MessageDialog {
        id: messageDialog
        parent: Overlay.overlay
    }

    // 阶段 13.2：模块搜索弹窗（对应 SearchModuleAlt::RenderUI）
    // 由双击空白触发 workspaceController.moduleSearchRequested 信号
    SearchModuleDialog {
        id: searchModuleDialog
        parent: Overlay.overlay
    }

    // 导出模块弹窗（对应 ImGui GUI_ExportModule → OutputSelected）
    // 由多选右键菜单"导出模块"触发 workspaceController.outputModuleRequested 信号
    ExportModuleDialog {
        id: exportModuleDialog
        parent: Overlay.overlay
    }

    Connections {
        target: workspaceController
        function onModuleSearchRequested(x, y) {
            searchModuleDialog.open()
        }
        function onOutputModuleRequested() {
            exportModuleDialog.open()
        }
    }

    // 阶段 13.3.2：SHP 文件类型选择弹窗（对应 IBR_ProjManager.cpp:1129-1217）
    // 由 dialogController.shpTypeSelectionRequested 信号触发
    ShpTypeDialog {
        id: shpTypeDialog
        parent: Overlay.overlay
    }

    // 窗口关闭流程：退出整个应用（而非仅关闭项目）
    // 对应 ImGui 单文档模式：关闭窗口=退出应用
    // 有未保存修改时弹三态确认（保存/不保存/取消）
    onClosing: (close) => {
        close.accepted = false
        projectController.requestQuit()
    }
}
