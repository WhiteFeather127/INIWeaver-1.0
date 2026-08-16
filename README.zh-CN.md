# INI Weaver（INI 织网者）

[English version](README.md) | 简体中文

INI Weaver 是面向《红警 2 / 尤里复仇》modder 的可视化 INI 编辑器。
不再在记事本里手抄一节节 `<section>` 跨文件拼 flag，而是在无限画布上
拖出现成的模块，在它们的连接点之间拉线 —— 编辑器帮你把它写成 INI。

![模块连接示意](pic/Module_Links.png)

## 它能做什么

- **画布式编辑**：把 `<section>` 模块放到画布上，在它们的连接点之间
  拉线来表达 `Weapon=`、`Projectile=`、`Warhead=` 这类引用。最终输出
  INI 时，没接线的连接点会被自动丢掉。
- **模块库**：`\Global\Modules` 下自带常用节。自己建库也很简单——在
  画布上选中一组模块 Ctrl+C，再粘贴到库的剪贴板模板 `Data=` 之后。
  通过剪贴板建出来的库模块会保留模块之间的相对位置，手写的库则办不到。
- **反向连接**：把一个弹丸模块往武器模块上一拖，就会自动用正确的
  flag 把线接好；如果武器上没有这个 flag，还会自动补出来。反向连接的
  规则写在 `RegisterTypes.json` 里。
- **flag 字典**：`TypeAlt.csv` 只需声明跨大类连接的 flag，其它 flag
  从已加载的模块继承在画布上的行为（`=yes`/`=no` → 布尔开关，其它值
  → 文本编辑）。
- **直接导入 SHP/VXL**：把 `.shp` 或 `.vxl` 拖进窗口，会自动建出
  对应类型的模块。
- **VS Code 风格深色 UI**：Qt6 + QML 无边框主窗 + 用于导航的小地图 +
  列表视图 + 设置面板。

## 快捷键

| 键位                       | 功能                                |
|:--------------------------|:------------------------------------|
| F2                         | 切换显示模式（注释名 / 真 section 名） |
| Ctrl+S                     | 保存项目                            |
| Ctrl+Shift+S               | 另存为                              |
| Ctrl+O                     | 打开项目                            |
| Ctrl+E                     | 导出 INI                            |
| Ctrl+W                     | 关闭项目                            |
| Ctrl+C / Ctrl+V             | 复制 / 粘贴模块                      |
| F3                         | 重命名 section 名                    |
| 左键点画布                  | 选中模块 / 进入编辑                  |
| 画布右键                    | 打开模块库                          |
| 画布双击                    | 模块搜索                            |
| 右键点模块标题              | 模块上下文菜单                       |
| 双击 flag                  | 编辑 flag 值                         |

完整功能列表、演示 GIF 和菜单导览见 [`docs/Info.md`](docs/Info.md)。

## 构建

INIWeaver 用 CMake + Qt 6.8.x + QML 在 Windows 上构建。

> 旧版 Dear ImGui（`legacy-imgui\INIBrowser.sln`）已归档，不再编译。
> 请使用根目录的 `INIWeaver.sln`。

```powershell
cd <仓库根>
cmake -S . -B build
.\sync-sln.ps1
& "C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe" INIWeaver.sln -p:Configuration=Release -p:Platform=x64 -m -verbosity:minimal
```

构建产物：仓库根目录下的 `Release\INIWeaver.exe`。

**重新配置**（新增 QML 文件或 C++ 源文件后）：

```powershell
cmake -S . -B build
.\sync-sln.ps1
```

根目录的 `INIWeaver.sln` 是由 `sync-sln.ps1` 从 `build\INIWeaver.sln`
派生出来的包装：每一个 `.vcxproj` 引用都被加上 `build\` 前缀，让 sln
留在仓库根、真正的工程文件留在 CMake 的构建目录里。它被 git 忽略，
每次 cmake 重新配置后必须跑一次 `.\sync-sln.ps1` 同步。中间产物、
CMake cache、Qt6 自动生成代码和导入库都留在 `build\` 下。

### 运行

把 `Release\INIWeaver.exe` 复制到一个已经放了 Qt6 运行时 DLL 的目录
（即发行包目录）。只替换 EXE 就够了。

## 文档

- [`docs/Info.md`](docs/Info.md) —— 完整英文用户手册（截图 + GIF 演示）。
- Sphinx 渲染站点：本地打开 `INIBrowser Docs.html`，或从 `docs/`
  目录构建（使用 Furo 主题、MyST parser、sphinx-design、
  sphinx-inline-tabs、sphinx-copybutton、sphinxext-opengraph 等扩展）。
- 基础教程视频（B 站）：https://www.bilibili.com/video/BV1b5EuzzEzT/

## 项目结构

```
INIBrowser/            Qt6 + QML 业务代码（Qt 桥接 / Controller / QML UI / QtMain.cpp）
INIBrowser/qt/         Qt 桥接层（ProjectController、WorkspaceController 等）
INIBrowser/ui/         QML UI，按 panels / workspace / dialogs / components 组织
ImGui/                 Dear ImGui 源码（仍被编译；Qt6 路径把它作依赖）
legacy-imgui/          归档的 ImGui 时代 VS 解决方案 + 入口，不再编译
libs/                  第三方库（glfw 等）
docs/                  Sphinx 文档源（英文 + zh_CN 翻译）
pic/                   文档与本 README 引用的截图和 GIF
CMakeLists.txt         CMake 源
sync-sln.ps1           从 build\INIWeaver.sln 重新生成根 INIWeaver.sln
INIWeaver.sln          派生包装 sln（被 git 忽略）
Release\               Release 配置的 EXE 输出目录（被 git 忽略）
build\                 CMake 构建目录（被 git 忽略）
```

## 许可证

[LGPL-2.1-or-later](LICENSE.txt) —— © 2025 Kenosis，作者 ProsperousBeyond。

INI Weaver 集成了
[Dear ImGui](https://github.com/ocornut/imgui)、
[Qt 6](https://www.qt.io/)、
[GLFW](https://www.glfw.org/)、
[fmt](https://github.com/fmtlib/fmt) 和
[cJSON](https://github.com/DaveGamble/cJSON)，
按它们各自的许可证分发。