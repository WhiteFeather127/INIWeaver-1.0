# INIWeaver 项目说明

## 构建系统（重要）

本项目存在两套 sln，必须区分清楚：

| 路径 | 类型 | 用途 |
|------|------|------|
| `legacy-imgui\INIBrowser.sln` | VS 原生（已归档） | ImGui 旧版本，**不要编译这个**；已移至 `legacy-imgui/` 子目录作纯静态归档，路径未修复，IDE 打开不可再编译。原 `INIBrowser.cpp`(ImGui 旧入口)/`*.vcxproj`/`*.filters` 一并归档于此；业务核心 `MainStage.h`/`SaveFile.h` 等 Qt6 仍在用，保留在 `INIBrowser/` |
| `INIWeaver.sln`（在源根） | CMake 派生 | **Qt6 + QML 当前版本，构建用这个**（CMake 源在根 `CMakeLists.txt`）。由 `sync-sln.ps1` 从 `build\INIWeaver.sln` 派生（每个 .vcxproj 引用加 `build\` 前缀），不进 git；每次 cmake 重新配置后必须跑 `.\sync-sln.ps1` 同步。中间产物仍在 `build\`。 |

### 正确构建命令（Qt6）

```powershell
cd "c:\Users\xhh12\Documents\INIWeaver-1.0"
& "C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe" INIWeaver.sln -p:Configuration=Release -p:Platform=x64 -m -verbosity:minimal
```

- 构建产物：`Release\INIWeaver.exe`（位于根目录；由 `set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}")` 配合 VS 多配置生成器自动追加 `<CONFIG>` 子目录得到）
- CMake 重新配置（新增 QML 文件或源文件后需要）：
  ```powershell
  cmake -S "c:\Users\xhh12\Documents\INIWeaver-1.0" -B "c:\Users\xhh12\Documents\INIWeaver-1.0\build"
  .\sync-sln.ps1
  ```
  cmake 重新配置会改写 `build\INIWeaver.sln`（项目列表/路径可能变化），必须立刻跑一次 `.\sync-sln.ps1` 同步根 `INIWeaver.sln`，否则根 sln 不会反映新增/删除的项目。

### 部署

```powershell
robocopy "c:\Users\xhh12\Documents\INIWeaver-1.0\Release" "C:\Program Files (x86)\INIWeaver 1.0.9 Test2" INIWeaver.exe /xf *.pdb
```

Qt DLL 和 QML 插件目录已就绪，仅替换 EXE 即可。

**为什么只拷 EXE 就够**：QML 通过 `qt_add_qml_module(INIWeaver ... QML_FILES ...)` 编译进
EXE（构建树里有 `.rcc/qmlcache` 与 `INIWeaver_qml_module_dir_map.qrc`）。改了 `INIBrowser/ui/**`
的 QML 后必须重新编译 EXE，但部署无需额外拷贝 QML 文件。

### 诊断构建（推荐用独立构建树）

需要抓诊断日志时，不要改 `build`（会污染常规产物并导致整树重编）。用独立树：

```powershell
cmake -S . -B build-diag -DINIWEAVER_ENABLE_DIAG=ON
cmake --build build-diag --config Release --target INIWeaver
# 产物在 bin-diag\Release\INIWeaver.exe（CMAKE_RUNTIME_OUTPUT_DIRECTORY=bin-diag）
copy bin-diag\Release\INIWeaver.exe Release\INIWeaver.exe
robocopy "Release" "C:\Program Files (x86)\INIWeaver 1.0.9 Test2" INIWeaver.exe /xf *.pdb
```

注意点：

- 开关是 **CMake option `INIWEAVER_ENABLE_DIAG`**（不是宏名 `INIWEAVER_DIAG`）；
  宏由 `CMakeLists.txt:121-123` 按 option 条件并入。
- `build-diag` 的产物落在 **`bin-diag/Release/`**（不是 `build-diag/Release/`，那里只有 .exp/.lib）。
- 诊断版带 perf 探针和每帧日志，大项目下明显比常规版慢，**只用于排查**，验证完换回常规版。
- `Release/`、`publish/`、`bin-diag/`、`build-diag/`、`build/`、`INIWeaver.sln` 均被 gitignore。

