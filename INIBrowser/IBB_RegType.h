#pragma once

#include "FromEngine/Include.h"
#include "IBB_Components.h"
#include "FromEngine/ImGuiDeps.h"

struct IBB_RegType
{

    std::string IniType; //如果从Register字段得到的可以直接访问；否则走GetIniTypeOfReg函数！

    IW::Color FrameColor;         //Base Color
    IW::Color FrameColorPlus1;    //Lightness  +
    IW::Color FrameColorPlus2;    //Lightness  ++
    IW::Color FrameColorH;        //Saturation -
    IW::Color FrameColorL;
    IW::Color FrameColorLPlus1;
    IW::Color FrameColorLPlus2;
    IW::Color FrameColorLH;
    IW::Color FrameColorD;
    IW::Color FrameColorDPlus1;
    IW::Color FrameColorDPlus2;
    IW::Color FrameColorDH;
    bool Export;
    bool RegNameAsDisplay;
    bool UseOwnName;
    bool ValidateOptions;
    std::string Name;
    std::string ExportName;
    int Count;
    std::unordered_map<StrPoolID, StrPoolID> DefaultLinks;
    std::unordered_map<std::string, std::string> Options;//AllowedValue : DisplayName ; if empty then any value allowed

    std::string GetNoName();
    std::string GetNoName(const std::string& Reg);
};

struct IBB_CompoundRegType
{
    std::string Name;
    std::string DisplayName;
    std::vector<std::string> Regs;
    IBB_VariableList DefaultLinks;
};

struct PairClipString;
struct LinkNodeSetting;
struct IBG_InputType;

extern const char* AnyTypeName;
extern const char* MyTypeName;
StrPoolID AnyTypeID();
StrPoolID MyTypeID();

IW::Color LoadColorFromJson(JsonObject Obj, bool& Colored);
IW::Color LoadColorFromJson(JsonObject Obj, const IW::Color& Default);

namespace IBB_DefaultRegType
{
    extern std::unordered_map<_TEXT_UTF8 std::string, IBB_RegType> RegisterTypes;
    extern const IW::Color DefaultColor;
    //create type && create ini
    void EnsureRegType(const _TEXT_UTF8 std::string& Type);
    bool Load(JsonObject Obj);
    bool LoadFromFile(const wchar_t* FileName);
    bool HasRegType(const _TEXT_UTF8 std::string& Type);
    bool HasRegType(StrPoolID Type);
    IBB_RegType& GetRegType(const _TEXT_UTF8 std::string& Type);
    IBB_RegType& GetRegType(StrPoolID Type);
    const _TEXT_UTF8 std::string& GetIniTypeOfReg(const _TEXT_UTF8 std::string& Type);
    const _TEXT_UTF8 std::string& GetIniTypeOfReg(StrPoolID Type);
    std::vector<std::string> GetIniTypeList();
    bool HasInputType(const _TEXT_UTF8 std::string& Type);
    IBG_InputType& GetInputType(const _TEXT_UTF8 std::string& Type);
    IBG_InputType& GetDefaultInputType();
    IW::Color GetDefaultNodeColor();
    LinkNodeSetting GetDefaultLinkNodeSetting();
    StrBoolType GetDefaultStrBoolType();
    IBG_InputType& SelectInputTypeByValue(const _TEXT_UTF8 std::string& Value);
    const bool MatchType(StrPoolID TypeA, StrPoolID TypeB);
    void GenerateDLK(const std::vector<PairClipString>& DLK1, StrPoolID Register, std::unordered_map<StrPoolID, StrPoolID>& DefaultLinkKey, std::unordered_map<StrPoolID, StrPoolID>*& UpValue);
    void SwitchLightColor();
    void SwitchDarkColor();
    void ClearModuleCount();
}
