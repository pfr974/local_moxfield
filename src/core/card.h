#ifndef CARD_H
#define CARD_H

#include <QJsonObject>
#include <QString>
#include <QStringList>

#include <optional>

// One Magic card as stored locally: the subset of a Scryfall card object the app uses.
// Multi-faced cards (transform, modal DFC, split, adventure...) are flattened: the name
// and type line keep Scryfall's "A // B" form, rules text joins the faces, and the image
// and mana cost fall back to the front face when there is no top-level value.
struct Card
{
    QString id;          // Scryfall id of the printing Scryfall picked for this card
    QString oracleId;    // stable across all printings
    QString name;        // full name, e.g. "Delver of Secrets // Insectile Aberration"
    QString frontName;   // front face only, as most decklists write it
    QString layout;
    QString manaCost;
    double manaValue = 0.0;
    QString typeLine;
    QString oracleText;
    QString power;
    QString toughness;
    QString loyalty;
    QString colors;         // WUBRG letters, e.g. "UR"; empty for colourless
    QString colorIdentity;  // same format
    QString rarity;
    QString setCode;
    QString setName;
    QString collectorNumber;
    QString imageSmall;
    QString imageNormal;
    QString scryfallUri;
    std::optional<double> priceUsd;
    std::optional<double> priceUsdFoil;
    std::optional<double> priceEur;
    std::optional<double> priceTix;
    QJsonObject legalities;  // format -> "legal" / "not_legal" / "restricted" / "banned"
    bool playable = true;    // false for tokens, art cards, emblems, planes...

    // std::nullopt if the object is missing an id, oracle id or name.
    static std::optional<Card> fromScryfall(const QJsonObject &json);

    // Layouts that exist in Scryfall's data but never go in a deck.
    static bool isPlayableLayout(const QString &layout);
};

#endif // CARD_H
