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

## 连线/拖拽一帧偏移注意事项（重要）

连线端点有两套坐标机制，任何改动都要维持它们的一致性，否则出现"连线比节点慢/快一帧"或大偏差：

### 坐标机制模型

1. **端点表快照 + 叠加偏移**（`rebuildLinkEndpoints` 产物 `m_linkEndpoints`/`m_linkEndpointsMap`）：表内 pa/pb 是"上次重建时刻"的屏幕坐标。渲染时 LinkRenderer 统一叠加 `canvasDragOffset`（画布平移）/ `zoomTransform`（缩放）/ `dragOffset`（拖拽模块）。
2. **QML 实时回写**：LineRow/LinkNodePoint/headLineRN 通过 `setLinkNodeCenter`/`setAcceptCenterByKey`/`setSectionAcceptPoint`/`setHeadLineRN` 把最新屏幕坐标写回 C++（LastCenter/acceptCenter/m_sectionAcceptPoint），**只在非叠加态（inputState!=1 且 !zoomPending）发生**。
3. **不变量**：叠加期间（平移 inputState==1 / 缩放 zoomPending / 拖模块 dragOffset 非零）端点表**禁止重建**（重建会清零 canvasDragOffset 并混入当前坐标系），一切靠偏移叠加；叠加结束的收尾链路（`onInputStateChanged`/`onZoomFinalizeRequested` → 回写 → `m_pendingRebuild` 队列重建）统一切换到新基准。

### 已踩过的坑（改动前先对照）

- **叠加期置脏 = 大偏差**：懒加载 culling（`setSectionCulled`）和新建 delegate 的 `reportSectionSize` 在平移/缩放中都会置脏端点表，消费端（refreshFromTimer 两处 rebuild 分支 + 全量路径）必须保持"叠加期不重建、dirty 保留"的门控。
- **同步重建抢在回写前 = 一帧偏移**：收尾时回写经 `Qt.callLater` 入队；tick 的重建若同步执行会先清 offset 并用旧坐标画一帧。tick 重建统一走 `m_pendingRebuild` 队列（QueuedConnection FIFO 排在 callLater 之后），且**执行时复核**过渡态（suppress/inputState==1/zoomPending/drag/massDrag 任一存在则放弃，保留 dirty 给收尾路径）。
- **cull 节点的端点坐标已含位移**：被懒加载 cull 的节点无回写，端点落 `EqPosToRePos` 兜底（按当前 EqPos 实时算，已含拖拽/平移位移）；LinkRenderer 对这类端点必须跳过 dragOffset 叠加（C++ 用 `paCulled`/`pbCulled` 标记），否则双重计数。
- **编组内子模块的 EqPos 是组内相对坐标**：无回写时 EqPos 兜底不适用；编组成员必须靠父节点 `updateAllCenters` 级联回写（`composedMembersList` + `dragMembers` 叠加）。
- **改叠加/重建时序前先跑一遍完整手测**：单模块拖拽、编组拖拽、平移、缩放、平移中跨视口边界、松手瞬间盯紧连线。

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
| `[LINK-DIAG] paintBegin` | LinkRenderer.qml | 每帧渲染开头的 canvasOffset/viewportOffset/dragOffset/eqCenter/ratio（与 EP-DIAG 联用：表坐标 + 叠加 = 实际画出的位置） |
| `[PERF-DIAG]` | WorkspaceController（perfBegin/perfEnd/perfFrame） | 每 120 帧输出按总耗时排序的探针统计 + 平均/最差帧时间（含 C++.isSectionSelected、QML.Shell.activate 等） |
