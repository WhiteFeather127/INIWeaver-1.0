// ContextMenuHost.qml
// 统一右键菜单宿主（Popup 单例）—— 项目内所有右键菜单的唯一入口。
// 对齐 ImGui IBR_PopupManager 单例架构：所有右键入口统一调用 show() 提交内容，
// 菜单项由统一数据协议 itemDescs 驱动（ContextMenuItemList 渲染）。
//
// 用法：
//   ContextMenuHost { id: contextMenuHost }   // 挂 Overlay.overlay，全局唯一
//   contextMenuHost.show(descs, screenX, screenY, handler)  // 弹出菜单（自动关旧，handler=动作回调）
//   contextMenuHost.hide()                            // 关闭（含全部子菜单）
//   contextMenuHost.moduleTreeDescs()                 // 空白右键内容（模块树+操作项）
//   handler: (action) => handle(action)               // 菜单项触发（show 传入）
//   onMenuClosed: ...                                 // 菜单关闭（可回读 checkedStates）
//
// 统一数据协议（元素）：
//   { type: "item", text, action, enabled?, checkable?, checked?, desc? }
//   { type: "separator" }
//   { type: "submenu", text, action, items? }   // items 缺省时按模块树 levelChildren(action) 懒加载
// 特殊 action："module:<key>" 在内部处理为添加模块，其余转发 actionTriggered。
import QtQuick
import QtQuick.Controls

