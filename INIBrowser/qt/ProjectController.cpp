// ProjectController.cpp
// Qt6 项目控制器实现
#include "ProjectController.h"
#include "Global.h"
#include "IBR_Misc.h"
#include "IBR_Components.h"
#include "IBR_Localization.h"
#include "IBG_UndoTree.h"
#include "IBR_Debug.h"
#include "IBR_TopMost.h"
#include "IBR_LinkNode.h"
#include "IBB_PropStringPool.h"
#include "IBB_Ini.h"
#include "IBB_ModuleAlt.h"
#include "DialogController.h"
#include <QMessageBox>
#include <QFileDialog>
#include <QApplication>
#include <QClipboard>
#include <QCursor>
#include <QGuiApplication>
#include <QKeyCombination>

// IBR_WorkSpace 内部状态变量（对应 IBR_Debug.cpp:56-66 的 extern 声明）
// 这些变量在 IBR_WorkSpace.cpp 中定义，但未在头文件中完整声明，这里按 IBR_Debug.cpp 的方式 extern 引用
namespace IBR_WorkSpace
{
    extern bool IsMassAfter, HasRightDownToWait, HasLefttDownToWait, MoveAfterMass;
    extern bool LastClickable, LastOnWindow, LastCont, Cont;
    extern bool OnCombo, OnPopupMenu;
    extern ImVec2 MassAfter_RightDownPos;
}

// ITD 输入表单函数（对应 IBR_Debug.cpp:13-15 的前向声明，实现在 IBR_InputTypeDebug.cpp）
void ITD_Init();
void ITD_Column1();
void ITD_Column2();

// 静态成员定义（对应 IBR_Debug.cpp:85 的 static bool Ext）
bool ProjectController::s_debugOutputExtra = false;

ProjectController::ProjectController(QObject *parent)
    : QObject(parent)
{
}

bool ProjectController::isOpen() const
{
    return IsProjectOpen;
}

QString ProjectController::projectName() const
{
    return QString::fromStdWString(IBF_Inst_Project.Project.ProjName);
}

QString ProjectController::projectPath() const
{
    return QString::fromStdWString(IBF_Inst_Project.Project.Path);
}

bool ProjectController::isModified() const
{
    return IBF_Inst_Project.Project.ChangeAfterSave;
}

bool ProjectController::canUndo() const
{
    return IBG_Undo.CanUndo();
}

bool ProjectController::canRedo() const
{
    return IBG_Undo.CanRedo();
}

bool ProjectController::canExport() const
{
    return !IBR_Inst_Project.IBR_SectionMap.empty();
}

QStringList ProjectController::recentFiles() const
{
    QStringList list;
    for (const auto &s : IBR_RecentManager::GetRecentList())
    {
        list << QString::fromUtf8(s);
    }
    return list;
}

void ProjectController::newProject()
{
    // 修复：ProjOpen_CreateAction 内部会先 CloseAction，若有未保存修改会触发 AskIfSave（ImGui 弹窗不可见）
    if (IBR_ProjectManager::IsOpen() && IBF_Inst_Project.Project.ChangeAfterSave)
    {
        if (m_dialogController)
        {
            m_dialogController->showConfirm3(
                QString::fromUtf8(u8"新建项目"),
                QString::fromUtf8(u8"项目有未保存的修改，是否在新建前保存？"),
                QString::fromUtf8("newProjectWithUnsaved"));
        }
        return;
    }
    IBR_ProjectManager::ProjOpen_CreateAction();
    refreshProperties();
}

void ProjectController::openProject()
{
    // 修复：原 ProjOpen_OpenAction 内部依赖 GetOpenFileNameW + ImGui 弹窗，Qt 环境下无法显示
    // 改为 QFileDialog 获取路径 + OpenRecentOptAction 直接加载（绕过 Win32 对话框）
    // 但 OpenRecentOptAction 内部会先 CloseAction，若有未保存修改会触发 AskIfSave（ImGui 弹窗不可见）
    // 所以这里先手动处理未保存修改的三态确认，确认后再调用 OpenRecentOptAction
    if (IBR_ProjectManager::IsOpen() && IBF_Inst_Project.Project.ChangeAfterSave)
    {
        // 有未保存修改：弹三态确认弹窗
        // actionId="openProjectWithUnsaved" 用于 onConfirmResult3 分发
        if (m_dialogController)
        {
            m_dialogController->showConfirm3(
                QString::fromUtf8(u8"打开项目"),
                QString::fromUtf8(u8"项目有未保存的修改，是否在打开前保存？"),
                QString::fromUtf8("openProjectWithUnsaved"));
        }
        return;
    }
    doOpenProject();
}

