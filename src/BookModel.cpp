#include "BookModel.h"

BookModel::BookModel(QObject *parent)
    : QAbstractListModel(parent)
{
    int role = Qt::UserRole + 1;
    for (const char *name : {"bookId", "title", "author", "series", "cover", "status", "fraction", "line"})
        m_roles.insert(role++, name);
}

int BookModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : int(m_rows.size());
}

QVariant BookModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= m_rows.size())
        return {};
    return m_rows[index.row()].value(QString::fromLatin1(m_roles.value(role)));
}

QHash<int, QByteArray> BookModel::roleNames() const
{
    return m_roles;
}

void BookModel::update(const QList<QVariantMap> &rows)
{
    bool sameBooks = rows.size() == m_rows.size();
    for (int i = 0; sameBooks && i < rows.size(); ++i)
        sameBooks = rows[i].value(QStringLiteral("bookId")) == m_rows[i].value(QStringLiteral("bookId"));
    if (!sameBooks) {
        beginResetModel();
        m_rows = rows;
        endResetModel();
        emit countChanged();
        return;
    }
    for (int i = 0; i < rows.size(); ++i) {
        if (rows[i] != m_rows[i]) {
            m_rows[i] = rows[i];
            emit dataChanged(index(i), index(i));
        }
    }
}
