#include "mainwindow.h"

#include "carddetails.h"
#include "cardlistmodel.h"
#include "core/bulkdatadownloader.h"

#include <QAction>
#include <QCloseEvent>
#include <QDateTime>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QLocale>
#include <QMessageBox>
#include <QProgressBar>
#include <QPromise>
#include <QSplitter>
#include <QStandardPaths>
#include <QStatusBar>
#include <QTableView>
#include <QTextBrowser>
#include <QToolBar>
#include <QVBoxLayout>
#include <QtConcurrent/QtConcurrentRun>

namespace {
constexpr int kProgressSteps = 1000;
constexpr int kSearchDelayMs = 150;
} // namespace

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , m_database(std::make_unique<CardDatabase>(databasePath(), QStringLiteral("ui")))
    , m_downloader(new BulkDataDownloader(this))
{
    buildUi();

    m_searchDelay.setSingleShot(true);
    m_searchDelay.setInterval(kSearchDelayMs);
    connect(&m_searchDelay, &QTimer::timeout, this, &MainWindow::runSearch);

    connect(m_downloader, &BulkDataDownloader::progress, this, [this](qint64 received, qint64 total) {
        m_progressBar->setRange(0, total > 0 ? kProgressSteps : 0);
        if (total > 0)
            m_progressBar->setValue(int(received * kProgressSteps / total));
    });
    connect(m_downloader, &BulkDataDownloader::upToDate, this, [this] {
        setBusy(false);
        statusBar()->showMessage(tr("Card data is already up to date."), 5000);
    });
    connect(m_downloader, &BulkDataDownloader::finished, this, &MainWindow::startImport);
    connect(m_downloader, &BulkDataDownloader::failed, this, [this](const QString &message) {
        setBusy(false);
        QMessageBox::warning(this, tr("Download failed"),
                             tr("Could not download card data from Scryfall:\n%1").arg(message));
    });

    connect(&m_importWatcher, &QFutureWatcher<ImportOutcome>::progressRangeChanged,
            m_progressBar, &QProgressBar::setRange);
    connect(&m_importWatcher, &QFutureWatcher<ImportOutcome>::progressValueChanged,
            m_progressBar, &QProgressBar::setValue);
    connect(&m_importWatcher, &QFutureWatcher<ImportOutcome>::finished, this, &MainWindow::onImportFinished);

    m_databaseOk = m_database->open();
    if (!m_databaseOk) {
        m_updateAction->setEnabled(false);
        QMessageBox::critical(this, tr("Card database"),
                              tr("Could not open the card database at %1:\n%2")
                                  .arg(databasePath(), m_database->lastError()));
    }
    refreshStatus();

    // First launch: there's nothing to search until the card data is downloaded.
    if (m_databaseOk && m_database->cardCount() == 0)
        QTimer::singleShot(0, this, &MainWindow::updateCardData);
}

MainWindow::~MainWindow() = default;

void MainWindow::buildUi()
{
    resize(1100, 700);

    m_updateAction = new QAction(tr("Update Card Data"), this);
    m_updateAction->setToolTip(tr("Download the latest card data and prices from Scryfall"));
    connect(m_updateAction, &QAction::triggered, this, &MainWindow::updateCardData);
    QToolBar *toolBar = addToolBar(tr("Main"));
    toolBar->setMovable(false);
    toolBar->addAction(m_updateAction);

    m_searchField = new QLineEdit;
    m_searchField->setPlaceholderText(tr("Search cards by name…"));
    m_searchField->setClearButtonEnabled(true);
    connect(m_searchField, &QLineEdit::textChanged, this, [this] { m_searchDelay.start(); });
    connect(m_searchField, &QLineEdit::returnPressed, this, &MainWindow::runSearch);

    m_model = new CardListModel(this);
    m_results = new QTableView;
    m_results->setModel(m_model);
    m_results->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_results->setSelectionMode(QAbstractItemView::SingleSelection);
    m_results->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_results->setAlternatingRowColors(true);
    m_results->setWordWrap(false);
    m_results->verticalHeader()->hide();
    QHeaderView *header = m_results->horizontalHeader();
    header->setSectionResizeMode(QHeaderView::ResizeToContents);
    header->setSectionResizeMode(CardListModel::NameColumn, QHeaderView::Stretch);
    header->setSectionResizeMode(CardListModel::TypeColumn, QHeaderView::Stretch);
    connect(m_results->selectionModel(), &QItemSelectionModel::currentRowChanged,
            this, &MainWindow::showCurrentCard);

    auto *searchPanel = new QWidget;
    auto *searchLayout = new QVBoxLayout(searchPanel);
    searchLayout->setContentsMargins(0, 0, 0, 0);
    searchLayout->addWidget(m_searchField);
    searchLayout->addWidget(m_results);

    m_details = new QTextBrowser;
    m_details->setOpenExternalLinks(true);

    auto *splitter = new QSplitter;
    splitter->addWidget(searchPanel);
    splitter->addWidget(m_details);
    splitter->setStretchFactor(0, 3);
    splitter->setStretchFactor(1, 2);
    setCentralWidget(splitter);

    m_statusLabel = new QLabel;
    m_progressBar = new QProgressBar;
    m_progressBar->setMaximumWidth(220);
    m_progressBar->setTextVisible(false);
    m_progressBar->hide();
    statusBar()->addPermanentWidget(m_progressBar);
    statusBar()->addPermanentWidget(m_statusLabel);

    m_searchField->setFocus();
}

