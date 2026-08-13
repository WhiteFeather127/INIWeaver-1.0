// SettingController.h
// Qt6 设置控制器：封装 IBF_Inst_Setting
// 阶段 3.5：暴露常用设置属性，替代 IBFront.cpp 的 UploadSettingMap
#pragma once

#include <QObject>
#include <QString>
#include <QVariantList>
#include <QtQmlIntegration/qqmlintegration.h>

class SettingController : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(bool darkMode READ darkMode WRITE setDarkMode NOTIFY darkModeChanged)
    Q_PROPERTY(bool openFolderOnOutput READ openFolderOnOutput WRITE setOpenFolderOnOutput NOTIFY settingsChanged)
    Q_PROPERTY(bool outputOnSave READ outputOnSave WRITE setOutputOnSave NOTIFY settingsChanged)
    Q_PROPERTY(int frameRateLimit READ frameRateLimit WRITE setFrameRateLimit NOTIFY settingsChanged)
    Q_PROPERTY(int autoWrapThreshold READ autoWrapThreshold WRITE setAutoWrapThreshold NOTIFY settingsChanged)
    Q_PROPERTY(QString outputDir READ outputDir NOTIFY settingsChanged)
    // 阶段 13.1：Lang 类型语言切换（对应 IBR_L10n::RenderUI 的 Combo）
    Q_PROPERTY(QString currentLanguage READ currentLanguage NOTIFY languagesChanged)
    Q_PROPERTY(QString currentLanguageName READ currentLanguageName NOTIFY languagesChanged)
    // v3 批次 1.1：全局透明度基准（对应 IBF_Inst_Setting.TransparencyBase()）
    // 默认 WindowTransparencyLevel=8，TransparencyBase=0.8
    Q_PROPERTY(float transparencyBase READ transparencyBase NOTIFY transparencyBaseChanged)

public:
    explicit SettingController(QObject *parent = nullptr);

    bool darkMode() const;
    bool openFolderOnOutput() const;
    bool outputOnSave() const;
    int frameRateLimit() const;
    int autoWrapThreshold() const;
    QString outputDir() const;
    // v3 批次 1.1：透明度基准（对应 IBF_Inst_Setting.TransparencyBase() = WindowTransparencyLevel * 0.1f）
    float transparencyBase() const;

    // 阶段 13.1：语言相关接口（对应 IBR_L10n::RenderUI / SetLanguage）
    QString currentLanguage() const;
    QString currentLanguageName() const;
    // 返回可用语言列表（每项含 key/displayName，跳过 "Basic"）
    Q_INVOKABLE QVariantList availableLanguages() const;
    // 通过语言 key 切换（对应 IBR_L10n::SetLanguage）
    Q_INVOKABLE void setLanguage(const QString &key);

    // 获取所有设置项（name/descShort/descLong/type/value/limits）供 QML 列表渲染
    Q_INVOKABLE QVariantList settingTypes() const;
    // 阶段 6.2：设置项写回（对应 IBR_Setting.cpp:88-93 Li.Action() 触发 Changing）
    Q_INVOKABLE void setSettingValue(const QString &name, const QVariant &value);
    // 阶段 6.2：触发保存到 setting 文件（对应 IBR_Setting.cpp:55-60 CallSaveSetting）
    Q_INVOKABLE void saveSettings();

public slots:
    void setDarkMode(bool v);
    void setFrameRateLimit(int v);
    void setOutputOnSave(bool v);
    void setOpenFolderOnOutput(bool v);
    void setAutoWrapThreshold(int v);
    void refresh();

signals:
    void darkModeChanged();
    void settingsChanged();
    void languagesChanged();
    // v3 批次 1.1：透明度基准变化信号（WindowTransparencyLevel 修改时触发）
    void transparencyBaseChanged();

private:
    mutable QVariantList m_cachedTypes;
};
