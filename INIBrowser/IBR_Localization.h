#pragma once
#include "FromEngine/global_tool_func.h"
#include "IBG_Ini.h"


namespace IBR_L10n
{
    bool LoadFromINI(const std::wstring& FileName);
    const std::string& _TEXT_UTF8 GetString(const std::string& Key);
    const std::wstring& _TEXT_WIDE GetWString(const std::string& Key);
    std::string _TEXT_UTF8 GetStringAligned(const std::string& Key, int AlignMax);
    std::wstring _TEXT_WIDE GetWStringAligned(const std::string& Key, int AlignMax);
    const std::string& _TEXT_UTF8 GetStringOr(const std::string& Key, const std::string& Fallback);
    const std::wstring& _TEXT_WIDE GetWStringOr(const std::string& Key, const std::wstring& Fallback);
    void SetLanguage(const std::string& Language);
    bool RenderUI(std::string_view Title);
    std::string ProcessEscape(const std::string& v);

    // 阶段 13.1：Qt 侧语言切换支持（对应 RenderUI 内 Combo 的数据源）
    // 返回当前语言 key（对应 CurrentLanguage）
    const std::string& GetCurrentLanguage();
    // 返回可用语言列表（key, LangName 显示名），跳过 "Basic"
    // 对应 RenderUI 中 for(auto&[k,v]:LocalizationMap) if(k=="Basic")continue; 分支
    std::vector<std::pair<std::string, std::string>> GetAvailableLanguages();
}

#define loc(x) IBR_L10n::GetString((x))
#define aloc(x,n) IBR_L10n::GetStringAligned((x),(n))
#define oloc(x,f) IBR_L10n::GetStringOr((x),(f))
#define locw(x) IBR_L10n::GetWString((x))
#define alocw(x,n) IBR_L10n::GetWStringAligned((x),(n))
#define olocw(x,f) IBR_L10n::GetWStringOr((x),(f))
#define locc(x) IBR_L10n::GetString((x)).c_str()
#define alocc(x,n) IBR_L10n::GetStringAligned((x),(n)).c_str()
#define olocc(x,f) IBR_L10n::GetStringOr((x),(f)).c_str()
#define locwc(x) IBR_L10n::GetWString((x)).c_str()
#define alocwc(x,n) IBR_L10n::GetWStringAligned((x),(n)).c_str()
#define olocwc(x,f) IBR_L10n::GetWStringOr((x),(f)).c_str()


#define _AppName locc("AppName")
#define _AppNameW locwc("AppName")
