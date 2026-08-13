// RightClickMenu.qml
// 阶段 11.4：通用右键菜单，对应 IBR_PopupManager::SetRightClickMenu 转发的右键菜单
// 由 DialogController.rightClickMenuRequested(title, items, posX, posY) 信号触发
// 菜单项通过 PushTextBack 注入 Texts 字段
import QtQuick
import QtQuick.Controls

Menu {
    id: root

    // 标题（用于调试，不显示）
    property string menuTitle: ""

    // 由 Connections 动态填充菜单项
    Instantiator {
        id: itemsInstantiator
        model: root.items
        delegate: MenuItem {
            text: modelData
            onTriggered: {
                // 通知 DialogController 用户选择了第 index 项
                dialogController.notifyRightClickMenuResult(index)
            }
        }
        onObjectAdded: (index, object) => root.insertItem(index, object)
        onObjectRemoved: (index, object) => root.removeItem(object)
    }

    property var items: []

    Connections {
        target: dialogController
        function onRightClickMenuRequested(title, items, posX, posY) {
            root.menuTitle = title
            root.items = items
            // 弹出在指定位置
            // 注意：QML Menu.popup 接受父项内相对坐标，需转换
            root.x = posX
            root.y = posY
            root.open()
        }
        function onRightClickMenuCleared() {
            root.close()
        }
    }

    onClosed: {
        // 用户点击外部关闭时通知 -1=取消
        // 注意：仅在尚未通知过结果时通知（避免与 MenuItem.onTriggered 重复）
        // 简化实现：每次 closed 都通知 -1，DialogController 侧做幂等处理
        if (dialogController.hasRightClickMenu) {
            dialogController.notifyRightClickMenuResult(-1)
        }
    }
}
