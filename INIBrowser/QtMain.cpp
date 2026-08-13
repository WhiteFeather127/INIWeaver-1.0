// QtMain.cpp
// Qt6 + QML 入口点（替代 INIBrowser.cpp 的 wWinMain）
// 阶段 2：建立 Qt 骨架；阶段 4.1：注册 Controller 为 context property
// 阶段 6.1：QTimer 驱动主循环（替代 ShellLoop）
#include <QApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QTimer>
#include <QThread>
#include <QLoggingCategory>
#include <QQuickStyle>
#include <QFont>
#include <QFontDatabase>
#include <QEvent>
#include <QMouseEvent>
#include <QQuickItem>
#include <QQuickWindow>
#include <cstdio>
#include <Windows.h>
#include <mmsystem.h>  // timeBeginPeriod 提升系统定时器分辨率（winmm.lib）
#include "imgui.h"
#include "Initialize.h"

// 全局点击失焦过滤器：点击 TextField 外部时清除焦点
// QML TapHandler 无法捕获被 nodeMouseArea.preventStealing 拦截的画布点击事件，
// 用 QApplication 级事件过滤器在事件分发前拦截 QMouseEvent
class FocusBlurFilter : public QObject {
public:
    explicit FocusBlurFilter(QObject* parent = nullptr) : QObject(parent) {}
protected:
    bool eventFilter(QObject* watched, QEvent* event) override {
        // 仅处理鼠标按下事件（MouseButtonPress）
        if (event->type() == QEvent::MouseButtonPress) {
            auto me = static_cast<QMouseEvent*>(event);
            auto win = qobject_cast<QQuickWindow*>(watched);
            if (win) {
                QQuickItem* focusItem = win->activeFocusItem();
                if (focusItem) {
                    // 检查 focusItem 是否为 TextInput（QQuickTextInput）或 TextField
                    // QQuickTextInput 有 cursorPosition 属性，用 metaObject 检测
                    const QMetaObject* mo = focusItem->metaObject();
                    bool isInput = false;
                    while (mo) {
                        if (strcmp(mo->className(), "QQuickTextInput") == 0) {
                            isInput = true;
                            break;
                        }
                        mo = mo->superClass();
                    }
                    if (isInput) {
                        // 当前焦点是输入框，检查点击是否在输入框矩形外
                        QRectF itemRect = focusItem->mapRectToScene(focusItem->boundingRect());
                        QPointF clickPos = me->position();
                        bool outside = clickPos.x() < itemRect.x()
                                    || clickPos.x() > itemRect.x() + itemRect.width()
                                    || clickPos.y() < itemRect.y()
                                    || clickPos.y() > itemRect.y() + itemRect.height();
                        if (outside) {
                            // 让输入框失去焦点：setFocus(false) 在 FocusScope 中生效
                            // QML 的 ApplicationWindow 是 FocusScope，setFocus(false) 可清除 activeFocus
                            focusItem->setFocus(false);
                        }
                    }
                }
            }
        }
        return QObject::eventFilter(watched, event);
    }
};

// 调试日志：写入文件，用于定位崩溃位置
static FILE* g_debugLog = nullptr;
static void debugLog(const char* msg)
{
    if (g_debugLog) {
        fprintf(g_debugLog, "[%lu] %s\n", GetCurrentThreadId(), msg);
        fflush(g_debugLog);
    }
    OutputDebugStringA(msg);
    OutputDebugStringA("\n");
}
#include "IBB_RegType.h"
#include "IBFront.h"
#include "IBB_ModuleAlt.h"
#include "Global.h"
#include "IBR_Components.h"
#include "IBR_ErrorCollector.h"
#include "IBR_ImportIni.h"
#include "qt/ProjectController.h"
#include "qt/ModuleTreeModel.h"
#include "qt/SectionListModel.h"
#include "qt/MenuController.h"
#include "qt/SettingController.h"
#include "qt/DialogController.h"
#include "qt/WorkspaceController.h"
#include "qt/EditPanelController.h"
#include "qt/SelfTest.h"

// LoadDatabaseComplete 在 Initialize.cpp 中定义，未在头文件中声明
// IBR_ProjManager.cpp 的 Load() 内部会 while(!LoadDatabaseComplete) 阻塞等待
extern std::atomic_bool LoadDatabaseComplete;

