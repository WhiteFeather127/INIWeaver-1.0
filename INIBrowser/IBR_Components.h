#pragma once
#include "FromEngine/Include.h"
#include "FromEngine/RFBump.h"
#include "FromEngine/ImGuiDeps.h"

struct IBR_Section;
struct IBR_Project;
struct IBB_Section_Desc;

namespace IBR_SelectMode
{
    const std::vector<ModuleID_t>& GetMassSelected();
    bool IsWindowMassSelected(const IBB_Section_Desc& Desc);
    void RenderUI();
}

namespace IBR_DynamicData
{
    void Read(int DefaultResX, int DefaultResY);
    void SetRandom();
    void Open();
    void Save();
    void Close();
    void SetDefaultWidth(int W);
    void SetDefaultHeight(int H);
}

namespace IBR_RecentManager
{
    extern std::wstring Path;
    void RenderUI();
    void Load();
    void Push(const std::wstring& Path);
    void Save();
    void WanDuZiLe();
    // Qt 迁移：暴露最近文件列表（UTF-8），供 ProjectController 读取
    const std::vector<std::string>& GetRecentList();
}


namespace IBR_PopupManager
{
    struct Popup
    {
        bool CanClose;//Only when Modal==true
        bool Modal;
        bool HasOwnStyle{ false };
        bool InstantClose{ false };
        _TEXT_UTF8 std::string Title;
        ImGuiWindowFlags Flag{ ImGuiWindowFlags_None };
        StdMessage Show;
        StdMessage Close;//run when it can be closed and it is closed

        ImVec2 Size{ 0,0 };

        // Qt UI Hook：存储文本内容供 QML 弹窗读取
        std::vector<std::string> Texts;

        Popup& Create(const _TEXT_UTF8 std::string& title);
        Popup& CreateModal(const _TEXT_UTF8 std::string& title, bool canclose, StdMessage close = []() {});
        Popup& SetTitle(const _TEXT_UTF8 std::string& title);
        Popup& SetFlag(ImGuiWindowFlags flag);
        Popup& UnsetFlag(ImGuiWindowFlags flag);
        Popup& ClearFlag();
        Popup& UseMyStyle();
        Popup& SetSize(ImVec2 NewSize = { 0,0 });//{0,0}=default/auto
        Popup& PushTextPrev(const _TEXT_UTF8 std::string& Text);
        Popup& PushTextBack(const _TEXT_UTF8 std::string& Text);
        Popup& PushMsgPrev(StdMessage Msg);
        Popup& PushMsgBack(StdMessage Msg);
        Popup& EnableInstantClose(bool Enable = true) { InstantClose = Enable; return *this; }
    };
    extern Popup CurrentPopup;
    extern Popup RightClickMenu;
    extern bool HasPopup;
    extern bool HasRightClickMenu;
    extern bool FirstRightClick;
    extern ImVec2 RightClickMenuPos;
    extern std::vector<StdMessage> DelayedPopupAction;

    // Qt UI Hook：允许 Qt 注册回调接收 ImGui 弹窗请求
    // 当 SetCurrentPopup 被调用时，钩子被触发，Qt 侧转发到 QML 弹窗
    using PopupHookFn = std::function<void(const Popup&)>;
    void SetPopupHook(PopupHookFn fn);
    void SetRightClickMenuHook(PopupHookFn fn);

    void SetCurrentPopup(Popup&& sc);
    void UpdatePopupPosForResize();
    // 阶段 11.5：ClearCurrentPopup 不再 inline，实现中会通知 Qt Hook 同步 hasPopup=false
    void ClearCurrentPopup();
    void ClearRightClickMenu();
    void SetRightClickMenu(Popup&& sc, ImVec2 Pos);
    Popup SingleText(const _TEXT_UTF8 std::string& StrId, const _TEXT_UTF8 std::string& Text, bool Modal);
    Popup MessageModal(const _TEXT_UTF8 std::string& Title, const _TEXT_UTF8 std::string& Text, ImVec2 Size, bool CanClose, bool UseDefaultOK, StdMessage Close = []() {});
    void RenderUI();
    void ClearPopupDelayed();
    bool IsMouseOnPopup();
    void AddJsonParseErrorPopup(std::string&& ErrorStr, const std::string& Info);
    void AddModuleParseErrorPopup(std::string&& ErrorStr, const std::string& Info);
    void AddLoadConfigErrorPopup(std::string&& ErrorStr, const std::string& Info);
    void AddOutputErrorPopup(std::string&& ErrorStr, const std::string& Info);
    void CurrentPopupFixPos(int OldW, int OldH, int NewW, int NewH);
}


template<typename Cont>
class IBR_ListMenu
{
    std::vector<Cont>& List;
    int Page;
public:
    typedef Cont Type;
    typedef std::function<void(Cont&, int, int)> ActionType;
    std::string Tag;
    ActionType Action;

    IBR_ListMenu() = delete;
    IBR_ListMenu(std::vector<Cont>& L, const std::string& t, const ActionType& a) :
        List(L), Page(0), Tag(t), Action(a) {
    }

    void Rewind()
    {
        Page = 0;
    }
    void RenderUI();
};

namespace IBR_HintManager
{
    void Clear();
    void RenderUI();
    float GetHeight();
    void SetHint(const _TEXT_UTF8 std::string& Str, int TimeLimitMillis = -1);
    void SetHintCustom(const std::function<bool(_TEXT_UTF8 std::string&)>& Fn);//返回true继续，false停止并Clear
    const std::string& GetHint();
    void Load();
}


struct BrowseParamList
{
    int RenderF;
    int RenderN;
    int Sz;
    bool HasPrev;
    bool HasNext;
    int RealRF;
    int RealNF;
    int PageN;
};
BrowseParamList MakeParamList(size_t size, int Page);
void Browse_ShowList_Impl(const std::string& suffix, int* Page, BrowseParamList& List);

template<typename Cont>
void Browse_ShowList(const std::string& suffix, std::vector<Cont>& Ser, int* Page, const std::function<void(Cont&, int, int)>& Callback)
{
    BrowseParamList L{ MakeParamList(Ser.size(), *Page) };
    for (int On = L.RealRF; On < L.RealNF; On++)
        Callback(Ser.at(On), On - L.RealRF + 1, On);
    Browse_ShowList_Impl(suffix, Page, L);
}

template<typename Cont>
void IBR_ListMenu<Cont>::RenderUI()
{
    Browse_ShowList(Tag, List, &Page, Action);
}
