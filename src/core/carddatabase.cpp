#include "carddatabase.h"

#include "gziplinereader.h"

#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QSqlError>
#include <QSqlQuery>
#include <QVariant>

namespace {

// The database is a cache of Scryfall data, so a schema change just rebuilds it.
constexpr int kSchemaVersion = 1;

const QString kColumns = QStringLiteral(
    "id, oracle_id, name, front_name, layout, mana_cost, mana_value, type_line, oracle_text, "
    "power, toughness, loyalty, colors, color_identity, rarity, set_code, set_name, "
    "collector_number, image_small, image_normal, scryfall_uri, "
    "price_usd, price_usd_foil, price_eur, price_tix, legalities, playable");
constexpr int kColumnCount = 27;

const QStringList kSchema = {
    QStringLiteral("CREATE TABLE cards ("
                   " oracle_id TEXT PRIMARY KEY,"
                   " id TEXT NOT NULL,"
                   " name TEXT NOT NULL,"
                   " front_name TEXT NOT NULL,"
                   " layout TEXT, mana_cost TEXT, mana_value REAL, type_line TEXT, oracle_text TEXT,"
                   " power TEXT, toughness TEXT, loyalty TEXT, colors TEXT, color_identity TEXT,"
                   " rarity TEXT, set_code TEXT, set_name TEXT, collector_number TEXT,"
                   " image_small TEXT, image_normal TEXT, scryfall_uri TEXT,"
                   " price_usd REAL, price_usd_foil REAL, price_eur REAL, price_tix REAL,"
                   " legalities TEXT,"
                   " playable INTEGER NOT NULL)"),
    QStringLiteral("CREATE INDEX cards_name ON cards (name COLLATE NOCASE)"),
    QStringLiteral("CREATE INDEX cards_front_name ON cards (front_name COLLATE NOCASE)"),
    QStringLiteral("CREATE TABLE metadata (key TEXT PRIMARY KEY, value TEXT)"),
};

QVariant optionalValue(const std::optional<double> &value)
{
    return value ? QVariant(*value) : QVariant(QMetaType::fromType<double>());
}

std::optional<double> optionalDouble(const QVariant &value)
{
    return value.isNull() ? std::nullopt : std::optional<double>(value.toDouble());
}

void bindCard(QSqlQuery &query, const Card &card)
{
    const QVariantList values = {
        card.id, card.oracleId, card.name, card.frontName, card.layout, card.manaCost,
        card.manaValue, card.typeLine, card.oracleText, card.power, card.toughness, card.loyalty,
        card.colors, card.colorIdentity, card.rarity, card.setCode, card.setName,
        card.collectorNumber, card.imageSmall, card.imageNormal, card.scryfallUri,
        optionalValue(card.priceUsd), optionalValue(card.priceUsdFoil),
        optionalValue(card.priceEur), optionalValue(card.priceTix),
        QString::fromUtf8(QJsonDocument(card.legalities).toJson(QJsonDocument::Compact)),
        card.playable ? 1 : 0,
    };
    Q_ASSERT(values.size() == kColumnCount);
    for (int i = 0; i < values.size(); ++i)
        query.bindValue(i, values.at(i));
}

// Reads a row selected with kColumns, in that order.
Card cardFromRow(const QSqlQuery &query)
{
    int i = 0;
    auto next = [&] { return query.value(i++); };
    Card card;
    card.id = next().toString();
    card.oracleId = next().toString();
    card.name = next().toString();
    card.frontName = next().toString();
    card.layout = next().toString();
    card.manaCost = next().toString();
    card.manaValue = next().toDouble();
    card.typeLine = next().toString();
    card.oracleText = next().toString();
    card.power = next().toString();
    card.toughness = next().toString();
    card.loyalty = next().toString();
    card.colors = next().toString();
    card.colorIdentity = next().toString();
    card.rarity = next().toString();
    card.setCode = next().toString();
    card.setName = next().toString();
    card.collectorNumber = next().toString();
    card.imageSmall = next().toString();
    card.imageNormal = next().toString();
    card.scryfallUri = next().toString();
    card.priceUsd = optionalDouble(next());
    card.priceUsdFoil = optionalDouble(next());
    card.priceEur = optionalDouble(next());
    card.priceTix = optionalDouble(next());
    card.legalities = QJsonDocument::fromJson(next().toString().toUtf8()).object();
    card.playable = next().toBool();
    return card;
}

// Escapes LIKE wildcards so user text is matched literally (used with ESCAPE '\').
QString escapeLike(QString text)
{
    text.replace(QLatin1Char('\\'), QLatin1StringView("\\\\"));
    text.replace(QLatin1Char('%'), QLatin1StringView("\\%"));
    text.replace(QLatin1Char('_'), QLatin1StringView("\\_"));
    return text;
}

} // namespace