int main(int argc, char* argv[])
{
    // 关闭 Qt Quick 渲染循环的 vsync（在 QApplication/渲染循环创建前设置）
    // 帧率不再被显示器刷新率锁死，允许更高帧率（代价：可能画面撕裂、GPU 占用升高）
    qputenv("QSG_NO_VSYNC", "1");

    // 提高 Windows 系统定时器分辨率到 1ms（默认 15.6ms ≈ 64Hz）
    // 否则 QTimer interval=0 最快只能 ~60FPS，拖动/缩放帧率被锁死
    // 对应 ImGui Initialize.cpp:327-337 AdjustFrameRate 的软件限帧前提
    timeBeginPeriod(1);

    // -selftest 模式：跳过 QML 加载，直接运行模拟点击测试
    bool selfTestMode = false;
    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "-selftest") == 0) selfTestMode = true;
    }

    // 打开调试日志文件（写入工作目录）
    g_debugLog = fopen("iniweaver_debug.log", "w");
    // 安装消息处理器：把 qDebug 输出重定向到 debug log 文件（selftest 模式下捕获测试详细输出）
    qInstallMessageHandler([](QtMsgType type, const QMessageLogContext &ctx, const QString &msg) {
        if (g_debugLog) {
            fprintf(g_debugLog, "[%lu] %s\n", GetCurrentThreadId(), msg.toUtf8().constData());
            fflush(g_debugLog);
        }
        OutputDebugStringW(msg.toStdWString().c_str());
        OutputDebugStringW(L"\n");
    });
    debugLog(selfTestMode ? "=== INIWeaver selftest starting ===" : "=== INIWeaver Qt starting ===");

    debugLog("before QApplication");
    QApplication app(argc, argv);
    debugLog("after QApplication");

    // 设置 Qt Quick Controls 样式为 Basic，支持自定义 Button/MenuItem background
    // 不设置会使用 Windows 原生样式，导致 QML 中自定义 background/contentItem 被忽略
    QQuickStyle::setStyle("Basic");
    debugLog("after QQuickStyle::setStyle");

    // 设置全局字体：Windows 上默认字体不含中文字符，部分 Text 会回退到楷体
    // Main.qml 的 font.family 只能设单个字体名，Loader/ToolTip 等可能不继承窗口字体
    // 在此用 QApplication::setFont 确保所有 QML 元素统一使用微软雅黑
    QFont appFont("Microsoft YaHei");
    appFont.setPointSize(9);
    QApplication::setFont(appFont);
    debugLog("after QApplication::setFont");

    HINSTANCE hInstance = GetModuleHandle(NULL);
    LPWSTR lpCmdLine = GetCommandLineW();

    // 阶段1：路径/命令行/语言/日志/分辨率
    debugLog("before Initialize_Stage_I");
    Initialize::Initialize_Stage_I(hInstance, lpCmdLine);
    debugLog("after Initialize_Stage_I");

    // 同步加载模块数据库（阶段 6 改为通过前端线程异步加载）
    // 顺序：RegisterTypes -> TypeAlt -> Modules（存在依赖关系）
    debugLog("before LoadFromFile RegisterTypes");
    IBB_DefaultRegType::LoadFromFile(L".\\Global\\RegisterTypes*.json");
    debugLog("after LoadFromFile RegisterTypes");
    IBF_Inst_DefaultTypeList.ReadAltSetting(L".\\Global\\TypeAlt*");
    debugLog("after ReadAltSetting");
    IBB_ModuleAltDefault::Load(
        L".\\Global\\Modules\\*.*",
        L".\\Global\\ImageModules\\*.*",
        L"\\Global\\Modules\\",
        L".\\Global\\SpecialModules\\*.*"
    );
    debugLog("after IBB_ModuleAltDefault::Load");

    // 标记数据库加载完成（对应 Initialize.cpp:209 CallInitializeDatabase 中的设置）
    // IBR_ProjManager.cpp:204 Load() 内部有 while(!LoadDatabaseComplete)Sleep(0) 死循环
    // 不设置会导致 OpenRecentAction -> IBS_Push -> Load 永远阻塞在 save thread
    LoadDatabaseComplete = true;
    debugLog("LoadDatabaseComplete set");

    // 阶段3：启动 save thread 和 front thread（对应 Initialize.cpp:675 Initialize_Stage_III）
    // save thread (IBS_Thr_SaveLoop)：处理 IBS_Push 推入的任务（项目加载/保存等）
    // front thread (IBF_Thr_FrontLoop)：处理 IBRF_CoreBump.SendToF 推入的 F 队列消息
    // 不启动这两个线程会导致 OpenRecentAction 内部的 IBS_Push(Load) 永远不执行
    debugLog("Starting Stage III (save/front threads)");
    Initialize::Initialize_Stage_III();
    debugLog("Stage III done");

    // 阶段 IV 关键部分：触发设置异步加载（CallReadSetting 通过 SendToF 推入 front thread）
    // Qt 版不调用完整 Stage_IV（含 ImGui/GLFW 初始化，需要 Stage_II 的 GLFW 窗口）
    // 但必须触发 ReadSetting，否则 SettingLoadComplete 永远 false，transparencyBase=0
    // 同时需要创建 ImGui 上下文：F→R 消息处理（IBRF_CoreBump.IBR_AutoProc）可能调用 ImGui API
    // 缺少上下文会导致 ACCESS_VIOLATION（selftest 的 waitMs 中驱动 IBRF_CoreBump.IBR_AutoProc 时崩溃）
    debugLog("Creating ImGui context (for F->R message processing)");
    ImGui::CreateContext();
    ImGui::GetIO().IniFilename = NULL;
    ImGui::GetIO().LogFilename = NULL;
    debugLog("Triggering CallReadSetting");
    IBR_Inst_Setting.SetSettingName(SettingFileName);
    IBR_Inst_Setting.CallReadSetting();

    // 创建 Controller/Model 实例（桥接全局单例，不接管所有权）
    ProjectController projectController;
    ModuleTreeModel moduleTreeModel;
    SectionListModel sectionListModel;
    MenuController menuController;
    SettingController settingController;
    DialogController dialogController;
    WorkspaceController workspaceController;
    EditPanelController editPanelController;

    // 注入 EditPanelController 和 MenuController 给 WorkspaceController
    // 选中模块时 WorkspaceController 调用 editPanelController.setActive 并切到 EditPanel
    workspaceController.setEditPanelController(&editPanelController);
    workspaceController.setMenuController(&menuController);

    // 连接：EditPanel 修改值后，通知 WorkspaceController 刷新对应 sectionId 的画布行模型
    // 避免 EditPanel 改值后画布上 SectionNode 的键显示不同步
    QObject::connect(&editPanelController, &EditPanelController::sectionDataChanged,
                     &workspaceController, &WorkspaceController::refreshSectionLines);

    // 连接：语言切换后通知 ProjectController 刷新 appName 等本地化属性
    // 对应 IBR_L10n::SetLanguage 后 QML 绑定 locc("AppName") 的属性需重新求值
    QObject::connect(&settingController, &SettingController::languagesChanged,
                     &projectController, &ProjectController::languageChanged);

    // 注入 DialogController 给 ProjectController，支持异步确认弹窗
    // 当 DialogController.confirmCompleted 信号触发时回调 ProjectController.onConfirmResult
    projectController.setDialogController(&dialogController);
    QObject::connect(&dialogController, &DialogController::confirmCompleted,
                     &projectController, &ProjectController::onConfirmResult);
    // 阶段 11.3：三态确认信号连接（保存/不保存/取消）
    QObject::connect(&dialogController, &DialogController::confirm3Completed,
                     &projectController, &ProjectController::onConfirmResult3);

    // 阶段 3：注册 IBR_PopupManager Hook，将 ImGui 弹窗请求转发到 QML
    // 当 C++ 侧调用 IBR_PopupManager::SetCurrentPopup 时，钩子触发
    // DialogController 通过 popupRequested 信号通知 QML 显示对应弹窗
    IBR_PopupManager::SetPopupHook([&dialogController](const IBR_PopupManager::Popup &p) {
        QString title = QString::fromUtf8(p.Title);
        // 阶段 11.5：空 Title 表示 ClearCurrentPopup 通知
        if (title.isEmpty()) {
            QMetaObject::invokeMethod(&dialogController, "onPopupCleared",
                Qt::QueuedConnection);
            return;
        }
        QStringList texts;
        for (const auto &t : p.Texts) {
            texts.append(QString::fromUtf8(t));
        }
        // v3 批次 2.2：读取 Size 字段（ImVec2，{0,0} 表示自动尺寸，QML 用默认值兜底）
        QSizeF size(static_cast<qreal>(p.Size.x), static_cast<qreal>(p.Size.y));
        // 通过 QMetaObject::invokeMethod 确保线程安全（钩子可能在渲染线程触发）
        QMetaObject::invokeMethod(&dialogController, "onPopupHooked",
            Qt::QueuedConnection,
            Q_ARG(QString, title),
            Q_ARG(QStringList, texts),
            Q_ARG(bool, p.Modal),
            Q_ARG(bool, p.CanClose),
            Q_ARG(QSizeF, size));
    });

    // 阶段 11.4：注册右键菜单 Hook，将 IBR_PopupManager::SetRightClickMenu 转发到 QML
    // 对应 IBR_Components.cpp:544-554 SetRightClickMenu 中新增的 g_rightClickMenuHook 调用
    IBR_PopupManager::SetRightClickMenuHook([&dialogController](const IBR_PopupManager::Popup &p) {
        QString title = QString::fromUtf8(p.Title);
        // 空 Title 表示 ClearRightClickMenu 通知
        if (title.isEmpty()) {
            QMetaObject::invokeMethod(&dialogController, "onRightClickMenuCleared",
                Qt::QueuedConnection);
            return;
        }
        QStringList items;
        for (const auto &t : p.Texts) {
            items.append(QString::fromUtf8(t));
        }
        // 右键菜单位置（屏幕坐标）
        int posX = static_cast<int>(IBR_PopupManager::RightClickMenuPos.x);
        int posY = static_cast<int>(IBR_PopupManager::RightClickMenuPos.y);
        QMetaObject::invokeMethod(&dialogController, "onRightClickMenuHooked",
            Qt::QueuedConnection,
            Q_ARG(QString, title),
            Q_ARG(QStringList, items),
            Q_ARG(int, posX),
            Q_ARG(int, posY));
    });

    // 阶段 11.2：注册错误收集器 Hook，将 IBB_ErrorReport 上报的错误转发到 QML 错误队列
    // 对应 IBR_ErrorCollector.cpp:51-54 AddErrorPopup 中新增的 g_errorHook 调用
    IBR_ErrorCollector::SetErrorHook([&dialogController](const std::string &title,
                                                          const std::string &errorStr,
                                                          const std::string &info,
                                                          bool forJson) {
        QMetaObject::invokeMethod(&dialogController, "onErrorPushed",
            Qt::QueuedConnection,
            Q_ARG(QString, QString::fromUtf8(title)),
            Q_ARG(QString, QString::fromUtf8(errorStr)),
            Q_ARG(QString, QString::fromUtf8(info)),
            Q_ARG(bool, forJson));
    });

    // 阶段 11.1：注册导入预览 Hook，将 IBR_ImportPreview::Open 转发到 QML 预览弹窗
    // 对应 IBR_ImportIni.cpp:39-46 Open() 中新增的 g_qtHook 调用
    // Hook 在渲染线程被调用，通过 shared_ptr + invokeMethod(lambda) 安全切到 GUI 线程
    IBR_ImportPreview::SetHook([&dialogController](ImportedIniFile &&file,
                                                    const std::function<void(const IBR_ImportResult &)> &callback) {
        // 使用 shared_ptr 确保 file 和 callback 在跨线程传递期间存活
        auto pFile = std::make_shared<ImportedIniFile>(std::move(file));
        auto pCallback = std::make_shared<std::function<void(const IBR_ImportResult &)>>(callback);
        QMetaObject::invokeMethod(&dialogController, [&dialogController, pFile, pCallback]() {
            dialogController.onImportPreviewRequested(std::move(*pFile), *pCallback);
        }, Qt::QueuedConnection);
    });

    // 模块树加载完成后刷新模型
    debugLog("refreshing moduleTreeModel");
    moduleTreeModel.refresh();
    debugLog("refreshing workspaceController");
    workspaceController.refresh();
    debugLog("refreshing menuController debugMenu");

    // 命令行解析完成后刷新 debugMenuEnabled（对应 -debugmenu 参数）
    menuController.refreshDebugMenu();

    // ===== selftest 模式：跳过 QML，直接运行测试 =====
    if (selfTestMode) {
        debugLog("selftest mode: waiting for setting load...");
        // 等待设置异步加载完成（ReadSetting 在 front thread 中执行）
        for (int i = 0; i < 100 && !SettingLoadComplete.load(); ++i) {
            QThread::msleep(50);
        }
        settingController.refresh();
        moduleTreeModel.refresh();
        workspaceController.refresh();
        // 等待 save/front 线程就绪
        QThread::msleep(500);
        debugLog("selftest mode: running tests...");
        int failures = runSelfTests(workspaceController, moduleTreeModel,
                                     settingController, projectController);
        debugLog(QString("selftest done, failures=%1").arg(failures).toUtf8().constData());
        return failures > 0 ? 1 : 0;
    }

    debugLog("creating QML engine");

    // QML 主窗口
    QQmlApplicationEngine engine;
    debugLog("registering context properties");

    // 注册为 context property，供 QML 全局访问
    engine.rootContext()->setContextProperty("projectController", &projectController);
    engine.rootContext()->setContextProperty("moduleTreeModel", &moduleTreeModel);
    engine.rootContext()->setContextProperty("sectionListModel", &sectionListModel);
    engine.rootContext()->setContextProperty("menuController", &menuController);
    engine.rootContext()->setContextProperty("settingController", &settingController);
    engine.rootContext()->setContextProperty("dialogController", &dialogController);
    engine.rootContext()->setContextProperty("workspaceController", &workspaceController);
    engine.rootContext()->setContextProperty("editPanelController", &editPanelController);
    debugLog("loading QML Main.qml");

    // 捕获 QML 警告/错误到日志文件
    QObject::connect(&engine, &QQmlApplicationEngine::warnings, [](const QList<QQmlError> &warnings) {
        for (const auto &w : warnings) {
            QString msg = QString("[QML WARNING] %1:%2:%3 %4")
                .arg(w.url().toString())
                .arg(w.line())
                .arg(w.column())
                .arg(w.description());
            debugLog(msg.toUtf8().constData());
        }
    });

    // 注：loadFromModule 在打包后 qmldir 查找失败，暂用直接 URL 加载
    engine.load(QUrl("qrc:/INIWeaver/INIBrowser/ui/Main.qml"));
    debugLog(QString("QML root objects count: %1").arg(engine.rootObjects().size()).toUtf8().constData());
    if (engine.rootObjects().isEmpty())
        return -1;
    debugLog("QML loaded OK, starting timer");

    // 全局点击失焦：在 QML 根窗口安装事件过滤器
    // 点击 TextField 外部时清除输入框焦点（画布、面板、对话框等所有区域通用）
    {
        auto rootWindow = qobject_cast<QQuickWindow*>(engine.rootObjects().first());
        if (rootWindow) {
            rootWindow->installEventFilter(new FocusBlurFilter(rootWindow));
            debugLog("FocusBlurFilter installed on root window");
        }
    }

    // 阶段 6.1：QTimer 驱动主循环（替代 ShellLoop_Unprotected）
    // 定时处理 F->R 消息队列 + 热键检查 + 工作区刷新
    // 帧率上限对应 ImGui Initialize.cpp:327-337 AdjustFrameRate：
    //   - 读 IBF_Inst_Setting.FrameRateLimit()（设置项默认 25，范围 15~2000，-1=不限）
    //   - interval = 1000 / FrameRateLimit（ms）
    //   - -1 时 interval=0（QTimer 0 表示尽快触发，但仍受 Qt 事件循环 + VSync 限制）
    QTimer timer;
    // PreciseTimer：配合 timeBeginPeriod(1)，让 interval=0 的 QTimer 突破 ~60Hz 系统节拍限制
    // 默认 CoarseTimer 会与系统 15.6ms 节拍合并，拖动/缩放时帧率上不去
    timer.setTimerType(Qt::PreciseTimer);
    auto applyFrameRateLimit = [&]() {
        int fps = settingController.frameRateLimit();
        int intervalMs = (fps <= 0) ? 0 : (1000 / fps);
        timer.setInterval(intervalMs);
    };
    applyFrameRateLimit();
    QObject::connect(&settingController, &SettingController::settingsChanged, [&]() { applyFrameRateLimit(); });
    QObject::connect(&timer, &QTimer::timeout, [&]() {
        // 处理核心消息队列（对应 MainStage.h:147 IBRF_CoreBump.IBR_AutoProc()）
        // 这会执行所有通过 SendToR 发送的消息：项目打开/保存/导出、Section 操作等
        IBRF_CoreBump.IBR_AutoProc();

        // v3 批次 2.1 修复：设置异步加载完成后刷新 SettingController（仅一次）
        // ReadSetting 在 front/save thread 中异步执行，QML 引擎创建时 WindowTransparencyLevel 可能还是 0
        // 导致 transparencyBase 返回 0，节点 opacity=0 不可见
        static bool settingRefreshed = false;
        if (!settingRefreshed && SettingLoadComplete.load()) {
            settingController.refresh();
            settingRefreshed = true;
            // 设置加载完成后立即应用 FrameRateLimit（默认 25，之前用 50ms=20FPS 占位）
            applyFrameRateLimit();
        }

        // 同步项目状态（IsProjectOpen 等在 SendToR lambda 中异步更新）
        projectController.refreshProperties();
        // 工作区 + Section 列表刷新（内部有脏标记检查，无变化时跳过全量重建）
        workspaceController.refreshFromTimer();
        sectionListModel.refreshFromTimer();
    });
    timer.start();

    return app.exec();
}