void ProjectController::doOpenProject()
{
    // QFileDialog 选择 .iproj 文件
    QString path = QFileDialog::getOpenFileName(
        nullptr,
        QString::fromUtf8(u8"打开项目"),
        QString(),
        QString::fromUtf8(u8"INIWeaver 项目文件 (*.iproj);;所有文件 (*.*)"));
    if (path.isEmpty())
    {
        emit hintMessage(QString::fromUtf8(u8"已取消"));
        return;
    }
    // OpenRecentOptAction：已打开则先关再开，未打开则直接开
    // 内部通过 IBS_Push 调用 Load，不依赖 Win32 对话框
    IBR_ProjectManager::OpenRecentOptAction(path.toStdWString());
    refreshProperties();
}

void ProjectController::openRecentProject(const QString &path)
{
    IBR_ProjectManager::OpenRecentOptAction(path.toStdWString());
    refreshProperties();
}

void ProjectController::saveProject()
{
    IBR_ProjectManager::SaveOptAction();
    refreshProperties();
}

void ProjectController::saveAsProject()
{
    // 修复：原 SaveAsAction 内部依赖 GetSaveFileNameW + ImGui SetWaitingPopup，Qt 环境下无法显示
    // 改为 QFileDialog 获取路径，设置 Project.Path + IsNewlyCreated=false，然后调用 SaveAction（不弹对话框）
    QString path = QFileDialog::getSaveFileName(
        nullptr,
        QString::fromUtf8(u8"另存为"),
        QString::fromStdWString(IBF_Inst_Project.Project.Path),
        QString::fromUtf8(u8"INIWeaver 项目文件 (*.iproj);;所有文件 (*.*)"));
    if (path.isEmpty())
    {
        emit hintMessage(QString::fromUtf8(u8"已取消"));
        return;
    }
    // 设置新路径，标记为非新建项目，然后调用 SaveAction（使用 Project.Path 保存）
    IBF_Inst_Project.Project.Path = path.toStdWString();
    IBF_Inst_Project.Project.IsNewlyCreated = false;
    IBR_ProjectManager::SaveAction();
    refreshProperties();
}

void ProjectController::saveOptProject()
{
    IBR_ProjectManager::SaveOptAction();
    refreshProperties();
}

void ProjectController::closeProject()
{
    IBR_ProjectManager::CloseAction();
    refreshProperties();
}

void ProjectController::exportIni()
{
    IBR_ProjectManager::OutputAction();
    refreshProperties();
}

void ProjectController::exportIni(const QString &outputDir, const QVariantMap &iniNames)
{
    // 由 ExportDialog 调用：先写入用户指定的输出目录和 INI 文件名
    if (!outputDir.isEmpty())
    {
        IBF_Inst_Project.Project.LastOutputDir = outputDir.toStdWString();
    }
    for (auto it = iniNames.begin(); it != iniNames.end(); ++it)
    {
        std::string iniName = it.key().toStdString();
        std::wstring fileName = it.value().toString().toStdWString();
        IBF_Inst_Project.Project.LastOutputIniName[iniName] = fileName;
    }
    // 调用 AutoOutputAction（无 UI 版本，直接使用 LastOutputDir/LastOutputIniName）
    IBR_ProjectManager::AutoOutputAction();
    refreshProperties();
}

void ProjectController::importIni()
{
    // 修复：原 ImportIniAction 内部依赖 GetOpenFileNameW + ImGui SetWaitingPopup，Qt 环境下无法显示
    // 改为 QFileDialog 获取路径，然后调用 ImportIni(path)（已实现的带参数版本，跳过文件选择对话框）
    QString path = QFileDialog::getOpenFileName(
        nullptr,
        QString::fromUtf8(u8"导入 INI"),
        QString(),
        QString::fromUtf8(u8"INI 文件 (*.ini);;所有文件 (*.*)"));
    if (path.isEmpty())
    {
        emit hintMessage(QString::fromUtf8(u8"已取消"));
        return;
    }
    importIni(path);
}

void ProjectController::importIni(const QString &path)
{
    // 由 ImportIniDialog 调用：用户已在 QML FileDialog 中选择路径
    // 直接调用 ImportIni(Path) 跳过原 ImportIniAction 中的系统文件选择对话框
    // ImportIni 是 _IN_SAVE_THREAD，通过 IBS_Push 切到 save thread 执行
    // 内部会通过 IBRF_CoreBump.SendToR 弹 ImGui 预览弹窗并处理后续导入流程
    if (path.isEmpty())
    {
        emit hintMessage(QString::fromUtf8(u8"路径为空"));
        return;
    }
    auto wPath = path.toStdWString();
    IBS_Push([wPath = std::move(wPath)]()
    {
        IBR_ProjectManager::ImportIni(wPath);
    });
    refreshProperties();
}

