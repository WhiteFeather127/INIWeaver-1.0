// SelfTest.h
// 模拟点击测试：直接调用 Q_INVOKABLE 方法模拟鼠标交互，验证状态变化
// 通过 -selftest 命令行参数触发，不加载 QML，直接在初始化后运行
#pragma once

class WorkspaceController;
class ModuleTreeModel;
class SettingController;
class ProjectController;

// 运行所有自测试，返回失败用例数（0=全部通过）
int runSelfTests(WorkspaceController &wc, ModuleTreeModel &mtm,
                 SettingController &sc, ProjectController &pc);