CardDatabase::CardDatabase(const QString &path, const QString &connectionName)
    : m_path(path)
    , m_connectionName(connectionName)
{
}

CardDatabase::~CardDatabase()
{
    {
        QSqlDatabase db = QSqlDatabase::database(m_connectionName, false);
        if (db.isValid())
            db.close();
    }
    if (QSqlDatabase::contains(m_connectionName))
        QSqlDatabase::removeDatabase(m_connectionName);
}

QSqlDatabase CardDatabase::connection() const
{
    return QSqlDatabase::database(m_connectionName, false);
}

bool CardDatabase::open()
{
    QDir().mkpath(QFileInfo(m_path).absolutePath());
    QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), m_connectionName);
    db.setDatabaseName(m_path);
    if (!db.open()) {
        m_error = db.lastError().text();
        return false;
    }
    QSqlQuery pragma(db);
    // WAL lets the UI keep reading while an import writes on another thread.
    pragma.exec(QStringLiteral("PRAGMA journal_mode = WAL"));
    pragma.exec(QStringLiteral("PRAGMA synchronous = NORMAL"));
    return ensureSchema();
}

bool CardDatabase::ensureSchema()
{
    QSqlDatabase db = connection();
    QSqlQuery query(db);
    int version = 0;
    if (query.exec(QStringLiteral("PRAGMA user_version")) && query.next())
        version = query.value(0).toInt();
    if (version == kSchemaVersion)
        return true;

    db.transaction();
    const QStringList statements = QStringList{QStringLiteral("DROP TABLE IF EXISTS cards"),
                                               QStringLiteral("DROP TABLE IF EXISTS metadata")}
                                   + kSchema
                                   + QStringList{QStringLiteral("PRAGMA user_version = %1").arg(kSchemaVersion)};
    for (const QString &statement : statements) {
        if (!query.exec(statement)) {
            m_error = query.lastError().text();
            db.rollback();
            return false;
        }
    }
    return db.commit();
}

int CardDatabase::cardCount() const
{
    QSqlQuery query(connection());
    if (query.exec(QStringLiteral("SELECT COUNT(*) FROM cards WHERE playable = 1")) && query.next())
        return query.value(0).toInt();
    m_error = query.lastError().text();
    return 0;
}

QList<Card> CardDatabase::searchByName(const QString &text, int limit) const
{
    const QString trimmed = text.trimmed();
    if (trimmed.isEmpty())
        return {};

    QSqlQuery query(connection());
    query.prepare(QStringLiteral("SELECT %1 FROM cards"
                                 " WHERE playable = 1 AND name LIKE :contains ESCAPE '\\'"
                                 " ORDER BY name LIKE :prefix ESCAPE '\\' DESC, name COLLATE NOCASE"
                                 " LIMIT :limit")
                      .arg(kColumns));
    const QString escaped = escapeLike(trimmed);
    query.bindValue(QStringLiteral(":contains"), QLatin1Char('%') + escaped + QLatin1Char('%'));
    query.bindValue(QStringLiteral(":prefix"), escaped + QLatin1Char('%'));
    query.bindValue(QStringLiteral(":limit"), limit);

    QList<Card> cards;
    if (!query.exec()) {
        m_error = query.lastError().text();
        return cards;
    }
    while (query.next())
        cards << cardFromRow(query);
    return cards;
}

