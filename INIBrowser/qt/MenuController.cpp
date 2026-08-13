// MenuController.cpp
#include "MenuController.h"

// Initialize.cpp:21
extern bool EnableDebugList;

MenuController::MenuController(QObject *parent)
    : QObject(parent)
{
}

void MenuController::setActiveMenu(int v)
{
    if (m_activeMenu == v) return;
    m_activeMenu = v;
    emit activeMenuChanged();
}

bool MenuController::debugMenuEnabled() const
{
    return m_debugMenuEnabled;
}

void MenuController::refreshDebugMenu()
{
    bool newVal = EnableDebugList;
    if (m_debugMenuEnabled != newVal)
    {
        m_debugMenuEnabled = newVal;
        emit debugMenuEnabledChanged();
    }
}
