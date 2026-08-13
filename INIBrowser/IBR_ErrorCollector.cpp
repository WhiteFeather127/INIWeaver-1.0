// IBR_ErrorCollector.cpp
// 错误报告收集器实现
// 从 IBR_Components.cpp 迁移而来，将错误收集状态与 ImGui 弹窗渲染逻辑集中于此
// 业务层通过 IBB_ErrorReport 上报错误，渲染层通过 IBR_PopupManager 显示弹窗
#include "IBR_ErrorCollector.h"
#include "IBR_Components.h"
#include "IBR_Localization.h"
#include "Global.h"
#include "FromEngine/RFBump.h"
#include "FromEngine/ImGuiDeps.h"
#include <ranges>

// 匿名命名空间：内部状态与实现，仅本文件可见
namespace
{
    struct ErrorEntry
    {
        std::string Title;
        std::string ErrorStr;
        std::string Info;
        bool ForJson;
    };
    std::vector<ErrorEntry> ErrorList;
    size_t ErrorListShown = 0;

    // 阶段 11.2：Qt 错误钩子（可选，由 QtMain.cpp 注册）
    IBR_ErrorCollector::ErrorHookFn g_errorHook;

    void ShowErrorPopupImpl();

    // 核心入队函数：等价于原 IBR_PopupManager::AddErrorPopup
    void AddErrorPopup(std::string&& ErrorStr, const std::string& Title,
                       const std::string& Info, const std::wstring& LogFormat, bool ForJson)
    {
        if (ErrorStr.empty())return;
        if (ErrorListShown != 0)
        {
            ErrorList.erase(ErrorList.begin(), ErrorList.begin() + ErrorListShown);
            ErrorListShown = 0;
        }
        if (ErrorListShown == ErrorList.size())
            IBRF_CoreBump.SendToR({ [] { ShowErrorPopupImpl(); } });
        if (EnableLog)
        {
            GlobalLog.AddLog_CurTime(false);
            auto V = UTF8toUnicode(ErrorStr);
            GlobalLog.AddLog(std::vformat(LogFormat, std::make_wformat_args(V)));
        }
        // 阶段 11.2：同步转发到 Qt 错误队列（若已注册钩子）
        // 注意：ErrorStr 即将被 move，先复制一份给 Qt 侧
        if (g_errorHook) {
            std::string ErrCopy = ErrorStr;
            g_errorHook(Title, ErrCopy, Info, ForJson);
        }
        ErrorList.push_back({ Title, std::move(ErrorStr), Info, ForJson });
    }

    // 等价于原 IBR_PopupManager::ShowJsonParseErrorImpl
    void ShowErrorPopupImpl()
    {
        if (ErrorListShown < ErrorList.size())
        {
            auto& _W = ErrorList[ErrorListShown];
            IBR_PopupManager::SetCurrentPopup(
                std::move(IBR_PopupManager::Popup{}
                    .CreateModal(_W.Title, true, []() { {
                            ErrorListShown++;
                            IBRF_CoreBump.SendToR({ [] { ShowErrorPopupImpl(); } });
                        }})
                .SetFlag(ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_AlwaysAutoResize)
                .PushMsgBack([]()
                    {
                        auto& W = ErrorList[ErrorListShown];
                        ImGui::Text(W.Info.c_str());
                        if (!W.ForJson)
                        {
                            ImGui::Text(W.ErrorStr.c_str());
                            return;
                        }
                        auto V = W.ErrorStr | std::views::split('\n');
                        int TgIdx = 0;
                        int Idx = 1;
                        for (auto L : V)
                        {
                            auto R = std::string_view{ L.data(), L.size() };
                            if (R.find(loc("Error_JsonParseErrorPos")) != R.npos)
                            {
                                TgIdx = Idx;
                                ImGui::Text(locc("Error_JsonParseErrorLine"), Idx);
                                break;
                            }
                            Idx++;
                        }
                        Idx = 1;
                        for (auto L : V)
                        {
                            if (Idx >= TgIdx - 4 && Idx <= TgIdx + 4)
                                ImGui::Text(std::string(L.data(), L.size()).c_str());
                            Idx++;
                        }
                        ImGui::Text(locc("GUI_SeeLogForDetails"));
                    }))
            );
        }
        else IBR_ErrorCollector::ClearDelayed();
    }
}

// 业务层错误上报接口实现
namespace IBB_ErrorReport
{
    void AddJsonParseErrorPopup(std::string&& ErrorStr, const std::string& Info)
    {
        AddErrorPopup(std::move(ErrorStr), loc("GUI_JsonParseError"), Info, locw("Log_JsonParseErrorInfo"), true);
    }
    void AddModuleParseErrorPopup(std::string&& ErrorStr, const std::string& Info)
    {
        AddErrorPopup(std::move(ErrorStr), loc("GUI_ModuleParseError"), Info, locw("Log_ModuleParseErrorInfo"), false);
    }
    void AddLoadConfigErrorPopup(std::string&& ErrorStr, const std::string& Info)
    {
        AddErrorPopup(std::move(ErrorStr), loc("GUI_LoadConfigError"), Info, locw("Log_LoadConfigErrorInfo"), false);
    }
    void AddOutputErrorPopup(std::string&& ErrorStr, const std::string& Info)
    {
        AddErrorPopup(std::move(ErrorStr), loc("GUI_ExportIniError"), Info, locw("Log_ExportIniErrorInfo"), false);
    }
}

// 渲染层错误收集器接口实现
namespace IBR_ErrorCollector
{
    void ClearDelayed()
    {
        ErrorList.clear();
        ErrorListShown = 0;
        IBRF_CoreBump.SendToR({ [] { IBR_PopupManager::ClearCurrentPopup(); } });
    }

    // 阶段 11.2：注册 Qt 错误钩子
    void SetErrorHook(ErrorHookFn fn)
    {
        g_errorHook = std::move(fn);
    }
}
