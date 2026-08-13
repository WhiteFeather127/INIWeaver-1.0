// MenuController.h
// Qt6 菜单状态控制器：管理当前激活的菜单面板
// 阶段 3.4：对应 ImGui 的 IBR_Inst_Menu.Choice
// 菜单 ID 与 IBR_Misc.h 的 MenuItemID_* 宏对齐
#pragma once

#include <QObject>
#include <QtQmlIntegration/qqmlintegration.h>

class MenuController : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(int activeMenu READ activeMenu WRITE setActiveMenu NOTIFY activeMenuChanged)
    Q_PROPERTY(bool canEdit READ canEdit CONSTANT)
    Q_PROPERTY(bool debugMenuEnabled READ debugMenuEnabled NOTIFY debugMenuEnabledChanged)

public:
    explicit MenuController(QObject *parent = nullptr);

    int activeMenu() const { return m_activeMenu; }
    void setActiveMenu(int v);

    // IBR_Panel.cpp:219 always returns false（Edit 菜单永远禁用）
    bool canEdit() const { return false; }

    // 对应 Initialize.cpp 的 EnableDebugList（-debugmenu 启动参数）
    bool debugMenuEnabled() const;

signals:
    void activeMenuChanged();
    void debugMenuEnabledChanged();

public slots:
    // 命令行解析完成后调用，刷新 debugMenuEnabled 状态
    void refreshDebugMenu();

private:
    int m_activeMenu{0};  // 默认 File 菜单
    bool m_debugMenuEnabled{false};
};
