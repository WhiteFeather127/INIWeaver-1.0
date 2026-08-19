#include "LocalizationController.h"
#include "IBR_Localization.h"
#include "SettingController.h"

LocalizationController::LocalizationController(QObject *parent)
    : QObject(parent)
{
}

QString LocalizationController::tr(const QString &key) const
{
    // 查 IBR_L10n::GetString（对应 locc(key)），找不到返回 key 本身
    auto &str = IBR_L10n::GetString(key.toUtf8().constData());
    if (str.empty())
        return key;
    return QString::fromUtf8(str.c_str());
}

void LocalizationController::connectToLanguageSignal(SettingController *settingController)
{
    if (!settingController)
        return;
    // 语言切换链：SettingController.setLanguage → IBR_L10n::SetLanguage（刷新 CurrentMap）
    //   → emit languagesChanged → 本控制器递增 rev + emit languageChanged
    //   → QML 所有 (i18n.rev, i18n.tr(...)) 绑定自动重新求值
    QObject::connect(settingController, &SettingController::languagesChanged,
                     this, [this]() {
                         ++m_rev;
                         emit languageChanged();
                     });
    // 初始 rev 设为 1，让 QML 首次绑定就建立对 languageChanged 的依赖
    ++m_rev;
}
