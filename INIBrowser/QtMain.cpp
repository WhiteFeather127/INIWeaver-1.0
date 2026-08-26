// QtMain.cpp
// Qt6 + QML 入口点（替代 INIBrowser.cpp 的 wWinMain）
// 阶段 2：建立 Qt 骨架；阶段 4.1：注册 Controller 为 context property
// 阶段 6.1：QTimer 驱动主循环（替代 ShellLoop）
#include <QApplication>
#include <QClipboard>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QTimer>
#include <QThread>
#include <QElapsedTimer>
#include <atomic>
#include <string>
#include <QLoggingCategory>
#include <QQuickStyle>
#include <QFont>
#include <QFontDatabase>
#include <QEvent>
#include <QMouseEvent>
#include <QQuickItem>
#include <QQuickWindow>
#include <QSurfaceFormat>
#include <QScreen>
#include <QFileInfo>
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
                // 沿焦点链向上找输入控件（含父级）：
                // - TextInput / TextField → QQuickTextInput（TextField 继承 QQuickTextInput）
                // - TextEdit / TextArea → QQuickTextEdit（TextArea 继承 QQuickTextEdit）
                // - SpinBox → QQuickSpinBox（本体不继承输入类，需单独识别）
                // 用 metaObject 沿继承链检测 className
                QQuickItem* inputItem = nullptr;
                QQuickItem* node = focusItem;
                while (node && !inputItem) {
                    const QMetaObject* mo = node->metaObject();
                    bool isInput = false;
                    while (mo) {
                        const char* cn = mo->className();
                        if (strcmp(cn, "QQuickTextInput") == 0
                            || strcmp(cn, "QQuickTextEdit") == 0
                            || strcmp(cn, "QQuickSpinBox") == 0) {
                            isInput = true;
                            break;
                        }
                        mo = mo->superClass();
                    }
                    if (isInput) inputItem = node;
                    else node = node->parentItem();
                }
                if (inputItem) {
                    // 当前焦点在输入控件内，检查点击是否在其矩形外
                    QRectF itemRect = inputItem->mapRectToScene(inputItem->boundingRect());
                    QPointF clickPos = me->position();
                    bool outside = clickPos.x() < itemRect.x()
                                || clickPos.x() > itemRect.x() + itemRect.width()
                                || clickPos.y() < itemRect.y()
                                || clickPos.y() > itemRect.y() + itemRect.height();
                    if (outside) {
                        // 清焦点链：从 activeFocusItem 逐层到输入控件本体全部 setFocus(false)，
                        // 确保 SpinBox（焦点在其内部 TextField）整体失焦
                        QQuickItem* cur = focusItem;
                        while (cur) {
                            cur->setFocus(false);
                            if (cur == inputItem) break;
                            cur = cur->parentItem();
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
#include "IBR_Misc.h"
#include "qt/ProjectController.h"
#include "qt/ModuleTreeModel.h"
#include "qt/SectionListModel.h"
#include "qt/MenuController.h"
#include "qt/SettingController.h"
#include "cjson/cJSON.h"

#include "qt/LocalizationController.h"
#include "qt/DialogController.h"
#include "qt/WorkspaceController.h"
#include "qt/EditPanelController.h"
#include "qt/SelfTest.h"

// LoadDatabaseComplete 在 Initialize.cpp 中定义，未在头文件中声明
// IBR_ProjManager.cpp 的 Load() 内部会 while(!LoadDatabaseComplete) 阻塞等待
extern std::atomic_bool LoadDatabaseComplete;

// 从 Resources\config.json 读取 HotKeys，返回 动作名 -> QKeySequence 字符串。
// 对应 ImGui 的 Initialize::InitializeHotKeys（Initialize.cpp:153-158，Qt 版不执行 Stage_IV 故自行加载）。
// config 缺失/无效/某项为空时返回空 map（QML 采用默认键位兜底）。
static QVariantMap qtLoadHotKeyMap()
{
    QVariantMap map;
    QFile file(QStringLiteral(".\\Resources\\config.json"));
    if (!file.open(QIODevice::ReadOnly)) return map;
    const QByteArray raw = file.readAll();
    file.close();
    const std::string buf(raw.constData(), size_t(raw.size()));
    cJSON *root = cJSON_Parse(buf.c_str());
    if (!root) return map;
    cJSON *hot = cJSON_GetObjectItem(root, "HotKeys");
    if (hot && hot->type == cJSON_Object) {
        for (cJSON *it = hot->child; it; it = it->next) {
            const char *name = it->string;
            if (!name || it->type != cJSON_Array) continue;
            QStringList mods, keys;
            for (cJSON *k = it->child; k; k = k->next) {
                if (k->type != cJSON_String || !k->valuestring) continue;
                QString tok = QString::fromUtf8(k->valuestring);
                if (tok == QLatin1String("Ctrl")) mods << QLatin1String("Ctrl");
                else if (tok == QLatin1String("Shift")) mods << QLatin1String("Shift");
                else if (tok == QLatin1String("Alt")) mods << QLatin1String("Alt");
                else if (tok == QLatin1String("Super")) mods << QLatin1String("Meta");
                else keys << tok;
            }
            if (keys.isEmpty()) continue;
            QString seq = mods.join(QLatin1Char('+'));
            map.insert(QString::fromUtf8(name),
                       seq.isEmpty() ? keys.last() : (seq + QLatin1Char('+') + keys.last()));
        }
    }
    cJSON_Delete(root);
    return map;
}

int main(int argc, char* argv[])
{
    // 关闭 Qt Quick 渲染循环的 vsync（在 QApplication/渲染循环创建前设置）
    // 帧率不再被显示器刷新率锁死，允许更高帧率（代价：可能画面撕裂、GPU 占用升高）
    // 动画驱动：QSG_NO_VSYNC 关闭 vsync 后，默认动画驱动依赖渲染循环持续刷帧才推进，
    // 不再产生中间帧 → NumberAnimation 瞬跳。设置 QSG_USE_SIMPLE_ANIMATION_DRIVER=1
    //（官方建议，基于全局 QElapsedTimer），并配合 WorkspaceView.qml 在动画期间
    // 每帧 requestUpdate 强制持续渲染，使标准 QML 动画逐帧推进。
    qputenv("QSG_NO_VSYNC", "1");
    qputenv("QSG_USE_SIMPLE_ANIMATION_DRIVER", "1");
    // 双保险：QSurfaceFormat::swapInterval(0) 请求禁用 vsync（Qt 6.4+ 文档：与 QSG_NO_VSYNC
    // 等效，均请求渲染线程 swap 时不阻塞；对 D3D11 RHI 后端是否真正生效取决于驱动，见诊断日志）
    // 必须在 QGuiApplication / 窗口创建前设置默认格式
    {
        QSurfaceFormat fmt;
        fmt.setSwapInterval(0);
        // 8bit alpha 通道：视图窗口（miniMapWindow）用透明背景 + 圆角容器，
        // 无 alpha 时 QQuickWindow 透明背景（color: transparent）无法渲染，圆角窗口退化为方窗
        fmt.setAlphaBufferSize(8);
        QSurfaceFormat::setDefaultFormat(fmt);
    }

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
    // 注意：不能用 msg.toStdWString() —— 它返回 std::wstring，由 Qt DLL 的 CRT 分配、EXE 的 CRT 释放，
    // Debug 下两套 CRT 堆不一致会导致堆损坏（0xC0000374）。改用 Qt 自管内存的 toUtf8()。
    qInstallMessageHandler([](QtMsgType type, const QMessageLogContext &ctx, const QString &msg) {
        const QByteArray utf8 = msg.toUtf8();
        if (g_debugLog) {
            fprintf(g_debugLog, "[%lu] %s\n", GetCurrentThreadId(), utf8.constData());
            fflush(g_debugLog);
        }
        OutputDebugStringA(utf8.constData());
        OutputDebugStringA("\n");
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

    // 加载最近文件列表（对应 Initialize.cpp:728 IBR_RecentManager::Load）
    // 原版在 Stage_IV 中调用，Qt 版不执行 Stage_IV，否则启动后最近文件列表为空
    // 且首次 Push 后保存的文件内容无法在下次启动时读回
    IBR_RecentManager::Load();
    debugLog("RecentManager Load done");

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
    // 桥接 ImGui 剪贴板到 Qt 系统剪贴板（对应 GLFW 后端的 SetClipboardTextFn/GetClipboardTextFn）：
    // 复制/粘贴（IBR_WorkSpace::CopySelected/Paste、IBR_SectionData::CopyToClipBoard）经
    // ImGui::SetClipboardText/GetClipboardText 读写剪贴板；Qt 版无平台后端，不设置回调时
    // SetClipboardText 静默无效、GetClipboardText 返回空串 → 粘贴永远报"粘贴失败"。
    // 所有调用均在 GUI 线程（QML → Q_INVOKABLE → 主线程），QGuiApplication::clipboard() 线程安全。
    {
        ImGuiIO &imGuiIo = ImGui::GetIO();
        imGuiIo.SetClipboardTextFn = [](void *, const char *text) {
            if (!text) text = "";
            QGuiApplication::clipboard()->setText(QString::fromUtf8(text));
        };
        imGuiIo.GetClipboardTextFn = [](void *) -> const char * {
            // ImGui 期望返回指针在下一次调用前保持有效：用静态 QByteArray 缓存
            static QByteArray buf;
            buf = QGuiApplication::clipboard()->text().toUtf8();
            return buf.constData();
        };
    }
    debugLog("Triggering CallReadSetting");
    IBR_Inst_Setting.SetSettingName(SettingFileName);
    IBR_Inst_Setting.CallReadSetting();

    // 创建 Controller/Model 实例（桥接全局单例，不接管所有权）
    ProjectController projectController;
    ModuleTreeModel moduleTreeModel;
    SectionListModel sectionListModel;
    MenuController menuController;
    SettingController settingController;
    // 多语言翻译查询控制器（语言切换复用 SettingController，本控制器只提供 tr + rev 刷新）
    LocalizationController localizationController;
    localizationController.connectToLanguageSignal(&settingController);
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
    engine.rootContext()->setContextProperty("i18n", &localizationController);
    engine.rootContext()->setContextProperty("dialogController", &dialogController);
    engine.rootContext()->setContextProperty("workspaceController", &workspaceController);
    engine.rootContext()->setContextProperty("editPanelController", &editPanelController);
    // 可配置快捷键（从 Resources\config.json 的 HotKeys 读取；缺失时 QML 用默认键位）
    engine.rootContext()->setContextProperty("hotKeyMap", qtLoadHotKeyMap());
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
    QQuickWindow *rootWindow = nullptr;
    {
        rootWindow = qobject_cast<QQuickWindow*>(engine.rootObjects().first());
        if (rootWindow) {
            rootWindow->installEventFilter(new FocusBlurFilter(rootWindow));
            debugLog("FocusBlurFilter installed on root window");
            // 记录实际图形渲染后端：Qt 6 在 Windows 默认 D3D11（GPU 硬件加速），
            // 仅在驱动不可用/远程桌面等场景才回退到 Software 软件渲染
            const char *apiName = "unknown";
            switch (QQuickWindow::graphicsApi()) {
            case QSGRendererInterface::Direct3D11: apiName = "Direct3D11 (GPU)"; break;
            case QSGRendererInterface::Direct3D12: apiName = "Direct3D12 (GPU)"; break;
            case QSGRendererInterface::OpenGL:      apiName = "OpenGL (GPU)"; break;
            case QSGRendererInterface::Metal:       apiName = "Metal (GPU)"; break;
            case QSGRendererInterface::Vulkan:      apiName = "Vulkan (GPU)"; break;
            case QSGRendererInterface::Software:    apiName = "Software (CPU)"; break;
            case QSGRendererInterface::Null:        apiName = "Null (no rendering)"; break;
            default: break;
            }
            debugLog(QString("SceneGraph backend: %1 (QSG_NO_VSYNC=%2, QSG_USE_SIMPLE_ANIMATION_DRIVER=%3)")
                         .arg(apiName)
                         .arg(qEnvironmentVariableIsSet("QSG_NO_VSYNC") ? "1" : "0")
                         .arg(qEnvironmentVariableIsSet("QSG_USE_SIMPLE_ANIMATION_DRIVER") ? "1" : "0").toUtf8().constData());
            // 诊断：窗口所在屏幕的刷新率，用于区分 60fps 是 VSync 锁定（60Hz 屏）还是渲染耗时瓶颈
            if (rootWindow->screen()) {
                debugLog(QString("Screen refreshRate=%1Hz")
                             .arg(rootWindow->screen()->refreshRate()).toUtf8().constData());
            }
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

    // 全局渲染帧率节流（对齐 ImGui AdjustFrameRate 的软限帧）：
    // Qt 6.8.1 无 QQuickWindow::setFrameSwappedInterval（该 API 于 Qt 6.9 才引入），
    // 改在渲染线程的 beforeRendering 里按帧率间隔睡眠：SceneGraph 是"内容变化才渲染"，
    // 任何 UI 变化（画布/连线/侧边栏 hover/框选矩形/拖拽预览等）触发的每一帧渲染都
    // 经过此处被拉长到帧间隔 → 帧率上限**全局生效**（而非仅定时器驱动的画布数据刷新）。
    // 无内容变化时不渲染、不进入本回调，无额外开销。
    // 线程安全：frameIntervalUs 由 GUI 线程（设置变更）与渲染线程（每帧读取）共享，用原子。
    std::atomic<qint64> g_frameIntervalUs{0};   // 帧间隔（微秒），0=不限帧
    std::atomic<qint64> g_nextFrameUs{0};       // 下一帧允许渲染的时刻（相对时钟起点，微秒）
    QElapsedTimer frameClock;
    frameClock.start();  // 单调时钟，渲染线程只读 nsecsElapsed，跨线程安全

    auto applyFrameRateLimit = [&]() {
        int fps = settingController.frameRateLimit();
        int intervalMs = (fps <= 0) ? 0 : (1000 / fps);
        timer.setInterval(intervalMs);
        // 全局渲染节流间隔（微秒）；fps<=0（含 -1=不限）→ 0=不节流
        g_frameIntervalUs.store((fps <= 0) ? 0 : (1000000 / fps));
        // 帧率限制的实现说明（Qt 6.8.1 无 QQuickWindow::setFrameSwappedInterval，该 API 于 Qt 6.9 才引入）：
        // 1) QTimer（本定时器）按帧率设置驱动数据刷新：连线端点表/项目同步/节流后的拖拽位置
        //    （WorkspaceController 的拖拽/平移位置更新延迟到此 tick 统一应用，见 applyPendingDrag）。
        // 2) SceneGraph 渲染是"内容变化才渲染"，拖动画布/模块时的位置属性按帧率设置变化
        //    → 实际渲染帧率即受帧率设置限制（避免鼠标事件率驱动渲染跑满本机帧率）。
        // 3) beforeRendering 软节流（见下）把**全部**渲染统一限制到帧间隔，覆盖侧边栏/框选等
        //    不经过定时器数据节流的 UI 变化。
    };
    applyFrameRateLimit();
    // 全局字号缩放（对应 ImGui InitializeStyle 的 FontSize）：设置变化时重设 QApplication 字体
    auto applyUiFont = [&]() {
        QFont uiFont(QStringLiteral("Microsoft YaHei"));
        uiFont.setPointSizeF(double(9) * settingController.fontScale());
        QApplication::setFont(uiFont);
    };
    applyUiFont();
    QObject::connect(&settingController, &SettingController::settingsChanged, [&]() {
        applyFrameRateLimit();
        applyUiFont();
    });
    if (rootWindow) {
        QObject::connect(rootWindow, &QQuickWindow::beforeRendering,
                         [&frameClock, &g_frameIntervalUs, &g_nextFrameUs]() {
            qint64 intervalUs = g_frameIntervalUs.load();
            if (intervalUs <= 0) return;  // 不限帧
            qint64 nowUs = frameClock.nsecsElapsed() / 1000;
            qint64 targetUs = g_nextFrameUs.load();
            if (targetUs == 0) {
                // 首帧：规划下一帧目标时刻
                g_nextFrameUs.store(nowUs + intervalUs);
                return;
            }
            qint64 remainUs = targetUs - nowUs;
            if (remainUs > 0) {
                // 距上一帧不足帧间隔 → 渲染线程睡眠到目标时刻，本帧被拉长
                QThread::usleep(static_cast<unsigned int>(remainUs));
                nowUs = frameClock.nsecsElapsed() / 1000;
            }
            // 推进到下一帧目标；若严重落后（sleep 精度/卡顿）则跳到"现在+间隔"，避免积压追赶
            qint64 nextTarget = targetUs + intervalUs;
            if (nextTarget <= nowUs)
                g_nextFrameUs.store(nowUs + intervalUs);
            else
                g_nextFrameUs.store(nextTarget);
        });
    }
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

    // 启动时若无附加 .iproj 文件则自动新建项目（对应 Initialize.cpp:832-835
    // 数据库加载完成后无条件 CreateAction；Qt 版按需求仅"无附加文件"时新建）
    // 附加文件判定与 ProcessCommandLine 相同：命令行参数含存在的 .iproj 文件
    const QStringList cmdArgs = QApplication::arguments();
    bool hasAttachedProject = false;
    for (int i = 1; i < cmdArgs.size(); ++i) {
        QFileInfo fi(cmdArgs.at(i));
        if (fi.exists() && fi.suffix().compare("iproj", Qt::CaseInsensitive) == 0) {
            hasAttachedProject = true;
            break;
        }
    }
    if (!hasAttachedProject) {
        // 延迟到事件循环开始后执行：QML 已加载、save/front 线程已就绪，
        // CreateAction 同步创建空白项目（无弹窗副作用，Create() 见 IBR_ProjManager.cpp:74）
        QTimer::singleShot(0, &projectController, []() {
            IBR_ProjectManager::CreateAction();
        });
    }

    return app.exec();
}
