#include "bulkdatadownloader.h"

#include <QDir>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QSaveFile>
#include <QUrl>

namespace {

const QUrl kIndexUrl(QStringLiteral("https://api.scryfall.com/bulk-data/oracle-cards"));

// Abort a transfer after this long without receiving any data.
constexpr int kTransferTimeoutMs = 60 * 1000;

} // namespace

BulkDataDownloader::BulkDataDownloader(QObject *parent)
    : QObject(parent)
    , m_network(new QNetworkAccessManager(this))
{
}

BulkDataDownloader::~BulkDataDownloader()
{
    // Abort quietly: no signals to receivers that may already be half destroyed.
    if (m_reply) {
        m_reply->disconnect(this);
        m_reply->abort();
    }
    if (m_file)
        m_file->cancelWriting();
}

QNetworkRequest BulkDataDownloader::scryfallRequest(const QUrl &url)
{
    // https://scryfall.com/docs/api: identify the app and say what we accept.
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::UserAgentHeader,
                      QStringLiteral("local_moxfield/%1 (+https://github.com/pfr974/local_moxfield)")
                          .arg(QStringLiteral(LOCAL_MOXFIELD_VERSION)));
    request.setRawHeader("Accept", "application/json;q=0.9,*/*;q=0.8");
    request.setTransferTimeout(kTransferTimeoutMs);
    return request;
}

void BulkDataDownloader::start(const QString &knownUpdatedAt, const QString &destinationPath)
{
    if (isRunning())
        return;
    m_knownUpdatedAt = knownUpdatedAt;
    m_destinationPath = destinationPath;
    m_reply = m_network->get(scryfallRequest(kIndexUrl));
    connect(m_reply, &QNetworkReply::finished, this, &BulkDataDownloader::onIndexFinished);
}

void BulkDataDownloader::cancel()
{
    if (m_reply)
        m_reply->abort();
}

QString BulkDataDownloader::replyError(QNetworkReply *reply) const
{
    if (reply->error() == QNetworkReply::OperationCanceledError)
        return QStringLiteral("Download canceled");
    const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    if (reply->error() != QNetworkReply::NoError)
        return reply->errorString();
    if (status != 200)
        return QStringLiteral("Scryfall returned HTTP %1").arg(status);
    return {};
}

void BulkDataDownloader::onIndexFinished()
{
    QNetworkReply *reply = m_reply;
    reply->deleteLater();
    m_reply = nullptr;

    if (const QString error = replyError(reply); !error.isEmpty()) {
        emit failed(error);
        return;
    }

    const QJsonObject index = QJsonDocument::fromJson(reply->readAll()).object();
    // Scryfall moved from a JSON array (download_uri) to JSON Lines (jsonl_download_uri);
    // both work with the importer.
    QString url = index.value(QLatin1StringView("jsonl_download_uri")).toString();
    if (url.isEmpty())
        url = index.value(QLatin1StringView("download_uri")).toString();
    m_updatedAt = index.value(QLatin1StringView("updated_at")).toString();
    m_expectedSize = index.value(QLatin1StringView("compressed_size")).toInteger();
    if (url.isEmpty() || m_updatedAt.isEmpty()) {
        emit failed(QStringLiteral("Unexpected response from Scryfall's bulk-data index"));
        return;
    }

    if (m_updatedAt == m_knownUpdatedAt) {
        emit upToDate(m_updatedAt);
        return;
    }

    QDir().mkpath(QFileInfo(m_destinationPath).absolutePath());
    // QSaveFile writes to a temporary file and only replaces the destination on commit(),
    // so a failed download never leaves a half-written file behind.
    m_file = std::make_unique<QSaveFile>(m_destinationPath);
    if (!m_file->open(QIODevice::WriteOnly)) {
        emit failed(m_file->errorString());
        m_file.reset();
        return;
    }

    m_reply = m_network->get(scryfallRequest(QUrl(url)));
    connect(m_reply, &QNetworkReply::readyRead, this, &BulkDataDownloader::onFileReadyRead);
    connect(m_reply, &QNetworkReply::downloadProgress, this, [this](qint64 received, qint64 total) {
        emit progress(received, total > 0 ? total : m_expectedSize);
    });
    connect(m_reply, &QNetworkReply::finished, this, &BulkDataDownloader::onFileFinished);
}

void BulkDataDownloader::onFileReadyRead()
{
    if (m_file && m_reply && m_file->write(m_reply->readAll()) < 0)
        m_reply->abort();
}

void BulkDataDownloader::onFileFinished()
{
    QNetworkReply *reply = m_reply;
    reply->deleteLater();
    m_reply = nullptr;
    const std::unique_ptr<QSaveFile> file = std::move(m_file);

    QString error = replyError(reply);
    if (error.isEmpty() && file->error() != QFileDevice::NoError)
        error = file->errorString();
    if (error.isEmpty()) {
        file->write(reply->readAll());
        if (!file->commit())
            error = file->errorString();
    } else {
        file->cancelWriting();
    }

    if (error.isEmpty())
        emit finished(m_destinationPath, m_updatedAt);
    else
        emit failed(error);
}
