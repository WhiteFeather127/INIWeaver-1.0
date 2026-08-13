// IBR_ModuleTree.cpp
// 模块树渲染实现
// 从 IBB_ModuleAlt.cpp 迁移而来，将 ImGui 渲染逻辑集中于此
// 业务层（IBB_ModuleAlt）维护 ModuleTree 数据结构，渲染层（IBR_ModuleTree）负责绘制
// 设计要点：
//   - DrawFolderIcon / DrawOpenFolderIcon 为文件内静态实现，不暴露到头文件
//   - RenderModuleTreeUI 为文件内静态函数，对应原 ModuleTree::RenderUI() 递归渲染
//   - Tree_RenderUI / SpecialTree_RenderUI / Tree_RenderUISidebar 为公开接口
#include "IBR_ModuleTree.h"
#include "IBB_ModuleAlt.h"
#include "FromEngine/ImGuiDeps.h"
#include "IBR_Components.h"
#include "IBR_Misc.h"
#include "IBR_Localization.h"
#include "Global.h"
#include "FromEngine/global_tool_func.h"
#include <functional>

// 前向声明：定义于 IBR_WorkSpace.cpp
namespace SearchModuleAlt
{
    void RenderModuleAltSelect(IBB_ModuleAlt* pModule);
}

// 前向声明：定义于 MainStage.h（inline）
namespace ImGui
{
    ImVec2 GetLineEndPos();
}

extern int FontHeight;

namespace
{
    // 绘制关闭状态的文件夹图标（黄色主体 + 顶部曲线）
    void DrawFolderIcon(ImVec2 Pos, float Size)
    {
        ImDrawList* draw_list = ImGui::GetWindowDrawList();

        // 文件夹主体（黄色部分）
        ImVec2 folder_body_top_left(Pos.x, Pos.y + Size * 0.05f);
        ImVec2 folder_body_bottom_right(Pos.x + Size, Pos.y + Size * 0.8f);
        draw_list->AddRectFilled(folder_body_top_left, folder_body_bottom_right, IM_COL32(255, 215, 0, 255), Size * 0.05f);

        // 文件夹顶部曲线
        ImVec2 c1(Pos.x, Pos.y + Size * 0.15f);
        ImVec2 c2(Pos.x + Size * 0.3f, Pos.y + Size * 0.15f);
        ImVec2 c3(Pos.x + Size * 0.5f, Pos.y);
        ImVec2 c4(Pos.x + Size, Pos.y);
        draw_list->AddLine(c1, c2, IM_COL32(227, 161, 50, 255), Size * 0.1f);
        draw_list->AddLine(c2, c3, IM_COL32(227, 161, 50, 255), Size * 0.1f);
        draw_list->AddLine(c3, c4, IM_COL32(227, 161, 50, 255), Size * 0.1f);
    }

    // 绘制打开状态的文件夹图标（黄色主体 + 展开曲线）
    void DrawOpenFolderIcon(ImVec2 Pos, float Size)
    {
        ImDrawList* draw_list = ImGui::GetWindowDrawList();

        // 文件夹主体（黄色部分）
        ImVec2 folder_body_top_left(Pos.x, Pos.y + Size * 0.05f);
        ImVec2 folder_body_bottom_right(Pos.x + Size, Pos.y + Size * 0.8f);
        draw_list->AddRectFilled(folder_body_top_left, folder_body_bottom_right, IM_COL32(255, 215, 0, 255), Size * 0.05f);

        // 文件夹顶部曲线
        ImVec2 c0(Pos.x, Pos.y + Size * 0.65f);
        ImVec2 c1(Pos.x + Size * 0.05f, Pos.y + Size * 0.15f);
        ImVec2 c2(Pos.x + Size * 0.35f, Pos.y + Size * 0.15f);
        ImVec2 c3(Pos.x + Size * 0.55f, Pos.y);
        ImVec2 c4(Pos.x + Size * 1.05f, Pos.y);
        ImVec2 c5(Pos.x + Size, Pos.y + Size * 0.65f);
        draw_list->AddLine(c0, c1, IM_COL32(227, 161, 50, 255), Size * 0.1f);
        draw_list->AddLine(c1, c2, IM_COL32(227, 161, 50, 255), Size * 0.1f);
        draw_list->AddLine(c2, c3, IM_COL32(227, 161, 50, 255), Size * 0.1f);
        draw_list->AddLine(c3, c4, IM_COL32(227, 161, 50, 255), Size * 0.1f);
        draw_list->AddLine(c4, c5, IM_COL32(227, 161, 50, 255), Size * 0.1f);
    }

