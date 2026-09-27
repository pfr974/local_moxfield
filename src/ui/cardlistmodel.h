#ifndef CARDLISTMODEL_H
#define CARDLISTMODEL_H

#include "core/card.h"

#include <QAbstractTableModel>
#include <QList>

// Table of cards: name, mana cost, type, set and price.
class CardListModel : public QAbstractTableModel
{
    Q_OBJECT

public:
    enum Column { NameColumn, ManaCostColumn, TypeColumn, SetColumn, PriceColumn, ColumnCount };

    using QAbstractTableModel::QAbstractTableModel;

    void setCards(const QList<Card> &cards);
    const Card &cardAt(int row) const { return m_cards.at(row); }

    int rowCount(const QModelIndex &parent = {}) const override;
    int columnCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;

private:
    QList<Card> m_cards;
};

#endif // CARDLISTMODEL_H
