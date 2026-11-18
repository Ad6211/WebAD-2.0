#include <QApplication>
#include <QEventLoop>
#include <QFile>
#include <QIcon>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QJsonValue>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTimer>
#include <QUrl>
#include <QWebEngineCookieStore>
#include <QWebEngineLoadingInfo>
#include <QWebEnginePage>
#include <QWebEngineProfile>
#include <QWebEngineScript>
#include <QWebEngineScriptCollection>
#include <QWebEngineSettings>
#include <QWebEngineView>

#include "navigation_error.h"

#include <iostream>

namespace
{
class TestServer final : public QTcpServer
{
public:
    bool start()
    {
        connect(this, &QTcpServer::newConnection, this, [this] {
            while (hasPendingConnections()) {
                QTcpSocket *socket = nextPendingConnection();
                connect(socket, &QTcpSocket::readyRead, this, [this, socket] {
                    const QByteArray request = socket->readAll();
                    const qsizetype pathEnd = request.indexOf(' ', 4);
                    const QByteArray path = request.mid(4, pathEnd - 4);
                    if (request.contains("Cookie: webad_smoke=stored")) {
                        sawCookie = true;
                    }
                    QByteArray body = path == "/set"
                        ? "<!doctype html><title>storage</title><script>"
                          "localStorage.setItem('webad_smoke','persisted');"
                          "document.title='ready';</script>"
                        : "<!doctype html><title>storage</title><body>read</body>";
                    QByteArray contentType = "text/html; charset=utf-8";
                    if (path == "/favicon-page") {
                        body = "<!doctype html><html><head><title>favicon</title>"
                               "<link rel=\"icon\" href=\"/favicon.svg\" "
                               "type=\"image/svg+xml\"></head><body>favicon</body></html>";
                    } else if (path == "/favicon.svg") {
                        contentType = "image/svg+xml";
                        body = "<svg xmlns=\"http://www.w3.org/2000/svg\" "
                               "viewBox=\"0 0 16 16\"><circle cx=\"8\" cy=\"8\" "
                               "r=\"7\" fill=\"#7135f5\"/></svg>";
                    }
                    const bool notFound = path == "/http404";
                    QByteArray response;
                    if (notFound) {
                        response = "HTTP/1.1 404 Not Found\r\n"
                                   "Content-Type: text/html; charset=utf-8\r\n";
                    } else {
                        response = QByteArray("HTTP/1.1 200 OK\r\nContent-Type: ")
                            + contentType + "\r\n";
                    }
                    response += "Cache-Control: no-store\r\nConnection: close\r\n";
                    if (path == "/set") {
                        response += "Set-Cookie: webad_smoke=stored; Max-Age=3600; Path=/; SameSite=Lax\r\n";
                    }
                    response += "Content-Length: " + QByteArray::number(body.size())
                        + "\r\n\r\n" + body;
                    socket->write(response);
                    socket->disconnectFromHost();
                });
                connect(socket, &QTcpSocket::disconnected, socket, &QObject::deleteLater);
            }
        });
        return listen(QHostAddress::LocalHost, 0);
    }

    QUrl url(const QString &path) const
    {
        return QUrl(QStringLiteral("http://127.0.0.1:%1%2")
                        .arg(serverPort()).arg(path));
    }