    // 递归渲染模块树（右键菜单弹出形式）
    // 等价于原 ModuleTree::RenderUI()
    void RenderModuleTreeUI(IBB_ModuleAltDefault::ModuleTree& tree)
    {
        for (auto& S : tree.Sub)
        {
            auto Pos = ImGui::GetCursorScreenPos();
            bool Hovered = false;
            ImRect R{ ImGui::GetCursorScreenPos(), ImGui::GetCursorScreenPos() + ImVec2{ ImGui::GetWindowWidth() + 0.5F * FontHeight, ImGui::GetTextLineHeightWithSpacing() } };
            if (R.Contains(ImGui::GetMousePos()))Hovered = true;
            //if (Hovered)ImGui::GetForegroundDrawList()->AddRect(R.Min, R.Max, IBR_Color::FocusLineColor);
            ImGui::Dummy(ImVec2((float)FontHeight, (float)FontHeight));
            ImGui::SameLine();
            ImGui::Text(S->Name.c_str());
            Hovered |= ImGui::IsItemHovered();
            bool V = S->ChildHovered();
            if (S->LastHovered || V)
            {
                DrawOpenFolderIcon(Pos, (float)FontHeight);
                if (!S->Sub.empty() || !S->Modules.empty())
                {
                    ImGui::SameLine();
                    auto w = ImGui::GetCurrentWindow();
                    auto mx = w->DC.CursorMaxPos;
                    ImGui::SetCursorPosX(ImGui::GetWindowWidth() - 1.0F * FontHeight);
                    ImGui::Text(u8">");
                    w->DC.CursorMaxPos = mx;

                    ImVec2 ppos = ImGui::GetLineEndPos();
                    ppos.y -= ImGui::GetTextLineHeightWithSpacing();

                    auto PredictedHeight = S->Sub.size() * (ImGui::GetTextLineHeightWithSpacing());
                    PredictedHeight += S->Modules.size() * (ImGui::GetTextLineHeightWithSpacing());
                    auto WorkspaceMaxHeight = IBR_UICondition::CurrentScreenHeight - IBR_HintManager::GetHeight();
                    if (ppos.y + PredictedHeight > WorkspaceMaxHeight)
                    {
                        ppos.y = WorkspaceMaxHeight - PredictedHeight - ImGui::GetTextLineHeightWithSpacing();
                        if (ppos.y < 0)ppos.y = 0;
                    }


                    IBR_PopupManager::DelayedPopupAction.push_back(
                        [ppos, P = S.get()] {
                            ImRect R = ImGui::GetCurrentWindow()->Rect();
                            P->ChildMenuHovered = R.Contains(ImGui::GetMousePos());// ImGui::IsWindowHovered(ImGuiHoveredFlags_RectOnly);
                            ImGui::SetWindowPos(ppos);
                            RenderModuleTreeUI(*P);
                        }
                    );
                }
            }
            else DrawFolderIcon(Pos, (float)FontHeight);
            S->LastHovered = Hovered;
        }
        for (auto& name : tree.ModuleOrder)
        {
            auto M = tree.Modules.at(name);
            SearchModuleAlt::RenderModuleAltSelect(M);
        }
    }
}

namespace IBR_ModuleTree
{
    void Tree_RenderUI()
    {
        RenderModuleTreeUI(IBB_ModuleAltDefault::GetAllModulesTree());
    }

    void SpecialTree_RenderUI()
    {
        RenderModuleTreeUI(IBB_ModuleAltDefault::GetSpecialModulesTree());
    }

    void Tree_RenderUISidebar()
    {
        std::function<void(const IBB_ModuleAltDefault::ModuleTree&)> Render;
        Render = [&](const IBB_ModuleAltDefault::ModuleTree& tree) {
            for (auto& sub : tree.Sub)
            {
                if (sub->Sub.empty() && sub->Modules.empty()) continue;
                ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow;
                auto pos = ImGui::GetCursorScreenPos();
                DrawFolderIcon(pos, (float)FontHeight);
                ImGui::Dummy(ImVec2((float)FontHeight, (float)FontHeight));
                ImGui::SameLine();
                bool open = ImGui::TreeNodeEx(sub->Name.c_str(), flags);
                if (open)
                {
                    Render(*sub);
                    ImGui::TreePop();
                }
            }
            for (auto& name: tree.ModuleOrder)
            {
                auto mod = tree.Modules.at(name);
                if (!mod) continue;
                auto label = mod->DescShort.empty() ? name : mod->DescShort;
                ImGui::Selectable(label.c_str());
                if (ImGui::IsItemHovered() && !mod->DescLong.empty())
                    ImGui::SetTooltip("%s", mod->DescLong.c_str());
                if (ImGui::IsItemClicked(ImGuiMouseButton_Left))
                {
                    mod->FullyLoad();
                    IBR_Inst_Project.AddModule(*mod, GenerateModuleTag());
                }
            }
        };
        auto& SpecialModules = IBB_ModuleAltDefault::GetSpecialModulesTree();
        auto& AllModules = IBB_ModuleAltDefault::GetAllModulesTree();

        // 系统模块（SpecialModules）—— 作为同级别文件夹
        if (!SpecialModules.Sub.empty() || !SpecialModules.Modules.empty())
        {
            auto pos = ImGui::GetCursorScreenPos();
            Render(SpecialModules);
        }

        // 用户模块（AllModules）
        Render(AllModules);
    }
}