void ProjectController::undo()
{
    IBG_Undo.Undo();
    refreshProperties();
}

void ProjectController::redo()
{
    IBG_Undo.Redo();
    refreshProperties();
}

int ProjectController::setFileAssociation()
{
    // IBB_FileAssoc.cpp 的三个函数（无头文件，前向声明）
    bool SetFileAssociation();
    bool GuideUserToSetDefaultProgram();
    bool IsDefaultForExtension();

    if (!SetFileAssociation())
    {
        emit hintMessage(QString::fromUtf8(u8"文件关联设置失败"));
        return 0;
    }
    if (IsDefaultForExtension())
    {
        emit hintMessage(QString::fromUtf8(u8"已设置文件关联"));
        return 1;
    }
    emit hintMessage(QString::fromUtf8(u8"文件关联设置成功"));
    GuideUserToSetDefaultProgram();
    return 2;
}

void ProjectController::clearRecentFiles()
{
    // 阶段 13.3.1：对应 ImGui IBR_Components.cpp:199-228 的模态确认弹窗
    // 不再直接清空，而是弹 Yes/No 确认弹窗（actionId="clearRecentFiles"）
    // 实际清空逻辑在 onConfirmResult 中处理
    if (m_dialogController)
    {
        m_dialogController->showConfirm(
            QString::fromUtf8(u8"清空最近文件"),
            QString::fromUtf8(u8"是否清空最近文件列表？"),
            QString::fromUtf8("clearRecentFiles"));
    }
    else
    {
        // 无 DialogController 时退化为直接清空（兼容旧调用路径）
        doClearRecentFiles();
    }
}

void ProjectController::doClearRecentFiles()
{
    // 通过 WanDuZiLe 循环清空 RecentName（每次删除首个）
    while (!IBR_RecentManager::GetRecentList().empty())
    {
        IBR_RecentManager::WanDuZiLe();
    }
    IBR_RecentManager::Save();
    emit recentFilesChanged();
}

void ProjectController::onDropFiles(const QStringList &paths)
{
    // 阶段 13.3.2：对应 IBR_ProjectManager::OnDropFile
    // SHP 文件单独拦截走 Qt 弹窗（ImGui 的 PushMsgBack 弹窗无法通过 Hook 转发）
    // 其他文件继续走 OnDropFile
    QStringList nonShpPaths;
    QStringList shpNames;

    for (const auto &path : paths)
    {
        auto stdPath = path.toStdString();
        auto dotPos = stdPath.find_last_of('.');
        std::string ext = (dotPos != std::string::npos)
            ? stdPath.substr(dotPos + 1)
            : std::string();
        std::transform(ext.begin(), ext.end(), ext.begin(), ::toupper);

        if (ext == "SHP")
        {
            // 提取文件名（不含扩展名），转大写（对应 ImGui FileNameNoExt + toupper）
            auto slashPos = stdPath.find_last_of("/\\");
            std::string fileName = (slashPos != std::string::npos)
                ? stdPath.substr(slashPos + 1)
                : stdPath;
            std::string nameNoExt = (dotPos != std::string::npos)
                ? fileName.substr(0, dotPos - (slashPos != std::string::npos ? slashPos + 1 : 0))
                : fileName;
            std::transform(nameNoExt.begin(), nameNoExt.end(), nameNoExt.begin(), ::toupper);
            shpNames.append(QString::fromUtf8(nameNoExt.c_str()));
        }
        else if (ext == "IPROJ")
        {
            // 项目文件：直接打开（不走 OnDropFile 的批量处理）
            auto wPath = path.toStdWString();
            IBR_ProjectManager::ProjOpen_OpenRecentAction(wPath);
        }
        else
        {
            // INI/VXL/PCX/WAV 等模块文件：交给 OnDropFile 处理
            nonShpPaths.append(path);
        }
    }

    // 处理非 SHP 文件
    if (!nonShpPaths.isEmpty())
    {
        std::vector<QByteArray> bas;
        std::vector<const char *> argv;
        bas.reserve(nonShpPaths.size());
        argv.reserve(nonShpPaths.size());
        for (const auto &p : nonShpPaths)
        {
            bas.push_back(p.toLocal8Bit());
            argv.push_back(bas.back().constData());
        }
        IBR_ProjectManager::OnDropFile(nullptr, static_cast<int>(argv.size()), argv.data());
    }

    // 处理 SHP 文件：通过 Qt 弹窗让用户选择类型
    if (!shpNames.isEmpty() && m_dialogController)
    {
        // 注册一次性回调，用户确认后创建对应模块
        // shpNames 按值捕获（局部变量，回调时原变量已销毁）
        m_dialogController->setShpConfirmCallback(
            [this, shpNames](const std::vector<int> &types)
            {
                createShpModules(shpNames, types);
                refreshProperties();
            });
        m_dialogController->requestShpTypeSelection(shpNames);
    }

    refreshProperties();
}

