cmake_minimum_required(VERSION 3.21)
project(INIWeaver VERSION 1.0.9 LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# MSVC /MD 运行时（Qt6 标准发行版使用 /MD，需匹配；原 ImGui 版用 /MT）
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreadedDLL")

# Qt6
set(CMAKE_PREFIX_PATH "C:/Qt6/6.8.1/msvc2022_64")
find_package(Qt6 REQUIRED COMPONENTS Core Gui Qml QmlIntegration Quick QuickControls2 Widgets QuickDialogs2)

qt_standard_project_setup()

# QTP0001 OLD: 资源前缀用 :/ 而非 :/qt/qml/，匹配 qmldir 的 prefer 路径
qt_policy(SET QTP0001 OLD)

# --------------------------------------------------------------------------
# 源文件收集
# --------------------------------------------------------------------------
file(GLOB_RECURSE IB_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/INIBrowser/*.cpp"
)
# 排除原 ImGui 入口（用 QtMain.cpp 替代）
list(REMOVE_ITEM IB_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/INIBrowser/INIBrowser.cpp")
# 排除嵌套构建产物目录（旧 MSBuild/Slint 遗留的 INIBrowser/INIBrowser/x64/...）
list(FILTER IB_SOURCES EXCLUDE REGEX
    "INIBrowser[/\\\\]INIBrowser[/\\\\]|INIBrowser[/\\\\]x64[/\\\\]|slint-gen[/\\\\]")
# 排除第三方库自带的 cpp（fmt/cjson 单独处理）
list(FILTER IB_SOURCES EXCLUDE REGEX
    "INIBrowser[/\\\\](fmt|cjson)[/\\\\]")

# ImGui 源文件
file(GLOB IMGUI_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/ImGui/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/ImGui/backends/*.cpp"
)

# fmt 源文件（header-only 模式下仍编译以匹配原配置）
set(FMT_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/INIBrowser/fmt/src/fmt.cc"
    "${CMAKE_CURRENT_SOURCE_DIR}/INIBrowser/fmt/src/format.cc"
)

# cJSON（C 文件）
set(CJSON_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/INIBrowser/cjson/cJSON.c"
)

# 资源文件
set(IB_RESOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/INIBrowser/Resource.rc"
)

# --------------------------------------------------------------------------
# 可执行目标
# --------------------------------------------------------------------------
qt_add_executable(INIWeaver WIN32
    INIBrowser/QtMain.cpp
    ${IB_SOURCES}
    ${IMGUI_SOURCES}
    ${FMT_SOURCES}
    ${CJSON_SOURCES}
    ${IB_RESOURCES}
)

# --------------------------------------------------------------------------
# 包含路径（匹配原 vcxproj AdditionalIncludeDirectories + IncludePath）
# --------------------------------------------------------------------------
target_include_directories(INIWeaver PRIVATE
    INIBrowser
    INIBrowser/FromEngine
    INIBrowser/fmt/include
    INIBrowser/qt
    ImGui
    ImGui/backends
    libs/glfw/include
)

# --------------------------------------------------------------------------
# 预处理器定义（匹配原 vcxproj）
# --------------------------------------------------------------------------
target_compile_definitions(INIWeaver PRIVATE
    FMT_DEPRECATED_SCAN
    FMT_END_EXPORT=
    FMT_BEGIN_EXPORT=
    FMT_EXPORT=
    FMT_HEADER_ONLY
    IMGUI_USE_WCHAR32
    _CRT_SECURE_NO_WARNINGS
    NDEBUG
    _WINDOWS
    UNICODE
    _UNICODE
    NOMINMAX
)

# 匹配原 vcxproj AdditionalOptions：禁用 C++20 distinct char8_t 类型
# （代码中存在 u8"..." 字面量与 std::string 直接拼接的写法）
if(MSVC)
    target_compile_options(INIWeaver PRIVATE /Zc:char8_t-)
endif()

# --------------------------------------------------------------------------
# 链接库（匹配原 vcxproj AdditionalDependencies）
# --------------------------------------------------------------------------
target_link_directories(INIWeaver PRIVATE
    libs/glfw/lib-vc2010-64
)

target_link_libraries(INIWeaver PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Qml
    Qt6::QmlIntegration
    Qt6::Quick
    Qt6::QuickControls2
    Qt6::Widgets
    Qt6::QuickDialogs2
    shlwapi
    opengl32
    glfw3
    user32
    gdi32
    comdlg32
    advapi32
    shell32
    ole32
    oleaut32
    uuid
    dbghelp
    winmm
)

# --------------------------------------------------------------------------
# QML 模块
# --------------------------------------------------------------------------
qt_add_qml_module(INIWeaver
    URI INIWeaver
    VERSION 1.0
    QML_FILES
        INIBrowser/ui/Main.qml
        INIBrowser/ui/AppMenuBar.qml
        INIBrowser/ui/StatusBar.qml
        INIBrowser/ui/Shortcuts.qml
        INIBrowser/ui/components/LinkRow.qml
        INIBrowser/ui/components/StyledButton.qml
        INIBrowser/ui/components/StyledCheckBox.qml
        INIBrowser/ui/dialogs/ConfirmDialog.qml
        INIBrowser/ui/dialogs/ConfirmDialog3.qml
        INIBrowser/ui/dialogs/ExportDialog.qml
        INIBrowser/ui/dialogs/ImportIniDialog.qml
        INIBrowser/ui/dialogs/ImportPreviewDialog.qml
        INIBrowser/ui/dialogs/WaitingDialog.qml
        INIBrowser/ui/dialogs/MessageDialog.qml
        INIBrowser/ui/dialogs/ErrorQueueDialog.qml
        INIBrowser/ui/dialogs/RightClickMenu.qml
        INIBrowser/ui/dialogs/SearchModuleDialog.qml
        INIBrowser/ui/dialogs/ShpTypeDialog.qml
        INIBrowser/ui/panels/FilePanel.qml
        INIBrowser/ui/panels/ModulesPanel.qml
        INIBrowser/ui/panels/ViewPanel.qml
        INIBrowser/ui/panels/ListPanel.qml
        INIBrowser/ui/panels/EditPanel.qml
        INIBrowser/ui/panels/SettingPanel.qml
        INIBrowser/ui/panels/AboutPanel.qml
        INIBrowser/ui/panels/DebugPanel.qml
        INIBrowser/ui/workspace/WorkspaceView.qml
        INIBrowser/ui/workspace/SectionNode.qml
        INIBrowser/ui/workspace/LinkRenderer.qml
        INIBrowser/ui/workspace/MiniMap.qml
        INIBrowser/ui/workspace/SelectionBox.qml
        INIBrowser/ui/workspace/SelectionActionButton.qml
        INIBrowser/ui/workspace/ContextMenu.qml
        INIBrowser/ui/workspace/LineRow.qml
        INIBrowser/ui/workspace/LinkNodePoint.qml
    SOURCES
        INIBrowser/qt/ProjectController.h
        INIBrowser/qt/ProjectController.cpp
        INIBrowser/qt/ModuleTreeModel.h
        INIBrowser/qt/ModuleTreeModel.cpp
        INIBrowser/qt/SectionListModel.h
        INIBrowser/qt/SectionListModel.cpp
        INIBrowser/qt/SectionLineModel.h
        INIBrowser/qt/SectionLineModel.cpp
        INIBrowser/qt/MenuController.h
        INIBrowser/qt/MenuController.cpp
        INIBrowser/qt/SettingController.h
        INIBrowser/qt/SettingController.cpp
        INIBrowser/qt/DialogController.h
        INIBrowser/qt/DialogController.cpp
        INIBrowser/qt/WorkspaceController.h
        INIBrowser/qt/WorkspaceController.cpp
        INIBrowser/qt/EditPanelController.h
        INIBrowser/qt/EditPanelController.cpp
)
