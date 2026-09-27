#include "core/card.h"

#include <QFile>
#include <QJsonDocument>
#include <QTest>

// Parses real Scryfall card objects from tests/data/sample-cards.jsonl.
class TestCard : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void normalCard();
    void transformCardUsesFrontFace();
    void modalDoubleFacedCard();
    void splitCard();
    void missingPriceIsEmpty();
    void nonGameLayoutsAreNotPlayable();
    void rejectsObjectsWithoutIds();

private:
    Card card(const QString &name, const QString &layout) const;
    QList<QJsonObject> m_objects;
};

void TestCard::initTestCase()
{
    QFile file(QStringLiteral(TEST_DATA_DIR "/sample-cards.jsonl"));
    QVERIFY2(file.open(QIODevice::ReadOnly), qPrintable(file.errorString()));
    while (!file.atEnd()) {
        const QByteArray line = file.readLine().trimmed();
        if (!line.isEmpty())
            m_objects << QJsonDocument::fromJson(line).object();
    }
    QCOMPARE(m_objects.size(), 8);
}

Card TestCard::card(const QString &name, const QString &layout) const
{
    for (const QJsonObject &object : m_objects) {
        if (object.value(QLatin1StringView("name")).toString() == name
            && object.value(QLatin1StringView("layout")).toString() == layout) {
            const std::optional<Card> card = Card::fromScryfall(object);
            if (card)
                return *card;
        }
    }
    qFatal("No fixture card %s (%s)", qPrintable(name), qPrintable(layout));
}

void TestCard::normalCard()
{
    const Card bolt = card(QStringLiteral("Lightning Bolt"), QStringLiteral("normal"));
    QCOMPARE(bolt.frontName, QStringLiteral("Lightning Bolt"));
    QCOMPARE(bolt.manaCost, QStringLiteral("{R}"));
    QCOMPARE(bolt.manaValue, 1.0);
    QCOMPARE(bolt.typeLine, QStringLiteral("Instant"));
    QVERIFY(bolt.oracleText.contains(QLatin1StringView("3 damage")));
    QCOMPARE(bolt.colors, QStringLiteral("R"));
    QCOMPARE(bolt.colorIdentity, QStringLiteral("R"));
    QVERIFY(bolt.imageNormal.startsWith(QLatin1StringView("https://cards.scryfall.io/normal/")));
    QVERIFY(bolt.priceUsd.has_value());
    QCOMPARE(bolt.legalities.value(QLatin1StringView("modern")).toString(), QStringLiteral("legal"));
    QVERIFY(bolt.playable);
}

void TestCard::transformCardUsesFrontFace()
{
    const Card delver = card(QStringLiteral("Delver of Secrets // Insectile Aberration"), QStringLiteral("transform"));
    QCOMPARE(delver.frontName, QStringLiteral("Delver of Secrets"));
    // No top-level cost, image or colours on transform cards: they come from the faces.
    QCOMPARE(delver.manaCost, QStringLiteral("{U}"));
    QCOMPARE(delver.power, QStringLiteral("1"));
    QCOMPARE(delver.colors, QStringLiteral("U"));
    QVERIFY(delver.imageNormal.contains(QLatin1StringView("/front/")));
    QVERIFY(delver.oracleText.contains(QLatin1StringView("Insectile Aberration\n")));
}

void TestCard::modalDoubleFacedCard()
{
    const Card witch = card(QStringLiteral("Witch Enchanter // Witch-Blessed Meadow"), QStringLiteral("modal_dfc"));
    QCOMPARE(witch.frontName, QStringLiteral("Witch Enchanter"));
    QCOMPARE(witch.manaValue, 4.0);
    QCOMPARE(witch.colors, QStringLiteral("W"));
    QVERIFY(!witch.imageSmall.isEmpty());
}

void TestCard::splitCard()
{
    const Card fireIce = card(QStringLiteral("Fire // Ice"), QStringLiteral("split"));
    QCOMPARE(fireIce.frontName, QStringLiteral("Fire"));
    QCOMPARE(fireIce.manaCost, QStringLiteral("{1}{R} // {1}{U}"));
    QCOMPARE(fireIce.colors, QStringLiteral("UR"));  // WUBRG order
}

void TestCard::missingPriceIsEmpty()
{
    const Card token = card(QStringLiteral("Storm Crow"), QStringLiteral("token"));
    QVERIFY(!token.priceUsd.has_value());
}

void TestCard::nonGameLayoutsAreNotPlayable()
{
    QVERIFY(!card(QStringLiteral("Storm Crow"), QStringLiteral("token")).playable);
    QVERIFY(!card(QStringLiteral("Brightglass Gearhulk // Brightglass Gearhulk"), QStringLiteral("art_series")).playable);
    QVERIFY(card(QStringLiteral("Bonecrusher Giant // Stomp"), QStringLiteral("adventure")).playable);
}

void TestCard::rejectsObjectsWithoutIds()
{
    QVERIFY(!Card::fromScryfall(QJsonObject{{QStringLiteral("name"), QStringLiteral("Nameless")}}));
}

QTEST_GUILESS_MAIN(TestCard)
#include "tst_card.moc"