## 项目架构

- **UI 框架**：Qt6 + QML（替代原 ImGui）
- **业务逻辑**：C++（`INIBrowser/` 下的 IB* 文件保留原 ImGui 时代的核心逻辑）
- **Qt 桥接层**：`INIBrowser/qt/` 下的 Controller/Model
- **QML UI**：`INIBrowser/ui/` 下按 panels/workspace/dialogs 组织
- **入口**：`INIBrowser/qt/QtMain.cpp`

## 编码约定

- 源码含中文注释必须保存为 UTF-8 with BOM，否则 MSVC 在 code page 936 下报 C4819/C2182/C2065
- Slint UI 已废弃，当前仅 QML
- 不要修改 `IBB_ModProject.h/.cpp` 相关代码（已移除）
- UI 逻辑必须保留 ImGui 版本的禁用规则（按钮 enabled 条件等）
- **每次代码修改完成并验证/部署后，必须立即 git commit**（提交源码与 CLAUDE.md，不提交截图/脚本/build 等杂项）

## 常见陷阱

- **编译错 sln**：`legacy-imgui\INIBrowser.sln` 是 ImGui 旧版本（已归档，路径未修复），打开/编译它不会反映 QML 改动。务必用源根目录的 `INIWeaver.sln`（由 `sync-sln.ps1` 从 `build\INIWeaver.sln` 派生；cmake 重新配置后必须跑 `.\sync-sln.ps1` 同步）。
- **根 sln 是派生产物**：源根的 `INIWeaver.sln` 不进 git，由 `sync-sln.ps1` 从 `build\INIWeaver.sln` 派生（每个 .vcxproj 引用加 `build\` 前缀）。cmake 重新配置后必须跑一次 `.\sync-sln.ps1`，否则新增/删除的项目不会反映到根 sln，IDE 编译会缺文件/剩残留。
- **构建产物根化**：根 `Release\INIWeaver.exe` 落点由 `CMAKE_RUNTIME_OUTPUT_DIRECTORY` 控制（详见 `CMakeLists.txt` 可执行目标段），不再落 `build\Release\`。
- **新增 QML 文件未加入 CMakeLists.txt**：会导致 QML 类型注册失败，启动时 `engine.rootObjects().isEmpty()` 返回 true，进程秒退。
- **QML 子目录未显式 import**：`Main.qml` 需要 `import "./workspace"` 等，否则组件解析失败启动崩溃。
- **布局管理中的元素不能用 anchors**：QML 规则，RowLayout/ColumnLayout 内的元素用 anchors 是未定义行为。
- **自定义组件名冲突**：避免与 QtQuick.Controls 内置类型重名（如本地 MenuBar 要改名为 AppMenuBar）。
- **QML Set/Map 键类型必须与数据源一致**：`links` 的 `sourceId`/`destId` 是 C++ `QString::number` 下发的**字符串**（防大整数精度丢失），而 `sections` 快照的 `sectionId` 是**数字**。用 Set/Map 做 O(1) 查询时写入与查找两侧必须统一 `String()` 归一，否则 `has()` 恒 false 且无报错（曾导致编组拖拽成员连线不跟随、连线选中高亮静默失效）。

## 连线坐标架构（重要）

> **2026-08-30 架构变更**：本节已从"端点表屏幕快照 + 三层补偿"重写为"世界坐标 + 每帧投影"。
> 旧的 `canvasDragOffset` / `zoomTransform`(zoomBaseRatio) / `viewportOffset` 三层补偿及其
> 配套的防抖收尾、回写门控、`zoomPending` 概念**已全部删除**，不要再按旧模型改动。

### 核心模型：世界坐标 + 每帧投影

模块节点与连线端点共用同一个定位模型，这是"缩放+平移永远同步"的根本原因：

```
screen = (worldEq − eqCenter) × ratio + viewCenter
```

- **共享视口态**：`viewRatio / viewBaseX / viewBaseY` 是 `WorkspaceView.qml` 的**根级属性**。
  模块壳 `sectionDelegate.updatePosition()` 与 `LinkRenderer` 读**同一份数据** ——
  不是"两份代码写对同一个表达式"，而是物理上不可能分叉。
- **端点表**（`rebuildLinkEndpoints` 产物）存 **Eq 空间绝对点**：
  `{sessionId, wx, wy, paWValid, pbWx, pbWy, pbWValid, isCollapsed, destId}`。
  视口无关 —— 平移/缩放/视口尺寸变化都不改变端点值，因此**都不需要重建端点表**。
- **QML 回写**写的是**世界坐标**（`screenToEq(screen) − topAncestorEqPos(sec)`，
  存"相对顶层祖先的偏移"），只在**世界几何变化**时发生：节点移动、折叠切换、
  行布局变化、拖拽结束、节点创建/入视口。**视口变化不触发回写。**

### 唯一的"补偿层"：dragOffset

拖拽期间 `EqPos` 不落盘（避免污染 Undo 栈），模块靠 `dragTerm` 位移、连线靠 `dragOffset`
位移，是同一个临时量的两种体现。保留 `LinkRenderer` 里
`srcSec.dragging / dstSec.dragging / dragMembers / lastDraggedId / isLastMassDragged`
的全部判定逻辑，不要动。

### 端点解析 helper

`resolveSourceWorld()` / `resolveDestWorld()`（`WorkspaceController`）是**唯一**的端点解析入口，
`rebuildLinkEndpoints` 与 `refreshLinkEndpoint` 共用。改端点逻辑只改这两处，不要在各调用点复制。
`resolveSourceWorld` 的可选出参 `int *outBranch` 输出命中的优先级分支号，供 `[LINK-SRC]` 诊断。

### 单位换算（最容易错的地方）

`EqSize` 是**逻辑尺寸**（`reportSectionSize` 里已除以 ratio）。世界坐标下所有模块内部锚点
都是**逻辑量，不乘 ratio**：

| 屏幕量（旧） | 世界量（新） |
|---|---|
| `EqPosToRePos(EqPos)` | `EqPos` |
| `EqSize.x * ratio` | `EqSize.x` |
| `13.0f * ratio`（RadioButton 左偏） | `13.0f` |
| `halfLine = 6.5f * ratio` | `6.5f` |
| `fontHeight * 0.7f` | `9.1f` |
| `contentTop = 28.0f` | `28.0f`（本来就是逻辑量） |

**例外**：`LinkRenderer` 的 `fontHeight = 13 * ratio`（Bezier 控制点、线宽、`straight` 判定）
是屏幕量，保持不变 —— 曲线美学本来就跟随缩放。

### 仍需注意的坑

- **编组内子模块的 EqPos 是组内相对坐标**：世界缓存必须锚 `topAncestorEqPos()`（沿
  `IncludedByModule` 上溯，带 32 层防护），不能锚自身 EqPos，否则编组拖拽时连线不跟随。
- **编组成员靠父节点 `updateAllCenters` 级联回写**（`composedMembersList` + `dragMembers` 叠加）。
- **回写时机收敛后要盯紧"首次布局"路径**：IIF 分量圆点靠 `Component.onCompleted` 置
  `iifReady` 后回写一次 + `onX/onY/onWidth/onHeight` 实时回写。视口变化不再触发
  `updateAllCenters`，若某布局变化路径不触发上述信号，分量世界缓存会陈旧
  （表现为"分量连线位置不准"，用 `[LINK-SRC]` 的 `d` 字段判定）。
- **改端点/投影逻辑前先跑完整手测**：单模块拖拽、编组拖拽、平移、缩放、
  平移中跨视口边界、迷你地图跳转、松手瞬间盯紧连线。

## 诊断日志机制

定位连线/拖动/端点类 bug 时用，平时关闭零开销。

### 开关

- 宏 `INIWEAVER_DIAG` 定义在 `CMakeLists.txt` 的 `target_compile_definitions(INIWeaver PRIVATE ...)` 里
- 开启：确保该行存在 → `cmake -S . -B build` → `.\sync-sln.ps1` → 编译
- 关闭：注释掉 `INIWEAVER_DIAG` 那行 → 同上重新配置 → 编译，所有诊断日志编译不进去

### 原理

- **C++ 端**：诊断 `qDebug()` 全部用 `#ifdef INIWEAVER_DIAG` ... `#endif` 包裹，宏未定义时编译不进去
- **QML 端**：`WorkspaceController::diagLogEnabled()` 由同一宏决定返回值（`#ifdef` 返回 true / 否则 false），QML 的 `console.log` 用 `if (workspaceController.diagLogEnabled())` 门控，宏关闭时 `if` 恒 false 零开销
- **输出位置**：`QtMain.cpp` 的 `qInstallMessageHandler`（约 line 158）把 qDebug/console.log 路由到 EXE 同目录的 `iniweaver_debug.log`（部署后即 `C:\Program Files (x86)\INIWeaver 1.0.9 Test2\iniweaver_debug.log`）；该 handler 是常驻的，不受宏影响，宏只控制日志"产生"端
- **读 log**：文件可能很大（onPaint 每帧每条连线都打），用 Grep 工具按标签搜，不要整文件读