void ProjectController::createShpModules(const QStringList &names, const std::vector<int> &types)
{
    // 阶段 13.3.2：对应 IBR_ProjManager.cpp:1163-1212 的 OK 按钮回调
    // types[i]: 0=Animation, 1=Building, 2=Infantry, 3=Vehicle
    auto createError = [this](const QString &msg)
    {
        emit hintMessage(msg);
    };

    for (int i = 0; i < names.size() && i < static_cast<int>(types.size()); ++i)
    {
        auto nameStd = names[i].toStdString();
        int type = types[i];
        IBB_ModuleAlt *pShp = nullptr;
        QString errorMsg;

        switch (type)
        {
        case 0: // Animation
            pShp = IBB_ModuleAltDefault::DefaultArt_Animation();
            if (!pShp)
                errorMsg = QString::fromUtf8(u8"找不到 Animation 模块模板");
            break;
        case 1: // Building
            pShp = IBB_ModuleAltDefault::DefaultArt_SHPBuilding();
            if (!pShp)
                errorMsg = QString::fromUtf8(u8"找不到 Building 模块模板");
            break;
        case 2: // Infantry
            pShp = IBB_ModuleAltDefault::DefaultArt_SHPInfantry();
            if (!pShp)
                errorMsg = QString::fromUtf8(u8"找不到 Infantry 模块模板");
            break;
        case 3: // Vehicle
            pShp = IBB_ModuleAltDefault::DefaultArt_SHPVehicle();
            if (!pShp)
                errorMsg = QString::fromUtf8(u8"找不到 Vehicle 模块模板");
            break;
        default:
            continue;
        }

        if (!pShp)
        {
            createError(errorMsg);
            continue;
        }

        // 检查是否已存在同名 section（对应 ImGui HasSection 检查）
        auto firstIni = pShp->GetFirstINI();
        if (IBR_Inst_Project.HasSection({ firstIni, nameStd }))
        {
            createError(QString::fromUtf8(u8"已存在同名图像模块：") + names[i]);
            continue;
        }

        IBR_Inst_Project.AddModule(*pShp, nameStd);
    }
}

void ProjectController::requestClose()
{
    // 对应 ImGui 版 AskIfSave 三态关闭流程（IBR_ProjManager.cpp:221-249）
    // 注意：此方法仅关闭项目，不退出应用（对应 Ctrl+W 快捷键）
    if (IBR_ProjectManager::IsOpen())
    {
        if (IBF_Inst_Project.Project.ChangeAfterSave && m_dialogController)
        {
            // 有未保存修改：通过 DialogController 弹三态确认弹窗
            // 阶段 11.3：由 showConfirm 升级为 showConfirm3（保存/不保存/取消）
            // actionId="closeWithUnsaved" 用于 onConfirmResult3 分发
            m_dialogController->showConfirm3(
                QString::fromUtf8(u8"关闭项目"),
                QString::fromUtf8(u8"项目有未保存的修改，是否在关闭前保存？"),
                QString::fromUtf8("closeWithUnsaved"));
        }
        else
        {
            // 无修改：直接关闭
            IBR_ProjectManager::CloseAction();
            refreshProperties();
        }
    }
}

void ProjectController::requestQuit()
{
    // 窗口关闭按钮：退出整个应用（而非仅关闭项目）
    // 对应 ImGui 单文档模式：关闭窗口=退出应用
    if (IBR_ProjectManager::IsOpen() && IBF_Inst_Project.Project.ChangeAfterSave && m_dialogController)
    {
        // 有未保存修改：弹三态确认（保存/不保存/取消）
        // actionId="quitWithUnsaved" 用于 onConfirmResult3 分发
        m_dialogController->showConfirm3(
            QString::fromUtf8(u8"退出 INIWeaver"),
            QString::fromUtf8(u8"项目有未保存的修改，是否在退出前保存？"),
            QString::fromUtf8("quitWithUnsaved"));
    }
    else
    {
        // 无项目或无修改：直接退出
        QCoreApplication::quit();
    }
}

