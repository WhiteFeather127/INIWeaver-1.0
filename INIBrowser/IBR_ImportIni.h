#pragma once
#include "FromEngine/Include.h"
#include "IBB_IniImport.h"
#include <functional>

#ifndef _TEXT_UTF8
#define _TEXT_UTF8
#endif

// ---------- 导入预览结果回调 ----------
struct IBR_ImportResult
{
    bool Confirmed{ false };                    // 用户是否确认
    ImportedIniFile File;                       // 最终的 section 列表（含用户修正后的类型）
    std::string INIType;
};

// ---------- 导入预览弹窗 ----------
namespace IBR_ImportPreview
{
    // 打开导入预览弹窗（异步，用户确认后通过 Callback 通知）
    // Callback 会在渲染线程被调用
    void Open(ImportedIniFile&& File, const std::function<void(const IBR_ImportResult&)>& Callback);

    // 渲染弹窗（在渲染循环中调用）
    void RenderUI();

    // 阶段 11.1：Qt 导入预览 Hook
    // 当 Qt 侧注册此 Hook 后，Open() 不再设置 ImGui 弹窗，
    // 而是将 ImportedIniFile 和 Callback 转发给 Qt 侧由 QML 渲染预览 UI。
    // Hook 在渲染线程被调用，Qt 侧需自行处理线程切换。
    using ImportPreviewHookFn = std::function<void(
        ImportedIniFile&&,
        const std::function<void(const IBR_ImportResult&)>&)>;
    void SetHook(ImportPreviewHookFn fn);
}
