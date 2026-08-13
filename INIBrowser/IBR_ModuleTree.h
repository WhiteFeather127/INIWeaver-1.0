// IBR_ModuleTree.h
// 模块树渲染解耦头文件
// 设计要点：
//   - 业务层（IBB_ModuleAlt）维护 ModuleTree 数据结构（名称、子节点、模块映射、加载/新建）
//   - 渲染层（IBR_ModuleTree）负责将 ModuleTree 绘制为 ImGui 树形菜单 / 侧边栏
//   - 头文件不包含任何 ImGui 类型，可被任意模块包含
//   - ImGui 弹窗、图标绘制等逻辑实现在 IBR_ModuleTree.cpp 中
//   - 原 IBB_ModuleAltDefault::Tree_RenderUI* 迁移至此命名空间
#pragma once

namespace IBR_ModuleTree
{
    // 渲染主模块树（右键菜单弹出形式，等价于原 IBB_ModuleAltDefault::Tree_RenderUI）
    void Tree_RenderUI();

    // 渲染特殊模块树（右键菜单弹出形式，等价于原 IBB_ModuleAltDefault::SpecialTree_RenderUI）
    void SpecialTree_RenderUI();

    // 渲染侧边栏模块树（树形展开 + 可选择添加，等价于原 IBB_ModuleAltDefault::Tree_RenderUISidebar）
    void Tree_RenderUISidebar();
}
