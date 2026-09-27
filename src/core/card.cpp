#include "card.h"

#include <QJsonArray>
#include <QSet>

namespace {

// Value of `key` on the card itself, or on its front face for multi-faced cards.
QString frontValue(const QJsonObject &json, const QJsonArray &faces, QLatin1StringView key)
{
    if (json.contains(key))
        return json.value(key).toString();
    return faces.isEmpty() ? QString() : faces.first().toObject().value(key).toString();
}

// Colour letters from one or more Scryfall colour arrays, in WUBRG order.
QString colorLetters(const QList<QJsonArray> &arrays)
{
    QString present;
    for (const QJsonArray &array : arrays) {
        for (const QJsonValue &value : array)
            present += value.toString();
    }
    QString ordered;
    for (const QChar letter : QStringLiteral("WUBRG")) {
        if (present.contains(letter))
            ordered += letter;
    }
    return ordered;
}

// Scryfall sends prices as decimal strings, or null when there is no price.
std::optional<double> parsePrice(const QJsonValue &value)
{
    bool ok = false;
    const double price = value.toString().toDouble(&ok);
    return ok ? std::optional<double>(price) : std::nullopt;
}

} // namespace

std::optional<Card> Card::fromScryfall(const QJsonObject &json)
{
    const QJsonArray faces = json.value(QLatin1StringView("card_faces")).toArray();

    Card card;
    card.id = json.value(QLatin1StringView("id")).toString();
    card.oracleId = frontValue(json, faces, QLatin1StringView("oracle_id"));
    card.name = json.value(QLatin1StringView("name")).toString();
    if (card.id.isEmpty() || card.oracleId.isEmpty() || card.name.isEmpty())
        return std::nullopt;

    card.frontName = card.name.section(QLatin1StringView(" // "), 0, 0);
    card.layout = json.value(QLatin1StringView("layout")).toString();
    card.manaCost = frontValue(json, faces, QLatin1StringView("mana_cost"));
    card.manaValue = json.value(QLatin1StringView("cmc")).toDouble();
    card.typeLine = json.value(QLatin1StringView("type_line")).toString();
    card.power = frontValue(json, faces, QLatin1StringView("power"));
    card.toughness = frontValue(json, faces, QLatin1StringView("toughness"));
    card.loyalty = frontValue(json, faces, QLatin1StringView("loyalty"));

    if (json.contains(QLatin1StringView("oracle_text"))) {
        card.oracleText = json.value(QLatin1StringView("oracle_text")).toString();
    } else {
        QStringList texts;
        for (const QJsonValue &face : faces) {
            const QJsonObject faceObject = face.toObject();
            texts << faceObject.value(QLatin1StringView("name")).toString() + QLatin1Char('\n')
                         + faceObject.value(QLatin1StringView("oracle_text")).toString();
        }
        card.oracleText = texts.join(QLatin1StringView("\n\n"));
    }

    if (json.contains(QLatin1StringView("colors"))) {
        card.colors = colorLetters({json.value(QLatin1StringView("colors")).toArray()});
    } else {
        QList<QJsonArray> faceColors;
        for (const QJsonValue &face : faces)
            faceColors << face.toObject().value(QLatin1StringView("colors")).toArray();
        card.colors = colorLetters(faceColors);
    }
    card.colorIdentity = colorLetters({json.value(QLatin1StringView("color_identity")).toArray()});

    card.rarity = json.value(QLatin1StringView("rarity")).toString();
    card.setCode = json.value(QLatin1StringView("set")).toString();
    card.setName = json.value(QLatin1StringView("set_name")).toString();
    card.collectorNumber = json.value(QLatin1StringView("collector_number")).toString();
    card.scryfallUri = json.value(QLatin1StringView("scryfall_uri")).toString();

    QJsonObject images = json.value(QLatin1StringView("image_uris")).toObject();
    if (images.isEmpty() && !faces.isEmpty())
        images = faces.first().toObject().value(QLatin1StringView("image_uris")).toObject();
    card.imageSmall = images.value(QLatin1StringView("small")).toString();
    card.imageNormal = images.value(QLatin1StringView("normal")).toString();

    const QJsonObject prices = json.value(QLatin1StringView("prices")).toObject();
    card.priceUsd = parsePrice(prices.value(QLatin1StringView("usd")));
    card.priceUsdFoil = parsePrice(prices.value(QLatin1StringView("usd_foil")));
    card.priceEur = parsePrice(prices.value(QLatin1StringView("eur")));
    card.priceTix = parsePrice(prices.value(QLatin1StringView("tix")));

    card.legalities = json.value(QLatin1StringView("legalities")).toObject();
    card.playable = isPlayableLayout(card.layout);
    return card;
}

bool Card::isPlayableLayout(const QString &layout)
{
    static const QSet<QString> nonGameLayouts = {
        QStringLiteral("token"),    QStringLiteral("double_faced_token"),
        QStringLiteral("emblem"),   QStringLiteral("art_series"),
        QStringLiteral("front_card"), QStringLiteral("planar"),
        QStringLiteral("scheme"),   QStringLiteral("vanguard"),
    };
    return !nonGameLayouts.contains(layout);
}