### 日志标签

| 标签 | 来源 | 用途 |
|------|------|------|
| `[ONSHOW-DIAG]` | EditPanelController.cpp / WorkspaceController.cpp | 键行 OnShow 切换时序：toggleOnShow → refreshSectionLines → reportSectionSize(EqSize 新旧值) → SYNC/DEFER rebuild |
| `[LINK-DIAG]` | WorkspaceView.qml / LineRow.qml / LinkRenderer.qml | 连线端点：SectionNode 尺寸变化时刻、LineRow 回写的 acceptCenter 坐标、LinkRenderer onPaint 每条连线最终 pa/pb + 端点表 pb 原值 |
| `[DRAG-DIAG]` | WorkspaceController.cpp（beginMoveSection/endMoveSection/cleanup） | 拖动时序：begin/end 的 sectionId/lastDraggedId/dragOffset、cleanup 清零条件 PASSED/SKIPPED |
| `[EP-DIAG]` | WorkspaceController.cpp（rebuildLinkEndpoints） | 端点表每次重建逐连线输出 pa/pb 结果 + 全部候选来源值（LastCenter/acceptPoint/EqPos/EqSize/ratio），排查连线偏差首选 |
| `[LINK-DIAG] paintBegin` | LinkRenderer.qml | 每帧渲染开头的 dragOffset/eqCenter/ratio/viewBase/viewRatio（与 LINK-SRC 联用：端点世界坐标 + 投影 = 实际画出的位置） |
| `[PERF-DIAG]` | WorkspaceController（perfBegin/perfEnd/perfFrame） | 每 120 帧输出按总耗时排序的探针统计 + 平均/最差帧时间（含 C++.isSectionSelected、QML.Shell.activate 等） |
| `[EPW-DIAG]` | WorkspaceController.cpp（rebuildLinkEndpoints） | 世界缓存 ↔ 屏幕缓存交叉校验：`eqToScreen(world)` 与 `sv.LastCenter`/`acceptCenterByKey` 比对，偏差 >1px 才打（预算 40 条）。排查世界坐标换算/回写链路不一致 |
| `[COMP-W]` | SectionLineModel.cpp（setLinkNodeCenterAtKey） | IIF 分量圆点回写：`sess/key/lineIdx/mult/comp/screen`（预算 80 条）。`sess` 是端点查询的唯一 key |
| `[LINK-SRC]` | WorkspaceController.cpp（rebuildLinkEndpoints） | 每条连线的起点来源对账（预算 80 条）。`branch`=命中分支：1 折叠右端 / **2 分量·行圆点世界缓存（分量应为 2）** / 3 行级 acceptEq / 4 头部 acceptEq(行可见) / 5 隐藏行 / 6 头部 acceptEq / 7 兜底估算。`d`=世界投影与旧锚点 `sv.LastCenter` 的距离（应≈0）。**分量连线位置不对时首选** |
