// DialogController.h
// Qt6 弹窗控制器：将 C++ 侧的弹窗请求以信号形式通知 QML
// 阶段 3.6：替代 IBR_PopupManager 的模态弹窗逻辑
// 阶段 10.3：新增 Hint 队列轮播（对应 IBR_HintManager::Load + HintChangeMillis）
// 阶段 11.3：新增 ConfirmDialog 三态支持（保存/不保存/取消）
// 阶段 11.4：新增右键菜单 Hook 转发（对应 SetRightClickMenuHook）
// 阶段 11.5：新增 hasPopup 全局标志门控（对应 IBR_PopupManager::HasPopup）
// 阶段 11.2：新增错误队列（对应 IBR_ErrorCollector ErrorList）
// 阶段 11.1：新增导入预览状态管理（对应 IBR_ImportPreview::Open）
// QML 侧通过 Connections 监听信号并显示对应对话框
#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <QSizeF>
#include <QVariantMap>
#include <QtQmlIntegration/qqmlintegration.h>
#include <functional>
#include "IBR_ImportIni.h"

class DialogController : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    // 阶段 10.3：自定义 hint 文本（对应 IBR_HintManager::CustomHint）
    // 非空时优先于 hintPool 轮播显示
    Q_PROPERTY(QString customHint READ customHint NOTIFY customHintChanged)
    // 阶段 11.5：全局弹窗存在标志（对应 IBR_PopupManager::HasPopup）
    // QML 侧可用于门控工作区点击/拖拽/连线渲染等交互
    Q_PROPERTY(bool hasPopup READ hasPopup NOTIFY hasPopupChanged)
    // 阶段 11.5：全局右键菜单存在标志（对应 IBR_PopupManager::HasRightClickMenu）
    Q_PROPERTY(bool hasRightClickMenu READ hasRightClickMenu NOTIFY hasRightClickMenuChanged)
    // 阶段 11.2：错误队列剩余条数
    Q_PROPERTY(int errorQueueSize READ errorQueueSize NOTIFY errorQueueSizeChanged)

