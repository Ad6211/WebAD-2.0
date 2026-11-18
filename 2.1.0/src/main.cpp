#include "browser_window.h"

#include <QApplication>
#include <QCoreApplication>
#include <QFileInfo>
#include <QUrl>

int main(int argc, char *argv[])
{
    QApplication application(argc, argv);
    QCoreApplication::setOrganizationName(QStringLiteral("WebAD"));
    QCoreApplication::setApplicationName(QStringLiteral("WebAD"));
    QCoreApplication::setApplicationVersion(QStringLiteral(WEBAD_VERSION));

    QUrl initialUrl;
    const QStringList arguments = QCoreApplication::arguments();
    if (arguments.size() > 1 && !arguments.at(1).startsWith(QLatin1Char('-'))) {
        const QString input = arguments.at(1);
        const QFileInfo file(input);
        initialUrl = file.exists()
            ? QUrl::fromLocalFile(file.absoluteFilePath())
            : QUrl::fromUserInput(input);
    }

    BrowserWindow window(nullptr, false, initialUrl);
    window.show();

    return application.exec();
}
