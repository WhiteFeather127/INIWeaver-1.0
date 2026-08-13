// INIWeaver ImGui 依赖集中头文件
// 集中包含 ImGui + GLFW 相关头文件，提供 IW 类型与 ImGui 类型的显式转换函数
// 设计要点：
//   - 业务层（IBB_*/IBF_*/IBS_*/FromEngine/）不应包含此文件
//   - 渲染层（IBR_*）包含此文件以使用 ImGui API
//   - IW ↔ ImGui 转换仅用显式函数，禁止隐式构造
#pragma once

#include <imgui.h>
#include <imgui_internal.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl2.h>

#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>

#include "IWTypes.h"

// --------------------------------------------------------------------------
// IW ↔ ImGui 类型转换函数（显式，禁止隐式构造）
// --------------------------------------------------------------------------

// IW::Vec2 (float) ↔ ImVec2
inline ImVec2 toImVec2(const IW::Vec2& v) { return ImVec2(v.x, v.y); }
inline IW::Vec2 toIWVec2(const ImVec2& v) { return IW::Vec2(v.x, v.y); }

// IW::DVec2 (double) ↔ dImVec2
inline dImVec2 toDImVec2(const IW::DVec2& v) { return dImVec2(v.x, v.y); }
inline IW::DVec2 toIWDVec2(const dImVec2& v) { return IW::DVec2(v.x, v.y); }

// IW::Color (float[4]) ↔ ImColor（ImColor::Value 为 ImVec4）
inline ImColor toImColor(const IW::Color& c)
{
    return ImColor(c.Value[0], c.Value[1], c.Value[2], c.Value[3]);
}
inline IW::Color toIWColor(const ImColor& c)
{
    return IW::Color(c.Value.x, c.Value.y, c.Value.z, c.Value.w);
}

// IW::Color (float[4]) ↔ ImVec4（直接 RGBA 浮点）
inline ImVec4 toImVec4(const IW::Color& c)
{
    return ImVec4(c.Value[0], c.Value[1], c.Value[2], c.Value[3]);
}
inline IW::Color toIWColorFromVec4(const ImVec4& v)
{
    return IW::Color(v.x, v.y, v.z, v.w);
}