void ProjectController::onConfirmResult(const QString &actionId, bool accepted)
{
    if (actionId == QString::fromUtf8("closeWithUnsaved"))
    {
        // 阶段 11.3：closeWithUnsaved 已迁移到 onConfirmResult3，此处保留兼容
        if (accepted)
        {
            IBR_ProjectManager::SaveOptAction();
            IBR_ProjectManager::CloseAction();
        }
        refreshProperties();
    }
    else if (actionId == QString::fromUtf8("deleteSection"))
    {
        // Section 列表的删除确认由 ListPanel 自行处理，这里不介入
    }
    else if (actionId == QString::fromUtf8("clearRecentFiles"))
    {
        // 阶段 13.3.1：清空最近文件确认结果
        if (accepted)
        {
            doClearRecentFiles();
            // 对应 ImGui 弹窗 Yes 分支的 SetHint(loc("GUI_ActionCanceled"), ...)
            // ImGui 这里实际上 Yes=执行清空，No=显示 ActionCanceled hint
            // 这里 Yes 分支不发 hint（ImGui 侧也未发成功 hint）
        }
    }
}

void ProjectController::onConfirmResult3(const QString &actionId, int result)
{
    // 阶段 11.3：三态确认结果分发
    // result: 0=取消, 1=保存, 2=不保存
    if (actionId == QString::fromUtf8("closeWithUnsaved"))
    {
        if (result == 1)
        {
            // 保存后关闭（对应 ImGui AskIfSave Yes）
            IBR_ProjectManager::SaveOptAction();
            IBR_ProjectManager::CloseAction();
        }
        else if (result == 2)
        {
            // 不保存直接关闭（对应 ImGui AskIfSave No）
            IBR_ProjectManager::CloseAction();
        }
        // result == 0：取消，什么都不做（对应 ImGui AskIfSave Cancel）
        refreshProperties();
    }
    else if (actionId == QString::fromUtf8("openProjectWithUnsaved"))
    {
        if (result == 1)
        {
            // 保存后打开新项目
            IBR_ProjectManager::SaveOptAction();
            // CloseAction 在 OpenRecentOptAction 内部自动执行
            doOpenProject();
        }
        else if (result == 2)
        {
            // 不保存直接打开
            doOpenProject();
        }
        // result == 0：取消
        refreshProperties();
    }
    else if (actionId == QString::fromUtf8("newProjectWithUnsaved"))
    {
        if (result == 1)
        {
            // 保存后新建
            IBR_ProjectManager::SaveOptAction();
            IBR_ProjectManager::ProjOpen_CreateAction();
        }
        else if (result == 2)
        {
            // 不保存直接新建
            IBR_ProjectManager::ProjOpen_CreateAction();
        }
        refreshProperties();
    }
    else if (actionId == QString::fromUtf8("quitWithUnsaved"))
    {
        // 窗口关闭触发的退出确认
        if (result == 1)
        {
            // 保存后退出（对应 ImGui AskIfSave Yes）
            IBR_ProjectManager::SaveOptAction();
            QCoreApplication::quit();
        }
        else if (result == 2)
        {
            // 不保存直接退出（对应 ImGui AskIfSave No）
            QCoreApplication::quit();
        }
        // result == 0：取消，不退出
        refreshProperties();
    }
}

void ProjectController::refreshAllRegName()
{
    // 对应 IBR_Inst_Project.RenameAll()
    IBR_Inst_Project.RenameAll();
    emit hintMessage(QString::fromUtf8(u8"已刷新所有寄存器名"));
}

void ProjectController::refreshProperties()
{
    // 简单实现：发所有信号，让 QML 重新读取
    // 后续可优化为只在值真正变化时发信号
    emit isOpenChanged();
    emit projectNameChanged();
    emit projectPathChanged();
    emit isModifiedChanged();
    emit canUndoChanged();
    emit canRedoChanged();
    emit canExportChanged();
    emit recentFilesChanged();
    emit debugInfoChanged();
}

int ProjectController::undoStackCount() const
{
    return static_cast<int>(IBG_Undo.Stack.size());
}

int ProjectController::undoStackCursor() const
{
    return IBG_Undo.Cursor;
}

QVariantList ProjectController::undoStackItems() const
{
    QVariantList list;
    for (const auto &item : IBG_Undo.Stack)
    {
        QVariantMap m;
        m["id"] = QString::fromUtf8(item.Id);
        m["isCurrent"] = (false); // 占位，后续可标记当前项
        list.append(m);
    }
    return list;
}