Popup {
    id: root
    padding: 0
    closePolicy: Popup.CloseOnPressOutside | Popup.CloseOnEscape

    // ===== 对外接口 =====
    property var itemDescs: []
    // 模块菜单选中模块时的工作区视口放置坐标（右键/拖放入口设置；非模块菜单为 null）
    property var placePos: null
    // 勾选状态表（action -> bool），checkable 项实时更新；关闭后供回读（如圆点 UseLink）
    property var checkedStates: ({})
    // 菜单关闭（用户关闭/Esc/点外部/菜单项触发后），供需要回读勾选状态的入口使用
    signal menuClosed()
    // 本次菜单的动作回调（show 时由入口传入，动作分发完全隔离；hide 时清空）
    property var _handler: null

    // 子菜单栈（ContextMenuSubLevel 实例）
    property var levels: []
    // 当前被 hover 的子菜单项计数（0 且鼠标不在任意层内 → 收起子层）
    property int folderHoverCount: 0

    background: Rectangle {
        color: "#252526"
        border.color: "#3c3c3c"
        border.width: 1
        radius: 6
    }

    contentItem: ContextMenuItemList {
        id: list
        itemDescs: root.itemDescs
        checkedStates: root.checkedStates
        levelRef: root
        onActionTriggered: (action) => root.dispatchAction(action)
        onSubmenuRequested: (desc, g, ref) => root.openChildFor(desc, g, ref)
        onFolderHoverChanged: (on) => root.onFolderHoverChanged(on)
        onHoverLeft: root.onHoverLeft()
    }

    // 子菜单组件（延迟实例化）
    Component {
        id: subLevelComp
        ContextMenuSubLevel {}
    }

    // ===== 显示/关闭 =====
    // 统一显示入口：descs=菜单内容，screenX/Y=屏幕坐标（自动转 Overlay 坐标并钳制到屏幕内），
    // handler=本次菜单的动作回调（action → 入口自行处理；hide 后清空）
    function show(descs, screenX, screenY, handler) {
        root.closeAllLevels()
        // 初始化勾选状态（checkable 项取描述里的初始值）
        var st = {}
        if (descs) {
            for (var i = 0; i < descs.length; i++) {
                var d = descs[i]
                if (d && d.checkable && d.checked) st[d.action] = true
            }
        }
        root.checkedStates = st
        root.itemDescs = descs || []
        root._handler = (typeof handler === "function") ? handler : null
        // 屏幕坐标 → Overlay 坐标，钳制到可用区域（对齐 ImGui 越界校正 IBR_Components.cpp:640-653）
        var o = Overlay.overlay.mapFromGlobal(screenX, screenY)
        root.x = root.clampMenuX(o.x, root.implicitWidth)
        root.y = Math.max(2, Math.min(o.y, Overlay.overlay.height - root.implicitHeight - 2))
        root.open()
    }

    // 垂直/水平定位辅助：水平位置优先从参考点向右展开，右侧空间不足则翻到参考点左侧，
    // 避免菜单/子层被钳到屏幕右缘而远离光标（对齐 ImGui 越界校正，但向左翻转而非硬钳制）
    function clampMenuX(x, w) {
        if (x + w <= Overlay.overlay.width - 2) return Math.max(2, x)
        return Math.max(2, x - w)
    }

    function hide() {
        root.closeAllLevels()
        root.close()
        root._handler = null
    }

    // ===== 动作分发 =====
    function dispatchAction(action) {
        if (!action) return
        if (action.indexOf("module:") === 0) {
            // 模块树：点选模块 → 添加到工作区（对应 ImGui Tree_RenderUISidebar 模块点选）。
            // 若该模块菜单带了工作区视口放置坐标（placePos），放到该位置；否则默认布局位置
            var key = action.substring(7)
            if (root.placePos)
                moduleTreeModel.placeModuleByKey(key, root.placePos.x, root.placePos.y)
            else
                moduleTreeModel.addModuleByKey(key)
            root.hide()
            return
        }
        // 其余动作全部经本次菜单的动作回调交给入口处理（单例被多入口共用，分发完全隔离）
        if (root._handler) root._handler(action)
        root.hide()
    }

    // ===== 级联子菜单 =====
    // 生成子菜单内容：submenu 描述带 items 用 items；否则按模块树路径懒加载（对齐 ImGui 逐层渲染）
    function subMenuDescs(desc) {
        if (!desc) return []
        if (desc.items) return desc.items
        if (desc.type === "submenu" && moduleTreeModel)
            return root.treeItemsToDescs(moduleTreeModel.levelChildren(desc.action))
        return []
    }

    // 展开子菜单：裁剪 ref 之后的所有层，在右侧创建新层（对齐 ModuleTreePopup.openChild）
    function openChildFor(desc, globalPos, ref) {
        var subDescs = root.subMenuDescs(desc)
        var idx = root.levels.indexOf(ref)
        var start = (idx < 0) ? 0 : idx + 1
        for (var i = root.levels.length - 1; i >= start; --i) {
            root.levels[i].destroy()
            root.levels.splice(i, 1)
        }
        if (subDescs.length === 0) return  // 空文件夹不弹
        var o = Overlay.overlay.mapFromGlobal(globalPos.x, globalPos.y)
        var lv = subLevelComp.createObject(Overlay.overlay)
        lv.itemDescs = subDescs
        lv.checkedStates = root.checkedStates
        // 动态 Popup 不依赖内容隐式尺寸（动态创建时 contentItem 可能未布局），显式计算：
        // 菜单项 30px / 分隔线 7px（与 ContextMenuItemList delegate 高度一致）
        var subW = 180
        var subH = 0
        for (var si = 0; si < subDescs.length; ++si)
            subH += (subDescs[si].type === "separator") ? 7 : 30
        lv.width = subW
        lv.height = subH
        // 右侧放不下子菜单时，翻到请求它的那一层（ref）的左侧，而非仅从本条右缘左移一个菜单宽
        //（左移一个宽会仍压在父菜单上）。ref 为主菜单时为 root Popup（x=root.x）。
        var parentLeft = (ref && ref.x !== undefined) ? ref.x : o.x
        lv.x = (o.x + lv.width <= Overlay.overlay.width - 2)
               ? Math.max(2, o.x)
               : Math.max(2, parentLeft - lv.width)
        lv.y = Math.max(2, Math.min(o.y, Overlay.overlay.height - lv.height - 2))
        lv.actionTriggered.connect((a) => root.dispatchAction(a))
        lv.submenuRequested.connect((d, g, r) => root.openChildFor(d, g, r))
        lv.folderHoverChanged.connect((on) => root.onFolderHoverChanged(on))
        lv.hoverLeft.connect(() => root.onHoverLeft())
        lv.open()
        root.levels.push(lv)
    }

    function closeAllLevels() {
        for (var i = root.levels.length - 1; i >= 0; --i) {
            root.levels[i].destroy()
            root.levels.splice(i, 1)
        }
        root.folderHoverCount = 0
    }

    // ===== 子菜单收起管理 =====
    function onFolderHoverChanged(on) {
        root.folderHoverCount += on ? 1 : -1
        if (root.folderHoverCount < 0) root.folderHoverCount = 0
    }

    function onHoverLeft() {
        Qt.callLater(root.maybeCollapse)
    }

    // 鼠标离开所有文件夹且不在主菜单/任一子层内 → 收起子层（主菜单保留，对齐 ImGui 悬停消失）
    function maybeCollapse() {
        if (root.folderHoverCount > 0) return
        var g = workspaceController.globalMousePos()
        var o = Overlay.overlay.mapFromGlobal(g.x, g.y)
        if (root.ptInRect(o, root.x, root.y, root.width, root.height)) return
        for (var i = 0; i < root.levels.length; ++i) {
            var lv = root.levels[i]
            if (root.ptInRect(o, lv.x, lv.y, lv.width, lv.height)) return
        }
        root.closeAllLevels()
    }

    function ptInRect(p, x, y, w, h) {
        return p.x >= x && p.x <= x + w && p.y >= y && p.y <= y + h
    }

    // ===== 模块树内容 =====
    // 文件夹 → submenu（action=path）；模块 → item（action="module:key"，desc=描述）
    function treeItemsToDescs(items) {
        var descs = []
        for (var i = 0; i < (items ? items.length : 0); ++i) {
            var it = items[i]
            if (it.isFolder) {
                descs.push({ type: "submenu", text: it.name, action: it.path })
            } else {
                descs.push({ type: "item", text: it.name,
                             action: "module:" + it.moduleKey, desc: it.descLong })
            }
        }
        return descs
    }

    // 空白右键完整内容（对齐 ImGui OpenRightClick IBR_WorkSpace.cpp:628-682）：
    //   特殊树根层 → 全选(空项目灰) → 粘贴 → 刷新寄存器名 → 完整树根层
    function moduleTreeDescs() {
        if (moduleTreeModel) moduleTreeModel.filter = ""
        var sp = [], al = []
        if (moduleTreeModel) {
            var items = moduleTreeModel.levelChildren("")
            for (var i = 0; i < items.length; ++i) {
                if (items[i].path.indexOf("S:") === 0) sp.push(items[i])
                else al.push(items[i])
            }
        }
        var descs = []
        descs = descs.concat(root.treeItemsToDescs(sp))
        var hasSections = workspaceController && workspaceController.sections.length > 0
        descs.push({ type: "item", text: (i18n.rev, i18n.tr("GUI_SelectAll")), action: "selectAll", enabled: hasSections })
        descs.push({ type: "item", text: (i18n.rev, i18n.tr("GUI_Paste")), action: "paste" })
        descs.push({ type: "item", text: (i18n.rev, i18n.tr("GUI_RefreshAllRegName")), action: "refreshRegName" })
        // 特殊块创建（对应 ImGui IBR_WorkSpace.cpp:663-674）
        descs.push({ type: "item", text: (i18n.rev, i18n.tr("GUI_CreateCommentBlock")), action: "createComment" })
        descs.push({ type: "item", text: (i18n.rev, i18n.tr("GUI_CreateSingleValBlock")), action: "createSingleVal" })
        descs.push({ type: "separator" })
        descs = descs.concat(root.treeItemsToDescs(al))
        return descs
    }

    // ===== 生命周期 =====
    onClosed: {
        root.closeAllLevels()
        root._handler = null
        root.placePos = null
        root.menuClosed()
    }
}
