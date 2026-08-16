// SettingController.cpp
#include "SettingController.h"
#include "Global.h"
#include "IBB_Setting.h"
#include "IBR_Project.h"  // IBR_Inst_Setting.CallSaveSetting
#include "IBR_Localization.h"  // 阶段 13.1：IBR_L10n 语言切换
#include <algorithm>

SettingController::SettingController(QObject *parent)
    : QObject(parent)
{
}

bool SettingController::darkMode() const { return IBF_Inst_Setting.IsDarkMode(); }
bool SettingController::openFolderOnOutput() const { return IBF_Inst_Setting.OpenFolderOnOutput(); }
bool SettingController::outputOnSave() const { return IBF_Inst_Setting.OutputOnSave(); }
int SettingController::frameRateLimit() const { return IBF_Inst_Setting.FrameRateLimit(); }
int SettingController::autoWrapThreshold() const { return IBF_Inst_Setting.List.Pack.AutoWrapThreshold; }
// v3 批次 1.1：透明度基准（对应 IBF_Inst_Setting.TransparencyBase() = WindowTransparencyLevel * 0.1f）
float SettingController::transparencyBase() const { return IBF_Inst_Setting.TransparencyBase(); }
QString SettingController::outputDir() const
{
    return QString::fromUtf8(IBF_Inst_Setting.OutputDir());
}

// 阶段 13.1：语言相关接口实现（对应 IBR_L10n::RenderUI / SetLanguage）
QString SettingController::currentLanguage() const
{
    return QString::fromUtf8(IBR_L10n::GetCurrentLanguage());
}

QString SettingController::currentLanguageName() const
{
    // 对应 RenderUI 中 CurrentMap["LangName"]（若不存在回退 CurrentLanguage）
    auto &cur = IBR_L10n::GetString("LangName");
    if (cur.empty() || cur.rfind("MISSING:", 0) == 0)
        return QString::fromUtf8(IBR_L10n::GetCurrentLanguage());
    return QString::fromUtf8(cur);
}

QVariantList SettingController::availableLanguages() const
{
    QVariantList list;
    for (const auto &[key, displayName] : IBR_L10n::GetAvailableLanguages())
    {
        QVariantMap item;
        item["key"] = QString::fromUtf8(key);
        item["displayName"] = QString::fromUtf8(displayName);
        list.append(item);
    }
    return list;
}

void SettingController::setLanguage(const QString &key)
{
    // 对应 IBR_L10n::SetLanguage：写 Language.ini + 刷新 CurrentMap + RefreshSettingTypes/RefreshSetting
    IBR_L10n::SetLanguage(key.toStdString());
    // 切换后重新读取设置项（本地化文本已变）
    m_cachedTypes.clear();
    emit languagesChanged();
    emit settingsChanged();
}

QVariantList SettingController::settingTypes() const
{
    QVariantList list;
    for (const auto &t : IBF_Inst_Setting.List.Types)
    {
        QVariantMap item;
        item["name"] = QString::fromUtf8(t.DescShortOri);
        item["descShort"] = QString::fromUtf8(t.DescShort);
        item["descLong"] = QString::fromUtf8(t.DescLong);
        // 类型标识：0=None,1=IntA,2=IntB,3=Bool,4=Lang
        item["type"] = static_cast<int>(t.Type);
        // 阶段 6.2：暴露当前值（对应 IBR_Setting.cpp:88 Li.Action() 读取的状态）
        switch (t.Type)
        {
        case IBB_SettingType::IntA:
        case IBB_SettingType::IntB:
            if (t.Data) item["value"] = *static_cast<const int32_t*>(t.Data);
            break;
        case IBB_SettingType::Bool:
            if (t.Data) item["value"] = *static_cast<const bool*>(t.Data);
            break;
        case IBB_SettingType::Lang:
            // Lang 类型暂不暴露 value（需通过 availableLanguages 单独处理）
            break;
        case IBB_SettingType::None:
        default:
            break;
        }
        // 阶段 6.2：暴露数值范围（对应 ImGui 的 InputInt min/max）
        if (!t.Limit.empty() &&
            (t.Type == IBB_SettingType::IntA || t.Type == IBB_SettingType::IntB))
        {
            QVariantList limits;
            for (const auto &lim : t.Limit)
            {
                if (lim) limits.append(*static_cast<const int32_t*>(lim));
            }
            item["limits"] = limits;
        }
        list.append(item);
    }
    return list;
}

