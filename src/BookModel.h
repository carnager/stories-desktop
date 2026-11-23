#pragma once

#include <QAbstractListModel>
#include <QVariantMap>

/** Rows of books for a grid. Unchanged rows stay put, so covers don't flicker. */
class BookModel : public QAbstractListModel {
    Q_OBJECT
    Q_PROPERTY(int count READ count NOTIFY countChanged)
public:
    explicit BookModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    int count() const { return int(m_rows.size()); }
    void update(const QList<QVariantMap> &rows);

signals:
    void countChanged();

private:
    QList<QVariantMap> m_rows;
    QHash<int, QByteArray> m_roles;
};
