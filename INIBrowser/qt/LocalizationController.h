#pragma once
#include <QObject>
#include <QString>
#include <QStringList>
#include <QVariant>
#include <QVariantList>

// LocalizationController：Qt/QML 侧多语言翻译查询桥接层
// 对接 ImGui 版 IBR_L10n（INI 存储，locc("GUI_key") 查 CurrentMap）
//
// 职责分工：
//   - 语言切换：由 SettingController.setLanguage(key) 负责（已实现，写 Language.ini +
//     刷新 CurrentMap + emit languagesChanged）。本 controller 不重复切换逻辑。
//   - 翻译查询：本 controller 提供 tr(key)，QML 用 i18n.tr("GUI_SelectAll") 查询
//   - 自动刷新：监听 SettingController::languagesChanged，递增 rev 触发 QML 绑定重求值
//
// QML 用法（建立依赖 + 查询）：
//   text: (i18n.rev, i18n.tr("GUI_SelectAll"))
//   - i18n.rev 读取 Q_PROPERTY 建立对 languageChanged 信号的依赖
//   - i18n.tr(key) 查 IBR_L10n::GetString，找不到返回 key 本身（开发期可见缺失）
//
// 语言切换入口（复用现有）：
//   settingController.setLanguage("en")  // 已实现，切换后 i18n.rev 自动递增
class LocalizationController : public QObject
{
    Q_OBJECT
    // 刷新版本号：每次语言切换递增，QML 绑定引用它建立依赖
    // 用法：(i18n.rev, i18n.tr("key")) —— rev 建立依赖，逗号表达式返回 tr 结果
    Q_PROPERTY(int rev READ rev NOTIFY languageChanged)
public:
    explicit LocalizationController(QObject *parent = nullptr);

    // 查询翻译：key 为 GUI_* 前缀的字符串 key（对应 locc("GUI_SelectAll")）
    // 找不到返回 key 本身，开发期可直接看到未翻译项
    Q_INVOKABLE QString tr(const QString &key) const;

    // 带格式的翻译查询：支持 C 风格格式符（%s %d %zu %.1f ...）与 {}（std::vformat）占位符
    // 内部统一转为 Qt 的 %1 %2 ... 后按顺序填充；数值请由 QML 侧先格式化（如 fps.toFixed(1)）
    // 用法：i18n.trF("GUI_TopRightHint", [appName, version, fps])
    // 找不到 key 时返回 key 本身（与 tr 一致）
    Q_INVOKABLE QString trF(const QString &key, const QVariantList &args) const;

    // 连接 SettingController::languagesChanged → 本控制器递增 rev + emit languageChanged
    // 在 QtMain 中创建后立即调用，建立信号链
    void connectToLanguageSignal(class SettingController *settingController);

    int rev() const { return m_rev; }

signals:
    // 语言切换信号：rev 属性依赖它，QML 绑定引用 rev 即建立依赖，切换时自动刷新
    void languageChanged();

private:
    int m_rev{0};
};