    bool sawCookie = false;
};

bool load(QWebEngineView &view, const QUrl &url)
{
    QEventLoop loop;
    QTimer timeout;
    timeout.setSingleShot(true);
    QObject::connect(&view, &QWebEngineView::loadFinished, &loop, &QEventLoop::quit);
    QObject::connect(&timeout, &QTimer::timeout, &loop, &QEventLoop::quit);
    view.setUrl(url);
    timeout.start(15000);
    loop.exec();
    return !timeout.isActive() ? false : true;
}

QString storageState(QWebEngineView &view)
{
    QEventLoop loop;
    QTimer timeout;
    timeout.setSingleShot(true);
    QString result;
    QObject::connect(&timeout, &QTimer::timeout, &loop, &QEventLoop::quit);
    view.page()->runJavaScript(
        QStringLiteral("JSON.stringify([document.cookie,localStorage.getItem('webad_smoke')])"),
        [&loop, &result](const QVariant &value) {
            result = value.toString();
            loop.quit();
        });
    timeout.start(5000);
    loop.exec();
    return result;
}

QString pageAttribute(QWebEngineView &view)
{
    QEventLoop loop;
    QTimer timeout;
    timeout.setSingleShot(true);
    QString result;
    QObject::connect(&timeout, &QTimer::timeout, &loop, &QEventLoop::quit);
    view.page()->runJavaScript(
        QStringLiteral("document.documentElement.getAttribute('data-webad-extension')"),
        [&loop, &result](const QVariant &value) {
            result = value.toString();
            loop.quit();
        });
    timeout.start(5000);
    loop.exec();
    return result;
}

bool hasValues(const QString &state, bool shouldPersist)
{
    QJsonParseError error;
    const QJsonDocument document = QJsonDocument::fromJson(state.toUtf8(), &error);
    if (error.error != QJsonParseError::NoError || !document.isArray()) {
        return false;
    }
    const QJsonArray values = document.array();
    return values.size() == 2
        && values.at(0).toString().contains(QStringLiteral("webad_smoke=stored"))
               == shouldPersist
        && (values.at(1).toString() == QStringLiteral("persisted")) == shouldPersist;
}

void waitForPersistence()
{
    QEventLoop loop;
    QTimer::singleShot(1200, &loop, &QEventLoop::quit);
    loop.exec();
}
} // namespace

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    TestServer server;
    if (!server.start()) {
        std::cerr << "Could not start the local HTTP test server.\n";
        return 1;
    }

    const QString storagePath = QCoreApplication::applicationDirPath()
        + QStringLiteral("/storage-profile");
    bool initialWritePassed = false;
    {
        QWebEngineProfile profile(QStringLiteral("WebADStorageSmoke"));
        profile.setPersistentStoragePath(storagePath);
        profile.setCachePath(storagePath + QStringLiteral("/cache"));
        profile.setHttpCacheType(QWebEngineProfile::NoCache);
        profile.setPersistentCookiesPolicy(QWebEngineProfile::AllowPersistentCookies);
        QWebEngineView view(&profile);
        view.settings()->setAttribute(QWebEngineSettings::LocalStorageEnabled, true);
        const bool loaded = load(view, server.url(QStringLiteral("/set")));
        initialWritePassed = loaded && hasValues(storageState(view), true);
        waitForPersistence();
    }
    bool persistentReadPassed = false;
    {
        QWebEngineProfile profile(QStringLiteral("WebADStorageSmoke"));
        profile.setPersistentStoragePath(storagePath);
        profile.setCachePath(storagePath + QStringLiteral("/cache"));
        profile.setHttpCacheType(QWebEngineProfile::NoCache);
        profile.setPersistentCookiesPolicy(QWebEngineProfile::AllowPersistentCookies);
        QWebEngineView view(&profile);
        view.settings()->setAttribute(QWebEngineSettings::LocalStorageEnabled, true);
        const bool loaded = load(view, server.url(QStringLiteral("/read")));
        persistentReadPassed = loaded && hasValues(storageState(view), true) && server.sawCookie;
    }

    bool privateSessionWritePassed = false;
    bool privatePersistencePassed = false;
    {
        QWebEngineProfile profile;
        profile.setHttpCacheType(QWebEngineProfile::MemoryHttpCache);
        profile.setPersistentCookiesPolicy(QWebEngineProfile::NoPersistentCookies);
        QWebEngineView view(&profile);
        view.settings()->setAttribute(QWebEngineSettings::LocalStorageEnabled, true);
        const bool loaded = load(view, server.url(QStringLiteral("/set")));
        const QString state = storageState(view);
        privateSessionWritePassed = loaded && hasValues(state, true);
        waitForPersistence();
    }
    {
        QWebEngineProfile profile;
        profile.setHttpCacheType(QWebEngineProfile::MemoryHttpCache);
        profile.setPersistentCookiesPolicy(QWebEngineProfile::NoPersistentCookies);
        QWebEngineView view(&profile);
        view.settings()->setAttribute(QWebEngineSettings::LocalStorageEnabled, true);
        const bool loaded = load(view, server.url(QStringLiteral("/read")));
        privatePersistencePassed = loaded && hasValues(storageState(view), false);
    }

    bool http404Reported = false;
    bool connectionFailureReported = false;
    {
        QWebEngineProfile profile;
        QWebEngineView view(&profile);
        QEventLoop loop;
        QObject::connect(view.page(), &QWebEnginePage::loadingChanged, &loop,
                         [&http404Reported, &loop](const QWebEngineLoadingInfo &info) {
            const NavigationError error = navigationErrorFor(info);
            if (error.failed && error.httpStatus == 404) {
                http404Reported = true;
            }
            if (info.status() != QWebEngineLoadingInfo::LoadStartedStatus) {
                QTimer::singleShot(200, &loop, &QEventLoop::quit);
            }
        });
        QTimer::singleShot(15000, &loop, &QEventLoop::quit);
        view.setUrl(server.url(QStringLiteral("/http404")));
        loop.exec();
    }
    {
        QWebEngineProfile profile;
        QWebEngineView view(&profile);
        QEventLoop loop;
        QObject::connect(view.page(), &QWebEnginePage::loadingChanged, &loop,
                         [&connectionFailureReported, &loop](
                             const QWebEngineLoadingInfo &info) {
            const NavigationError error = navigationErrorFor(info);
            if (error.failed) {
                connectionFailureReported =
                    error.httpStatus == 0 && !error.message.isEmpty();
                loop.quit();
            }
        });
        QTimer::singleShot(15000, &loop, &QEventLoop::quit);
        view.setUrl(QUrl(QStringLiteral("http://127.0.0.1:1/unreachable")));
        loop.exec();
    }

    bool contentScriptPassed = false;
    {
        QWebEngineProfile profile(QStringLiteral("WebADExtensionSmoke"));
        profile.setHttpCacheType(QWebEngineProfile::NoCache);
        QWebEngineScript script;
        script.setName(QStringLiteral("WebAD extension smoke"));
        script.setInjectionPoint(QWebEngineScript::DocumentReady);
        script.setWorldId(QWebEngineScript::ApplicationWorld);
        script.setSourceCode(QStringLiteral(
            "(()=>{const p=[\"^http://127\\\\.0\\\\.0\\\\.1:[0-9]+/read$\"];"
            "if(p.some(x=>new RegExp(x).test(location.href)))"
            "document.documentElement.setAttribute('data-webad-extension','active');})();"));
        profile.scripts()->insert(script);
        QWebEngineView view(&profile);
        const bool matchingPageLoaded = load(view, server.url(QStringLiteral("/read")));
        const bool matchingPageInjected =
            matchingPageLoaded && pageAttribute(view) == QStringLiteral("active");
        const bool otherPageLoaded = load(view, server.url(QStringLiteral("/set")));
        const bool otherPageExcluded =
            otherPageLoaded && pageAttribute(view).isEmpty();
        contentScriptPassed = matchingPageInjected && otherPageExcluded;
    }

    bool faviconReceived = false;
    {
        QWebEngineProfile profile;
        QWebEngineView view(&profile);
        QEventLoop loop;
        QTimer timeout;
        timeout.setSingleShot(true);
        QObject::connect(view.page(), &QWebEnginePage::iconChanged, &loop,
                         [&faviconReceived, &loop](const QIcon &icon) {
            faviconReceived = !icon.isNull();
            if (faviconReceived) {
                loop.quit();
            }
        });
        QObject::connect(&timeout, &QTimer::timeout, &loop, &QEventLoop::quit);
        view.setUrl(server.url(QStringLiteral("/favicon-page")));
        timeout.start(10000);
        loop.exec();
    }

    bool mimoInstalled = false;
    {
        QFile extensionFile(QStringLiteral(WEBAD_SOURCE_DIR "/ext/content.js"));
        if (!extensionFile.open(QIODevice::ReadOnly)) {
            std::cerr << "Could not read the Mimo extension source.\n";
        } else {
            QWebEngineProfile profile;
            profile.setHttpCacheType(QWebEngineProfile::NoCache);
            QWebEngineScript script;
            script.setName(QStringLiteral("Mimo extension smoke"));
            script.setInjectionPoint(QWebEngineScript::DocumentReady);
            script.setWorldId(QWebEngineScript::ApplicationWorld);
            script.setRunsOnSubFrames(false);
            script.setSourceCode(QString::fromUtf8(extensionFile.readAll()));
            profile.scripts()->insert(script);

            QWebEngineView view(&profile);
            const bool loaded = load(view, server.url(QStringLiteral("/read")));
            if (loaded) {
                QEventLoop loop;
                QTimer timeout;
                timeout.setSingleShot(true);
                QString result;
                QObject::connect(&timeout, &QTimer::timeout, &loop, &QEventLoop::quit);
                view.page()->runJavaScript(
                    QStringLiteral(
                        "(()=>{const h=document.querySelector("
                        "'div[aria-label=\"Mimo, petit compagnon WebAD\"]');"
                        "return JSON.stringify({host:!!h,closed:!!h?.shadowRoot});})()"),
                    [&loop, &result](const QVariant &value) {
                        result = value.toString();
                        loop.quit();
                    });
                timeout.start(5000);
                loop.exec();
                const QJsonDocument state = QJsonDocument::fromJson(result.toUtf8());
                mimoInstalled = state.isObject()
                    && state.object().value(QStringLiteral("host")).toBool()
                    && !state.object().value(QStringLiteral("closed")).toBool();
            }
        }
    }

    const bool passed =
        initialWritePassed && persistentReadPassed && privateSessionWritePassed
        && privatePersistencePassed && http404Reported && connectionFailureReported
        && contentScriptPassed
        && faviconReceived && mimoInstalled;
    std::cout << "storage write: " << (initialWritePassed ? "PASS" : "FAIL") << '\n';
    std::cout << "persistent cookies/localStorage: "
              << (persistentReadPassed ? "PASS" : "FAIL") << '\n'
              << "private session-only cookies/localStorage: "
              << (privateSessionWritePassed && privatePersistencePassed ? "PASS" : "FAIL")
              << '\n'
              << "HTTP 404 status reporting: " << (http404Reported ? "PASS" : "FAIL") << '\n'
              << "network failures distinguished from HTTP 404: "
              << (connectionFailureReported ? "PASS" : "FAIL") << '\n'
              << "extension script match/injection: "
              << (contentScriptPassed ? "PASS" : "FAIL") << '\n'
              << "website favicon received: " << (faviconReceived ? "PASS" : "FAIL") << '\n'
              << "Mimo extension injection: " << (mimoInstalled ? "PASS" : "FAIL") << '\n';
    return passed ? 0 : 1;
}
