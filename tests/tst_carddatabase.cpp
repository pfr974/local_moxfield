#include "core/carddatabase.h"

#include <QFile>
#include <QTemporaryDir>
#include <QTest>

#include <zlib.h>

// Imports the fixture cards into a throwaway database and queries them.
class TestCardDatabase : public QObject
{
    Q_OBJECT

private slots:
    void init();
    void cleanup();
    void importsAndCounts();
    void searchPutsPrefixMatchesFirst();
    void searchIgnoresTokensAndWildcards();
    void findByNameAcceptsFrontFace();
    void findByNamePrefersRealCardOverToken();
    void failedImportKeepsOldData();

private:
    QString gzipFixture(const QByteArray &contents, const QString &fileName);

    std::unique_ptr<QTemporaryDir> m_dir;
    std::unique_ptr<CardDatabase> m_db;
};

QString TestCardDatabase::gzipFixture(const QByteArray &contents, const QString &fileName)
{
    const QString path = m_dir->filePath(fileName);
    gzFile file = gzopen(QFile::encodeName(path).constData(), "wb");
    if (!file)
        return {};
    gzwrite(file, contents.constData(), unsigned(contents.size()));
    gzclose(file);
    return path;
}

void TestCardDatabase::init()
{
    m_dir = std::make_unique<QTemporaryDir>();
    QVERIFY(m_dir->isValid());
    m_db = std::make_unique<CardDatabase>(m_dir->filePath(QStringLiteral("cards.sqlite")), QStringLiteral("test"));
    QVERIFY2(m_db->open(), qPrintable(m_db->lastError()));

    QFile fixture(QStringLiteral(TEST_DATA_DIR "/sample-cards.jsonl"));
    QVERIFY(fixture.open(QIODevice::ReadOnly));
    const QString gz = gzipFixture(fixture.readAll(), QStringLiteral("sample.jsonl.gz"));
    const std::optional<ImportStats> stats = m_db->importScryfallBulk(gz, QStringLiteral("2026-09-26T21:01:58.161+00:00"));
    QVERIFY2(stats.has_value(), qPrintable(m_db->lastError()));
    QCOMPARE(stats->imported, 8);
    QCOMPARE(stats->skipped, 0);
}

void TestCardDatabase::cleanup()
{
    m_db.reset();
    m_dir.reset();
}

void TestCardDatabase::importsAndCounts()
{
    QCOMPARE(m_db->cardCount(), 6);  // 8 cards minus the token and the art card
    QCOMPARE(m_db->sourceUpdatedAt(), QStringLiteral("2026-09-26T21:01:58.161+00:00"));
}

void TestCardDatabase::searchPutsPrefixMatchesFirst()
{
    const QList<Card> results = m_db->searchByName(QStringLiteral("ice"));
    QCOMPARE(results.size(), 1);
    QCOMPARE(results.first().name, QStringLiteral("Fire // Ice"));

    const QList<Card> witch = m_db->searchByName(QStringLiteral("WITCH"));
    QCOMPARE(witch.size(), 1);

    const QList<Card> mixed = m_db->searchByName(QStringLiteral("s"));
    QVERIFY(mixed.size() > 1);
    QVERIFY(mixed.first().name.startsWith(QLatin1Char('S'), Qt::CaseInsensitive));
}

void TestCardDatabase::searchIgnoresTokensAndWildcards()
{
    const QList<Card> crows = m_db->searchByName(QStringLiteral("Storm Crow"));
    QCOMPARE(crows.size(), 1);
    QCOMPARE(crows.first().layout, QStringLiteral("normal"));

    QVERIFY(m_db->searchByName(QStringLiteral("%")).isEmpty());
    QVERIFY(m_db->searchByName(QStringLiteral("   ")).isEmpty());
}

void TestCardDatabase::findByNameAcceptsFrontFace()
{
    const std::optional<Card> delver = m_db->findByName(QStringLiteral("delver of secrets"));
    QVERIFY(delver.has_value());
    QCOMPARE(delver->name, QStringLiteral("Delver of Secrets // Insectile Aberration"));
    QVERIFY(m_db->findByName(QStringLiteral("Fire // Ice")).has_value());
    QVERIFY(!m_db->findByName(QStringLiteral("Not A Card")).has_value());
}

void TestCardDatabase::findByNamePrefersRealCardOverToken()
{
    const std::optional<Card> crow = m_db->findByName(QStringLiteral("Storm Crow"));
    QVERIFY(crow.has_value());
    QVERIFY(crow->playable);
    QCOMPARE(crow->manaCost, QStringLiteral("{1}{U}"));
}

void TestCardDatabase::failedImportKeepsOldData()
{
    // A truncated download must roll back rather than leave a half-imported database.
    QFile fixture(QStringLiteral(TEST_DATA_DIR "/sample-cards.jsonl"));
    QVERIFY(fixture.open(QIODevice::ReadOnly));
    const QString gz = gzipFixture(fixture.readAll(), QStringLiteral("full.jsonl.gz"));
    QFile full(gz);
    QVERIFY(full.open(QIODevice::ReadOnly));
    const QByteArray truncatedBytes = full.read(full.size() / 2);
    QFile truncated(m_dir->filePath(QStringLiteral("truncated.jsonl.gz")));
    QVERIFY(truncated.open(QIODevice::WriteOnly));
    truncated.write(truncatedBytes);
    truncated.close();

    QVERIFY(!m_db->importScryfallBulk(truncated.fileName(), QStringLiteral("newer")).has_value());
    QVERIFY(m_db->lastError().contains(QLatin1StringView("truncated")));
    QCOMPARE(m_db->cardCount(), 6);
    QCOMPARE(m_db->sourceUpdatedAt(), QStringLiteral("2026-09-26T21:01:58.161+00:00"));

    QVERIFY(!m_db->importScryfallBulk(m_dir->filePath(QStringLiteral("missing.gz")), QStringLiteral("x")).has_value());
    QCOMPARE(m_db->cardCount(), 6);
}

QTEST_GUILESS_MAIN(TestCardDatabase)
#include "tst_carddatabase.moc"
