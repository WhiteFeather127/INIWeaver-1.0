// WorkspaceSectionModel.cpp
#include "WorkspaceSectionModel.h"

#include <algorithm>

WorkspaceSectionModel::WorkspaceSectionModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

int WorkspaceSectionModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid())
        return 0;
    return static_cast<int>(m_items.size());
}

QVariant WorkspaceSectionModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= static_cast<int>(m_items.size()))
        return {};
    // 任意 role 都返回 QVariantMap：
    // - Repeater 的 modelData 取 DisplayRole → 仍为 QVariantMap，QML 现有 modelData.xxx 绑定不变
    // - 自定义 SectionDataRole 供显式访问
    return m_items[index.row()];
}

QHash<int, QByteArray> WorkspaceSectionModel::roleNames() const
{
    return {
        { Qt::DisplayRole, "display" },
        { SectionDataRole, "sectionData" },
    };
}

qulonglong WorkspaceSectionModel::sectionIdOf(const QVariant &item)
{
    return item.toMap().value(QStringLiteral("sectionId")).toULongLong();
}

void WorkspaceSectionModel::updateFrom(const QVariantList &items)
{
    // 快速路径：内容完全相同 → 无操作
    if (items == m_items)
        return;

    // 空 ↔ 非空：全量
    if (m_items.isEmpty() || items.isEmpty())
    {
        beginResetModel();
        m_items = items;
        endResetModel();
        return;
    }

    // 建索引（sectionId → 列表位置）
    QHash<qulonglong, int> oldIdx, newIdx;
    oldIdx.reserve(m_items.size());
    newIdx.reserve(items.size());
    for (int i = 0; i < m_items.size(); ++i)
        oldIdx.insert(sectionIdOf(m_items[i]), i);
    for (int j = 0; j < items.size(); ++j)
        newIdx.insert(sectionIdOf(items[j]), j);

    // 删除集（旧有新无）与插入集（新有旧无）
    QVector<int> removed, inserted;
    for (int i = 0; i < m_items.size(); ++i)
    {
        if (!newIdx.contains(sectionIdOf(m_items[i])))
            removed.push_back(i);
    }
    for (int j = 0; j < items.size(); ++j)
    {
        if (!oldIdx.contains(sectionIdOf(items[j])))
            inserted.push_back(j);
    }

    // 检查公共项（两边都有）相对顺序是否一致。
    // std::map 遍历序稳定，删除/新建只影响局部；顺序真正变化（折叠/重组等）时全量兜底，
    // 保证 QML 侧每个 index 的 delegate 始终对应正确节点。
    QVector<qulonglong> oldCommon, newCommon;
    oldCommon.reserve(m_items.size());
    newCommon.reserve(items.size());
    for (int i = 0; i < m_items.size(); ++i)
    {
        auto sid = sectionIdOf(m_items[i]);
        if (newIdx.contains(sid))
            oldCommon.push_back(sid);
    }
    for (int j = 0; j < items.size(); ++j)
    {
        auto sid = sectionIdOf(items[j]);
        if (oldIdx.contains(sid))
            newCommon.push_back(sid);
    }
    if (oldCommon != newCommon)
    {
        beginResetModel();
        m_items = items;
        endResetModel();
        return;
    }

    // 先删除（倒序，索引不冲突）
    std::sort(removed.begin(), removed.end(), std::greater<int>());
    for (int r : removed)
    {
        beginRemoveRows(QModelIndex(), r, r);
        m_items.removeAt(r);
        endRemoveRows();
    }

    // 再插入（倒序）：删除完成后模型 == 旧列表去删除项（顺序同新列表公共子序列）。
    // 对新增项 j（新列表索引），插入位置 k = j 前面公共项数量 = j - (items[0..j-1] 中新增项数)。
    // 倒序插入保证已插入的大索引不干扰后续小索引。
    std::sort(inserted.begin(), inserted.end(), std::greater<int>());
    for (int j : inserted)
    {
        int frontAdded = 0;
        for (int t = 0; t < j; ++t)
        {
            if (!oldIdx.contains(sectionIdOf(items[t])))
                ++frontAdded;
        }
        int k = j - frontAdded;
        beginInsertRows(QModelIndex(), k, k);
        m_items.insert(k, items[j]);
        endInsertRows();
    }

    // 重要：先把新值写回 m_items，否则 dataChanged 后 data() 仍返回旧 QVariantMap，
    // 冻结/忽略/隐藏等字段的更新永远不会反映到 QML（隐藏"看似生效"只是因 removeRows 移除了节点，
    // 字段值实际也没下发）。此步之前 m_items 已与 items 同长同序（remove/insert 已处理，公共子序列一致）。
    for (int j = 0; j < items.size(); ++j)
        m_items[j] = items[j];

    // 公共项内容变化 → dataChanged（QML 重求值绑定但不重建 delegate）。
    // 覆盖选中/位置/行模型引用等变化；值未变的项由 QML 惰性绑定自然跳过。
    QVector<int> changedRows;
    for (int i = 0; i < m_items.size(); ++i)
    {
        if (newIdx.contains(sectionIdOf(m_items[i])))
            changedRows.push_back(i);
    }
    if (!changedRows.isEmpty())
    {
        emit dataChanged(index(changedRows.first()), index(changedRows.last()));
    }
}