public:
    explicit DialogController(QObject *parent = nullptr);

    // QML 调用：请求显示确认对话框（同步返回用户选择）
    // actionId 用于 QML 侧识别回调目标
    Q_INVOKABLE void showConfirm(const QString &title, const QString &message, const QString &actionId);
    // 阶段 11.3：请求显示三态确认对话框（保存/不保存/取消）
    // 对应 ImGui AskIfSave（IBR_ProjManager.cpp:234）
    Q_INVOKABLE void showConfirm3(const QString &title, const QString &message, const QString &actionId);
    // 请求显示导出对话框
    Q_INVOKABLE void showExportDialog();
    // 请求显示等待弹窗
    Q_INVOKABLE void showWaiting(const QString &message);
    // 关闭等待弹窗
    Q_INVOKABLE void hideWaiting();
    // 设置提示信息（对应 IBR_HintManager::SetHint，默认 -1 不倒计时）
    Q_INVOKABLE void setHint(const QString &text);
    // 设置带类型和停留时间的提示（durationMs: -1=永不倒计时, >0=倒计时毫秒）
    Q_INVOKABLE void setHintWithType(const QString &text, int type, int durationMs);

    // ===== 阶段 10.3 新增：Hint 队列轮播（对应 IBR_Components.cpp:801-908 IBR_HintManager） =====
    void loadHintPool();
    // 首帧固定显示 Hint[0]（对应 ImGui UseHint 初始值=0，首帧不切换）
    Q_INVOKABLE QString firstHint() const;
    // 后续随机切换（对应 ImGui UseHint = rand() % Hint.size()）
    Q_INVOKABLE QString nextRandomHint() const;
    Q_INVOKABLE void setHintCustom(const QString &text);
    Q_INVOKABLE void clearHintCustom();
    QString customHint() const { return m_customHint; }

    // ===== 阶段 3 新增：PopupManager Hook =====
    // v3 批次 2.2：新增 size 参数（对应 ImGui Popup.Size，{0,0} 表示自动尺寸）
    // Q_INVOKABLE：QtMain.cpp 中通过 QMetaObject::invokeMethod 跨线程调用（钩子在渲染线程触发）
    Q_INVOKABLE void onPopupHooked(const QString &title, const QStringList &texts, bool modal, bool canClose, const QSizeF &size);
    // 阶段 11.4：右键菜单 Hook（从 IBR_PopupManager::SetRightClickMenu 钩子转发）
    Q_INVOKABLE void onRightClickMenuHooked(const QString &title, const QStringList &items, int posX, int posY);
    // 阶段 11.5：弹窗关闭通知（对应 ClearCurrentPopup）
    Q_INVOKABLE void onPopupCleared();
    // 阶段 11.5：右键菜单关闭通知（对应 ClearRightClickMenu）
    Q_INVOKABLE void onRightClickMenuCleared();

    // ===== 阶段 11.2 新增：错误队列（对应 IBR_ErrorCollector ErrorList） =====
    // 从 IBR_ErrorCollector::AddErrorPopup 钩子转发
    // forJson=true 时 QML 侧需展示 ±4 行上下文（对应 IBR_ErrorCollector.cpp:71-92）
    Q_INVOKABLE void onErrorPushed(const QString &title, const QString &errorStr,
                       const QString &info, bool forJson);
    int errorQueueSize() const { return static_cast<int>(m_errorQueue.size()); }
    // QML 调用：获取当前错误条目（不弹出）
    Q_INVOKABLE QVariantMap currentError() const;
    // QML 调用：用户关闭当前错误后推进到下一条（对应 ErrorListShown++）
    Q_INVOKABLE void advanceErrorQueue();
    // QML 调用：清空错误队列（对应 IBR_ErrorCollector::ClearDelayed）
    Q_INVOKABLE void clearErrorQueue();

    // ===== 阶段 11.1 新增：导入预览状态管理（对应 IBR_ImportPreview::Open） =====
    // 从 IBR_ImportPreview::SetHook 钩子转发（在渲染线程被调用，QtMain 通过 invokeMethod 切到 GUI 线程）
    // 存储 ImportedIniFile 和 callback，供 QML 读取数据并在确认时回调
    void onImportPreviewRequested(ImportedIniFile &&file,
                                  std::function<void(const IBR_ImportResult &)> callback);
    // QML 调用：获取预览数据（sections 列表 + iniTypeOptions + availableRegTypes + conflicts）
    Q_INVOKABLE QVariantMap importPreviewData() const;
    // QML 调用：用户确认导入
    // iniType: 用户选择的 INI 类型
    // selectedIndices: 勾选的 section Index 列表
    // unmatchedRegTypes: 未匹配 section 的 RegType 选择（key=section Index 字符串, value=RegType 名）
    Q_INVOKABLE void confirmImportPreview(const QString &iniType,
                                          const QVariantList &selectedIndices,
                                          const QVariantMap &unmatchedRegTypes);
    // QML 调用：用户取消导入
    Q_INVOKABLE void cancelImportPreview();
    // 是否有待处理的导入预览
    Q_INVOKABLE bool hasImportPreview() const { return m_importCallback != nullptr; }

    // 阶段 13.3.2：供 ProjectController 注册 SHP 确认回调（在 onDropFiles 中调用）
    using ShpConfirmCallback = std::function<void(const std::vector<int> &)>;
    void setShpConfirmCallback(ShpConfirmCallback cb) { m_shpCallback = std::move(cb); }

    // ===== 阶段 13.3.2 新增：SHP 文件类型选择弹窗（对应 IBR_ProjManager.cpp:1129-1217） =====
    // QML 调用：请求显示 SHP 类型选择弹窗
    // names: SHP 文件名列表（不含扩展名，已转大写）
    Q_INVOKABLE void requestShpTypeSelection(const QStringList &names);
    // QML 调用：获取待选择的 SHP 文件名列表
    Q_INVOKABLE QStringList shpFileNames() const { return m_shpFileNames; }
    // QML 调用：用户确认选择
    // types: 每个 SHP 文件对应的类型（0=Animation, 1=Building, 2=Infantry, 3=Vehicle）
    Q_INVOKABLE void confirmShpTypeSelection(const QVariantList &types);
    // QML 调用：用户取消
    Q_INVOKABLE void cancelShpTypeSelection();

    // ===== 访问器 =====
    bool hasPopup() const { return m_hasPopup; }
    bool hasRightClickMenu() const { return m_hasRightClickMenu; }

