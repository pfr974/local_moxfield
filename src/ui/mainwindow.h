#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include "core/carddatabase.h"

#include <QFutureWatcher>
#include <QMainWindow>
#include <QTimer>

#include <memory>

class BulkDataDownloader;
class CardListModel;
class QAction;
class QLabel;
class QLineEdit;
class QProgressBar;
class QTableView;
class QTextBrowser;

struct ImportOutcome
{
    std::optional<ImportStats> stats;  // empty on failure
    QString error;
};

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

protected:
    void closeEvent(QCloseEvent *event) override;

private:
    void buildUi();
    void runSearch();
    void showCurrentCard();
    void updateCardData();
    void startImport(const QString &filePath, const QString &updatedAt);
    void onImportFinished();
    void setBusy(bool busy, const QString &message = {});
    void refreshStatus();

    static QString databasePath();
    static QString downloadPath();

    std::unique_ptr<CardDatabase> m_database;
    bool m_databaseOk = false;
    BulkDataDownloader *m_downloader = nullptr;
    QFutureWatcher<ImportOutcome> m_importWatcher;
    QTimer m_searchDelay;

    CardListModel *m_model = nullptr;
    QLineEdit *m_searchField = nullptr;
    QTableView *m_results = nullptr;
    QTextBrowser *m_details = nullptr;
    QLabel *m_statusLabel = nullptr;
    QProgressBar *m_progressBar = nullptr;
    QAction *m_updateAction = nullptr;
};

#endif // MAINWINDOW_H
