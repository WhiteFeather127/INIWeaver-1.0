// IifComponentHelper.cpp
// 画布（SectionLineModel）与侧边栏（EditPanelController）共用的 IIF 分量统一导出实现。
// 按 ImGui 原版组件驱动模型：遍历 IIF 的 InputComponents，输出各分量的 UI 描述。
#include "IifComponentHelper.h"
#include "IBB_IniLine.h"
#include "IBB_Ini.h"        // IBB_SubSec / IBB_NewLink（链接表）
#include "IBR_Misc.h"       // IBR_WorkSpace::ShowRegName
#include <cctype>
#include <cstdio>           // sprintf（ImColor → #RRGGBB）

namespace {
// 侧边栏枚举选项导出（Options/OptionOrder → QVariantList，对齐 EditPanel 的 iifOptArr）
QVariantList IifExportOpts(const std::unordered_map<std::string, IICDescStr>& opts,
                           const std::vector<std::string>& order, bool showReg)
{
    QVariantList out;
    for (auto& k : order) {
        auto it = opts.find(k);
        if (it == opts.end()) continue;
        QVariantMap o;
        o["key"] = QString::fromUtf8(k.c_str());
        o["label"] = QString::fromUtf8(showReg ? k.c_str() : it->second.Short.c_str());
        o["desc"]  = QString::fromUtf8(it->second.Long.c_str());
        out << o;
    }
    return out;
}

bool IifStrTrue(const std::string& s)
{
    std::string t;
    t.reserve(s.size());
    for (char c : s) if (c != ' ' && c != '\t') t.push_back((char)tolower((unsigned char)c));
    return t == "yes" || t == "true" || t == "1" || t == "on";
}

// ImColor(RGBA float) → "#RRGGBB"（对齐 ImGui 各 IIC 的 Color 成员）
QString IifImColorToHex(const ImColor& c)
{
    auto f2i = [](float f) -> int {
        return f < 0.0f ? 0 : (f > 1.0f ? 255 : static_cast<int>(f * 255.0f + 0.5f));
    };
    char buf[8];
    sprintf(buf, "#%02X%02X%02X", f2i(c.Value.x), f2i(c.Value.y), f2i(c.Value.z));
    return QString::fromLatin1(buf);
}

// StrBoolType → 字符串名（对齐 ImGui IIC_Bool::FmtType）
QString IifBoolFmtName(StrBoolType t)
{
    switch (t) {
    case StrBoolType::Str_true_false: return "true_false";
    case StrBoolType::Str_True_False: return "True_False";
    case StrBoolType::Str_TRUE_FALSE: return "TRUE_FALSE";
    case StrBoolType::Str_yes_no: return "yes_no";
    case StrBoolType::Str_Yes_No: return "Yes_No";
    case StrBoolType::Str_YES_NO: return "YES_NO";
    case StrBoolType::Str_t_f: return "t_f";
    case StrBoolType::Str_T_F: return "T_F";
    case StrBoolType::Str_y_n: return "y_n";
    case StrBoolType::Str_Y_N: return "Y_N";
    case StrBoolType::Str_1_0: return "1_0";
    case StrBoolType::Str_yeah_fuck: return "yeah_fuck";
    default: return "custom";
    }
}

// IIC_ColorPanel::ValueMode → 字符串（rgb/bgr/hsv）
const char* IifColorVModeName(IIC_ColorPanel::ValueMode m)
{
    switch (m) {
    case IIC_ColorPanel::RGB: return "rgb";
    case IIC_ColorPanel::BGR: return "bgr";
    case IIC_ColorPanel::HSV: return "hsv";
    }
    return "rgb";
}

// IIC_ColorPanel::FormatMode → 字符串（int/float/hex/hashhex）
const char* IifColorFModeName(IIC_ColorPanel::FormatMode m)
{
    switch (m) {
    case IIC_ColorPanel::Int: return "int";
    case IIC_ColorPanel::Float: return "float";
    case IIC_ColorPanel::Hex: return "hex";
    case IIC_ColorPanel::Hash_Hex: return "hashhex";
    }
    return "int";
}
} // namespace

