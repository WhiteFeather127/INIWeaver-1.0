// INIWeaver 基础类型抽象层
// 替代 ImGui 的 ImVec2/dImVec2/ImColor/ImU32，使业务层可脱离 ImGui 独立编译
// 设计要点：
//   - 模板化 Vec2Base<T>，using Vec2 = Vec2Base<float> / DVec2 = Vec2Base<double>
//   - 跨类型运算符使用模板 + std::common_type_t，避免 C2666 歧义
//   - 保留与 ImGui dImVec2 一致的隐式跨类型转换
#pragma once
#include <cstdint>
#include <type_traits>
#include <cstddef>

namespace IW
{

// --------------------------------------------------------------------------
// Vec2Base<T>：二维向量模板，float/double 特化
// --------------------------------------------------------------------------
template<typename T>
struct Vec2Base
{
    T x, y;

    constexpr Vec2Base() : x(T(0)), y(T(0)) {}
    constexpr Vec2Base(T _x, T _y) : x(_x), y(_y) {}

    // 跨类型隐式转换构造（匹配 ImGui dImVec2(ImVec2) 行为）
    template<typename U, std::enable_if_t<!std::is_same_v<U, T>, int> = 0>
    constexpr Vec2Base(const Vec2Base<U>& other)
        : x(static_cast<T>(other.x)), y(static_cast<T>(other.y)) {}

    T  operator[](size_t idx) const { return (&x)[idx]; }
    T& operator[](size_t idx)       { return (&x)[idx]; }

    T max() const { return x > y ? x : y; }
    T min() const { return x < y ? x : y; }
};

using Vec2  = Vec2Base<float>;
using DVec2 = Vec2Base<double>;

// --------------------------------------------------------------------------
// 运算符重载（模板化，同时处理同类型与跨类型运算）
// 返回类型使用 std::common_type_t，例如 Vec2 - DVec2 -> DVec2
// --------------------------------------------------------------------------
template<typename T, typename U>
constexpr auto operator+(const Vec2Base<T>& a, const Vec2Base<U>& b)
    -> Vec2Base<std::common_type_t<T, U>>
{
    using R = std::common_type_t<T, U>;
    return Vec2Base<R>(static_cast<R>(a.x) + static_cast<R>(b.x),
                       static_cast<R>(a.y) + static_cast<R>(b.y));
}

template<typename T, typename U>
constexpr auto operator-(const Vec2Base<T>& a, const Vec2Base<U>& b)
    -> Vec2Base<std::common_type_t<T, U>>
{
    using R = std::common_type_t<T, U>;
    return Vec2Base<R>(static_cast<R>(a.x) - static_cast<R>(b.x),
                       static_cast<R>(a.y) - static_cast<R>(b.y));
}

template<typename T, typename U>
constexpr auto operator*(const Vec2Base<T>& a, U s)
    -> Vec2Base<std::common_type_t<T, U>>
{
    using R = std::common_type_t<T, U>;
    return Vec2Base<R>(static_cast<R>(a.x) * static_cast<R>(s),
                       static_cast<R>(a.y) * static_cast<R>(s));
}

template<typename T, typename U>
constexpr auto operator/(const Vec2Base<T>& a, U s)
    -> Vec2Base<std::common_type_t<T, U>>
{
    using R = std::common_type_t<T, U>;
    return Vec2Base<R>(static_cast<R>(a.x) / static_cast<R>(s),
                       static_cast<R>(a.y) / static_cast<R>(s));
}

// 复合赋值运算符
template<typename T, typename U>
constexpr Vec2Base<T>& operator+=(Vec2Base<T>& a, const Vec2Base<U>& b)
{
    a.x = static_cast<T>(a.x + b.x);
    a.y = static_cast<T>(a.y + b.y);
    return a;
}

template<typename T, typename U>
constexpr Vec2Base<T>& operator-=(Vec2Base<T>& a, const Vec2Base<U>& b)
{
    a.x = static_cast<T>(a.x - b.x);
    a.y = static_cast<T>(a.y - b.y);
    return a;
}

template<typename T, typename U>
constexpr Vec2Base<T>& operator*=(Vec2Base<T>& a, U s)
{
    a.x = static_cast<T>(a.x * s);
    a.y = static_cast<T>(a.y * s);
    return a;
}

template<typename T, typename U>
constexpr Vec2Base<T>& operator/=(Vec2Base<T>& a, U s)
{
    a.x = static_cast<T>(a.x / s);
    a.y = static_cast<T>(a.y / s);
    return a;
}

// abs（对应 imgui.h 中 abs(dImVec2)）
template<typename T>
constexpr Vec2Base<T> abs(const Vec2Base<T>& a)
{
    return Vec2Base<T>(a.x < T(0) ? -a.x : a.x,
                       a.y < T(0) ? -a.y : a.y);
}

// --------------------------------------------------------------------------
// u32：32 位无符号整数（对应 ImU32）
// --------------------------------------------------------------------------
using u32 = uint32_t;

// --------------------------------------------------------------------------
// Color：RGBA 浮点颜色（对应 ImColor::Value 即 ImVec4）
// --------------------------------------------------------------------------
struct Color
{
    float Value[4];

    constexpr Color() : Value{0.0f, 0.0f, 0.0f, 0.0f} {}
    constexpr Color(float r, float g, float b, float a = 1.0f) : Value{r, g, b, a} {}
};

// 将 RGBA 浮点颜色打包为 32 位整数（与 ImGui IM_COL32 的 ABGR 布局一致）
// 业务层使用此函数替代 ImColor -> ImU32 转换，避免依赖 ImGui
inline u32 ToU32(const Color& c)
{
    auto f2i = [](float f) -> u32 {
        return f < 0.0f ? 0u : (f > 1.0f ? 255u : static_cast<u32>(f * 255.0f + 0.5f));
    };
    u32 r = f2i(c.Value[0]);
    u32 g = f2i(c.Value[1]);
    u32 b = f2i(c.Value[2]);
    u32 a = f2i(c.Value[3]);
    return (a << 24) | (b << 16) | (g << 8) | r;
}

} // namespace IW
