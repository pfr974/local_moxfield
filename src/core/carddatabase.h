#ifndef CARDDATABASE_H
#define CARDDATABASE_H

#include "card.h"

#include <QList>
#include <QSqlDatabase>
#include <QString>

#include <functional>
#include <optional>

struct ImportStats
{
    int imported = 0;
    int skipped = 0;  // lines that weren't valid card objects
};

// Local SQLite store of card data. Each instance owns one named Qt SQL connection,
// and Qt connections can't cross threads, so create one instance per thread.
class CardDatabase
{
public:
    CardDatabase(const QString &path, const QString &connectionName);
    ~CardDatabase();

    CardDatabase(const CardDatabase &) = delete;
    CardDatabase &operator=(const CardDatabase &) = delete;

    bool open();
    QString lastError() const { return m_error; }

    // Number of playable cards (tokens, art cards etc. aren't counted).
    int cardCount() const;

    // Playable cards whose name contains `text`, names starting with it first.
    QList<Card> searchByName(const QString &text, int limit = 200) const;

    // Exact, case-insensitive match on the full name or the front-face name,
    // preferring a real card over a token of the same name.
    std::optional<Card> findByName(const QString &name) const;

    // Scryfall's updated_at for the data currently stored, or empty if none.
    QString sourceUpdatedAt() const;

    // Replaces all cards with a Scryfall bulk file (gzipped JSON Lines) in one transaction:
    // on failure or cancellation the previous data is left untouched.
    // `progress` receives 0..1; `isCanceled` is polled as the import runs.
    std::optional<ImportStats> importScryfallBulk(const QString &gzipPath,
                                                  const QString &sourceUpdatedAt,
                                                  const std::function<void(double)> &progress = {},
                                                  const std::function<bool()> &isCanceled = {});

private:
    QSqlDatabase connection() const;
    bool ensureSchema();
    QString metadata(const QString &key) const;

    QString m_path;
    QString m_connectionName;
    mutable QString m_error;
};

#endif // CARDDATABASE_H