void MainWindow::runSearch()
{
    m_searchDelay.stop();
    if (!m_databaseOk)
        return;
    m_model->setCards(m_database->searchByName(m_searchField->text()));
    if (m_model->rowCount() > 0)
        m_results->selectRow(0);
    else
        showCurrentCard();
}

void MainWindow::showCurrentCard()
{
    const QModelIndex current = m_results->selectionModel()->currentIndex();
    if (current.isValid()) {
        m_details->setHtml(cardDetailsHtml(m_model->cardAt(current.row())));
    } else if (m_searchField->text().trimmed().isEmpty()) {
        m_details->setHtml(tr("<p>Type a card name to search.</p>"));
    } else {
        m_details->setHtml(tr("<p>No cards match “%1”.</p>").arg(m_searchField->text().trimmed().toHtmlEscaped()));
    }
}

void MainWindow::updateCardData()
{
    if (m_downloader->isRunning() || m_importWatcher.isRunning())
        return;
    setBusy(true, tr("Checking Scryfall for new card data…"));
    const QString known = m_database->cardCount() > 0 ? m_database->sourceUpdatedAt() : QString();
    m_downloader->start(known, downloadPath());
}

void MainWindow::startImport(const QString &filePath, const QString &updatedAt)
{
    setBusy(true, tr("Importing cards…"));
    const QString dbPath = databasePath();
    m_importWatcher.setFuture(QtConcurrent::run([dbPath, filePath, updatedAt](QPromise<ImportOutcome> &promise) {
        promise.setProgressRange(0, kProgressSteps);
        // Runs on a worker thread, so it needs its own connection.
        CardDatabase database(dbPath, QStringLiteral("import"));
        if (!database.open()) {
            promise.addResult(ImportOutcome{std::nullopt, database.lastError()});
            return;
        }
        const std::optional<ImportStats> stats = database.importScryfallBulk(
            filePath, updatedAt,
            [&promise](double fraction) { promise.setProgressValue(int(fraction * kProgressSteps)); },
            [&promise] { return promise.isCanceled(); });
        promise.addResult(ImportOutcome{stats, stats ? QString() : database.lastError()});
    }));
}

void MainWindow::onImportFinished()
{
    setBusy(false);
    const QFuture<ImportOutcome> future = m_importWatcher.future();
    if (future.resultCount() == 0)
        return;
    const ImportOutcome outcome = future.result();
    if (!outcome.stats) {
        QMessageBox::warning(this, tr("Import failed"),
                             tr("Could not import the card data:\n%1").arg(outcome.error));
        return;
    }
    statusBar()->showMessage(tr("Imported %1 cards.").arg(QLocale().toString(outcome.stats->imported)), 5000);
    refreshStatus();
    runSearch();
}

void MainWindow::setBusy(bool busy, const QString &message)
{
    m_updateAction->setEnabled(!busy && m_databaseOk);
    m_progressBar->setVisible(busy);
    m_progressBar->setRange(0, 0);  // indeterminate until real progress arrives
    if (busy)
        m_statusLabel->setText(message);
    else
        refreshStatus();
}

void MainWindow::refreshStatus()
{
    if (!m_databaseOk) {
        m_statusLabel->setText(tr("Card database unavailable"));
        return;
    }
    const int count = m_database->cardCount();
    if (count == 0) {
        m_statusLabel->setText(tr("No card data yet"));
        m_details->setHtml(tr("<p>No card data yet. Click <b>Update Card Data</b> to download it from Scryfall.</p>"));
        return;
    }
    const QDateTime updated = QDateTime::fromString(m_database->sourceUpdatedAt(), Qt::ISODateWithMs);
    m_statusLabel->setText(tr("%1 cards · Scryfall data from %2")
                               .arg(QLocale().toString(count),
                                    QLocale().toString(updated.toLocalTime().date(), QLocale::ShortFormat)));
    if (m_searchField->text().trimmed().isEmpty())
        showCurrentCard();
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    // Stop background work without the usual error dialogs.
    m_downloader->disconnect(this);
    m_downloader->cancel();
    if (m_importWatcher.isRunning()) {
        m_importWatcher.disconnect(this);
        m_importWatcher.cancel();
        m_importWatcher.waitForFinished();
    }
    QMainWindow::closeEvent(event);
}

QString MainWindow::databasePath()
{
    return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + QStringLiteral("/cards.sqlite");
}

QString MainWindow::downloadPath()
{
    return QStandardPaths::writableLocation(QStandardPaths::CacheLocation) + QStringLiteral("/oracle-cards.jsonl.gz");
}