QVariantList ProjectController::debugMessages() const
{
    QVariantList list;
    // StdMessage 为 std::function<void()>，无文本可读
    // 仅暴露 DebugVec/DebugVecOnce 数量及调试开关
    QVariantMap info;
    info["debugVecCount"] = static_cast<int>(IBR_Inst_Debug.DebugVec.size());
    info["debugVecOnceCount"] = static_cast<int>(IBR_Inst_Debug.DebugVecOnce.size());
    info["useModuleProperties"] = IBR_Inst_Debug.UseModuleProperties;
    info["showWorkspaceWindowFrame"] = IBR_Inst_Debug.ShowWorkspaceWindowFrame;
    info["dontGoToEdit"] = IBR_Inst_Debug.DontGoToEdit;
    info["dontDrawBg"] = IBR_Inst_Debug.DontDrawBg;
    info["linkDebugMode"] = IBR_Inst_Debug.LinkDebugMode;
    info["poolQueryBuf"] = QString::fromUtf8(IBR_Inst_Debug.PoolQueryBuf);
    info["lastQueryResult"] = QString::fromUtf8(IBR_Inst_Debug.LastQueryResult);
    list.append(info);
    return list;
}

void ProjectController::refreshDebug()
{
    emit debugInfoChanged();
}

// ===== 阶段 8.1 新增：Debug 属性读取器（对应 IBR_Inst_Debug 成员） =====

bool ProjectController::debugOutputExtra() const
{
    return s_debugOutputExtra;
}

bool ProjectController::useModuleProperties() const
{
    return IBR_Inst_Debug.UseModuleProperties;
}

bool ProjectController::showWorkspaceWindowFrame() const
{
    return IBR_Inst_Debug.ShowWorkspaceWindowFrame;
}

bool ProjectController::dontGoToEdit() const
{
    return IBR_Inst_Debug.DontGoToEdit;
}

bool ProjectController::dontDrawBg() const
{
    return IBR_Inst_Debug.DontDrawBg;
}

bool ProjectController::linkDebugMode() const
{
    return IBR_Inst_Debug.LinkDebugMode;
}

QString ProjectController::poolQueryBuf() const
{
    return QString::fromUtf8(IBR_Inst_Debug.PoolQueryBuf);
}

QString ProjectController::lastQueryResult() const
{
    return QString::fromUtf8(IBR_Inst_Debug.LastQueryResult);
}

QString ProjectController::versionString() const
{
    // 对应 AppMenuBar.qml 右侧标题，读取全局 Version 变量（Global.h:24）
    return QString::fromUtf8(Version);
}

QString ProjectController::appName() const
{
    // 对应 MainStage.h:167 GUI_TopRightHint 第一个参数 _AppName = locc("AppName")
    // 返回本地化的应用名（中文=INI织网者，English=INI Weaver）
    return QString::fromUtf8(locc("AppName"));
}

bool ProjectController::itdFormOpen() const
{
    // 阶段 8.3：对应 IBR_Debug.cpp:100 IBR_TopMost::MenuMatchesSource(MenuItemID_DEBUG, 114514)
    return IBR_TopMost::MenuMatchesSource(MenuItemID_DEBUG, 114514);
}

// ===== 阶段 8.1 新增：Debug setter =====

void ProjectController::setDebugOutputExtra(bool v)
{
    if (s_debugOutputExtra == v) return;
    s_debugOutputExtra = v;
    emit debugInfoChanged();
}

void ProjectController::setUseModuleProperties(bool v)
{
    IBR_Inst_Debug.UseModuleProperties = v;
    emit debugInfoChanged();
}

void ProjectController::setShowWorkspaceWindowFrame(bool v)
{
    IBR_Inst_Debug.ShowWorkspaceWindowFrame = v;
    emit debugInfoChanged();
}

void ProjectController::setDontGoToEdit(bool v)
{
    IBR_Inst_Debug.DontGoToEdit = v;
    emit debugInfoChanged();
}

void ProjectController::setDontDrawBg(bool v)
{
    IBR_Inst_Debug.DontDrawBg = v;
    emit debugInfoChanged();
}

void ProjectController::setLinkDebugMode(bool v)
{
    IBR_Inst_Debug.LinkDebugMode = v;
    emit debugInfoChanged();
}

void ProjectController::setPoolQueryBuf(const QString &v)
{
    // 阶段 8.3：相等性守卫，避免每次按键都发 debugInfoChanged 信号
    auto stdV = v.toStdString();
    if (IBR_Inst_Debug.PoolQueryBuf == stdV) return;
    IBR_Inst_Debug.PoolQueryBuf = stdV;
    emit debugInfoChanged();
}

// ===== 阶段 8.1 新增：Debug 按钮 Q_INVOKABLE 方法 =====

void ProjectController::copyOutput()
{
    // 对应 IBR_Debug.cpp:87-92 CopyOutput
    // 复制 IBR_Inst_Project.GetText(Ext) 到剪贴板
    std::string text = IBR_Inst_Project.GetText(s_debugOutputExtra);
    QApplication::clipboard()->setText(QString::fromUtf8(text));
    emit hintMessage(QString::fromUtf8(u8"已复制项目输出到剪贴板"));
}

