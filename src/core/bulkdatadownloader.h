#ifndef BULKDATADOWNLOADER_H
#define BULKDATADOWNLOADER_H

#include <QNetworkRequest>
#include <QObject>
#include <QPointer>
#include <QString>

#include <memory>

class QNetworkAccessManager;
class QNetworkReply;
class QSaveFile;

// Downloads Scryfall's "Oracle Cards" bulk file: one entry per card, as gzipped JSON Lines.
// Scryfall regenerates it daily; start() first checks the bulk-data index and skips the
// download when the file hasn't changed since `knownUpdatedAt`.
class BulkDataDownloader : public QObject
{
    Q_OBJECT

public:
    explicit BulkDataDownloader(QObject *parent = nullptr);
    ~BulkDataDownloader() override;

    void start(const QString &knownUpdatedAt, const QString &destinationPath);
    void cancel();
    bool isRunning() const { return !m_reply.isNull(); }

    // A request with the headers Scryfall asks every API client to send.
    static QNetworkRequest scryfallRequest(const QUrl &url);

signals:
    void progress(qint64 bytesReceived, qint64 bytesTotal);
    void upToDate(const QString &updatedAt);
    void finished(const QString &filePath, const QString &updatedAt);
    void failed(const QString &message);

private:
    void onIndexFinished();
    void onFileReadyRead();
    void onFileFinished();
    QString replyError(QNetworkReply *reply) const;

    QNetworkAccessManager *m_network;
    QPointer<QNetworkReply> m_reply;
    std::unique_ptr<QSaveFile> m_file;
    QString m_knownUpdatedAt;
    QString m_destinationPath;
    QString m_updatedAt;
    qint64 m_expectedSize = 0;
};

#endif // BULKDATADOWNLOADER_H
