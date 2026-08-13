// ContextMenu.qml
// 右键菜单组件，对应 IBR_WorkSpace::OpenRightClick() (line 627-681)
// 菜单项：添加模块 / 全选 / 粘贴 / 复制 / 剪切 / 删除 / 缩合 / 冻结 / 解冻 / 隐藏 / 显示 / 忽略 / 取消忽略 / 刷新寄存器名
import QtQuick
import QtQuick.Controls

Menu {
    id: root

    property var clickPos: ({ x: 0, y: 0 })

    MenuItem {
        text: qsTr("添加模块")
        onTriggered: menuController.activeMenu = 1  // 切到模块面板
    }

    MenuSeparator {}

    MenuItem {
        text: qsTr("复制")
        onTriggered: workspaceController.copySelected()
    }
    MenuItem {
        text: qsTr("剪切")
        onTriggered: workspaceController.cutSelected()
    }
    MenuItem {
        text: qsTr("粘贴")
        onTriggered: workspaceController.paste()
    }
    MenuItem {
        text: qsTr("删除")
        onTriggered: workspaceController.deleteSelected()
    }
    MenuItem {
        text: qsTr("克隆")
        onTriggered: workspaceController.duplicateSelected()
    }

    MenuSeparator {}

    MenuItem {
        text: qsTr("全选")
        onTriggered: workspaceController.selectAll()
    }
    MenuItem {
        text: qsTr("取消选择")
        onTriggered: workspaceController.clearSelection()
    }

    MenuSeparator {}

    // 智能互斥显示（对应 ImGui IBR_WorkSpace.cpp:1213-1260）
    // 全冻结时只显示"解冻"，否则只显示"冻结"
    MenuItem {
        text: qsTr("冻结")
        visible: !workspaceController.selectedAllFrozen()
        onTriggered: workspaceController.freezeSelected(true)
    }
    MenuItem {
        text: qsTr("解冻")
        visible: workspaceController.selectedAllFrozen()
        onTriggered: workspaceController.freezeSelected(false)
    }
    MenuItem {
        text: qsTr("隐藏")
        visible: !workspaceController.selectedAllHidden()
        onTriggered: workspaceController.hideSelected(true)
    }
    MenuItem {
        text: qsTr("显示")
        visible: workspaceController.selectedAllHidden()
        onTriggered: workspaceController.hideSelected(false)
    }
    MenuItem {
        text: qsTr("忽略")
        visible: !workspaceController.selectedAllIgnored()
        onTriggered: workspaceController.ignoreSelected(true)
    }
    MenuItem {
        text: qsTr("取消忽略")
        visible: workspaceController.selectedAllIgnored()
        onTriggered: workspaceController.ignoreSelected(false)
    }
    MenuItem {
        text: qsTr("缩合")
        onTriggered: workspaceController.composeSelected()
    }

    MenuSeparator {}

    MenuItem {
        text: qsTr("刷新寄存器名")
        onTriggered: projectController.refreshAllRegName()
    }
}