signals:
    void confirmRequested(const QString &title, const QString &message, const QString &actionId);
    // 阶段 11.3：三态确认信号
    void confirm3Requested(const QString &title, const QString &message, const QString &actionId);
    void exportRequested();
    void waitingShown(const QString &message);
    void waitingHidden();
    void hintSet(const QString &text);
    void hintSetWithType(const QString &text, int type, int durationMs);

    // v3 批次 2.2：新增 size 参数（对应 ImGui Popup.Size，{0,0} 表示自动尺寸）
    void popupRequested(const QString &title, const QStringList &texts, bool modal, bool canClose, const QSizeF &size);
    // 阶段 11.4：右键菜单请求信号
    void rightClickMenuRequested(const QString &title, const QStringList &items, int posX, int posY);
    // 阶段 11.1：导入预览请求信号（QML 侧监听后调用 importPreviewData() 获取数据并显示弹窗）
    void importPreviewRequested();
    // 阶段 13.3.2：SHP 类型选择请求信号（QML 侧监听后调用 shpFileNames() 获取文件列表并显示弹窗）
    void shpTypeSelectionRequested();

    void confirmCompleted(const QString &actionId, bool accepted);
    // 阶段 11.3：三态确认完成信号
    // result: 0=取消, 1=保存, 2=不保存
    void confirm3Completed(const QString &actionId, int result);
    // 阶段 11.4：右键菜单选择完成信号
    // index: -1=取消, >=0=选中项索引
    void rightClickMenuCompleted(int index);

    void customHintChanged();
    // 阶段 11.5
    void hasPopupChanged();
    void hasRightClickMenuChanged();
    // 阶段 11.2
    void errorQueueSizeChanged();
    // 阶段 11.2：当前错误条目变化（advanceErrorQueue/clearErrorQueue 时触发）
    // QML 侧 Connections 监听此信号后调用 currentError() 获取新条目并显示
    void currentErrorChanged();

public slots:
    void notifyConfirmResult(const QString &actionId, bool accepted);
    // 阶段 11.3：QML 侧用户点击三态按钮后调用
    // result: 0=取消, 1=保存, 2=不保存
    void notifyConfirmResult3(const QString &actionId, int result);
    // 阶段 11.4：QML 侧用户在右键菜单中选择后调用
    void notifyRightClickMenuResult(int index);

private:
    // 阶段 10.3：hint.txt 加载的内容
    QStringList m_hintPool;
    QString m_customHint;

    // 阶段 11.5：弹窗/右键菜单状态
    bool m_hasPopup{false};
    bool m_hasRightClickMenu{false};

    // 阶段 11.2：错误队列
    struct ErrorEntry {
        QString title;
        QString errorStr;
        QString info;
        bool forJson{false};
    };
    std::vector<ErrorEntry> m_errorQueue;

    // 阶段 11.1：导入预览状态（对应 IBR_ImportPreview::g_File + g_Callback）
    ImportedIniFile m_importFile;
    std::function<void(const IBR_ImportResult &)> m_importCallback;
    QStringList m_iniTypeOptions;
    QStringList m_availableRegTypes;
    QStringList m_conflictSections;

    // 阶段 13.3.2：SHP 类型选择状态
    // m_shpFileNames 存储 SHP 文件名（不含扩展名，已转大写）
    // m_shpCallback 在用户确认后回调 ProjectController 创建模块
    QStringList m_shpFileNames;
    ShpConfirmCallback m_shpCallback;

    void setHasPopup(bool v);
    void setHasRightClickMenu(bool v);
};