// 阶段 6.2：设置项写回（对应 IBR_Setting.cpp:88-93 Li.Action() 触发 Changing）
void SettingController::setSettingValue(const QString &name, const QVariant &value)
{
    auto target = name.toStdString();
    for (auto &t : IBF_Inst_Setting.List.Types)
    {
        if (t.DescShortOri != target) continue;
        switch (t.Type)
        {
        case IBB_SettingType::IntA:
            if (t.Data)
            {
                int32_t v = value.toInt();
                // 合法区间检查（对应 ImGui InputInt：从 i=1 起步长 2 检查 [Limit[i], Limit[i+1]]）
                // 不在任何合法区间则回退 Def=Limit[0]；-1 等 SpV 特殊值（单独区间）可正常接受
                bool legal = false;
                for (size_t i = 1; i + 1 < t.Limit.size(); i += 2)
                {
                    if (t.Limit[i] && t.Limit[i + 1])
                    {
                        int32_t lo = *static_cast<const int32_t*>(t.Limit[i]);
                        int32_t hi = *static_cast<const int32_t*>(t.Limit[i + 1]);
                        if (lo <= v && v <= hi) { legal = true; break; }
                    }
                }
                if (!legal)
                {
                    if (!t.Limit.empty() && t.Limit[0])
                        v = *static_cast<const int32_t*>(t.Limit[0]);
                    else
                        v = 0;
                }
                *static_cast<int32_t*>(t.Data) = v;
            }
            break;
        case IBB_SettingType::IntB:
            if (t.Data)
            {
                int32_t v = value.toInt();
                // IntB 布局 {Min, Max, Format}：clamp 到 [Min, Max]（对应 SliderInt 范围）
                if (t.Limit.size() >= 1 && t.Limit[0])
                    v = std::max(v, *static_cast<const int32_t*>(t.Limit[0]));
                if (t.Limit.size() >= 2 && t.Limit[1])
                    v = std::min(v, *static_cast<const int32_t*>(t.Limit[1]));
                *static_cast<int32_t*>(t.Data) = v;
            }
            break;
        case IBB_SettingType::Bool:
            if (t.Data) *static_cast<bool*>(t.Data) = value.toBool();
            break;
        case IBB_SettingType::Lang:
            // 语言切换暂不支持（需额外接口）
            break;
        default:
            break;
        }
        *t.Changing = true;  // 标记为已修改，触发 IBR_Setting.cpp:117-125 的 CallSaveSetting
        // v3 批次 1.1：WindowTransparencyLevel 变化时通知 QML 重新计算节点/连线透明度
        if (target == "Back_DescShort_Transparency") {
            emit transparencyBaseChanged();
        }
        break;
    }
    emit settingsChanged();
}

// 阶段 6.2：触发保存到 setting 文件（对应 IBR_Setting.cpp:55-60 CallSaveSetting）
void SettingController::saveSettings()
{
    IBR_Inst_Setting.CallSaveSetting();
}

void SettingController::setDarkMode(bool v)
{
    IBF_Inst_Setting.List.Pack.DarkMode = v;
    emit darkModeChanged();
    emit settingsChanged();
}

void SettingController::setFrameRateLimit(int v)
{
    // 对应 IBG_SettingPack 的范围约束（min=15, max=2000, SpV=-1）
    using P = IBG_SettingPack;
    if (v != P::____FrameRateLimit_SpV)
    {
        v = std::clamp<int>(v, P::____FrameRateLimit_Min, P::____FrameRateLimit_Max);
    }
    IBF_Inst_Setting.List.Pack.FrameRateLimit = v;
    emit settingsChanged();
}

void SettingController::setOutputOnSave(bool v)
{
    IBF_Inst_Setting.List.Pack.OutputOnSave = v;
    emit settingsChanged();
}

void SettingController::setOpenFolderOnOutput(bool v)
{
    IBF_Inst_Setting.List.Pack.OpenFolderOnOutput = v;
    emit settingsChanged();
}

void SettingController::setAutoWrapThreshold(int v)
{
    using P = IBG_SettingPack;
    if (v != P::____AutoWrapThreshold_SpV)
    {
        v = std::clamp<int>(v, P::____AutoWrapThreshold_Min, P::____AutoWrapThreshold_Max);
    }
    IBF_Inst_Setting.List.Pack.AutoWrapThreshold = v;
    emit settingsChanged();
}

void SettingController::refresh()
{
    m_cachedTypes.clear();
    emit darkModeChanged();
    // v3 批次 1.1：刷新时同步通知透明度基准（项目加载/设置变更后）
    emit transparencyBaseChanged();
    emit settingsChanged();
}
