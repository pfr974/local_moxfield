#include "carddetails.h"

#include <QCoreApplication>
#include <QLocale>

namespace {

QString tr(const char *text)
{
    return QCoreApplication::translate("CardDetails", text);
}

QString price(const std::optional<double> &value, const QString &symbol)
{
    return value ? QLocale().toCurrencyString(*value, symbol) : QStringLiteral("—");
}

QString legalityLabel(const QString &status)
{
    if (status == QLatin1StringView("legal"))
        return tr("Legal");
    if (status == QLatin1StringView("restricted"))
        return tr("Restricted");
    if (status == QLatin1StringView("banned"))
        return tr("Banned");
    return tr("Not legal");
}

} // namespace

QString cardDetailsHtml(const Card &card)
{
    QString html;
    html += QStringLiteral("<h2>%1</h2>").arg(card.name.toHtmlEscaped());
    if (!card.manaCost.isEmpty())
        html += QStringLiteral("<p><b>%1</b> &nbsp; (mana value %2)</p>")
                    .arg(card.manaCost.toHtmlEscaped(), QLocale().toString(card.manaValue));
    html += QStringLiteral("<p><i>%1</i></p>").arg(card.typeLine.toHtmlEscaped());

    if (!card.oracleText.isEmpty()) {
        QString text = card.oracleText.toHtmlEscaped();
        text.replace(QLatin1Char('\n'), QLatin1StringView("<br>"));
        html += QStringLiteral("<p>%1</p>").arg(text);
    }
    if (!card.power.isEmpty() || !card.toughness.isEmpty())
        html += QStringLiteral("<p><b>%1/%2</b></p>").arg(card.power.toHtmlEscaped(), card.toughness.toHtmlEscaped());
    if (!card.loyalty.isEmpty())
        html += QStringLiteral("<p>%1: <b>%2</b></p>").arg(tr("Loyalty"), card.loyalty.toHtmlEscaped());

    html += QStringLiteral("<p>%1 (%2 #%3) · %4</p>")
                .arg(card.setName.toHtmlEscaped(), card.setCode.toUpper().toHtmlEscaped(),
                     card.collectorNumber.toHtmlEscaped(), card.rarity.toHtmlEscaped());

    html += QStringLiteral("<h3>%1</h3><table cellspacing='0' cellpadding='2'>").arg(tr("Prices"));
    const QList<std::pair<QString, QString>> prices = {
        {tr("USD"), price(card.priceUsd, QStringLiteral("$"))},
        {tr("USD foil"), price(card.priceUsdFoil, QStringLiteral("$"))},
        {tr("EUR"), price(card.priceEur, QStringLiteral("€"))},
        {tr("MTGO tix"), price(card.priceTix, QString())},
    };
    for (const auto &[label, value] : prices)
        html += QStringLiteral("<tr><td>%1</td><td align='right'>%2</td></tr>").arg(label, value);
    html += QStringLiteral("</table>");

    html += QStringLiteral("<h3>%1</h3><table cellspacing='0' cellpadding='2'>").arg(tr("Legality"));
    const QList<std::pair<QString, QString>> formats = {
        {QStringLiteral("standard"), tr("Standard")}, {QStringLiteral("pioneer"), tr("Pioneer")},
        {QStringLiteral("modern"), tr("Modern")},     {QStringLiteral("legacy"), tr("Legacy")},
        {QStringLiteral("vintage"), tr("Vintage")},   {QStringLiteral("pauper"), tr("Pauper")},
        {QStringLiteral("commander"), tr("Commander")},
    };
    for (const auto &[key, label] : formats) {
        html += QStringLiteral("<tr><td>%1</td><td>%2</td></tr>")
                    .arg(label, legalityLabel(card.legalities.value(key).toString()));
    }
    html += QStringLiteral("</table>");

    if (!card.scryfallUri.isEmpty())
        html += QStringLiteral("<p><a href='%1'>%2</a></p>").arg(card.scryfallUri.toHtmlEscaped(), tr("View on Scryfall"));
    return html;
}