QVariantList IifExportComponents(IBB_IniLine_Data_IIF *ii, IBB_SubSec *sub,
                                 size_t lineIdx, size_t mult, bool isWorkSpace)
{
    QVariantList out;
    if (!ii || !ii->Value) return out;
    IBG_InputForm &form = *ii->Value;
    // 确保各分量值已就绪（Dirty 时按格式化串重算，但链接分量保留 StateValPtr 目标名）
    auto &cs = form.GetComponentStatus();
    auto &comps = *form.InputComponents;
    const bool showReg = IBR_WorkSpace::ShowRegName;

    qDebug("[IIF-EXPORT] ws=%d lineIdx=%zu mult=%zu nComp=%zu nStatus=%zu lineFmt='%s'",
           (int)isWorkSpace, lineIdx, mult, comps.size(), cs.size(), form.GetFormattedString().c_str());
    for (size_t ci = 0; ci < comps.size(); ++ci) {
        int cvid = comps[ci]->GetCurrentTargetValueID();
        if (cvid < 0) continue;
        auto &cv = form.GetValue(cvid);
        std::string stTxt, stType = cv.StateValPtr ? typeid(*cv.StateValPtr).name() : "(null)";
        if (auto st = cv.StateValue<IIS_String>()) stTxt = st->Text;
        else if (auto st = cv.StateValue<IIS_Int>()) stTxt = std::to_string(st->Value);
        else if (auto st = cv.StateValue<IIS_Bool>()) stTxt = st->Value ? "true" : "false";
        int im = (ci < cs.size()) ? (int)cs[ci].InputMethod : -1;
        qDebug("[IIF-EXPORT]   comp=%d type=%s supportLink=%d im=%d state=%s stTxt='%s' raw='%s' dirty=%d",
               (int)ci, typeid(*comps[ci]).name(), (int)comps[ci]->SupportLinks(), im,
               stType.c_str(), stTxt.c_str(), cv.Value.c_str(), (int)cv.Dirty);
    }

    qDebug("[IIF-EXPORT] OnExport=%d before GetFormattedString", (int)ExportContext::OnExport);
    form.GetFormattedString();
    for (size_t i = 0; i < comps.size(); ++i) {
        auto &p = comps[i];
        int vid = p->GetCurrentTargetValueID();
        // 分量 InputMethod：画布读 ComponentStatus；侧边栏恒 Input（对应 ImGui Status_Sidebar）
        bool isLink = isWorkSpace
                      ? ((i < cs.size()) && (cs[i].InputMethod == IICStatus::Link))
                      : false;

        QVariantMap m;
        m["compIdx"] = static_cast<int>(i);
        m["idx"] = static_cast<int>(i);
        m["isLink"] = isLink;
        m["supportLinks"] = p->SupportLinks();
        m["readOnly"] = p->Disabled;
        m["disabled"] = p->Disabled;
        m["valueID"] = vid;
        // 对齐 ImGui CreateInputComponent 通用设置：InitialStatus / Constraint / ColoredFrame / LinkNode
        m["initialStatus"] = (p->InitialStatus.InputMethod == IICStatus::Link) ? "link" : "input";
        m["coloredFrame"] = p->UseNodeColorInFrame;
        // Constraint：对齐 ImGui IBG_InputForm::RenderUI 的 `if (!ValueContainer.Satisfy(IC->Constraint))continue;`
        // 约束不满足的分量不渲染（QML 端跳过 hidden）
        m["hidden"] = !form.GetValues().Satisfy(p->Constraint);
        // LinkNode.LinkCol 原始色（ABGR ImU32 → #RRGGBB），供 ColoredFrame 染框等使用
        {
            uint32_t lc = p->NodeSetting.LinkCol;
            m["linkCol"] = QString("#%1%2%3")
                .arg((lc >> 0) & 0xFF, 2, 16, QLatin1Char('0'))
                .arg((lc >> 8) & 0xFF, 2, 16, QLatin1Char('0'))
                .arg((lc >> 16) & 0xFF, 2, 16, QLatin1Char('0'));
        }

        QString label, tooltip;
        IifComponentHint(p.get(), label, tooltip);
        m["label"] = label;
        m["tooltip"] = tooltip;

        // 分量类型（统一 IifComponentKind）
        m["type"] = IifComponentKind(p.get());

        // 值：优先 StateValPtr（ImGui 原版权威值，链接分量=目标名），其次 raw V.Value
        std::string valueStr = IifComponentValue(form, vid, p.get());
        m["value"] = QString::fromUtf8(valueStr.c_str());

        // 纯文本类：value 用组件自身文本（覆盖），tooltip 回退
        if (auto t = dynamic_cast<IIC_PureText*>(p.get())) {
            m["value"] = QString::fromUtf8(t->Text.c_str()); m["readOnly"] = true;
            m["colored"] = t->Colored;
            m["wrapped"] = t->Wrapped;
            if (t->Colored) m["color"] = IifImColorToHex(t->Color);
        } else if (auto t = dynamic_cast<IIC_LocalizedText*>(p.get())) {
            m["value"] = QString::fromUtf8(t->FallbackText.c_str()); m["readOnly"] = true;
            m["colored"] = t->Colored;
            m["wrapped"] = t->Wrapped;
            if (t->Colored) m["color"] = IifImColorToHex(t->Color);
            m["locKey"] = QString::fromUtf8(t->Key.c_str());
        } else if (auto t = dynamic_cast<IIC_Setter_String*>(p.get())) {
            m["value"] = QString::fromUtf8(t->Value.c_str()); m["readOnly"] = true;
        } else if (m["type"] == "samel" || m["type"] == "newl" || m["type"] == "sep") {
            m["readOnly"] = true;
        }
        if (m["tooltip"].toString().isEmpty()
            && (m["type"] == "text" || m["type"] == "locale" || m["type"] == "setter"))
            m["tooltip"] = m["value"];

        // 组件特有参数（对齐 ImGui 各 IIC 控件参数）
        if (auto t = dynamic_cast<IIC_Bool*>(p.get())) {
            m["boolVal"] = IifStrTrue(valueStr);
            m["fmtType"] = IifBoolFmtName(t->FmtType);
        } else if (auto t = dynamic_cast<IIC_InputInt*>(p.get())) {
            m["min"] = t->Min; m["max"] = t->Max;
        } else if (auto t = dynamic_cast<IIC_MultipleChoice*>(p.get())) {
            m["delim"] = QString::fromUtf8(t->Delim.c_str());
            m["opts"] = IifExportOpts(t->Options, t->OptionOrder, showReg);
            // 对齐 ImGui IIC_MultipleChoice::RenderUI 的 SameLine/MaxInOneLine：
            // 每 MaxInOneLine 个选项换一行（SameLine=false 则每项都换行），避免选项溢出
            m["sameLine"] = t->SameLine;
            m["maxInOneLine"] = t->MaxInOneLine;
            m["hintID"] = QString::fromUtf8(t->HintID.c_str());
        } else if (auto t = dynamic_cast<IIC_EnumCombo*>(p.get())) {
            m["opts"] = IifExportOpts(t->Options, t->OptionOrder, showReg);
        } else if (auto t = dynamic_cast<IIC_EnumRadio*>(p.get())) {
            m["opts"] = IifExportOpts(t->Options, t->OptionOrder, showReg);
            // 对齐 ImGui IIC_EnumRadio::RenderUI 的 SameLine/MaxInOneLine：
            // 每 MaxInOneLine 个选项换一行（SameLine=false 则每项都换行）
            m["sameLine"] = t->SameLine;
            m["maxInOneLine"] = t->MaxInOneLine;
            m["hintID"] = QString::fromUtf8(t->HintID.c_str());
        } else if (auto t = dynamic_cast<IIC_SliderInt*>(p.get())) {
            m["min"] = t->Min; m["max"] = t->Max;
            m["slideFormat"] = QString::fromUtf8(t->SlideFormat.c_str());
            m["logarithmic"] = t->Logarithmic;
        } else if (auto t = dynamic_cast<IIC_ColorPanel*>(p.get())) {
            m["color"] = true;
            m["vMode"] = IifColorVModeName(t->VMode);
            m["fMode"] = IifColorFModeName(t->FMode);
        }

        // 链接分量：仅当 ComponentStatus 为 Link（isLink）且支持链接时才渲染为链接。
        // 值统一为 StateValPtr 目标名；isWorkSpace 决定画布连线 / 侧边栏只读。
        // 非 Link 态的 SupportLinks 分量（如 InputInt/InputText 但当前为 Input 态）保持原控件，不画成节点。
        if (isLink && p->SupportLinks() && m["type"] != "text") {
            if (isWorkSpace) {
                // 画布：链接分量渲染为连线节点（节点元数据由画布 iifComponents 补充 links/sessionId 等）
                m["type"] = "link";
                m["linkType"] = QString::fromUtf8(PoolStr(p->NodeSetting.LinkType));
                m["linkLimit"] = p->NodeSetting.LinkLimit;
            } else {
                // 侧边栏：只读目标名（不建链）
                m["type"] = "link";
                m["readOnly"] = true;
            }
        }

        out << m;
    }
    return out;
}

