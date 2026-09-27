#include "cardlistmodel.h"

#include <QLocale>

void CardListModel::setCards(const QList<Card> &cards)
{
    beginResetModel();
    m_cards = cards;
    endResetModel();
}

int CardListModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : int(m_cards.size());
}

int CardListModel::columnCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : ColumnCount;
}

QVariant CardListModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= m_cards.size())
        return {};
    const Card &card = m_cards.at(index.row());

    if (role == Qt::TextAlignmentRole && index.column() == PriceColumn)
        return QVariant(Qt::AlignRight | Qt::AlignVCenter);
    if (role == Qt::ToolTipRole && index.column() == SetColumn)
        return card.setName;
    if (role != Qt::DisplayRole)
        return {};

    switch (index.column()) {
    case NameColumn:
        return card.name;
    case ManaCostColumn:
        return card.manaCost;
    case TypeColumn:
        return card.typeLine;
    case SetColumn:
        return card.setCode.toUpper();
    case PriceColumn:
        return card.priceUsd ? QLocale().toCurrencyString(*card.priceUsd, QStringLiteral("$")) : QString();
    }
    return {};
}

QVariant CardListModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation != Qt::Horizontal || role != Qt::DisplayRole)
        return {};
    switch (section) {
    case NameColumn:
        return tr("Name");
    case ManaCostColumn:
        return tr("Cost");
    case TypeColumn:
        return tr("Type");
    case SetColumn:
        return tr("Set");
    case PriceColumn:
        return tr("USD");
    }
    return {};
}
