#pragma once
#include <QObject>
#include <QVariantList>
#include <QVariantMap>
#include <QString>
#include <QtQmlIntegration/qqmlintegration.h>
#include "IBR_Project.h"  // ModuleID_t, INVALID_MODULE_ID（"无激活" 哨兵不能用 0，0 是合法模块 ID）

// EditPanelController：编辑面板桥接层
// 对应 ImGui 版本 IBR_EditFrame 命名空间（IBR_Misc.cpp:587-940）
// 负责在侧边栏显示选中模块的 INI 键值列表，支持新增/删除行、OnShow 编辑、文本编辑模式
class EditPanelController : public QObject
{
    Q_OBJECT
    QML_ELEMENT

    // 当前编辑模块信息
    Q_PROPERTY(qulonglong currentSectionId READ currentSectionId NOTIFY currentSectionIdChanged)
    Q_PROPERTY(QString displayName READ displayName NOTIFY displayNameChanged)
    Q_PROPERTY(bool isEmpty READ isEmpty NOTIFY isEmptyChanged)

    // 键值列表（对应 IBR_EditFrame::EditLines + RenderUI_Lines）
    Q_PROPERTY(QVariantList editLines READ editLines NOTIFY editLinesChanged)

    // 文本编辑模式（对应 IBR_EditFrame::OnTextEdit + RenderUI_TextEdit）
    Q_PROPERTY(bool onTextEdit READ onTextEdit NOTIFY onTextEditChanged)
    Q_PROPERTY(QString textEditContent READ textEditContent WRITE setTextEditContent NOTIFY textEditContentChanged)

    // 粘贴时刷新注册名（对应 IBR_EditFrame::RenderUI_UseOwnName + NeedtoMangle）
    Q_PROPERTY(bool needtoMangle READ needtoMangle NOTIFY needtoMangleChanged)

public:
    explicit EditPanelController(QObject *parent = nullptr);

    qulonglong currentSectionId() const { return m_currentSectionId; }
    QString displayName() const { return m_displayName; }
    bool isEmpty() const { return m_isEmpty; }
    QVariantList editLines() const { return m_editLines; }
    bool onTextEdit() const { return m_onTextEdit; }
    QString textEditContent() const { return m_textEditContent; }
    void setTextEditContent(const QString &v) { m_textEditContent = v; }
    bool needtoMangle() const { return m_needtoMangle; }

    // 设置当前编辑模块（由 WorkspaceController 选中模块时调用）
    // 对应 IBR_EditFrame::SetActive（IBR_Misc.cpp:615-640）
    void setActive(qulonglong sectionId);

    // 清空编辑面板（对应 IBR_EditFrame::Clear，IBR_Misc.cpp:942-946）
    // 取消选中模块时调用，Empty=true → 侧边栏不显示编辑内容
    Q_INVOKABLE void clear();

    // 刷新键值列表（从 IBR_EditFrame 读取最新数据）
    Q_INVOKABLE void refreshLines();

    // 新增行（对应 RenderUI_NewLine 的 "＋" 按钮逻辑，IBR_Misc.cpp:701-727）
    Q_INVOKABLE void addLine(const QString &key, const QString &value);

    // 多值键添加新值（对应 WorkSpaceLine::RenderUI 的 "+" 按钮，IBR_Misc.cpp:373-386）
    // MergeLine(key, Index_AlwaysNew, 初始格式化串, Replace)，同一组键只有一个 "+" 按钮
    Q_INVOKABLE void addLineValue(const QString &key);

    // 获取 Key 的初始值提示（对应 RenderUI_NewLine 的 TextDisabled，IBR_Misc.cpp:737-748）
    // 当 Value 为空时，从默认类型列表查询 Key 的初始值，用于显示提示
    Q_INVOKABLE QString getInitialValue(const QString &key) const;

    // 新增行 Key 输入检索候选（对应 IBR_Combo.cpp:144-186 EditStringWithOptions）
    // 输入时实时从默认类型列表检索匹配键（Name/DescShort 不分大小写包含 + 大写缩写匹配），
    // 返回 {items:[{name,descShort,descLong,inWrongSection}], tooMany}，最多 100 条
    Q_INVOKABLE QVariantMap queryKeySuggestions(const QString &input) const;

    // 删除行（对应 RenderUI_OnShow 的 "移除行" 按钮，IBR_Misc.cpp:858-864）
    Q_INVOKABLE void removeLine(const QString &key);

    // 切换 OnShow 显示状态（对应 RenderUI_OnShow 的 RadioButton，IBR_Misc.cpp:827-832）
    Q_INVOKABLE void toggleOnShow(const QString &key);

    // 设置 OnShow 描述（对应 RenderUI_OnShow 的描述编辑框，IBR_Misc.cpp:849-856）
    Q_INVOKABLE void setOnShowDesc(const QString &key, const QString &desc);

    // 设置行值（对应 SidebarLine::RenderUI → IBB_IniLine::RenderUI 的值编辑控件）
    Q_INVOKABLE void setLineValue(const QString &key, const QString &value);
    // 按分量索引设置行值（同名多行键 isMultiple 的每个值独立编辑）
    Q_INVOKABLE void setLineValueAt(const QString &key, int index, const QString &value);
    // IIF 分量写回（对齐 imgui mf：写该分量 V.Value 后 RegenFormattedString），idx=edge 分量索引
    Q_INVOKABLE void setIifValue(const QString &key, int mult, int idx, const QString &v);
    // IIF bool 分量写回（IIS_Bool + IIC_Bool::FmtType）
    Q_INVOKABLE void setIifBool(const QString &key, int mult, int idx, bool v);

    // 切换到文本编辑模式（对应 IBR_EditFrame::SwitchToText，IBR_Misc.cpp:649-661）
    Q_INVOKABLE void switchToText();

    // 退出文本编辑模式（对应 IBR_EditFrame::ExitTextEdit，IBR_Misc.cpp:663-685）
    Q_INVOKABLE void exitTextEdit(bool save);

    // 切换"粘贴时刷新注册名"（对应 RenderUI_UseOwnName，IBR_Misc.cpp:771-793）
    Q_INVOKABLE void toggleUseOwnName();

signals:
    void currentSectionIdChanged();
    void displayNameChanged();
    void isEmptyChanged();
    void editLinesChanged();
    void onTextEditChanged();
    void textEditContentChanged();
    void needtoMangleChanged();
    // 通知画布：指定 sectionId 的行数据已变更，需刷新对应 SectionLineModel
    // WorkspaceController 监听此信号，只刷新对应 section 的画布行模型，避免全量 refresh
    void sectionDataChanged(qulonglong sectionId);

private:
    // "无激活"哨兵用 INVALID_MODULE_ID(UINT64_MAX)，不能用 0——首个新建模块 ID 就是 0，
    // 用 0 会导致"选中模块 0"与"无激活"混淆（模块 0 无法正确显示编辑行/无法判断取消选中）。
    qulonglong m_currentSectionId{INVALID_MODULE_ID};
    QString m_displayName;
    bool m_isEmpty{true};
    QVariantList m_editLines;
    bool m_onTextEdit{false};
    QString m_textEditContent;
    bool m_needtoMangle{false};
    bool m_pendingRebuild{false};  // 异步 rebuild 去重标志

    // 从 IBR_EditFrame::EditLines + CurSection 重建 QVariantList
    void rebuildEditLines();
};
