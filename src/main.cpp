#include "ui/mainwindow.h"

#include <QApplication>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    // Used by QStandardPaths: data goes in ~/Library/Application Support/local_moxfield.
    QApplication::setApplicationName(QStringLiteral("local_moxfield"));
    QApplication::setApplicationDisplayName(QStringLiteral("Local Moxfield"));
    QApplication::setApplicationVersion(QStringLiteral(LOCAL_MOXFIELD_VERSION));

    MainWindow w;
    w.show();
    return QApplication::exec();
}
