// ProjectController.h
// Qt6 项目控制器：暴露项目状态属性 + 委托 Action 给 IBR_ProjectManager
// 阶段 3.1：过渡期设计，保留原线程模型（IBS_Push/IBRF_CoreBump）
//           UI 弹窗暂用 QMessageBox/QFileDialog 同步替代，后续阶段改为异步信号
#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <QVariantMap>
#include <QtQmlIntegration/qqmlintegration.h>

class DialogController;

class ProjectController : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(bool isOpen READ isOpen NOTIFY isOpenChanged)
    Q_PROPERTY(QString projectName READ projectName NOTIFY projectNameChanged)
    Q_PROPERTY(QString projectPath READ projectPath NOTIFY projectPathChanged)
    Q_PROPERTY(bool isModified READ isModified NOTIFY isModifiedChanged)
    Q_PROPERTY(bool canUndo READ canUndo NOTIFY canUndoChanged)
    Q_PROPERTY(bool canRedo READ canRedo NOTIFY canRedoChanged)
    Q_PROPERTY(bool canExport READ canExport NOTIFY canExportChanged)
    Q_PROPERTY(QStringList recentFiles READ recentFiles NOTIFY recentFilesChanged)
    // ===== 阶段 8.1 新增：Debug 开关属性（对应 IBR_Inst_Debug 成员） =====
    // Ext 为 IBR_Debug.cpp:85 的 static bool，这里用 ProjectController 静态成员承载
    Q_PROPERTY(bool debugOutputExtra READ debugOutputExtra WRITE setDebugOutputExtra NOTIFY debugInfoChanged)
    Q_PROPERTY(bool useModuleProperties READ useModuleProperties WRITE setUseModuleProperties NOTIFY debugInfoChanged)
    Q_PROPERTY(bool showWorkspaceWindowFrame READ showWorkspaceWindowFrame WRITE setShowWorkspaceWindowFrame NOTIFY debugInfoChanged)
    Q_PROPERTY(bool dontGoToEdit READ dontGoToEdit WRITE setDontGoToEdit NOTIFY debugInfoChanged)
    Q_PROPERTY(bool dontDrawBg READ dontDrawBg WRITE setDontDrawBg NOTIFY debugInfoChanged)
    Q_PROPERTY(bool linkDebugMode READ linkDebugMode WRITE setLinkDebugMode NOTIFY debugInfoChanged)
    Q_PROPERTY(QString poolQueryBuf READ poolQueryBuf WRITE setPoolQueryBuf NOTIFY debugInfoChanged)
    Q_PROPERTY(QString lastQueryResult READ lastQueryResult NOTIFY debugInfoChanged)
    Q_PROPERTY(QString versionString READ versionString CONSTANT)
    // AppName 本地化（对应 locc("AppName")，MainStage.h:167 GUI_TopRightHint 第一个参数）
    Q_PROPERTY(QString appName READ appName NOTIFY languageChanged)
    // 阶段 8.3：ITD 输入表单是否打开（对应 IBR_TopMost::MenuMatchesSource(MenuItemID_DEBUG, 114514)）
    Q_PROPERTY(bool itdFormOpen READ itdFormOpen NOTIFY itdFormOpenChanged)

public:
    explicit ProjectController(QObject *parent = nullptr);

    // 注入 DialogController 以支持异步确认弹窗（替代原 QMessageBox 同步调用）
    void setDialogController(DialogController *dc) { m_dialogController = dc; }

    // 属性读取
    bool isOpen() const;
    QString projectName() const;
    QString projectPath() const;
    bool isModified() const;
    bool canUndo() const;
    bool canRedo() const;
    bool canExport() const;
    QStringList recentFiles() const;

    // ===== 阶段 8.1 新增：Debug 属性读取器 =====
    bool debugOutputExtra() const;
    bool useModuleProperties() const;
    bool showWorkspaceWindowFrame() const;
    bool dontGoToEdit() const;
    bool dontDrawBg() const;
    bool linkDebugMode() const;
    QString poolQueryBuf() const;
    QString lastQueryResult() const;
    QString versionString() const;
    QString appName() const;
    bool itdFormOpen() const;

