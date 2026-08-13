// Main.qml
// INIWeaver 主窗口：菜单栏 + 左侧面板区 + 中央工作区 + 底部提示栏
// 配色：VS Code 风格 (#1e1e1e/#2d2d2d/#3c3c3c/#007acc)
// 布局对应 ImGui MainStage.h ControlPanel()
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "./workspace"
import "./dialogs"

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
    onAfterRendering: _frameCount++
    Timer {
        interval: 1000
        repeat: true
        running: true
        onTriggered: {
            window.fpsValue = window._frameCount
            window._frameCount = 0
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

                // 面板宽度调整手柄
                Rectangle {
                    anchors.right: parent.right
                    anchors.top: parent.top
                    anchors.bottom: parent.bottom
                    width: 2
                    color: "#1e1e1e"
                    z: 1
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

    // 阶段 11.4：通用右键菜单（对应 IBR_PopupManager::SetRightClickMenu）
    // 由 rightClickMenuRequested 信号触发
    RightClickMenu {
        id: rightClickMenu
        parent: Overlay.overlay
    }

    // 等待/加载弹窗（监听 waitingShown / waitingHidden 信号）
    WaitingDialog {
        parent: Overlay.overlay
    }

    // 导出对话框
    ExportDialog {
        id: exportDialog
        parent: Overlay.overlay
        outputDir: settingController.outputDir
        // 监听 DialogController.exportRequested 信号
        Connections {
            target: dialogController
            function onExportRequested() {
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

    Connections {
        target: workspaceController
        function onModuleSearchRequested(x, y) {
            searchModuleDialog.open()
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
    onClosing: {
        close.accepted = false
        projectController.requestQuit()
    }
}