void ProjectController::clipboard2Json()
{
    // 对应 IBR_Debug.cpp:117-128 Clipboard2Json
    // 读取剪贴板内容，通过 IBB_ClipBoardData 转 JSON，再写回剪贴板
    QString clip = QApplication::clipboard()->text();
    if (clip.isEmpty())
    {
        emit hintMessage(QString::fromUtf8(u8"剪贴板为空"));
        return;
    }
    IBB_ClipBoardData cb;
    if (cb.SetString(clip.toStdString()))
    {
        auto j = cb.ToJson();
        std::string jsonStr = j.GetObj().PrintData();
        QApplication::clipboard()->setText(QString::fromUtf8(jsonStr));
        emit hintMessage(QString::fromUtf8(u8"已将剪贴板内容转为 JSON"));
    }
    else
    {
        emit hintMessage(QString::fromUtf8(u8"剪贴板内容无法解析"));
    }
}

void ProjectController::queryStringToId()
{
    // 对应 IBR_Debug.cpp:131-141 StringToID
    // 调用 NewPoolStr 将字符串注册到池中，返回 ID（16 进制字符串）
    auto result = NewPoolStr(IBR_Inst_Debug.PoolQueryBuf);
    char res[32]{};
#ifdef _WIN64
    _i64toa(result, res, 16);
#else
    _itoa(result, res, 16);
#endif
    // 阶段 8.3：走翻译系统（对应 ImGui loc("GUI_DebugStringToID")）
    IBR_Inst_Debug.LastQueryResult = loc("GUI_DebugStringToID") + " : " + IBR_Inst_Debug.PoolQueryBuf + " -> " + res;
    emit debugInfoChanged();
}

void ProjectController::queryIdToString()
{
    // 对应 IBR_Debug.cpp:143-147 IDToString
    // 调用 PoolStr 将 ID（16 进制）转回字符串
    auto result = PoolStr(strtol(IBR_Inst_Debug.PoolQueryBuf.c_str(), nullptr, 16));
    // 阶段 8.3：走翻译系统（对应 ImGui loc("GUI_DebugIDToString")）
    IBR_Inst_Debug.LastQueryResult = loc("GUI_DebugIDToString") + " : " + IBR_Inst_Debug.PoolQueryBuf + " -> " + result;
    emit debugInfoChanged();
}

void ProjectController::triggerRefreshLink()
{
    // 对应 IBR_Debug.cpp:150-153 TriggerRefreshLink
    IBR_Inst_Project.TriggerRefreshLink();
    emit hintMessage(QString::fromUtf8(u8"已触发刷新连线"));
}

void ProjectController::clearOnceInfo()
{
    // 对应 IBR_Debug.cpp:194 ClearOnceInfo
    IBR_Inst_Debug.DebugVecOnce.clear();
    emit debugInfoChanged();
}

void ProjectController::itdOpenInputForm()
{
    // 对应 IBR_Debug.cpp:105-115 ITDOpenInputForm
    // 注意：OpenTopMostFrom 的回调依赖 ImGui 渲染循环，Qt 版本中回调不会执行
    // 但保持 API 调用一致性，避免后端状态不一致
    ITD_Init();
    IBR_TopMost::OpenTopMostFrom(MenuItemID_DEBUG, 114514, []() {
        // ImGui 侧会渲染 ITD_Column1/ITD_Column2 两列布局
        // Qt 版本中此回调不会被调用
        ITD_Column1();
        ITD_Column2();
    });
    emit itdFormOpenChanged();
    emit hintMessage(QString::fromUtf8(u8"已打开输入表单调试窗口"));
}

void ProjectController::itdCloseInputForm()
{
    // 对应 IBR_Debug.cpp:102-104 ITDCloseInputForm
    IBR_TopMost::CloseTopMostMenu();
    emit itdFormOpenChanged();
    emit hintMessage(QString::fromUtf8(u8"已关闭输入表单调试窗口"));
}

void ProjectController::markDblClickLeft()
{
    // 阶段 8.3：标记鼠标左键双击事件（对应 ImGui IsMouseDoubleClicked）
    // 由 WorkspaceView.qml onDoubleClicked 调用，debugUIState() 读取后自动清除
    m_dblClickLeft = true;
}

// ===== 阶段 8.1 新增：Debug TreeNode 数据查询 =====