std::optional<Card> CardDatabase::findByName(const QString &name) const
{
    QSqlQuery query(connection());
    query.prepare(QStringLiteral("SELECT %1 FROM cards"
                                 " WHERE name = :name COLLATE NOCASE OR front_name = :front COLLATE NOCASE"
                                 " ORDER BY playable DESC LIMIT 1")
                      .arg(kColumns));
    query.bindValue(QStringLiteral(":name"), name.trimmed());
    query.bindValue(QStringLiteral(":front"), name.trimmed());
    if (!query.exec()) {
        m_error = query.lastError().text();
        return std::nullopt;
    }
    if (!query.next())
        return std::nullopt;
    return cardFromRow(query);
}

QString CardDatabase::sourceUpdatedAt() const
{
    return metadata(QStringLiteral("source_updated_at"));
}

QString CardDatabase::metadata(const QString &key) const
{
    QSqlQuery query(connection());
    query.prepare(QStringLiteral("SELECT value FROM metadata WHERE key = :key"));
    query.bindValue(QStringLiteral(":key"), key);
    if (query.exec() && query.next())
        return query.value(0).toString();
    return {};
}

std::optional<ImportStats> CardDatabase::importScryfallBulk(const QString &gzipPath,
                                                            const QString &sourceUpdatedAt,
                                                            const std::function<void(double)> &progress,
                                                            const std::function<bool()> &isCanceled)
{
    GzipLineReader reader(gzipPath);
    if (!reader.open()) {
        m_error = reader.errorString();
        return std::nullopt;
    }

    QSqlDatabase db = connection();
    if (!db.transaction()) {
        m_error = db.lastError().text();
        return std::nullopt;
    }
    auto fail = [&](const QString &message) -> std::optional<ImportStats> {
        m_error = message;
        db.rollback();
        return std::nullopt;
    };

    QSqlQuery query(db);
    if (!query.exec(QStringLiteral("DELETE FROM cards")))
        return fail(query.lastError().text());

    QStringList placeholders;
    for (int i = 0; i < kColumnCount; ++i)
        placeholders << QStringLiteral("?");
    QSqlQuery insert(db);
    if (!insert.prepare(QStringLiteral("INSERT OR REPLACE INTO cards (%1) VALUES (%2)")
                            .arg(kColumns, placeholders.join(QLatin1Char(',')))))
        return fail(insert.lastError().text());

    ImportStats stats;
    const double totalBytes = qMax<qint64>(1, reader.compressedSize());
    QByteArray line;
    while (reader.readLine(line)) {
        line = line.trimmed();
        // Also accept the older bulk format: a JSON array with one card per line.
        if (line.isEmpty() || line == "[" || line == "]")
            continue;
        if (line.endsWith(','))
            line.chop(1);

        QJsonParseError parseError;
        const QJsonDocument document = QJsonDocument::fromJson(line, &parseError);
        const std::optional<Card> card = parseError.error == QJsonParseError::NoError && document.isObject()
                                             ? Card::fromScryfall(document.object())
                                             : std::nullopt;
        if (!card) {
            ++stats.skipped;
            continue;
        }
        bindCard(insert, *card);
        if (!insert.exec())
            return fail(insert.lastError().text());
        ++stats.imported;

        if (stats.imported % 500 == 0) {
            if (isCanceled && isCanceled())
                return fail(QStringLiteral("Import canceled"));
            if (progress)
                progress(reader.compressedPosition() / totalBytes);
        }
    }
    if (!reader.errorString().isEmpty())
        return fail(reader.errorString());
    if (stats.imported == 0)
        return fail(QStringLiteral("The file contained no cards"));

    QSqlQuery meta(db);
    meta.prepare(QStringLiteral("INSERT OR REPLACE INTO metadata (key, value) VALUES (?, ?)"));
    const QList<std::pair<QString, QString>> entries = {
        {QStringLiteral("source_updated_at"), sourceUpdatedAt},
        {QStringLiteral("imported_at"), QDateTime::currentDateTimeUtc().toString(Qt::ISODate)},
    };
    for (const auto &[key, value] : entries) {
        meta.bindValue(0, key);
        meta.bindValue(1, value);
        if (!meta.exec())
            return fail(meta.lastError().text());
    }

    if (!db.commit())
        return fail(db.lastError().text());
    if (progress)
        progress(1.0);
    return stats;
}
