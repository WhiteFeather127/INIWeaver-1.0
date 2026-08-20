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

QString LocalizationController::trF(const QString &key, const QVariantList &args) const
{
    // 带格式的翻译查询：查 IBR_L10n::GetString 后，把 C 风格格式符（%s %d %zu %.1f ...）
    // 与 {}（std::vformat）占位符统一转为 Qt 的 %1 %2 ...，再按顺序用 args 填充。
    // 数值/精度由 QML 侧先格式化（如 fps.toFixed(1)）以字符串传入，避免类型转换歧义。
    auto &raw = IBR_L10n::GetString(key.toUtf8().constData());
    if (raw.empty())
        return key;
    const QString fmt = QString::fromUtf8(raw.c_str());

    // 第一遍：解析格式串 → Qt 的 %N 占位（低号优先替换）
    QString qtFmt;
    qtFmt.reserve(fmt.size() + 16);
    int argIndex = 1;
    const int n = fmt.size();
    int i = 0;
    while (i < n)
    {
        const QChar c = fmt.at(i);
        if (c == QLatin1Char('%'))
        {
            if (i + 1 < n && fmt.at(i + 1) == QLatin1Char('%'))
            {
                // %% → 字面 %
                qtFmt += QLatin1Char('%');
                i += 2;
                continue;
            }
            // 解析 %[flags][width][.precision][length]type
            int j = i + 1;
            while (j < n && QStringLiteral("-+ #0").contains(fmt.at(j))) ++j;              // flags
            while (j < n && (fmt.at(j).isDigit() || fmt.at(j) == QLatin1Char('*'))) ++j;   // width
            if (j < n && fmt.at(j) == QLatin1Char('.'))                                     // .precision
            {
                ++j;
                while (j < n && (fmt.at(j).isDigit() || fmt.at(j) == QLatin1Char('*'))) ++j;
            }
            while (j < n && QStringLiteral("hlLzjt").contains(fmt.at(j))) ++j;             // length
            if (j < n && QStringLiteral("diouxXfFeEgGaAcsp").contains(fmt.at(j)))           // type
            {
                qtFmt += QStringLiteral("%") + QString::number(argIndex++);
                i = j + 1;
                continue;
            }
        }
        else if (c == QLatin1Char('{') && i + 1 < n && fmt.at(i + 1) == QLatin1Char('}'))
        {
            // {} 风格（std::vformat）
            qtFmt += QStringLiteral("%") + QString::number(argIndex++);
            i += 2;
            continue;
        }
        qtFmt += c;
        ++i;
    }

    // 第二遍：按顺序填充 %1 %2 ...（QML 已把数值格式化为字符串）
    QString result = qtFmt;
    const qsizetype argCount = qMin(args.size(), qsizetype(argIndex - 1));
    for (qsizetype k = 0; k < argCount; ++k)
    {
        const QVariant &v = args.at(k);
        result = result.arg(v.isValid() ? v.toString() : QString());
    }
    return result;
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