QVariantMap ProjectController::debugUIState() const
{
    // 对应 IBR_Debug.cpp:159-186 GUI_DebugUIState TreeNode
    // 返回 20+ 个内部状态供 QML TreeNode 展示
    QVariantMap m;
    QPoint mousePos = QCursor::pos();
    // 阶段 8.3：数值格式化统一 %.1f（对应 ImGui Text("%.1f,%.1f")）
    m["mousePos"] = QString("(%1, %2)").arg(QString::number(mousePos.x(), 'f', 1),
                                            QString::number(mousePos.y(), 'f', 1));

    auto &mardp = IBR_WorkSpace::MassAfter_RightDownPos;
    if (mardp.x == FLT_MAX || mardp.y == FLT_MAX || mardp.x == -FLT_MAX || mardp.y == -FLT_MAX)
    {
        m["massAfterRightDownPos"] = "INVALID";
    }
    else
    {
        m["massAfterRightDownPos"] = QString("(%1, %2)").arg(QString::number(mardp.x, 'f', 1),
                                                             QString::number(mardp.y, 'f', 1));
    }

    m["currentEqMax"] = QString("(%1, %2)").arg(QString::number(IBR_FullView::CurrentEqMax.x, 'f', 1),
                                                QString::number(IBR_FullView::CurrentEqMax.y, 'f', 1));
    m["screenSize"] = QString("(%1, %2)").arg(IBR_UICondition::CurrentScreenWidth).arg(IBR_UICondition::CurrentScreenHeight);
    m["isBgDragging"] = IBR_WorkSpace::IsBgDragging ? "true" : "false";
    m["holdingModules"] = IBR_WorkSpace::HoldingModules ? "true" : "false";
    m["isMassSelecting"] = IBR_WorkSpace::IsMassSelecting ? "true" : "false";
    m["isMassAfter"] = IBR_WorkSpace::IsMassAfter ? "true" : "false";
    m["hasRightDownToWait"] = IBR_WorkSpace::HasRightDownToWait ? "true" : "false";
    m["hasLeftDownToWait"] = IBR_WorkSpace::HasLefttDownToWait ? "true" : "false";
    m["moveAfterMass"] = IBR_WorkSpace::MoveAfterMass ? "true" : "false";
    m["lastClickable"] = IBR_WorkSpace::LastClickable ? "true" : "false";
    m["lastOnWindow"] = IBR_WorkSpace::LastOnWindow ? "true" : "false";
    m["lastCont"] = IBR_WorkSpace::LastCont ? "true" : "false";
    m["onCombo"] = IBR_WorkSpace::OnCombo ? "true" : "false";
    m["onPopupMenu"] = IBR_WorkSpace::OnPopupMenu ? "true" : "false";
    m["hasDragNow"] = LinkNodeContext::HasDragNow ? "true" : "false";

    // 阶段 8.3：补齐 5 项键盘/鼠标状态（对应 IBR_Debug.cpp:179-183）
    // DblClickLeft：读取 markDblClickLeft() 设置的瞬态标志，读取后自动清除（对应 ImGui 单帧有效语义）
    m["dblClickLeft"] = m_dblClickLeft ? "true" : "false";
    m_dblClickLeft = false;
    // CTRL/SHIFT/ALT/SUPER：用 QGuiApplication::queryKeyboardModifiers() 替代 ImGui::GetIO().KeyCtrl 等
    Qt::KeyboardModifiers mods = QGuiApplication::queryKeyboardModifiers();
    m["ctrl"] = (mods & Qt::ControlModifier) ? "true" : "false";
    m["shift"] = (mods & Qt::ShiftModifier) ? "true" : "false";
    m["alt"] = (mods & Qt::AltModifier) ? "true" : "false";
    m["super"] = (mods & Qt::MetaModifier) ? "true" : "false";
    return m;
}

QStringList ProjectController::debugRealTimeMessages() const
{
    // 对应 IBR_Debug.cpp:188-192 GUI_DebugRealTimeInfo TreeNode
    // DebugVec 是 std::vector<StdMessage>，StdMessage 是 std::function<void()>
    // 在 ImGui 侧通过 x() 执行回调渲染文本，Qt 侧无法直接执行
    // 这里返回数量信息供 QML 展示
    QStringList list;
    list << QString::fromUtf8(u8"DebugVec 数量: %1").arg(IBR_Inst_Debug.DebugVec.size());
    list << QString::fromUtf8(u8"(实时调试信息需 ImGui 渲染循环执行，Qt 版本暂不支持)");
    return list;
}

QStringList ProjectController::debugOnceMessages() const
{
    // 对应 IBR_Debug.cpp:195-199 GUI_DebugOnceInfo TreeNode
    QStringList list;
    list << QString::fromUtf8(u8"DebugVecOnce 数量: %1").arg(IBR_Inst_Debug.DebugVecOnce.size());
    list << QString::fromUtf8(u8"(一次性调试信息需 ImGui 渲染循环执行，Qt 版本暂不支持)");
    return list;
}
