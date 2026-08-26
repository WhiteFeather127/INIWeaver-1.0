// Shortcuts.qml
// 快捷键声明，对应 IBR_HotKey.cpp 的 22 个快捷键
// 键位从 config.json 加载，此处使用默认值
// inputFieldFocused 条件对应 IBR_HotKey.cpp:24 LastOperateOnText 检查
import QtQuick
import QtQuick.Controls

Item {
    // 由 Main.qml 绑定：TextField/TextArea/SpinBox 获得焦点时为 true，此时禁用工作区快捷键
    // 对应 IBR_HotKey.cpp:24 `if (IBR_WorkSpace::LastOperateOnText) return false;`
    property bool inputFieldFocused: false

    // 从 config.json（QtMain 暴露的 hotKeyMap）取键位，缺失则用默认（对应 ImGui IBR_HotKey 可配置快捷键）
    function hk(name, def) {
        var v = hotKeyMap ? hotKeyMap[name] : undefined
        return (v && v.length > 0) ? v : def
    }

    // 项目级快捷键（对应 IBR_ProjectManager::ProjActionByKey）
    // 不受 inputFieldFocused 影响（ImGui 侧这些快捷键在 MainStage.h:208 检查 !HasPopup）
    Shortcut {
        sequence: hk("Save", "Ctrl+S")
        onActivated: projectController.saveProject()
    }
    Shortcut {
        sequence: hk("SaveAs", "Ctrl+Shift+S")
        onActivated: projectController.saveAsProject()
    }
    Shortcut {
        sequence: hk("Open", "Ctrl+O")
        onActivated: projectController.openProject()
    }
    Shortcut {
        sequence: hk("Close", "Ctrl+W")
        // 关闭项目=关闭后新建（对应 ImGui ProjActionByKey Close = ProjOpen_CreateAction）
        onActivated: projectController.newProject()
    }
    Shortcut {
        sequence: hk("Export", "Ctrl+E")
        onActivated: dialogController.showExportDialog()
    }

    // 工作区快捷键（对应 IBR_WorkSpace.cpp:927-958, 1131-1145, 1872-1878）
    // 在 TextField 获得焦点时禁用（对应 IBR_HotKey.cpp:24 LastOperateOnText 检查）
    Shortcut {
        sequence: hk("Copy", "Ctrl+C")
        enabled: !inputFieldFocused
        onActivated: workspaceController.copySelected()
    }
    Shortcut {
        sequence: hk("Paste", "Ctrl+V")
        enabled: !inputFieldFocused
        onActivated: workspaceController.paste()
    }
    Shortcut {
        sequence: hk("Cut", "Ctrl+X")
        enabled: !inputFieldFocused
        onActivated: workspaceController.cutSelected()
    }
    Shortcut {
        sequence: hk("SelectAll", "Ctrl+A")
        enabled: !inputFieldFocused
        onActivated: workspaceController.selectAll()
    }
    // SelectNone（Ctrl+Shift+A，对应 IBR_HotKey.h:45）
    Shortcut {
        sequence: hk("SelectNone", "Ctrl+Shift+A")
        enabled: !inputFieldFocused
        onActivated: workspaceController.clearSelection()
    }
    // SelectInvert（Ctrl+Shift+I，对应 IBR_HotKey.h:46）
    Shortcut {
        sequence: hk("SelectInvert", "Ctrl+Shift+I")
        enabled: !inputFieldFocused
        onActivated: workspaceController.invertSelection()
    }
    Shortcut {
        sequence: hk("Delete", "Delete")
        enabled: !inputFieldFocused
        onActivated: workspaceController.deleteSelected()
    }
    Shortcut {
        sequence: hk("Duplicate", "Ctrl+D")
        enabled: !inputFieldFocused
        onActivated: workspaceController.duplicateSelected()
    }

    // DeleteAll（对应 IBR_WorkSpace.cpp:944-957，删除所有模块）
    Shortcut {
        sequence: hk("DeleteAll", "Ctrl+Shift+D")
        enabled: !inputFieldFocused
        onActivated: workspaceController.deleteAllSections()
    }

    // Center（对应 IBR_WorkSpace.cpp:935-938 MoveToCenter）
    Shortcut {
        sequence: hk("Center", "F5")
        enabled: !inputFieldFocused
        onActivated: workspaceController.centerView()
    }

    // SwitchDisplayMode（对应 IBR_WorkSpace.cpp:958 ShowRegName ^= 1）
    // 切换寄存器名/显示名
    Shortcut {
        sequence: hk("SwitchDisplayMode", "F1")
        enabled: !inputFieldFocused
        onActivated: workspaceController.switchDisplayMode()
    }

    // RenameModule（对应 IBR_WorkSpace.cpp:1874 sd.RenameDisplay()）
    Shortcut {
        sequence: hk("RenameModule", "F2")
        enabled: !inputFieldFocused
        onActivated: workspaceController.renameSelected()
    }

    // RenameRegister（对应 IBR_WorkSpace.cpp:1875 sd.RenameRegister()）
    Shortcut {
        sequence: hk("RenameRegister", "F3")
        enabled: !inputFieldFocused
        onActivated: workspaceController.renameRegisterSelected()
    }

    // Refresh（对应 IBR_WorkSpace.cpp:939-943 IBR_Inst_Project.UpdateAll()）
    Shortcut {
        sequence: hk("Refresh", "Ctrl+R")
        enabled: !inputFieldFocused
        onActivated: workspaceController.refresh()
    }

    // Escape（对应 IBR_Components.cpp:533-538 Cancel 语义）
    // 弹窗自身的 CloseOnEscape 会处理关闭；这里仅在无弹窗时 clearSelection
    // dialogController.hasPopup/hasRightClickMenu 覆盖通用弹窗和右键菜单
    // 其他 Dialog（ConfirmDialog 等）打开时 Shortcut 仍会触发，但 clearSelection
    // 在无选中节点时是空操作，影响可忽略
    Shortcut {
        sequence: hk("Cancel", "Escape")
        onActivated: {
            if (!dialogController.hasPopup && !dialogController.hasRightClickMenu) {
                workspaceController.clearSelection()
            }
        }
    }

    // 撤销/重做（对应 IBG_UndoStack::Undo/Redo）
    Shortcut {
        sequence: hk("Undo", "Ctrl+Z")
        enabled: !inputFieldFocused
        onActivated: workspaceController.undo()
    }
    Shortcut {
        sequence: hk("Redo", "Ctrl+Y")
        enabled: !inputFieldFocused
        onActivated: workspaceController.redo()
    }
}
