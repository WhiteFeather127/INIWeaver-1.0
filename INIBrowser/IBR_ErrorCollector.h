// IBR_ErrorCollector.h
// 错误报告解耦头文件
// 设计要点：
//   - 业务层（IBB_*/IBF_*/IBS_*）通过 IBB_ErrorReport 命名空间上报错误，无 ImGui 依赖
//   - 渲染层（IBR_*）通过 IBR_ErrorCollector 命名空间清理错误队列
//   - ImGui 弹窗渲染逻辑实现在 IBR_ErrorCollector.cpp 中，不暴露到此头文件
//   - 原 IBR_PopupManager::Add*ErrorPopup 保留为转发包装，供渲染层旧调用点使用
#pragma once

#include <string>
#include <functional>

// 业务层错误上报接口（无 ImGui 依赖）
// 调用后错误会被加入队列，由渲染线程通过 IBR_PopupManager 弹窗逐条显示
namespace IBB_ErrorReport
{
    void AddJsonParseErrorPopup(std::string&& ErrorStr, const std::string& Info);
    void AddModuleParseErrorPopup(std::string&& ErrorStr, const std::string& Info);
    void AddLoadConfigErrorPopup(std::string&& ErrorStr, const std::string& Info);
    void AddOutputErrorPopup(std::string&& ErrorStr, const std::string& Info);
}

// 渲染层错误收集器接口
namespace IBR_ErrorCollector
{
    // 清空待显示错误队列并关闭当前错误弹窗
    // 等价于原 IBR_PopupManager::ClearPopupDelayed 的错误队列清理部分
    void ClearDelayed();

    // 阶段 11.2：Qt 错误钩子（用于将错误转发到 QML 错误队列）
    // 参数：Title, ErrorStr, Info, ForJson
    // 当 Qt 侧注册此钩子后，AddErrorPopup 会同时通知 Qt 侧
    using ErrorHookFn = std::function<void(const std::string&, const std::string&, const std::string&, bool)>;
    void SetErrorHook(ErrorHookFn fn);
}
