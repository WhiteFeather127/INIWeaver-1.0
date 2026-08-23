// IifComponentHelper.h
// 画布（SectionLineModel）与侧边栏（EditPanelController）共用的 IIF 分量导出辅助。
// 按 ImGui 原版组件驱动模型：每个分量描述统一导出，链接分量值从 StateValPtr 目标名读取，
// 不读 raw V.Value（避免链接目标名被 FormatValue 覆盖为 NaN/0）。
#pragma once

#include "IBG_InputType_Derived.h"
#include "IBB_Ini.h"
#include "IBB_PropStringPool.h"
#include <QVariant>
#include <QVariantList>
#include <QVariantMap>
#include <QString>
#include <QDebug>
#include <typeinfo>

// 分量类型字符串（对齐 ImGui IIC_* 分类），两端 QML 用同一套 type。
// 返回：text/locale/setter/samel/newl/sep/bool/int/choice/combo/radio/color/slider/input
inline const char* IifComponentKind(const IBG_InputComponent* p)
{
    if (dynamic_cast<const IIC_PureText*>(p)) return "text";
    if (dynamic_cast<const IIC_LocalizedText*>(p)) return "locale";
    if (dynamic_cast<const IIC_Setter_String*>(p)) return "setter";
    if (dynamic_cast<const IIC_SameLine*>(p)) return "samel";
    if (dynamic_cast<const IIC_NewLine*>(p)) return "newl";
    if (dynamic_cast<const IIC_Separator*>(p)) return "sep";
    if (dynamic_cast<const IIC_Bool*>(p)) return "bool";
    if (dynamic_cast<const IIC_InputInt*>(p)) return "int";
    if (dynamic_cast<const IIC_MultipleChoice*>(p)) return "choice";
    if (dynamic_cast<const IIC_EnumCombo*>(p)) return "combo";
    if (dynamic_cast<const IIC_EnumRadio*>(p)) return "radio";
    if (dynamic_cast<const IIC_ColorPanel*>(p)) return "color";
    if (dynamic_cast<const IIC_SliderInt*>(p)) return "slider";
    return "input";  // IIC_InputText 及未识别分量按输入框
}

// 分量是否支持链接（用于决定链接分量语义：画布连线 / 侧边栏只读）
inline bool IifComponentSupportsLink(const IBG_InputComponent* p)
{
    return p && p->SupportLinks();
}

// 读取分量当前显示值：对齐 ImGui `CurrentValue = Var.Dirty ? Var.StateValPtr->Format(ToString) : Var.Value`。
// Dirty 时从 StateValPtr 格式化（StateValPtr 是权威），否则用缓存 V.Value。
// 链接分量在此返回链接目标名（Sec$$Key），不依赖链接表覆盖。
inline std::string IifComponentValue(IBG_InputForm& form, int vid, const IBG_InputComponent* comp)
{
    (void)comp;
    if (vid < 0) return "";
    auto& V = form.GetValue(vid);
    static const IBB_InputFormat ToStringFmt{ IBB_InputFormat::ToString, "" };
    std::string res;
    if (V.Dirty && V.StateValPtr)
        res = V.StateValPtr->Format(ToStringFmt);
    else
        res = V.Value;
    // 显示兜底：NaN 是旧版导出污染产物，不是合法值，显示为空
    if (res.find("NaN") != std::string::npos || res == "nan" || res == "-nan")
        res.clear();
    return res;
}

// 分量 Hint（label/tooltip）：对齐 ImGui 各 IIC 的 Hint 成员
inline void IifComponentHint(const IBG_InputComponent* comp, QString& label, QString& tooltip)
{
    IICDescStr H;
    if (auto t = dynamic_cast<const IIC_InputText*>(comp)) H = t->Hint;
    else if (auto t = dynamic_cast<const IIC_InputInt*>(comp)) H = t->Hint;
    else if (auto t = dynamic_cast<const IIC_Bool*>(comp)) H = t->Hint;
    else if (auto t = dynamic_cast<const IIC_MultipleChoice*>(comp)) H = t->Hint;
    else if (auto t = dynamic_cast<const IIC_EnumCombo*>(comp)) H = t->Hint;
    else if (auto t = dynamic_cast<const IIC_EnumRadio*>(comp)) H = t->Hint;
    else if (auto t = dynamic_cast<const IIC_ColorPanel*>(comp)) H = t->Hint;
    else if (auto t = dynamic_cast<const IIC_SliderInt*>(comp)) H = t->Hint;
    else if (auto t = dynamic_cast<const IIC_InputInt*>(comp)) H = t->Hint;
    label = QString::fromUtf8(H.Short.c_str());
    tooltip = QString::fromUtf8(H.Long.empty() ? H.Short.c_str() : H.Long.c_str());
}

struct IBB_SubSec;
struct IBB_IniLine_Data_IIF;

// 从链接表（NewLinkTo/LinkSrc）恢复链接分量的目标名到 form 的 StateValPtr。
// 对齐 ImGui 原版 RenderUI_Node 每帧从链接表读目标名并写 StateValPtr 的机制，
// 解决 Qt 版链接分量 StateValPtr 目标名被污染成 0/NaN 的问题。
void IifSyncLinkTargets(IBG_InputForm& form, IBB_SubSec* sub, size_t lineIdx, size_t mult);

// 统一 IIF 分量导出（按 ImGui 原版组件驱动模型）。
// 遍历 IIF 的 InputComponents，输出每个分量的 UI 描述（type/value/label/tooltip/组件参数/链接目标名）。
// isWorkSpace=true（画布）：链接分量输出连线节点元数据；false（侧边栏）：链接分量只读目标名。
// 链接分量值统一从 StateValPtr 目标名读取（IifComponentValue），不读 raw 覆盖，避免 NaN/0。
QVariantList IifExportComponents(IBB_IniLine_Data_IIF *ii, IBB_SubSec *sub,
                                 size_t lineIdx, size_t mult, bool isWorkSpace);