// 链接分量显示前的无效值清洗。
// 注意：不从链接表恢复/覆盖目标名（那样会改写持久化 V.Value/Text，导致
// "显示侧边栏就改值"）。ImGui RenderUI_Node 只显示链接表目标名，不写回 V.Value。
// 写回链路已用 GetFormattedString（只格式化 Dirty 目标）不污染其他链接分量，
// 故链接分量 V.Value 保持用户输入值即可，显示直接读 V.Value。
void IifSyncLinkTargets(IBG_InputForm& form, IBB_SubSec* sub, size_t lineIdx, size_t mult)
{
    if (!sub) return;
    auto& cs = form.GetComponentStatus();
    auto& comps = *form.InputComponents;
    for (size_t i = 0; i < comps.size(); ++i) {
        auto &p = comps[i];
        if (!p->SupportLinks()) continue;
        bool isLink = (i < cs.size()) && (cs[i].InputMethod == IICStatus::Link);
        if (!isLink) continue;
        int vid = p->GetCurrentTargetValueID();
        if (vid < 0) continue;
        // 无效值清洗：NaN 是旧版本导出污染的产物，不是合法目标名。
        // 清空它（显示为空），让用户能重新输入正确目标名；否则输入框恒显示 NaN，
        // 用户提交时会把 NaN 再次写回，形成循环。
        auto &V = form.GetValue(vid);
        auto isBad = [](const std::string& s) {
            return s == "NaN" || s == "nan" || s == "-nan" || s.find("NaN") != std::string::npos;
        };
        std::string cur = V.Value;
        if (auto st = V.StateValue<IIS_String>()) cur = st->Text;
        if (isBad(cur)) {
            if (auto st = V.StateValue<IIS_String>()) st->Text.clear();
            else V.ResetState<IIS_String>("");
            V.Value.clear();
            V.Dirty = false;
        }
        // 不从链接表恢复：避免覆盖用户输入的持久化值
    }
}
