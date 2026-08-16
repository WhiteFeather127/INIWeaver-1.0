// WorkspaceSectionModel.h
// 画布 Section 节点的增量模型：包装 QVariantList（每项为 buildSectionItem 的 QVariantMap），
// 通过行插入/删除/dataChanged 信号让 QML Repeater 只增删变化的 delegate。
// 背景：QML Repeater 对 QVariantList 整表替换会销毁重建全部 SectionNode（新建/删除模块
// 卡顿根因）；QAbstractListModel 的增量通知（beginInsertRows/beginRemoveRows）让
// Repeater 复用无关节点，仅增删变化项。
#pragma once

#include <QAbstractListModel>
#include <QVariantList>
#include <QHash>
#include <QVector>

class WorkspaceSectionModel : public QAbstractListModel
{
    Q_OBJECT
public:
    enum Role
    {
        SectionDataRole = Qt::UserRole + 1  // 返回 QVariantMap（buildSectionItem 字段）
    };

    explicit WorkspaceSectionModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    // 增量更新：与当前内容 diff，仅对变化的项发信号。
    // 顺序保持的增删（删除/新建模块）走增量路径；顺序真正变化（折叠/重组等）全量兜底。
    void updateFrom(const QVariantList &items);

    // 取 QVariantMap 中的 sectionId（buildSectionItem 存为 qulonglong）
    static qulonglong sectionIdOf(const QVariant &item);

private:
    QVariantList m_items;
};