public slots:
    // 项目操作（委托给 IBR_ProjectManager）
    void newProject();
    void openProject();
    void openRecentProject(const QString &path);
    void saveProject();
    void saveAsProject();
    void saveOptProject();
    void closeProject();
    void exportIni();
    // 带参数版本：由 ExportDialog 调用，先写入 LastOutputDir/LastOutputIniName 再调用 AutoOutputAction
    Q_INVOKABLE void exportIni(const QString &outputDir, const QVariantMap &iniNames);
    void importIni();
    // 带参数版本：由 ImportIniDialog 调用，直接使用指定路径
    Q_INVOKABLE void importIni(const QString &path);
    void undo();
    void redo();

    // 文件关联（对应 IBR_Panel.cpp:93 SetFileAssociation 按钮）
    // 返回值含义：0=失败, 1=成功且已是默认, 2=成功并引导用户设置默认程序
    Q_INVOKABLE int setFileAssociation();
    // 清空最近文件列表（对应 IBR_RecentManager::RenderUI 的 ClearRecent 按钮）
    Q_INVOKABLE void clearRecentFiles();
    // 文件拖放（对应 IBR_ProjectManager::OnDropFile）
    Q_INVOKABLE void onDropFiles(const QStringList &paths);
    // 请求关闭（对应 CheckIfClose + 三态关闭流程）
    Q_INVOKABLE void requestClose();
    // 请求退出应用（窗口关闭按钮：有未保存→三态确认；保存/不保存→退出；取消→不退出）
    Q_INVOKABLE void requestQuit();
    // 刷新所有寄存器名（对应 IBR_Inst_Project.RenameAll）
    Q_INVOKABLE void refreshAllRegName();
    // 刷新所有属性并发出信号（供 QtMain QTimer 定期调用，同步异步更新的状态）
    void refreshProperties();

    // 调试信息（对应 IBR_Inst_Debug / IBG_Undo.RenderUI）
    Q_INVOKABLE int undoStackCount() const;
    Q_INVOKABLE int undoStackCursor() const;
    Q_INVOKABLE QVariantList undoStackItems() const;
    Q_INVOKABLE QVariantList debugMessages() const;
    Q_INVOKABLE void refreshDebug();

    // ===== 阶段 8.1 新增：Debug setter =====
    void setDebugOutputExtra(bool v);
    void setUseModuleProperties(bool v);
    void setShowWorkspaceWindowFrame(bool v);
    void setDontGoToEdit(bool v);
    void setDontDrawBg(bool v);
    void setLinkDebugMode(bool v);
    void setPoolQueryBuf(const QString &v);

    // ===== 阶段 8.1 新增：Debug 按钮 Q_INVOKABLE 方法（对应 IBR_Debug.cpp:87-153, 194） =====
    // 复制项目输出到剪贴板（对应 IBR_Debug.cpp:87-92 CopyOutput）
    Q_INVOKABLE void copyOutput();
    // 剪贴板内容转 JSON（对应 IBR_Debug.cpp:117-128 Clipboard2Json）
    Q_INVOKABLE void clipboard2Json();
    // 字符串转 ID（对应 IBR_Debug.cpp:131-141 StringToID，调用 NewPoolStr）
    Q_INVOKABLE void queryStringToId();
    // ID 转字符串（对应 IBR_Debug.cpp:143-147 IDToString，调用 PoolStr）
    Q_INVOKABLE void queryIdToString();
    // 触发刷新连线（对应 IBR_Debug.cpp:150-153 TriggerRefreshLink）
    Q_INVOKABLE void triggerRefreshLink();
    // 清空一次性调试信息（对应 IBR_Debug.cpp:194 ClearOnceInfo）
    Q_INVOKABLE void clearOnceInfo();
    // 打开/关闭 TopMost 输入表单（对应 IBR_Debug.cpp:102-115）
    Q_INVOKABLE void itdOpenInputForm();
    Q_INVOKABLE void itdCloseInputForm();
    // 阶段 8.3：标记鼠标左键双击事件（对应 ImGui IsMouseDoubleClicked），由 WorkspaceView onDoubleClicked 调用
    Q_INVOKABLE void markDblClickLeft();

    // ===== 阶段 8.1 新增：Debug TreeNode 数据查询 =====
    // GUI_DebugUIState：返回 20+ 个内部状态（对应 IBR_Debug.cpp:159-186）
    Q_INVOKABLE QVariantMap debugUIState() const;
    // GUI_DebugRealTimeInfo：返回 DebugVec 文本列表（对应 IBR_Debug.cpp:188-192）
    Q_INVOKABLE QStringList debugRealTimeMessages() const;
    // GUI_DebugOnceInfo：返回 DebugVecOnce 文本列表（对应 IBR_Debug.cpp:195-199）
    Q_INVOKABLE QStringList debugOnceMessages() const;

signals:
    void isOpenChanged();
    void projectNameChanged();
    void projectPathChanged();
    void isModifiedChanged();
    void canUndoChanged();
    void canRedoChanged();
    void canExportChanged();
    void recentFilesChanged();

    // 提示信息（对应 IBR_HintManager::SetHint）
    void hintMessage(const QString &text);
    // 调试信息刷新信号
    void debugInfoChanged();
    // 阶段 8.3：ITD 表单开闭状态变化信号
    void itdFormOpenChanged();
    // 语言切换信号（对应 IBR_L10n::SetLanguage 后通知 appName 等本地化属性刷新）
    void languageChanged();

public slots:
    // 处理 ConfirmDialog 的结果（由 DialogController.confirmCompleted 信号触发）
    void onConfirmResult(const QString &actionId, bool accepted);
    // 阶段 11.3：处理三态 ConfirmDialog 的结果（由 DialogController.confirm3Completed 信号触发）
    // result: 0=取消, 1=保存, 2=不保存
    void onConfirmResult3(const QString &actionId, int result);

private:
    // 阶段 13.3.1：实际执行清空最近文件（由 clearRecentFiles 弹确认后调用）
    void doClearRecentFiles();
    // 实际执行打开项目（由 openProject 弹三态确认后调用）
    void doOpenProject();
    // 阶段 13.3.2：根据用户选择的类型创建 SHP 模块（对应 IBR_ProjManager.cpp:1163-1212）
    void createShpModules(const QStringList &names, const std::vector<int> &types);
    DialogController *m_dialogController{nullptr};

    // 对应 IBR_Debug.cpp:85 的 static bool Ext（DebugOutputExtra 开关）
    // 因 Ext 是 ImGui RenderUI 内的 static 局部变量，不持久化到 IBR_Inst_Debug，这里用 ProjectController 静态成员承载
    static bool s_debugOutputExtra;
    // 阶段 8.3：DblClickLeft 瞬态标志（对应 ImGui IsMouseDoubleClicked，单帧有效）
    mutable bool m_dblClickLeft{false};
};
