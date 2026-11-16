#include "browser_window.h"

#include <QAbstractItemView>
#include <QAbstractButton>
#include <QAction>
#include <QApplication>
#include <QCheckBox>
#include <QCloseEvent>
#include <QComboBox>
#include <QColor>
#include <QClipboard>
#include <QContextMenuEvent>
#include <QDesktopServices>
#include <QDir>
#include <QDockWidget>
#include <QFileDialog>
#include <QFile>
#include <QFileInfo>
#include <QFormLayout>
#include <QFrame>
#include <QInputDialog>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QKeySequenceEdit>
#include <functional>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QIcon>
#include <QKeySequence>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QPainter>
#include <QPainterPath>
#include <QProgressBar>
#include <QMessageBox>
#include <QPalette>
#include <QPushButton>
#include <QPen>
#include <QPixmap>
#include <QRegularExpression>
#include <QScrollArea>
#include <QSettings>
#include <QShortcut>
#include <QSignalBlocker>
#include <QSpacerItem>
#include <QStackedWidget>
#include <QStandardPaths>
#include <QStatusBar>
#include <QStyle>
#include <QSvgRenderer>
#include <QTabBar>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QToolButton>
#include <QUrlQuery>
#include <QVariantMap>
#include <QVBoxLayout>
#include <QWebEngineDownloadRequest>
#include <QWebEngineContextMenuRequest>
#include <QWebEngineCookieStore>
#include <QWebEngineHistory>
#include <QWebEnginePage>
#include <QWebEngineProfile>
#include <QWebEngineLoadingInfo>
#include <QWebEngineScript>
#include <QWebEngineScriptCollection>
#include <QWebEngineSettings>
#include <QWebEngineView>
#include <QCoreApplication>
#include <QtMath>

namespace
{
class WebADPage final : public QWebEnginePage
{
public:
    using PopupFactory = std::function<QWebEnginePage *()>;
    using InternalNavigationHandler = std::function<void(const QUrl &)>;

    explicit WebADPage(QWebEngineProfile *profile, QObject *parent = nullptr)
        : QWebEnginePage(profile, parent)
    {
    }

    PopupFactory popupFactory;
    InternalNavigationHandler internalNavigationHandler;

protected:
    QWebEnginePage *createWindow(WebWindowType type) override
    {
        Q_UNUSED(type);
        return popupFactory ? popupFactory() : nullptr;
    }

    bool acceptNavigationRequest(const QUrl &url, NavigationType type,
                                bool isMainFrame) override
    {
        if (isMainFrame && url.scheme().compare(QStringLiteral("webad"), Qt::CaseInsensitive) == 0
            && internalNavigationHandler) {
            internalNavigationHandler(url);
            return false;
        }
        return QWebEnginePage::acceptNavigationRequest(url, type, isMainFrame);
    }
};

class WebADView final : public QWebEngineView
{
public:
    using BookmarkHandler = std::function<void(const QUrl &)>;
    using SearchHandler = std::function<void(const QString &)>;

    explicit WebADView(QWidget *parent = nullptr)
        : QWebEngineView(parent)
    {
    }

    BookmarkHandler bookmarkHandler;
    SearchHandler searchHandler;

protected:
    void contextMenuEvent(QContextMenuEvent *event) override
    {
        QMenu *menu = createStandardContextMenu();
        menu->setObjectName(QStringLiteral("webContextMenu"));
        QWebEngineContextMenuRequest *context = lastContextMenuRequest();
        const QUrl linkUrl = context ? context->linkUrl() : QUrl();
        const QString selectedText = context ? context->selectedText().trimmed() : QString();
        if (linkUrl.isValid() && !linkUrl.isEmpty()) {
            QAction *firstAction = menu->actions().value(0, nullptr);
            QAction *openLink = new QAction(
                style()->standardIcon(QStyle::SP_FileLinkIcon),
                tr("Ouvrir le lien dans un nouvel onglet"), menu);
            connect(openLink, &QAction::triggered, this, [this, link = linkUrl] {
                auto *webPage = static_cast<WebADPage *>(page());
                if (webPage && webPage->popupFactory) {
                    QWebEnginePage *newPage = webPage->popupFactory();
                    if (newPage) {
                        newPage->setUrl(link);
                    }
                }
            });
            menu->insertAction(firstAction, openLink);

            QAction *copyLink = new QAction(
                style()->standardIcon(QStyle::SP_FileIcon),
                tr("Copier l’adresse du lien"), menu);
            connect(copyLink, &QAction::triggered, this, [link = linkUrl] {
                QApplication::clipboard()->setText(link.toDisplayString());
            });
            menu->insertAction(firstAction, copyLink);

            QAction *bookmarkLink = new QAction(
                style()->standardIcon(QStyle::SP_DialogSaveButton),
                tr("Ajouter le lien aux favoris"), menu);
            connect(bookmarkLink, &QAction::triggered, this, [this, link = linkUrl] {
                if (bookmarkHandler) {
                    bookmarkHandler(link);
                }
            });
            menu->insertAction(firstAction, bookmarkLink);
            menu->insertSeparator(firstAction);
        }

        if (!selectedText.isEmpty()) {
            QAction *firstAction = menu->actions().value(0, nullptr);
            QAction *searchSelection = new QAction(
                style()->standardIcon(QStyle::SP_FileDialogContentsView),
                tr("Rechercher « %1 »").arg(selectedText.left(60)), menu);
            connect(searchSelection, &QAction::triggered, this, [this, selectedText] {
                if (searchHandler) {
                    searchHandler(selectedText);
                }
            });
            menu->insertAction(firstAction, searchSelection);
            menu->insertSeparator(firstAction);
        }

        menu->exec(event->globalPos());
        delete menu;
    }
};

QIcon makeLineIcon(const QString &name, const QColor &color, bool filled = false)
{
    QPixmap pixmap(72, 72);
    pixmap.fill(Qt::transparent);

    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.scale(3.0, 3.0);
    QPen pen(color, 1.8, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
    painter.setPen(pen);
    painter.setBrush(Qt::NoBrush);

    QPainterPath path;
    char iconKey = name.toLatin1().constData()[0];
    if (name == QStringLiteral("file")) {
        iconKey = 'i';
    } else if (name == QStringLiteral("close")) {
        iconKey = 'x';
    } else if (name == QStringLiteral("private")) {
        iconKey = 'v';
    }
    switch (iconKey) {
    case 'a':
        path.moveTo(12, 5);
        path.lineTo(12, 19);
        path.moveTo(5, 12);
        path.lineTo(19, 12);
        break;
    case 'b':
        path.moveTo(6, 3.5);
        path.lineTo(18, 3.5);
        path.lineTo(18, 20.5);
        path.lineTo(12, 16.5);
        path.lineTo(6, 20.5);
        path.closeSubpath();
        if (filled) {
            painter.setBrush(color);
        }
        break;
    case 'v':
        path.moveTo(3, 8);
        path.cubicTo(7, 6.5, 17, 6.5, 21, 8);
        path.lineTo(19, 17);
        path.cubicTo(14.5, 20.5, 9.5, 20.5, 5, 17);
        path.closeSubpath();
        path.moveTo(7, 12);
        path.cubicTo(8.5, 10, 10.5, 10, 12, 12);
        path.cubicTo(10.5, 14, 8.5, 14, 7, 12);
        path.moveTo(12, 12);
        path.cubicTo(13.5, 10, 15.5, 10, 17, 12);
        path.cubicTo(15.5, 14, 13.5, 14, 12, 12);
        break;
    case 'c':
        path.addEllipse(QRectF(3.5, 3.5, 17, 17));
        path.moveTo(12, 7);
        path.lineTo(12, 12);
        path.lineTo(16, 14.5);
        break;
    case 'd':
        path.moveTo(12, 3.5);
        path.lineTo(12, 15);
        path.moveTo(7.5, 10.5);
        path.lineTo(12, 15);
        path.lineTo(16.5, 10.5);
        path.moveTo(5, 17);
        path.lineTo(5, 20);
        path.lineTo(19, 20);
        path.lineTo(19, 17);
        break;
    case 'e':
        path.moveTo(8, 4);
        path.lineTo(16, 4);
        path.lineTo(16, 7);
        path.cubicTo(16, 10, 21, 9, 21, 12);
        path.cubicTo(21, 15, 16, 14, 16, 17);
        path.lineTo(16, 20);
        path.lineTo(8, 20);
        path.lineTo(8, 17);
        path.cubicTo(8, 14, 3, 15, 3, 12);
        path.cubicTo(3, 9, 8, 10, 8, 7);
        path.closeSubpath();
        break;
    case 'f':
        path.moveTo(5, 12);
        path.lineTo(19, 12);
        path.moveTo(13, 6);
        path.lineTo(19, 12);
        path.lineTo(13, 18);
        break;
    case 'h':
        path.moveTo(3, 11);
        path.lineTo(12, 4);
        path.lineTo(21, 11);
        path.moveTo(6.5, 9.5);
        path.lineTo(6.5, 20);
        path.lineTo(17.5, 20);
        path.lineTo(17.5, 9.5);
        path.moveTo(10, 20);
        path.lineTo(10, 14);
        path.lineTo(14, 14);
        path.lineTo(14, 20);
        break;
    case 'i':
        path.moveTo(7, 3.5);
        path.lineTo(14.5, 3.5);
        path.lineTo(19, 8);
        path.lineTo(19, 20.5);
        path.lineTo(7, 20.5);
        path.closeSubpath();
        path.moveTo(14, 4);
        path.lineTo(14, 8.5);
        path.lineTo(18.5, 8.5);
        path.moveTo(10, 13);
        path.lineTo(16, 13);
        path.moveTo(10, 16.5);
        path.lineTo(16, 16.5);
        break;
    case 'l':
        path.moveTo(15, 6);
        path.lineTo(9, 12);
        path.lineTo(15, 18);
        break;
    case 'r':
        painter.drawArc(QRect(4, 4, 16, 16), 35 * 16, 285 * 16);
        path.moveTo(19.5, 4);
        path.lineTo(19.5, 9);
        path.lineTo(14.5, 9);
        break;
    case 's':
        painter.drawEllipse(QRectF(3.5, 3.5, 17, 17));
        painter.drawEllipse(QRectF(9.4, 9.4, 5.2, 5.2));
        for (int angle = 0; angle < 360; angle += 45) {
            const qreal radians = qDegreesToRadians(static_cast<qreal>(angle));
            path.moveTo(12 + qCos(radians) * 8.7, 12 + qSin(radians) * 8.7);
            path.lineTo(12 + qCos(radians) * 11, 12 + qSin(radians) * 11);
        }
        break;
    case 'x':
        path.moveTo(7, 7);
        path.lineTo(17, 17);
        path.moveTo(17, 7);
        path.lineTo(7, 17);
        break;
    default:
        path.addEllipse(QRectF(5, 10.5, 2, 2));
        path.addEllipse(QRectF(11, 10.5, 2, 2));
        path.addEllipse(QRectF(17, 10.5, 2, 2));
        painter.setPen(Qt::NoPen);
        painter.setBrush(color);
        break;
    }
    painter.drawPath(path);
    painter.end();
    return QIcon(pixmap);
}

QIcon makeBrowserLogoIcon()
{
    static const QIcon browserIcon = [] {
        QIcon icon;
        QSvgRenderer renderer(QStringLiteral(":/webad.svg"));
        if (!renderer.isValid()) {
            qWarning("Failed to load the embedded WebAD SVG logo.");
            return icon;
        }
        for (const int size : {16, 24, 32, 40, 48, 64, 128, 256}) {
            QPixmap pixmap(size, size);
            pixmap.fill(Qt::transparent);

            QPainter painter(&pixmap);
            painter.setRenderHint(QPainter::Antialiasing);
            renderer.render(&painter, QRectF(0, 0, size, size));
            painter.end();
            icon.addPixmap(pixmap);
        }
        return icon;
    }();
    return browserIcon;
}

QColor lineIconColor(const QString &theme, bool highlighted = false)
{
    const bool systemDark = QApplication::palette().color(QPalette::Window).lightness() < 128;
    const bool dark = theme == QStringLiteral("Dark")
        || (theme == QStringLiteral("System") && systemDark);
    if (highlighted) {
        return QColor(dark ? QStringLiteral("#c1a3fb") : QStringLiteral("#7953dc"));
    }
    return QColor(dark ? QStringLiteral("#b5acbe") : QStringLiteral("#777184"));
}

bool isDarkTheme(const QString &theme)
{
    return theme == QStringLiteral("Dark")
        || (theme == QStringLiteral("System")
            && QApplication::palette().color(QPalette::Window).lightness() < 128);
}

QString searchBaseUrl(const QString &searchEngine)
{
    if (searchEngine == QStringLiteral("Google")) {
        return QStringLiteral("https://www.google.com/search");
    }
    if (searchEngine == QStringLiteral("Bing")) {
        return QStringLiteral("https://www.bing.com/search");
    }
    return QStringLiteral("https://duckduckgo.com/");
}

QString homePageHtml(const QString &searchEngine, const QStringList &bookmarks, bool dark)
{
    QString favoriteCards;
    for (const QString &savedUrl : bookmarks) {
        const QUrl url(savedUrl);
        if (!url.isValid() || url.isEmpty()) {
            continue;
        }

        const QString title = url.host().isEmpty() ? url.fileName() : url.host();
        if (title.isEmpty()) {
            continue;
        }
        const QString icon = title.left(1).toUpper().toHtmlEscaped();
        favoriteCards += QStringLiteral(
            "<a class=\"card\" href=\"%1\" title=\"%2\">"
            "<span class=\"shortcut-icon\">%3</span><span class=\"card-title\">%4</span></a>")
                             .arg(url.toString(QUrl::FullyEncoded).toHtmlEscaped(),
                                  url.toDisplayString().toHtmlEscaped(), icon,
                                  title.toHtmlEscaped());
    }
    if (favoriteCards.isEmpty()) {
        favoriteCards = QStringLiteral(
            "<div class=\"empty-favorites\">"
            "<span class=\"shortcut-icon\" aria-hidden=\"true\">☆</span>"
            "<span>Vos favoris apparaîtront ici</span>"
            "<a href=\"webad:bookmarks\">Gérer les favoris</a></div>");
    }

    QString html = QStringLiteral(R"HTML(
<!doctype html>
<html lang="fr">
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>Nouvel onglet — WebAD</title>
  <style>
    :root[data-theme="light"] {
      color-scheme: light;
      --page: #f7f6fb; --surface: #fff; --ink: #302a42;
      --muted: #777184; --line: #e9e5f0; --accent: #7953dc;
      --accent-soft: #eee8fb;
    }
    :root[data-theme="dark"] {
      color-scheme: dark;
      --page: #222125; --surface: #121212; --ink: #f7f3ff;
      --muted: #aaa2b6; --line: #414047; --accent: #9862ed;
      --accent-soft: #30283b;
    }
    * { box-sizing: border-box; }
    body {
      min-height: 100vh; margin: 0; display: grid; place-items: center;
      color: var(--ink); font: 16px/1.5 system-ui, sans-serif;
      background: radial-gradient(ellipse at 50% 28%, color-mix(in srgb, var(--accent) 8%, transparent), transparent 46%), var(--page);
    }
    main { width: min(760px, calc(100% - 38px)); text-align: center; padding: 56px 0; }
    .mark { display: block; width: 76px; height: 76px; margin: 0 auto; overflow: visible; }
    .brand { margin: 12px 0 24px; font-size: 14px; font-weight: 700; letter-spacing: .5px; }
    h1 { margin: 13px 0 3px; font-size: 37px; letter-spacing: -1.7px; }
    .subtitle { margin: 0 0 31px; color: var(--muted); }
    form {
      display: flex; gap: 10px; padding: 7px; border: 1px solid var(--line);
      border-radius: 25px; background: var(--surface); box-shadow: 0 12px 36px #35234d10;
      transition: border-color .16s, box-shadow .16s;
    }
    form:focus-within { border-color: var(--accent); box-shadow: 0 0 0 4px color-mix(in srgb, var(--accent) 16%, transparent); }
    input {
      flex: 1; min-width: 0; border: 0; outline: 0; padding: 11px 14px;
      color: var(--ink); font: inherit; background: transparent;
    }
    input::placeholder { color: var(--muted); opacity: 1; }
    button {
      border: 0; border-radius: 19px; padding: 0 23px; color: white;
      font: 650 14px system-ui, sans-serif;
      background: linear-gradient(135deg, #8965e5, #704bd2); cursor: pointer;
      transition: filter .16s, transform .16s;
    }
    button:hover { filter: brightness(1.06); transform: translateY(-1px); }
    .favorites-heading { margin: 32px 0 12px; color: var(--muted); font-size: 12px; font-weight: 700; letter-spacing: 1.2px; text-transform: uppercase; }
    .cards { display: flex; flex-wrap: wrap; justify-content: center; gap: 13px; }
    .card {
      display: grid; justify-items: center; align-content: center; gap: 10px;
      width: 136px; min-height: 104px; padding: 14px 12px; border: 1px solid var(--line);
      border-radius: 19px; color: var(--ink); text-decoration: none; overflow: hidden;
      background: var(--surface); box-shadow: 0 5px 18px #00000012;
      transition: transform .16s, border-color .16s, box-shadow .16s;
    }
    .shortcut-icon {
      display: grid; place-items: center; width: 38px; height: 38px;
      border-radius: 13px; color: var(--accent); background: var(--accent-soft);
      font-size: 18px; font-weight: 750;
    }
    .card-title { max-width: 100%; overflow: hidden; text-overflow: ellipsis; white-space: nowrap; }
    .card:hover { transform: translateY(-2px); border-color: var(--accent); box-shadow: 0 10px 24px #00000020; }
    .empty-favorites { display: grid; justify-items: center; gap: 8px; color: var(--muted); }
    .empty-favorites .shortcut-icon { font-size: 25px; font-weight: 400; }
    .empty-favorites a { color: var(--accent); font-size: 13px; text-decoration: none; }
    .empty-favorites a:hover { text-decoration: underline; }
    @media (max-width: 520px) {
      main { padding: 32px 0; }
      h1 { font-size: 31px; }
      form { gap: 4px; }
      button { padding: 0 15px; }
      .card { width: min(136px, calc(50% - 8px)); }
    }
  </style>
</head>
<body>
  <main>
    __WEBAD_LOGO_SVG__
    <div class="brand">WebAD</div>
    <form action="__SEARCH_URL__" method="get">
      <input type="search" name="q" placeholder="Que souhaitez-vous rechercher ?" aria-label="Rechercher" autofocus>
      <button type="submit">Rechercher</button>
    </form>
    <h2 class="favorites-heading">Favoris</h2>
    <nav class="cards" aria-label="Favoris">__FAVORITE_CARDS__</nav>
  </main>
</body>
</html>
)HTML");
    QFile logoFile(QStringLiteral(":/webad.svg"));
    if (!logoFile.open(QIODevice::ReadOnly)) {
        qWarning("Failed to read the embedded WebAD SVG logo.");
        html.replace(QStringLiteral("__WEBAD_LOGO_SVG__"), QString());
    } else {
        html.replace(QStringLiteral("__WEBAD_LOGO_SVG__"),
                     QString::fromUtf8(logoFile.readAll())
                         .replace(QStringLiteral("<svg "),
                                  QStringLiteral("<svg class=\"mark\" aria-hidden=\"true\" ")));
    }
    html.replace(QStringLiteral("<html lang=\"fr\">"),
                 QStringLiteral("<html lang=\"fr\" data-theme=\"%1\">")
                     .arg(dark ? QStringLiteral("dark") : QStringLiteral("light")));
    html.replace(QStringLiteral("__SEARCH_URL__"),
                 searchBaseUrl(searchEngine).toHtmlEscaped());
    html.replace(QStringLiteral("__FAVORITE_CARDS__"), favoriteCards);
    return html;
}

QToolButton *makeToolButton(QWidget *parent, const QString &label, const QString &iconName)
{
    auto *button = new QToolButton(parent);
    button->setProperty("lineIcon", iconName);
    button->setToolTip(label);
    button->setAccessibleName(label);
    button->setToolButtonStyle(Qt::ToolButtonIconOnly);
    button->setIconSize(QSize(19, 19));
    button->setAutoRaise(true);
    return button;
}

QFrame *makeCard(QWidget *parent, const QString &title, const QString &description,
                 QVBoxLayout **contentLayout)
{
    auto *card = new QFrame(parent);
    card->setObjectName(QStringLiteral("settingsCard"));
    auto *layout = new QVBoxLayout(card);
    layout->setContentsMargins(20, 18, 20, 18);
    layout->setSpacing(12);

    auto *heading = new QLabel(title, card);
    heading->setObjectName(QStringLiteral("cardHeading"));
    layout->addWidget(heading);
    if (!description.isEmpty()) {
        auto *detail = new QLabel(description, card);
        detail->setObjectName(QStringLiteral("mutedLabel"));
        detail->setWordWrap(true);
        layout->addWidget(detail);
    }
    *contentLayout = layout;
    return card;
}

QPushButton *makeActionButton(QWidget *parent, const QString &text)
{
    auto *button = new QPushButton(text, parent);
    button->setCursor(Qt::PointingHandCursor);
    return button;
}

QString downloadStateLabel(const QPointer<QWebEngineDownloadRequest> &request,
                           const QString &state)
{
    if (request) {
        switch (request->state()) {
        case QWebEngineDownloadRequest::DownloadInProgress:
            return QObject::tr("En cours");
        case QWebEngineDownloadRequest::DownloadCompleted:
            return QObject::tr("Termin\u00e9");
        case QWebEngineDownloadRequest::DownloadCancelled:
            return QObject::tr("Annul\u00e9");
        case QWebEngineDownloadRequest::DownloadInterrupted:
            return QObject::tr("Interrompu");
        }
    }
    return state;
}

QString internalPageUrl(const QString &pageKey)
{
    return QStringLiteral("webad:%1").arg(pageKey);
}

QString internalPageKey(const QUrl &url)
{
    if (url.scheme().compare(QStringLiteral("webad"), Qt::CaseInsensitive) != 0) {
        return {};
    }

    const QString route = url.host().isEmpty() ? url.path() : url.host() + url.path();
    const QString key = route.trimmed().remove(QLatin1Char('/')).toLower();
    static const QStringList knownPages = {
        QStringLiteral("settings"),
        QStringLiteral("downloads"),
        QStringLiteral("extensions"),
        QStringLiteral("bookmarks"),
        QStringLiteral("history"),
        QStringLiteral("404")
    };
    return knownPages.contains(key) ? key : QString();
}
} // namespace

namespace
{
QString notFoundPageHtml(const QUrl &failedUrl, bool dark)
{
    QString html = QStringLiteral(R"HTML(
<!doctype html><html lang="fr"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>404 — Page introuvable | WebAD</title>
<style>
*{box-sizing:border-box}body{margin:0;min-height:100vh;display:grid;place-items:center;
background:__PAGE__;color:__INK__;font:16px/1.5 system-ui,sans-serif}
main{width:min(650px,calc(100% - 36px));padding:32px;text-align:center;border:1px solid __LINE__;
border-radius:24px;background:__SURFACE__}h1{margin:0;color:__ACCENT__;font-size:48px}
p{color:__MUTED__}.failed{max-width:100%;overflow-wrap:anywhere;font-size:13px}
img{display:block;width:min(360px,100%);height:auto;margin:22px auto;border-radius:14px}
a{color:__ACCENT__}button{padding:11px 20px;border:0;border-radius:14px;background:__ACCENT__;color:white;
font:600 14px system-ui;cursor:pointer}
</style></head><body><main><h1>404</h1><h2>Page introuvable ou inaccessible</h2>
<p class="failed">__FAILED_URL__</p>
<img src="https://http.cat/404" alt="HTTP 404 — chat non trouvé">
<p>Le serveur a refusé la page ou la connexion a échoué.</p>
<button onclick="history.back()">Revenir à la page précédente</button>
<p><a href="https://http.cat/404">https://http.cat/404</a></p>
</main></body></html>
)HTML");
    const QStringList colors = dark
        ? QStringList{QStringLiteral("#222125"), QStringLiteral("#f7f3ff"),
                      QStringLiteral("#414047"), QStringLiteral("#121212"),
                      QStringLiteral("#9862ed"), QStringLiteral("#aaa2b6")}
        : QStringList{QStringLiteral("#f7f6fb"), QStringLiteral("#302a42"),
                      QStringLiteral("#e9e5f0"), QStringLiteral("#ffffff"),
                      QStringLiteral("#7953dc"), QStringLiteral("#777184")};
    const QStringList tokens = {QStringLiteral("__PAGE__"), QStringLiteral("__INK__"),
                                QStringLiteral("__LINE__"), QStringLiteral("__SURFACE__"),
                                QStringLiteral("__ACCENT__"), QStringLiteral("__MUTED__")};
    for (qsizetype index = 0; index < tokens.size(); ++index) {
        html.replace(tokens.at(index), colors.at(index));
    }
    html.replace(QStringLiteral("__FAILED_URL__"), failedUrl.toDisplayString().toHtmlEscaped());
    return html;
}
} // namespace

BrowserWindow::BrowserWindow(QWidget *parent, bool privateMode, const QUrl &initialUrl)
    : QMainWindow(parent)
    , privateMode_(privateMode)
{
    const QIcon browserIcon = makeBrowserLogoIcon();
    QApplication::setWindowIcon(browserIcon);
    setWindowIcon(browserIcon);
    setWindowTitle(privateMode_ ? tr("WebAD — Navigation privée") : QStringLiteral("WebAD"));
    setMinimumSize(880, 600);
    resize(1440, 900);

    QSettings settings;
    if (!privateMode_) {
        QSettings legacySettings(QStringLiteral("NovaBrowser"), QStringLiteral("Nova"));
        if (!settings.contains(QStringLiteral("bookmarks"))
            && legacySettings.contains(QStringLiteral("bookmarks"))) {
            settings.setValue(QStringLiteral("bookmarks"),
                              legacySettings.value(QStringLiteral("bookmarks")));
            settings.setValue(QStringLiteral("windowGeometry"),
                              legacySettings.value(QStringLiteral("windowGeometry")));
        }
        bookmarks_ = settings.value(QStringLiteral("bookmarks")).toStringList();
        const QVariantList savedHistory = settings.value(QStringLiteral("history")).toList();
        for (const QVariant &savedItem : savedHistory) {
            const QVariantMap historyData = savedItem.toMap();
            const QUrl url(historyData.value(QStringLiteral("url")).toString());
            if (!url.isValid() || url.isEmpty()) {
                continue;
            }
            HistoryEntry entry;
            entry.title = historyData.value(QStringLiteral("title")).toString();
            entry.url = url.toString();
            entry.lastVisited = historyData.value(QStringLiteral("lastVisited")).toDateTime();
            history_.append(entry);
        }

        const QVariantList savedDownloads =
            settings.value(QStringLiteral("downloads/history")).toList();
        for (const QVariant &savedDownload : savedDownloads) {
            const QVariantMap savedEntry = savedDownload.toMap();
            DownloadEntry entry;
            entry.fileName = savedEntry.value(QStringLiteral("fileName")).toString();
            entry.path = savedEntry.value(QStringLiteral("path")).toString();
            entry.state = savedEntry.value(QStringLiteral("state")).toString();
            entry.receivedBytes = savedEntry.value(QStringLiteral("receivedBytes")).toLongLong();
            entry.totalBytes = savedEntry.value(QStringLiteral("totalBytes")).toLongLong();
            if (entry.state == tr("En cours")) {
                entry.state = tr("Interrompu");
            }
            downloads_.append(entry);
        }
    }
    theme_ = settings.value(QStringLiteral("theme"), QStringLiteral("System")).toString();
    searchEngine_ = settings.value(QStringLiteral("searchEngine"),
                                   QStringLiteral("DuckDuckGo")).toString();
    bookmarksBarVisible_ = settings.value(QStringLiteral("showBookmarksBar"), true).toBool();
    blockThirdPartyCookies_ = settings.value(
        QStringLiteral("blockThirdPartyCookies"), false).toBool();
    const QString appData =
        QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
    extensionPath_ = QDir(appData).filePath(QStringLiteral("extensions"));

    if (privateMode_) {
        profile_ = new QWebEngineProfile(this);
        profile_->setHttpCacheType(QWebEngineProfile::MemoryHttpCache);
        profile_->setPersistentCookiesPolicy(QWebEngineProfile::NoPersistentCookies);
    } else {
        const QString cachePath = QDir(appData).filePath(QStringLiteral("cache"));
        const QString storagePath = QDir(appData).filePath(QStringLiteral("web-data"));
        if (!QDir().mkpath(cachePath) || !QDir().mkpath(storagePath)
            || !QDir().mkpath(extensionPath_)) {
            statusBar()->showMessage(
                tr("Impossible de préparer les dossiers du navigateur."), 5000);
        }

        profile_ = QWebEngineProfile::defaultProfile();
        profile_->setCachePath(cachePath);
        profile_->setPersistentStoragePath(storagePath);
        profile_->setHttpCacheType(QWebEngineProfile::DiskHttpCache);
        profile_->setPersistentCookiesPolicy(QWebEngineProfile::AllowPersistentCookies);
    }
    const bool blockThirdPartyCookies = blockThirdPartyCookies_;
    profile_->cookieStore()->setCookieFilter(
        [blockThirdPartyCookies](const QWebEngineCookieStore::FilterRequest &request) {
            return !blockThirdPartyCookies || !request.thirdParty;
        });
    if (!privateMode_) {
        refreshExtensions();
        refreshExtensionScripts();
    }
    connect(profile_, &QWebEngineProfile::downloadRequested,
            this, &BrowserWindow::handleDownload);

    auto *central = new QWidget(this);
    central->setObjectName(QStringLiteral("appRoot"));
    auto *mainLayout = new QVBoxLayout(central);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    auto *tabsToolbar = new QWidget(central);
    tabsToolbar->setObjectName(QStringLiteral("tabsToolbar"));
    auto *tabsRow = new QHBoxLayout(tabsToolbar);
    tabsRow->setContentsMargins(14, 6, 14, 5);
    tabsRow->setSpacing(9);
    auto *brandMark = new QLabel(central);
    brandMark->setObjectName(QStringLiteral("brandMark"));
    brandMark->setPixmap(makeBrowserLogoIcon().pixmap(QSize(32, 32)));
    brandMark->setAlignment(Qt::AlignCenter);
    brandMark->setFixedSize(32, 32);
    tabsRow->addWidget(brandMark);
    auto *brandName = new QLabel(QStringLiteral("WebAD"), central);
    brandName->setObjectName(QStringLiteral("brandName"));
    tabsRow->addWidget(brandName);
    if (privateMode_) {
        auto *privateBadge = new QLabel(tr("PRIVÉ"), central);
        privateBadge->setObjectName(QStringLiteral("privateBadge"));
        privateBadge->setAlignment(Qt::AlignCenter);
        tabsRow->addWidget(privateBadge);
    }

    auto *brandDivider = new QFrame(central);
    brandDivider->setObjectName(QStringLiteral("toolbarDivider"));
    brandDivider->setFrameShape(QFrame::VLine);
    brandDivider->setFixedHeight(22);
    tabsRow->addWidget(brandDivider);

    tabBar_ = new QTabBar(central);
    tabBar_->setObjectName(QStringLiteral("tabBar"));
    tabBar_->setDocumentMode(true);
    tabBar_->setMovable(true);
    tabBar_->setTabsClosable(false);
    tabBar_->setExpanding(false);
    tabBar_->setElideMode(Qt::ElideRight);
    tabBar_->setUsesScrollButtons(true);
    tabBar_->setIconSize(QSize(16, 16));
    tabsRow->addWidget(tabBar_, 1);

    auto *newTabButton = makeToolButton(central, tr("Nouvel onglet"), QStringLiteral("add"));
    newTabButton->setObjectName(QStringLiteral("iconButton"));
    newTabButton->setFixedSize(34, 34);
    tabsRow->addWidget(newTabButton);

    auto *bookmarksButton = makeToolButton(
        central, tr("Bibliothèque de favoris"), QStringLiteral("bookmark"));
    bookmarksButton->setObjectName(QStringLiteral("iconButton"));
    bookmarksButton->setFixedSize(36, 34);
    connect(bookmarksButton, &QToolButton::clicked, this, [this] {
        navigate(internalPageUrl(QStringLiteral("bookmarks")));
    });
    tabsRow->addWidget(bookmarksButton);

    auto *historyButton = makeToolButton(central, tr("Historique"), QStringLiteral("clock"));
    historyButton->setObjectName(QStringLiteral("iconButton"));
    historyButton->setFixedSize(36, 34);
    connect(historyButton, &QToolButton::clicked, this, [this] {
        navigate(internalPageUrl(QStringLiteral("history")));
    });
    tabsRow->addWidget(historyButton);

    auto *downloadsButton = makeToolButton(
        central, tr("Téléchargements"), QStringLiteral("download"));
    downloadsButton->setObjectName(QStringLiteral("iconButton"));
    downloadsButton->setFixedSize(36, 34);
    connect(downloadsButton, &QToolButton::clicked, this, [this] {
        navigate(internalPageUrl(QStringLiteral("downloads")));
    });
    tabsRow->addWidget(downloadsButton);

    auto *extensionsButton = makeToolButton(central, tr("Extensions"), QStringLiteral("extensions"));
    extensionsButton->setObjectName(QStringLiteral("iconButton"));
    extensionsButton->setFixedSize(36, 34);
    connect(extensionsButton, &QToolButton::clicked, this, [this] {
        navigate(internalPageUrl(QStringLiteral("extensions")));
    });
    tabsRow->addWidget(extensionsButton);

    auto *settingsButton = makeToolButton(central, tr("Paramètres"), QStringLiteral("settings"));
    settingsButton->setObjectName(QStringLiteral("iconButton"));
    settingsButton->setFixedSize(36, 34);
    connect(settingsButton, &QToolButton::clicked, this, [this] {
        navigate(internalPageUrl(QStringLiteral("settings")));
    });
    tabsRow->addWidget(settingsButton);

    auto *privateButton = makeToolButton(
        central, tr("Ouvrir une fenêtre privée"), QStringLiteral("private"));
    privateButton->setObjectName(QStringLiteral("iconButton"));
    privateButton->setFixedSize(36, 34);
    privateButton->setEnabled(!privateMode_);
    privateButton->setToolTip(privateMode_ ? tr("Fenêtre privée") : privateButton->toolTip());
    privateButton->setAccessibleName(privateButton->toolTip());
    connect(privateButton, &QToolButton::clicked, this, &BrowserWindow::openPrivateWindow);
    tabsRow->addWidget(privateButton);

    auto *pagesMenuButton = makeToolButton(central, tr("Pages WebAD"), QStringLiteral("more"));
    pagesMenuButton->setObjectName(QStringLiteral("iconButton"));
    pagesMenuButton->setFixedSize(38, 34);
    auto *pagesMenu = new QMenu(pagesMenuButton);
    pagesMenu->addAction(tr("Inspecter la page…"), this, &BrowserWindow::openDeveloperTools);
    pagesMenu->addSeparator();
    pagesMenu->addAction(tr("Historique"), this, [this] {
        navigate(internalPageUrl(QStringLiteral("history")));
    });
    pagesMenu->addAction(tr("Favoris"), this, [this] {
        navigate(internalPageUrl(QStringLiteral("bookmarks")));
    });
    pagesMenu->addAction(tr("Téléchargements"), this, [this] {
        navigate(internalPageUrl(QStringLiteral("downloads")));
    });
    pagesMenu->addSeparator();
    pagesMenu->addAction(tr("Extensions"), this, [this] {
        navigate(internalPageUrl(QStringLiteral("extensions")));
    });
    pagesMenu->addAction(tr("Paramètres"), this, [this] {
        navigate(internalPageUrl(QStringLiteral("settings")));
    });
    pagesMenu->addSeparator();
    QAction *privateWindowAction = pagesMenu->addAction(
        makeLineIcon(QStringLiteral("private"), lineIconColor(theme_)),
        tr("Nouvelle fenêtre privée"));
    privateWindowAction->setEnabled(!privateMode_);
    connect(privateWindowAction, &QAction::triggered,
            this, &BrowserWindow::openPrivateWindow);
    pagesMenu->addSeparator();
    pagesMenu->addAction(tr("Ouvrir un fichier local…"), this, &BrowserWindow::openLocalFile);
    pagesMenuButton->setMenu(pagesMenu);
    pagesMenuButton->setPopupMode(QToolButton::InstantPopup);
    tabsRow->addWidget(pagesMenuButton);
    mainLayout->addWidget(tabsToolbar);

    auto *navigation = new QWidget(central);
    navigation->setObjectName(QStringLiteral("navigationBar"));
    auto *navigationLayout = new QHBoxLayout(navigation);
    navigationLayout->setContentsMargins(12, 5, 12, 5);
    navigationLayout->setSpacing(7);
    backButton_ = makeToolButton(navigation, tr("Précédent"), QStringLiteral("left"));
    forwardButton_ = makeToolButton(navigation, tr("Suivant"), QStringLiteral("forward"));
    reloadButton_ = makeToolButton(navigation, tr("Actualiser"), QStringLiteral("refresh"));
    auto *homeButton = makeToolButton(navigation, tr("Accueil"), QStringLiteral("home"));
    for (QToolButton *button : {backButton_, forwardButton_, reloadButton_, homeButton}) {
        button->setObjectName(QStringLiteral("iconButton"));
        button->setFixedSize(38, 38);
        navigationLayout->addWidget(button);
    }
    auto *openFileButton = makeToolButton(
        navigation, tr("Ouvrir un fichier local"), QStringLiteral("file"));
    openFileButton->setObjectName(QStringLiteral("iconButton"));
    openFileButton->setFixedSize(38, 38);
    navigationLayout->addWidget(openFileButton);

    addressBar_ = new QLineEdit(navigation);
    addressBar_->setPlaceholderText(privateMode_
        ? tr("Navigation privée — rechercher ou saisir une adresse")
        : tr("Rechercher ou saisir une adresse"));
    addressBar_->setClearButtonEnabled(true);
    addressBar_->setMinimumHeight(40);
    addressBar_->setObjectName(QStringLiteral("addressBar"));
    navigationLayout->addWidget(addressBar_, 1);

    bookmarkButton_ = new QToolButton(navigation);
    bookmarkButton_->setObjectName(QStringLiteral("iconButton"));
    bookmarkButton_->setProperty("lineIcon", QStringLiteral("bookmark"));
    bookmarkButton_->setToolTip(tr("Ajouter ou retirer des favoris"));
    bookmarkButton_->setAccessibleName(bookmarkButton_->toolTip());
    bookmarkButton_->setCheckable(true);
    bookmarkButton_->setAutoRaise(true);
    bookmarkButton_->setFixedSize(38, 38);
    navigationLayout->addWidget(bookmarkButton_);
    mainLayout->addWidget(navigation);

    if (privateMode_) {
        auto *privacyNotice = new QLabel(
            tr("Les données de navigation restent temporaires. Les recherches sont envoyées au moteur choisi via HTTPS ; le réseau peut encore observer certaines métadonnées. Les téléchargements restent sur le disque."),
            central);
        privacyNotice->setObjectName(QStringLiteral("privacyNotice"));
        privacyNotice->setWordWrap(true);
        mainLayout->addWidget(privacyNotice);
    }

    bookmarksBar_ = new QWidget(central);
    bookmarksBar_->setObjectName(QStringLiteral("bookmarksBar"));
    refreshBookmarks();
    bookmarksBar_->setVisible(bookmarksBarVisible_);
    mainLayout->addWidget(bookmarksBar_);

    progressBar_ = new QProgressBar(central);
    progressBar_->setObjectName(QStringLiteral("pageProgress"));
    progressBar_->setTextVisible(false);
    progressBar_->setFixedHeight(3);
    progressBar_->setRange(0, 100);
    progressBar_->hide();
    mainLayout->addWidget(progressBar_);

    pages_ = new QStackedWidget(central);
    pages_->setObjectName(QStringLiteral("pageStack"));
    mainLayout->addWidget(pages_, 1);
    setCentralWidget(central);

    connect(newTabButton, &QToolButton::clicked, this, [this] { createTab(); });
    connect(tabBar_, &QTabBar::currentChanged, this, [this](int index) {
        if (index >= 0) {
            pages_->setCurrentIndex(index);
        }
        if (devToolsDock_ && devToolsDock_->isVisible()) {
            if (QWebEngineView *view = currentView()) {
                if (inspectedPage_ && inspectedPage_ != view->page()) {
                    inspectedPage_->setDevToolsPage(nullptr);
                }
                view->page()->setDevToolsPage(devToolsView_->page());
                inspectedPage_ = view->page();
            } else {
                devToolsDock_->hide();
            }
        }
        updateNavigationState();
    });
    connect(tabBar_, &QTabBar::tabMoved, this, [this](int from, int to) {
        QWidget *page = pages_->widget(from);
        pages_->removeWidget(page);
        pages_->insertWidget(to, page);
        pages_->setCurrentIndex(tabBar_->currentIndex());
        updateNavigationState();
    });
    connect(backButton_, &QToolButton::clicked, this, [this] {
        if (QWebEngineView *view = currentView()) {
            view->back();
        }
    });
    connect(forwardButton_, &QToolButton::clicked, this, [this] {
        if (QWebEngineView *view = currentView()) {
            view->forward();
        }
    });
    connect(reloadButton_, &QToolButton::clicked, this, [this] {
        if (QWebEngineView *view = currentView()) {
            view->reload();
        }
    });
    connect(homeButton, &QToolButton::clicked, this, &BrowserWindow::navigateHome);
    connect(openFileButton, &QToolButton::clicked, this, &BrowserWindow::openLocalFile);
    connect(addressBar_, &QLineEdit::returnPressed, this, [this] {
        navigate(addressBar_->text());
    });
    connect(bookmarkButton_, &QToolButton::clicked, this, [this] {
        const QUrl url = currentView() ? currentView()->url() : QUrl();
        if (bookmarkButton_->isChecked()) {
            addBookmark();
        } else {
            removeBookmark(url);
        }
    });

    const auto addShortcut = [this](const QString &id, const QString &defaultKey, auto action) {
        const QString key = QSettings().value(
            QStringLiteral("shortcuts/") + id, defaultKey).toString();
        auto *shortcut = new QShortcut(QKeySequence(key), this);
        shortcut->setContext(Qt::WidgetWithChildrenShortcut);
        connect(shortcut, &QShortcut::activated, this, action);
        shortcuts_.insert(id, shortcut);
    };
    addShortcut(QStringLiteral("focusAddress"), QStringLiteral("Ctrl+L"), [this] {
        addressBar_->setFocus();
        addressBar_->selectAll();
    });
    addShortcut(QStringLiteral("newTab"), QStringLiteral("Ctrl+T"), [this] { createTab(); });
    addShortcut(QStringLiteral("reopenClosedTab"), QStringLiteral("Ctrl+Shift+T"), [this] {
        if (closedTabs_.isEmpty()) {
            statusBar()->showMessage(tr("Aucun onglet fermé à rouvrir."), 3000);
            return;
        }
        const QUrl url = closedTabs_.takeLast();
        createTab(url);
    });
    addShortcut(QStringLiteral("closeTab"), QStringLiteral("Ctrl+W"), [this] {
        closeTab(tabBar_->currentIndex());
    });
    addShortcut(QStringLiteral("privateWindow"), QStringLiteral("Ctrl+Shift+P"), [this] {
        openPrivateWindow();
    });
    addShortcut(QStringLiteral("privateWindowAlternate"), QStringLiteral("Ctrl+Shift+N"), [this] {
        openPrivateWindow();
    });
    addShortcut(QStringLiteral("openFile"), QStringLiteral("Ctrl+O"), [this] { openLocalFile(); });
    addShortcut(QStringLiteral("bookmarks"), QStringLiteral("Ctrl+Shift+B"), [this] {
        openInternalPage(QStringLiteral("bookmarks"));
    });
    addShortcut(QStringLiteral("history"), QStringLiteral("Ctrl+H"), [this] {
        navigate(internalPageUrl(QStringLiteral("history")));
    });
    addShortcut(QStringLiteral("downloads"), QStringLiteral("Ctrl+J"), [this] {
        openInternalPage(QStringLiteral("downloads"));
    });
    addShortcut(QStringLiteral("settings"), QStringLiteral("Ctrl+,"), [this] {
        openInternalPage(QStringLiteral("settings"));
    });
    addShortcut(QStringLiteral("reload"), QStringLiteral("Ctrl+R"), [this] {
        if (QWebEngineView *view = currentView()) {
            view->reload();
        }
    });
    addShortcut(QStringLiteral("reloadBypassCache"), QStringLiteral("Ctrl+Shift+R"), [this] {
        if (QWebEngineView *view = currentView()) {
            view->page()->triggerAction(QWebEnginePage::ReloadAndBypassCache);
        }
    });
    addShortcut(QStringLiteral("stopLoading"), QStringLiteral("Escape"), [this] {
        if (QWebEngineView *view = currentView(); view && progressBar_->isVisible()) {
            view->stop();
        }
    });
    addShortcut(QStringLiteral("home"), QStringLiteral("Alt+Home"), [this] {
        navigateHome();
    });
    addShortcut(QStringLiteral("focusSearch"), QStringLiteral("Ctrl+K"), [this] {
        addressBar_->setFocus();
        addressBar_->selectAll();
    });
    addShortcut(QStringLiteral("back"), QStringLiteral("Alt+Left"), [this] {
        if (QWebEngineView *view = currentView()) {
            view->back();
        }
    });
    addShortcut(QStringLiteral("forward"), QStringLiteral("Alt+Right"), [this] {
        if (QWebEngineView *view = currentView()) {
            view->forward();
        }
    });
    addShortcut(QStringLiteral("addBookmark"), QStringLiteral("Ctrl+D"), [this] {
        addBookmark();
    });
    addShortcut(QStringLiteral("developerTools"), QStringLiteral("Ctrl+Shift+I"), [this] {
        openDeveloperTools();
    });
    addShortcut(QStringLiteral("developerToolsF12"), QStringLiteral("F12"), [this] {
        if (devToolsDock_ && devToolsDock_->isVisible()) {
            devToolsDock_->hide();
        } else {
            openDeveloperTools();
        }
    });
    addShortcut(QStringLiteral("nextTab"), QStringLiteral("Ctrl+Tab"), [this] {
        const int count = tabBar_->count();
        if (count > 1) {
            tabBar_->setCurrentIndex((tabBar_->currentIndex() + 1) % count);
        }
    });
    addShortcut(QStringLiteral("previousTab"), QStringLiteral("Ctrl+Shift+Tab"), [this] {
        const int count = tabBar_->count();
        if (count > 1) {
            tabBar_->setCurrentIndex((tabBar_->currentIndex() + count - 1) % count);
        }
    });
    for (int tabIndex = 0; tabIndex < 8; ++tabIndex) {
        addShortcut(
            QStringLiteral("tab%1").arg(tabIndex + 1),
            QStringLiteral("Ctrl+%1").arg(tabIndex + 1),
            [this, tabIndex] {
                if (tabIndex < tabBar_->count()) {
                    tabBar_->setCurrentIndex(tabIndex);
                }
            });
    }
    addShortcut(QStringLiteral("lastTab"), QStringLiteral("Ctrl+9"), [this] {
        if (tabBar_->count() > 0) {
            tabBar_->setCurrentIndex(tabBar_->count() - 1);
        }
    });
    addShortcut(QStringLiteral("find"), QStringLiteral("Ctrl+F"), [this] {
        QWebEngineView *view = currentView();
        if (!view) {
            return;
        }
        bool accepted = false;
        const QString text = QInputDialog::getText(
            this, tr("Rechercher dans la page"), tr("Texte à rechercher :"),
            QLineEdit::Normal, QString(), &accepted);
        if (accepted && !text.isEmpty()) {
            view->findText(text);
        }
    });

    if (!privateMode_) {
        const QByteArray savedGeometry =
            settings.value(QStringLiteral("windowGeometry")).toByteArray();
        if (!savedGeometry.isEmpty()) {
            restoreGeometry(savedGeometry);
        }
    }
    applyTheme();
    createTab(initialUrl);
}

QWebEngineView *BrowserWindow::currentView() const
{
    return qobject_cast<QWebEngineView *>(pages_->currentWidget());
}

void BrowserWindow::openPrivateWindow()
{
    if (privateMode_) {
        statusBar()->showMessage(tr("Vous êtes déjà dans une fenêtre privée."), 3500);
        return;
    }
    auto *window = new BrowserWindow(nullptr, true);
    window->setAttribute(Qt::WA_DeleteOnClose);
    window->show();
}

void BrowserWindow::openDeveloperTools()
{
    QWebEngineView *view = currentView();
    if (!view) {
        statusBar()->showMessage(tr("L’inspecteur est disponible sur une page web."), 3500);
        return;
    }
    if (!devToolsDock_) {
        devToolsDock_ = new QDockWidget(tr("Inspecteur WebAD"), this);
        devToolsDock_->setObjectName(QStringLiteral("devToolsDock"));
        devToolsView_ = new QWebEngineView(profile_, devToolsDock_);
        devToolsDock_->setWidget(devToolsView_);
        addDockWidget(Qt::BottomDockWidgetArea, devToolsDock_);
    }
    if (inspectedPage_ && inspectedPage_ != view->page()) {
        inspectedPage_->setDevToolsPage(nullptr);
    }
    view->page()->setDevToolsPage(devToolsView_->page());
    inspectedPage_ = view->page();
    devToolsDock_->show();
    devToolsDock_->raise();
}

void BrowserWindow::refreshExtensions()
{
    extensions_.clear();
    const QDir root(extensionPath_);
    const QStringList directories = root.exists()
        ? root.entryList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name)
        : QStringList{};
    for (const QString &id : directories) {
        const QString directoryPath = root.filePath(id);
        QFile manifestFile(QDir(directoryPath).filePath(QStringLiteral("manifest.json")));
        if (!manifestFile.open(QIODevice::ReadOnly)) {
            qWarning() << "Unable to read extension manifest:" << manifestFile.fileName();
            continue;
        }
        QJsonParseError parseError;
        const QJsonDocument document =
            QJsonDocument::fromJson(manifestFile.readAll(), &parseError);
        const QJsonObject manifest = document.object();
        const QString name = manifest.value(QStringLiteral("name")).toString().trimmed();
        const QString version = manifest.value(QStringLiteral("version")).toString().trimmed();
        const QString scriptName = manifest.value(QStringLiteral("script")).toString().trimmed();
        const QJsonArray matchArray = manifest.value(QStringLiteral("matches")).toArray();
        if (parseError.error != QJsonParseError::NoError || !document.isObject()
            || name.isEmpty() || name.size() > 80 || version.isEmpty()
            || QFileInfo(scriptName).fileName() != scriptName || scriptName.isEmpty()
            || matchArray.isEmpty() || matchArray.size() > 50) {
            qWarning() << "Invalid WebAD extension manifest:" << manifestFile.fileName()
                       << parseError.errorString();
            continue;
        }

        ExtensionEntry extension;
        extension.id = id;
        extension.name = name;
        extension.version = version;
        extension.scriptPath = QDir(directoryPath).filePath(scriptName);
        bool validMatches = true;
        for (const QJsonValue &matchValue : matchArray) {
            const QString pattern = matchValue.toString().trimmed();
            if (pattern.isEmpty()
                || (!pattern.startsWith(QStringLiteral("http://"))
                    && !pattern.startsWith(QStringLiteral("https://"))
                    && !pattern.startsWith(QStringLiteral("*://")))) {
                validMatches = false;
                break;
            }
            extension.matches.append(pattern);
        }
        QFile scriptFile(extension.scriptPath);
        if (!validMatches || !scriptFile.exists() || scriptFile.size() > 1024 * 1024) {
            qWarning() << "Invalid WebAD extension script or match patterns:" << id;
            continue;
        }
        extensions_.append(extension);
    }

    if (extensionsList_) {
        QLayout *listLayout = extensionsList_->layout();
        while (QLayoutItem *item = listLayout->takeAt(0)) {
            if (QWidget *widget = item->widget()) {
                widget->deleteLater();
            }
            delete item;
        }
        const QStringList enabled =
            QSettings().value(QStringLiteral("extensions/enabled")).toStringList();
        if (extensions_.isEmpty()) {
            auto *empty = new QLabel(tr("Aucune extension installée."), extensionsList_);
            empty->setObjectName(QStringLiteral("mutedLabel"));
            listLayout->addWidget(empty);
        }
        for (const ExtensionEntry &extension : std::as_const(extensions_)) {
            auto *row = new QWidget(extensionsList_);
            auto *rowLayout = new QHBoxLayout(row);
            rowLayout->setContentsMargins(0, 5, 0, 5);
            auto *toggle = new QCheckBox(
                tr("%1 — %2").arg(extension.name, extension.version), row);
            toggle->setChecked(enabled.contains(extension.id));
            toggle->setEnabled(!privateMode_);
            rowLayout->addWidget(toggle, 1);
            auto *remove = makeActionButton(row, tr("Supprimer"));
            remove->setEnabled(!privateMode_);
            rowLayout->addWidget(remove);
            connect(toggle, &QCheckBox::toggled, this, [this, id = extension.id](bool checked) {
                setExtensionEnabled(id, checked);
            });
            connect(remove, &QPushButton::clicked, this, [this, id = extension.id] {
                const QString path = QDir(extensionPath_).filePath(id);
                const QString canonicalRoot = QDir(extensionPath_).canonicalPath();
                const QString canonicalTarget = QFileInfo(path).canonicalFilePath();
                if (canonicalRoot.isEmpty() || canonicalTarget.isEmpty()
                    || !canonicalTarget.startsWith(canonicalRoot + QDir::separator())) {
                    QMessageBox::warning(this, tr("Extensions"),
                                         tr("Le dossier de l’extension n’est pas valide."));
                    return;
                }
                if (QMessageBox::question(this, tr("Supprimer l’extension"),
                                          tr("Supprimer « %1 » ?")
                                              .arg(id),
                                          QMessageBox::Yes | QMessageBox::Cancel,
                                          QMessageBox::Cancel) != QMessageBox::Yes) {
                    return;
                }
                if (!QDir(path).removeRecursively()) {
                    QMessageBox::warning(this, tr("Extensions"),
                                         tr("Impossible de supprimer cette extension."));
                    return;
                }
                QStringList active =
                    QSettings().value(QStringLiteral("extensions/enabled")).toStringList();
                active.removeAll(id);
                QSettings().setValue(QStringLiteral("extensions/enabled"), active);
                refreshExtensions();
                refreshExtensionScripts();
            });
            listLayout->addWidget(row);
        }
    }
}

void BrowserWindow::refreshExtensionScripts()
{
    if (privateMode_ || !profile_ || !profile_->scripts()) {
        return;
    }
    QWebEngineScriptCollection *scriptCollection = profile_->scripts();
    scriptCollection->clear();
    const QStringList enabled =
        QSettings().value(QStringLiteral("extensions/enabled")).toStringList();
    for (const ExtensionEntry &extension : std::as_const(extensions_)) {
        if (!enabled.contains(extension.id)) {
            continue;
        }

        QFile file(extension.scriptPath);
        if (!file.open(QIODevice::ReadOnly)) {
            qWarning() << "Unable to read WebAD extension script:" << extension.scriptPath;
            continue;
        }
        const QByteArray source = file.readAll();
        if (source.size() > 1024 * 1024) {
            qWarning() << "WebAD extension script exceeds the size limit:" << extension.id;
            continue;
        }

        QJsonArray patterns;
        for (const QString &pattern : extension.matches) {
            QString expression;
            for (const QChar character : pattern) {
                if (character == QLatin1Char('*')) {
                    expression += QStringLiteral(".*");
                } else if (character == QLatin1Char('?')) {
                    expression += QLatin1Char('.');
                } else {
                    if (QStringLiteral("\\.^$|()[]{}+").contains(character)) {
                        expression += QLatin1Char('\\');
                    }
                    expression += character;
                }
            }
            patterns.append(QStringLiteral("^%1$").arg(expression));
        }
        const QString guard = QStringLiteral(
            "(()=>{const p=%1;if(!p.some(x=>new RegExp(x).test(location.href)))return;\n")
                                  .arg(QString::fromUtf8(
                                      QJsonDocument(patterns).toJson(QJsonDocument::Compact)));
        QWebEngineScript script;
        script.setName(QStringLiteral("WebAD extension: ") + extension.id);
        script.setInjectionPoint(QWebEngineScript::DocumentReady);
        script.setWorldId(QWebEngineScript::ApplicationWorld);
        script.setRunsOnSubFrames(false);
        script.setSourceCode(guard + QString::fromUtf8(source) + QStringLiteral("\n})();"));
        scriptCollection->insert(script);
    }
}

void BrowserWindow::installExtension(const QString &sourcePath)
{
    if (privateMode_) {
        return;
    }
    QFile manifestFile(QDir(sourcePath).filePath(QStringLiteral("manifest.json")));
    if (!manifestFile.open(QIODevice::ReadOnly)) {
        QMessageBox::warning(this, tr("Extensions"),
                             tr("Le dossier ne contient pas de manifest.json lisible."));
        return;
    }
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(manifestFile.readAll(), &parseError);
    const QJsonObject manifest = document.object();
    const QString name = manifest.value(QStringLiteral("name")).toString().trimmed();
    const QString scriptName = manifest.value(QStringLiteral("script")).toString().trimmed();
    const QJsonArray matches = manifest.value(QStringLiteral("matches")).toArray();
    if (parseError.error != QJsonParseError::NoError || !document.isObject()
        || name.isEmpty() || name.size() > 80
        || manifest.value(QStringLiteral("version")).toString().trimmed().isEmpty()
        || QFileInfo(scriptName).fileName() != scriptName || scriptName.isEmpty()
        || scriptName == QStringLiteral(".") || scriptName == QStringLiteral("..")
        || matches.isEmpty() || matches.size() > 50) {
        QMessageBox::warning(this, tr("Extensions"),
                             tr("Manifeste invalide. Il doit définir name, version, script et matches."));
        return;
    }
    manifestFile.close();
    for (const QJsonValue &value : matches) {
        const QString pattern = value.toString().trimmed();
        if (pattern.isEmpty()
            || (!pattern.startsWith(QStringLiteral("http://"))
                && !pattern.startsWith(QStringLiteral("https://"))
                && !pattern.startsWith(QStringLiteral("*://")))) {
            QMessageBox::warning(this, tr("Extensions"),
                                 tr("Chaque règle matches doit cibler un site HTTP ou HTTPS."));
            return;
        }
    }
    QFile sourceScript(QDir(sourcePath).filePath(scriptName));
    if (!sourceScript.exists() || sourceScript.size() > 1024 * 1024) {
        QMessageBox::warning(this, tr("Extensions"),
                             tr("Le script est absent ou dépasse la limite de 1 Mio."));
        return;
    }

    QString id = QFileInfo(sourcePath).fileName().toLower();
    id.replace(QRegularExpression(QStringLiteral("[^a-z0-9_-]")), QStringLiteral("-"));
    if (id.isEmpty() || id == QStringLiteral(".") || id == QStringLiteral("..")) {
        QMessageBox::warning(this, tr("Extensions"), tr("Nom de dossier d’extension invalide."));
        return;
    }
    const QString destination = QDir(extensionPath_).filePath(id);
    if (QFileInfo::exists(destination)) {
        QMessageBox::warning(this, tr("Extensions"),
                             tr("Une extension portant cet identifiant est déjà installée."));
        return;
    }
    if (!QDir().mkpath(destination)) {
        QMessageBox::warning(this, tr("Extensions"),
                             tr("Impossible de créer le dossier d’installation."));
        return;
    }
    const QString manifestDestination =
        QDir(destination).filePath(QStringLiteral("manifest.json"));
    const QString scriptDestination = QDir(destination).filePath(scriptName);
    if (!QFile::copy(manifestFile.fileName(), manifestDestination)
        || !QFile::copy(sourceScript.fileName(), scriptDestination)) {
        QDir(destination).removeRecursively();
        QMessageBox::warning(this, tr("Extensions"),
                             tr("Impossible de copier les fichiers de l’extension."));
        return;
    }
    QStringList active = QSettings().value(QStringLiteral("extensions/enabled")).toStringList();
    active.append(id);
    QSettings().setValue(QStringLiteral("extensions/enabled"), active);
    refreshExtensions();
    refreshExtensionScripts();
    statusBar()->showMessage(
        tr("Extension installée. Elle sera active au prochain chargement de page."), 5000);
}

void BrowserWindow::setExtensionEnabled(const QString &id, bool enabled)
{
    QStringList active = QSettings().value(QStringLiteral("extensions/enabled")).toStringList();
    active.removeAll(id);
    if (enabled) {
        active.append(id);
    }
    QSettings().setValue(QStringLiteral("extensions/enabled"), active);
    refreshExtensionScripts();
    statusBar()->showMessage(
        tr("Le changement s’appliquera au prochain chargement de page."), 4000);
}

void BrowserWindow::registerOpenWith()
{
#ifdef Q_OS_WIN
    const QString executable = QDir::toNativeSeparators(QCoreApplication::applicationFilePath());
    const QString command = QStringLiteral("\"%1\" \"%2\"").arg(
        executable, QStringLiteral("%1"));
    QSettings classes(QStringLiteral("HKEY_CURRENT_USER\\Software\\Classes"),
                      QSettings::NativeFormat);
    classes.setValue(QStringLiteral("WebAD.HTML/."), tr("Page web HTML — WebAD"));
    classes.setValue(QStringLiteral("Applications/WebAD.exe/."), QStringLiteral("WebAD"));
    for (const QString &extension : {QStringLiteral(".html"), QStringLiteral(".htm"),
                                     QStringLiteral(".xhtml")}) {
        classes.setValue(extension + QStringLiteral("/OpenWithProgids/WebAD.HTML"),
                         QString());
        classes.setValue(QStringLiteral("Applications/WebAD.exe/SupportedTypes/") + extension,
                         QString());
    }
    classes.setValue(QStringLiteral("WebAD.HTML/DefaultIcon/."), executable + QStringLiteral(",0"));
    classes.setValue(QStringLiteral("WebAD.HTML/shell/open/command/."), command);
    classes.setValue(QStringLiteral("Applications/WebAD.exe/shell/open/command/."), command);
    classes.sync();
    if (classes.status() != QSettings::NoError) {
        QMessageBox::warning(this, tr("Ouvrir avec WebAD"),
                             tr("Windows n’a pas pu enregistrer WebAD dans les applications disponibles."));
        return;
    }
    statusBar()->showMessage(
        tr("WebAD est ajouté aux choix « Ouvrir avec » pour les fichiers HTML."), 5000);
#else
    statusBar()->showMessage(tr("L’intégration « Ouvrir avec » est disponible sous Windows."), 5000);
#endif
}

BrowserWindow::~BrowserWindow()
{
    if (!privateMode_) {
        return;
    }

    for (const DownloadEntry &entry : downloads_) {
        if (entry.request
            && entry.request->state() == QWebEngineDownloadRequest::DownloadInProgress) {
            entry.request->cancel();
        }
    }
    while (pages_ && pages_->count() > 0) {
        QWidget *page = pages_->widget(0);
        pages_->removeWidget(page);
        delete page;
    }
    if (inspectedPage_) {
        inspectedPage_->setDevToolsPage(nullptr);
        inspectedPage_.clear();
    }
    delete devToolsDock_.data();
    devToolsDock_.clear();
    devToolsView_.clear();
    delete profile_.data();
}

void BrowserWindow::createTab(const QUrl &url)
{
    if (url.scheme().compare(QStringLiteral("webad"), Qt::CaseInsensitive) == 0) {
        const QString pageKey = internalPageKey(url);
        if (pageKey.isEmpty()) {
            statusBar()->showMessage(tr("Page WebAD inconnue : %1").arg(url.toString()), 5000);
            openInternalPage(QStringLiteral("404"));
            return;
        }
        openInternalPage(pageKey);
        return;
    }

    createWebViewTab(url, true);
}

QWebEngineView *BrowserWindow::createWebViewTab(const QUrl &url, bool showHomePage)
{
    auto *view = new WebADView(pages_);
    auto *webPage = new WebADPage(profile_, view);
    view->setPage(webPage);
    webPage->popupFactory = [this] { return createPopupPage(); };
    webPage->internalNavigationHandler = [this](const QUrl &target) {
        navigate(target.toString());
    };
    view->bookmarkHandler = [this](const QUrl &target) {
        addBookmark(target);
    };
    view->searchHandler = [this](const QString &query) {
        createTab(QUrl(searchUrl(query)));
    };
    view->settings()->setAttribute(
        QWebEngineSettings::JavascriptEnabled,
        QSettings().value(QStringLiteral("enableJavaScript"), true).toBool());
    view->settings()->setAttribute(
        QWebEngineSettings::LocalStorageEnabled,
        QSettings().value(QStringLiteral("enableLocalStorage"), true).toBool());
    const int index = tabBar_->addTab(tr("Nouvel onglet"));
    tabBar_->setTabIcon(index, makeBrowserLogoIcon());
    installTabCloseButton(index);
    pages_->addWidget(view);
    tabBar_->setCurrentIndex(index);
    pages_->setCurrentWidget(view);

    connect(webPage, &QWebEnginePage::iconChanged, this, [this, view](const QIcon &icon) {
        const int tabIndex = pages_->indexOf(view);
        if (tabIndex >= 0) {
            tabBar_->setTabIcon(tabIndex, icon.isNull() ? makeBrowserLogoIcon() : icon);
        }
    });
    connect(view, &QWebEngineView::titleChanged, this, [this, view](const QString &title) {
        const int tabIndex = pages_->indexOf(view);
        if (tabIndex >= 0) {
            tabBar_->setTabText(tabIndex, title.isEmpty() ? tr("Nouvel onglet") : title);
            tabBar_->setTabToolTip(tabIndex, title);
            recordHistory(view, view->url());
            if (view == currentView()) {
                const QString pageTitle = title.isEmpty() ? tr("Nouvel onglet") : title;
                setWindowTitle(privateMode_
                    ? tr("WebAD — Privée — %1").arg(pageTitle)
                    : QStringLiteral("WebAD — %1").arg(pageTitle));
            }
        }
    });
    connect(view, &QWebEngineView::urlChanged, this, [this, view](const QUrl &url) {
        if (url.scheme() != QStringLiteral("about")
            && url.scheme() != QStringLiteral("data")) {
            view->setProperty("webadHomePage", false);
        }
        recordHistory(view, url);
        if (view == currentView()) {
            addressBar_->setText(
                url.scheme() == QStringLiteral("about") || url.scheme() == QStringLiteral("data")
                    ? QString()
                    : url.toDisplayString());
            updateBookmarkButton();
        }
    });
    const auto showNotFound = [this, view](const QUrl &failedUrl) {
        if (view->property("webadErrorPage").toBool()) {
            return;
        }
        view->setProperty("webadErrorPage", true);
        view->setProperty("webadSettingErrorPage", true);
        view->setHtml(notFoundPageHtml(failedUrl, isDarkTheme(theme_)), failedUrl);
    };
    connect(view, &QWebEngineView::loadStarted, this, [this, view] {
        const int tabIndex = pages_->indexOf(view);
        if (tabIndex >= 0) {
            tabBar_->setTabIcon(tabIndex, makeBrowserLogoIcon());
        }
        if (view->property("webadSettingErrorPage").toBool()) {
            view->setProperty("webadSettingErrorPage", false);
        } else {
            view->setProperty("webadErrorPage", false);
        }
        if (view == currentView()) {
            progressBar_->setValue(0);
            progressBar_->show();
            statusBar()->showMessage(tr("Chargement…"));
        }
    });
    connect(view, &QWebEngineView::loadProgress, this, [this, view](int progress) {
        if (view == currentView()) {
            progressBar_->setValue(progress);
        }
    });
    connect(view, &QWebEngineView::loadFinished, this, [this, view, showNotFound](bool ok) {
        if (!ok && !view->property("webadErrorPage").toBool()
            && (view->url().scheme() == QStringLiteral("http")
                || view->url().scheme() == QStringLiteral("https"))) {
            showNotFound(view->url());
        }
        if (view == currentView()) {
            progressBar_->hide();
            statusBar()->showMessage(
                ok ? QString() : tr("Page inaccessible — affichage de la page 404 WebAD."), 5000);
            updateNavigationState();
        }
    });
    connect(webPage, &QWebEnginePage::loadingChanged, this,
            [showNotFound](const QWebEngineLoadingInfo &info) {
        const bool httpFailure =
            info.errorDomain() == QWebEngineLoadingInfo::HttpStatusCodeDomain
            && info.errorCode() >= 400;
        if (info.status() == QWebEngineLoadingInfo::LoadFailedStatus || httpFailure) {
            showNotFound(info.url());
        }
    });

    if (url.isValid() && !url.isEmpty()) {
        view->setUrl(url);
    } else if (showHomePage) {
        view->setProperty("webadHomePage", true);
        view->setHtml(homePageHtml(searchEngine_, bookmarks_, isDarkTheme(theme_)));
    }
    updateNavigationState();
    return view;
}

QWebEnginePage *BrowserWindow::createPopupPage()
{
    return createWebViewTab(QUrl(), false)->page();
}

void BrowserWindow::installTabCloseButton(int index)
{
    auto *closeButton = new QToolButton(tabBar_);
    closeButton->setObjectName(QStringLiteral("tabCloseButton"));
    closeButton->setProperty("lineIcon", QStringLiteral("close"));
    closeButton->setToolTip(tr("Fermer l’onglet"));
    closeButton->setAccessibleName(closeButton->toolTip());
    closeButton->setAutoRaise(true);
    closeButton->setFixedSize(22, 22);
    closeButton->setIconSize(QSize(13, 13));
    closeButton->setIcon(makeLineIcon(
        QStringLiteral("close"), lineIconColor(theme_)));
    tabBar_->setTabButton(index, QTabBar::RightSide, closeButton);
    connect(closeButton, &QToolButton::clicked, this, [this, closeButton] {
        for (int tabIndex = 0; tabIndex < tabBar_->count(); ++tabIndex) {
            if (tabBar_->tabButton(tabIndex, QTabBar::RightSide) == closeButton) {
                closeTab(tabIndex);
                return;
            }
        }
    });
}

void BrowserWindow::openInternalPage(const QString &pageKey)
{
    for (int index = 0; index < pages_->count(); ++index) {
        QWidget *page = pages_->widget(index);
        if (page->property("internalKey").toString() == pageKey) {
            tabBar_->setCurrentIndex(index);
            pages_->setCurrentIndex(index);
            updateNavigationState();
            return;
        }
    }

    QWidget *page = createInternalPage(pageKey);
    const int index = tabBar_->addTab(page->windowTitle());
    tabBar_->setTabIcon(index, makeBrowserLogoIcon());
    installTabCloseButton(index);
    pages_->addWidget(page);
    tabBar_->setCurrentIndex(index);
    pages_->setCurrentWidget(page);
    updateNavigationState();
}

QWidget *BrowserWindow::createInternalPage(const QString &pageKey)
{
    QString title;
    if (pageKey == QStringLiteral("404")) {
        title = QStringLiteral("404 — WebAD");
    } else if (pageKey == QStringLiteral("bookmarks")) {
        title = tr("Favoris");
    } else if (pageKey == QStringLiteral("history")) {
        title = tr("Historique");
    } else if (pageKey == QStringLiteral("downloads")) {
        title = tr("Téléchargements");
    } else if (pageKey == QStringLiteral("extensions")) {
        title = tr("Extensions");
    } else {
        title = tr("Paramètres");
    }

    auto *outer = new QWidget;
    outer->setProperty("internalKey", pageKey);
    outer->setWindowTitle(title);
    auto *outerLayout = new QVBoxLayout(outer);
    outerLayout->setContentsMargins(0, 0, 0, 0);
    auto *scroll = new QScrollArea(outer);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    auto *content = new QWidget(scroll);
    content->setObjectName(QStringLiteral("internalPageContent"));
    auto *layout = new QVBoxLayout(content);
    layout->setContentsMargins(24, 22, 24, 32);
    layout->setSpacing(16);

    auto *heading = new QLabel(title, content);
    heading->setObjectName(QStringLiteral("pageHeading"));
    if (pageKey == QStringLiteral("404")) {
        auto *errorView = new QWebEngineView(profile_, content);
        errorView->setHtml(notFoundPageHtml(QUrl(), isDarkTheme(theme_)),
                           QUrl(QStringLiteral("https://http.cat/404")));
        layout->addWidget(errorView, 1);
    } else {
        layout->addWidget(heading);
    }

    if (pageKey == QStringLiteral("bookmarks")) {
        auto *intro = new QLabel(tr("Tous vos sites préférés, réunis au même endroit."), content);
        intro->setObjectName(QStringLiteral("mutedLabel"));
        layout->addWidget(intro);

        auto *cardLayout = static_cast<QVBoxLayout *>(nullptr);
        auto *card = makeCard(content, tr("Votre collection"),
                              tr("Ouvrez un favori dans l’onglet actuel ou supprimez-le de votre collection."),
                              &cardLayout);
        bookmarksTable_ = new QTableWidget(card);
        bookmarksTable_->setObjectName(QStringLiteral("bookmarksTable"));
        bookmarksTable_->setColumnCount(3);
        bookmarksTable_->setHorizontalHeaderLabels(
            {tr("Nom du site"), tr("Adresse"), tr("Actions")});
        bookmarksTable_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
        bookmarksTable_->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
        bookmarksTable_->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
        bookmarksTable_->verticalHeader()->setVisible(false);
        bookmarksTable_->setEditTriggers(QAbstractItemView::NoEditTriggers);
        bookmarksTable_->setSelectionMode(QAbstractItemView::NoSelection);
        bookmarksTable_->setMinimumHeight(240);
        cardLayout->addWidget(bookmarksTable_);
        layout->addWidget(card);
        refreshBookmarksPage();
    } else if (pageKey == QStringLiteral("history")) {
        auto *intro = new QLabel(privateMode_
            ? tr("Cette fenêtre n’enregistre pas votre historique de navigation.")
            : tr("Retrouvez les pages consultées récemment sur cet appareil."), content);
        intro->setObjectName(QStringLiteral("mutedLabel"));
        layout->addWidget(intro);

        auto *cardLayout = static_cast<QVBoxLayout *>(nullptr);
        auto *card = makeCard(content, tr("Pages récentes"),
                              privateMode_
                                  ? tr("L’historique temporaire du navigateur disparaît à la fermeture de cette fenêtre.")
                                  : tr("L’historique est conservé localement et peut être effacé à tout moment."),
                              &cardLayout);
        auto *toolbar = new QHBoxLayout;
        auto *openSelected = makeActionButton(card, tr("Ouvrir la page sélectionnée"));
        auto *removeSelected = makeActionButton(card, tr("Retirer la page sélectionnée"));
        auto *clearHistory = makeActionButton(card, tr("Effacer l’historique"));
        toolbar->addWidget(openSelected);
        toolbar->addWidget(removeSelected);
        toolbar->addWidget(clearHistory);
        toolbar->addStretch(1);
        cardLayout->addLayout(toolbar);
        connect(openSelected, &QPushButton::clicked, this, [this] {
            const auto selectedItems = historyTable_ ? historyTable_->selectedItems()
                                                      : QList<QTableWidgetItem *>{};
            const int row = !selectedItems.isEmpty()
                                ? selectedItems.first()->row()
                                : (historyTable_ ? historyTable_->currentRow() : -1);
            if (row >= 0 && row < history_.size()) {
                createTab(QUrl(history_.at(row).url));
            } else {
                statusBar()->showMessage(tr("Sélectionnez une page de l’historique."), 4000);
            }
        });
        connect(removeSelected, &QPushButton::clicked, this, [this] {
            const auto selectedItems = historyTable_ ? historyTable_->selectedItems()
                                                      : QList<QTableWidgetItem *>{};
            const int row = !selectedItems.isEmpty()
                                ? selectedItems.first()->row()
                                : (historyTable_ ? historyTable_->currentRow() : -1);
            if (row >= 0 && row < history_.size()) {
                removeHistoryEntry(history_.at(row).url);
            } else {
                statusBar()->showMessage(tr("Sélectionnez une page à retirer."), 4000);
            }
        });
        connect(clearHistory, &QPushButton::clicked, this, [this] {
            history_.clear();
            if (!privateMode_) {
                QSettings().remove(QStringLiteral("history"));
            }
            refreshHistoryPage();
        });

        historyTable_ = new QTableWidget(card);
        historyTable_->setObjectName(QStringLiteral("historyTable"));
        historyTable_->setColumnCount(3);
        historyTable_->setHorizontalHeaderLabels(
            {tr("Page"), tr("Adresse"), tr("Consultée le")});
        historyTable_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
        historyTable_->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
        historyTable_->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
        historyTable_->verticalHeader()->setVisible(false);
        historyTable_->setEditTriggers(QAbstractItemView::NoEditTriggers);
        historyTable_->setSelectionBehavior(QAbstractItemView::SelectRows);
        historyTable_->setSelectionMode(QAbstractItemView::SingleSelection);
        historyTable_->setMinimumHeight(300);
        connect(historyTable_, &QTableWidget::cellDoubleClicked, this, [this](int row) {
            if (row >= 0 && row < history_.size()) {
                createTab(QUrl(history_.at(row).url));
            }
        });
        cardLayout->addWidget(historyTable_);
        layout->addWidget(card);
        refreshHistoryPage();
    } else if (pageKey == QStringLiteral("downloads")) {
        auto *intro = new QLabel(privateMode_
            ? tr("Cette liste disparaît à la fermeture de la fenêtre. Les fichiers enregistrés, eux, restent sur le disque.")
            : tr("Suivez vos téléchargements et retrouvez vos fichiers."), content);
        intro->setObjectName(QStringLiteral("mutedLabel"));
        layout->addWidget(intro);

        auto *cardLayout = static_cast<QVBoxLayout *>(nullptr);
        auto *card = makeCard(content, tr("Historique des téléchargements"),
                              privateMode_
                                  ? tr("Les téléchargements ne sont pas conservés dans l’historique privé.")
                                  : tr("Les téléchargements terminés restent disponibles après la fermeture du navigateur."),
                              &cardLayout);
        auto *toolbar = new QHBoxLayout;
        auto *openFolder = makeActionButton(card, tr("Ouvrir le dossier"));
        auto *clearHistory = makeActionButton(card, tr("Effacer l’historique"));
        toolbar->addWidget(openFolder);
        toolbar->addWidget(clearHistory);
        toolbar->addStretch(1);
        cardLayout->addLayout(toolbar);
        connect(openFolder, &QPushButton::clicked, this, [] {
            const QString path = QStandardPaths::writableLocation(QStandardPaths::DownloadLocation);
            if (!QDesktopServices::openUrl(QUrl::fromLocalFile(path))) {
                qWarning("Could not open the downloads folder.");
            }
        });
        connect(clearHistory, &QPushButton::clicked, this, [this] {
            for (int index = downloads_.size() - 1; index >= 0; --index) {
                const QPointer<QWebEngineDownloadRequest> request = downloads_.at(index).request;
                if (!request
                    || request->state() != QWebEngineDownloadRequest::DownloadInProgress) {
                    downloads_.removeAt(index);
                }
            }
            saveDownloadHistory();
            refreshDownloadsPage();
        });
        downloadsTable_ = new QTableWidget(card);
        downloadsTable_->setObjectName(QStringLiteral("downloadsTable"));
        downloadsTable_->setColumnCount(4);
        downloadsTable_->setHorizontalHeaderLabels(
            {tr("Fichier"), tr("État"), tr("Progression"), tr("Actions")});
        downloadsTable_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
        downloadsTable_->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
        downloadsTable_->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
        downloadsTable_->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
        downloadsTable_->verticalHeader()->setVisible(false);
        downloadsTable_->setEditTriggers(QAbstractItemView::NoEditTriggers);
        downloadsTable_->setSelectionMode(QAbstractItemView::NoSelection);
        downloadsTable_->setMinimumHeight(260);
        cardLayout->addWidget(downloadsTable_);
        layout->addWidget(card);
        refreshDownloadsPage();
    } else if (pageKey == QStringLiteral("extensions")) {
        auto *intro = new QLabel(
            tr("Installez des scripts de contenu locaux. Ils s’exécutent uniquement sur les sites déclarés "
               "dans leur manifeste ; n’installez que des extensions auxquelles vous faites confiance."),
            content);
        intro->setObjectName(QStringLiteral("mutedLabel"));
        intro->setWordWrap(true);
        layout->addWidget(intro);

        auto *cardLayout = static_cast<QVBoxLayout *>(nullptr);
        auto *card = makeCard(content, tr("Extensions"),
                              tr("Une extension WebAD contient un fichier manifest.json et un script JavaScript local."),
                              &cardLayout);
        auto *install = makeActionButton(card, tr("Installer depuis un dossier…"));
        install->setEnabled(!privateMode_);
        cardLayout->addWidget(install, 0, Qt::AlignLeft);
        connect(install, &QPushButton::clicked, this, [this] {
            const QString source = QFileDialog::getExistingDirectory(
                this, tr("Choisir le dossier de l’extension"));
            if (!source.isEmpty()) {
                if (QMessageBox::question(
                        this, tr("Faire confiance à cette extension ?"),
                        tr("Les scripts de l’extension pourront modifier le contenu des pages "
                           "des sites déclarés. Installez uniquement du code de confiance."),
                        QMessageBox::Yes | QMessageBox::Cancel, QMessageBox::Cancel)
                    == QMessageBox::Yes) {
                    installExtension(source);
                }
            }
        });
        extensionsList_ = new QWidget(card);
        extensionsList_->setObjectName(QStringLiteral("extensionsList"));
        extensionsList_->setLayout(new QVBoxLayout);
        extensionsList_->layout()->setContentsMargins(0, 0, 0, 0);
        cardLayout->addWidget(extensionsList_);
        refreshExtensions();
        auto *folderLabel = new QLabel(tr("Dossier des extensions : %1").arg(extensionPath_), card);
        folderLabel->setObjectName(QStringLiteral("mutedLabel"));
        folderLabel->setWordWrap(true);
        cardLayout->addWidget(folderLabel);
        auto *openFolder = makeActionButton(card, tr("Ouvrir le dossier"));
        connect(openFolder, &QPushButton::clicked, this, [this] {
            if (!QDir().mkpath(extensionPath_)
                || !QDesktopServices::openUrl(QUrl::fromLocalFile(extensionPath_))) {
                QMessageBox::warning(this, tr("Extensions"),
                                     tr("Impossible d’ouvrir le dossier des extensions."));
            }
        });
        cardLayout->addWidget(openFolder, 0, Qt::AlignLeft);
        layout->addWidget(card);
    } else {
        auto *intro = new QLabel(tr("Personnalisez WebAD comme vous le souhaitez."), content);
        intro->setObjectName(QStringLiteral("mutedLabel"));
        layout->addWidget(intro);

        auto *appearanceLayout = static_cast<QVBoxLayout *>(nullptr);
        auto *appearance = makeCard(content, tr("Apparence"),
                                    tr("Choisissez un thème et les éléments visibles dans la barre de favoris."),
                                    &appearanceLayout);
        auto *appearanceForm = new QFormLayout;
        auto *themeCombo = new QComboBox(appearance);
        themeCombo->addItems({tr("Système"), tr("Clair"), tr("Sombre")});
        const QStringList themeKeys = {QStringLiteral("System"), QStringLiteral("Light"),
                                       QStringLiteral("Dark")};
        themeCombo->setCurrentIndex(qMax(0, themeKeys.indexOf(theme_)));
        appearanceForm->addRow(tr("Thème"), themeCombo);
        appearanceLayout->addLayout(appearanceForm);
        auto *showBookmarks = new QCheckBox(tr("Afficher la barre de favoris"), appearance);
        showBookmarks->setChecked(bookmarksBarVisible_);
        appearanceLayout->addWidget(showBookmarks);
        connect(themeCombo, qOverload<int>(&QComboBox::currentIndexChanged),
                this, [this, themeKeys](int index) {
            theme_ = themeKeys.value(index, QStringLiteral("System"));
            QSettings().setValue(QStringLiteral("theme"), theme_);
            applyTheme();
            refreshHomePages();
        });
        connect(showBookmarks, &QCheckBox::toggled, this, [this](bool visible) {
            bookmarksBarVisible_ = visible;
            bookmarksBar_->setVisible(visible);
            QSettings().setValue(QStringLiteral("showBookmarksBar"), visible);
        });
        layout->addWidget(appearance);

        auto *searchLayout = static_cast<QVBoxLayout *>(nullptr);
        auto *search = makeCard(content, tr("Recherche"),
                                tr("Le moteur choisi est utilisé pour les recherches depuis la barre d’adresse."),
                                &searchLayout);
        auto *searchForm = new QFormLayout;
        auto *searchCombo = new QComboBox(search);
        searchCombo->addItems({QStringLiteral("DuckDuckGo"), QStringLiteral("Google"),
                               QStringLiteral("Bing")});
        const int searchIndex = searchCombo->findText(searchEngine_);
        searchCombo->setCurrentIndex(searchIndex < 0 ? 0 : searchIndex);
        searchForm->addRow(tr("Moteur de recherche"), searchCombo);
        searchLayout->addLayout(searchForm);
        connect(searchCombo, &QComboBox::currentTextChanged, this, [this](const QString &engine) {
            searchEngine_ = engine;
            QSettings().setValue(QStringLiteral("searchEngine"), searchEngine_);
            refreshHomePages();
        });
        layout->addWidget(search);

        auto *privacyLayout = static_cast<QVBoxLayout *>(nullptr);
        auto *privacy = makeCard(content, tr("Confidentialité et données"),
                                 tr("Les cookies et le stockage local sont conservés dans le profil normal de WebAD. "
                                    "Une fenêtre privée utilise un profil temporaire distinct."),
                                 &privacyLayout);
        auto *javascript = new QCheckBox(tr("Autoriser JavaScript sur les pages web"), privacy);
        javascript->setChecked(
            QSettings().value(QStringLiteral("enableJavaScript"), true).toBool());
        privacyLayout->addWidget(javascript);
        connect(javascript, &QCheckBox::toggled, this, [this](bool enabled) {
            QSettings().setValue(QStringLiteral("enableJavaScript"), enabled);
            for (int index = 0; index < pages_->count(); ++index) {
                if (auto *view = qobject_cast<QWebEngineView *>(pages_->widget(index))) {
                    view->settings()->setAttribute(QWebEngineSettings::JavascriptEnabled, enabled);
                }
            }
        });
        auto *localStorage = new QCheckBox(
            tr("Autoriser les sites à enregistrer des données localement"), privacy);
        localStorage->setChecked(
            QSettings().value(QStringLiteral("enableLocalStorage"), true).toBool());
        privacyLayout->addWidget(localStorage);
        connect(localStorage, &QCheckBox::toggled, this, [this](bool enabled) {
            QSettings().setValue(QStringLiteral("enableLocalStorage"), enabled);
            for (int index = 0; index < pages_->count(); ++index) {
                if (auto *view = qobject_cast<QWebEngineView *>(pages_->widget(index))) {
                    view->settings()->setAttribute(QWebEngineSettings::LocalStorageEnabled, enabled);
                }
            }
        });
        auto *thirdPartyCookies = new QCheckBox(
            tr("Bloquer les cookies tiers"), privacy);
        thirdPartyCookies->setChecked(blockThirdPartyCookies_);
        privacyLayout->addWidget(thirdPartyCookies);
        connect(thirdPartyCookies, &QCheckBox::toggled, this, [this](bool blocked) {
            blockThirdPartyCookies_ = blocked;
            QSettings().setValue(QStringLiteral("blockThirdPartyCookies"), blocked);
            profile_->cookieStore()->setCookieFilter(
                [blocked](const QWebEngineCookieStore::FilterRequest &request) {
                    return !blocked || !request.thirdParty;
                });
        });
        auto *clearCookies = makeActionButton(privacy, tr("Effacer tous les cookies"));
        privacyLayout->addWidget(clearCookies, 0, Qt::AlignLeft);
        connect(clearCookies, &QPushButton::clicked, this, [this] {
            profile_->cookieStore()->deleteAllCookies();
            statusBar()->showMessage(tr("Les cookies de ce profil ont été supprimés."), 4000);
        });
        auto *clearCache = makeActionButton(privacy, tr("Effacer le cache de navigation"));
        privacyLayout->addWidget(clearCache, 0, Qt::AlignLeft);
        connect(clearCache, &QPushButton::clicked, this, [this] {
            profile_->clearHttpCache();
            statusBar()->showMessage(tr("Cache de navigation effacé."), 4000);
        });
        layout->addWidget(privacy);

        auto *downloadsLayout = static_cast<QVBoxLayout *>(nullptr);
        auto *downloadsCard = makeCard(content, tr("Téléchargements"),
                                       tr("Les fichiers téléchargés sont enregistrés dans ce dossier."),
                                       &downloadsLayout);
        auto *downloadFolderRow = new QHBoxLayout;
        auto *downloadFolder = new QLineEdit(
            QStandardPaths::writableLocation(QStandardPaths::DownloadLocation), downloadsCard);
        downloadFolder->setReadOnly(true);
        auto *chooseFolder = makeActionButton(downloadsCard, tr("Choisir…"));
        auto *openFolder = makeActionButton(downloadsCard, tr("Ouvrir"));
        downloadFolderRow->addWidget(downloadFolder, 1);
        downloadFolderRow->addWidget(chooseFolder);
        downloadFolderRow->addWidget(openFolder);
        downloadsLayout->addLayout(downloadFolderRow);
        connect(chooseFolder, &QPushButton::clicked, this, [this, downloadFolder] {
            const QString selectedFolder = QFileDialog::getExistingDirectory(
                this, tr("Choisir le dossier de téléchargements"), downloadFolder->text());
            if (!selectedFolder.isEmpty()) {
                downloadFolder->setText(selectedFolder);
                QSettings().setValue(QStringLiteral("downloadFolder"), selectedFolder);
            }
        });
        connect(openFolder, &QPushButton::clicked, this, [downloadFolder] {
            QDesktopServices::openUrl(QUrl::fromLocalFile(downloadFolder->text()));
        });
        layout->addWidget(downloadsCard);

        auto *shortcutsLayout = static_cast<QVBoxLayout *>(nullptr);
        auto *shortcutsCard = makeCard(
            content, tr("Raccourcis clavier"),
            tr("Cliquez sur un champ puis saisissez la combinaison souhaitée. "
               "Une combinaison ne peut être attribuée qu’à une seule action."),
            &shortcutsLayout);
        auto *shortcutForm = new QFormLayout;
        const QStringList shortcutIds = {
            QStringLiteral("focusAddress"), QStringLiteral("newTab"),
            QStringLiteral("reopenClosedTab"), QStringLiteral("closeTab"),
            QStringLiteral("nextTab"), QStringLiteral("previousTab"),
            QStringLiteral("tab1"), QStringLiteral("tab2"), QStringLiteral("tab3"),
            QStringLiteral("tab4"), QStringLiteral("tab5"), QStringLiteral("tab6"),
            QStringLiteral("tab7"), QStringLiteral("tab8"), QStringLiteral("lastTab"),
            QStringLiteral("privateWindow"), QStringLiteral("privateWindowAlternate"),
            QStringLiteral("openFile"), QStringLiteral("bookmarks"),
            QStringLiteral("history"), QStringLiteral("downloads"),
            QStringLiteral("settings"), QStringLiteral("reload"),
            QStringLiteral("reloadBypassCache"), QStringLiteral("stopLoading"),
            QStringLiteral("home"), QStringLiteral("focusSearch"),
            QStringLiteral("back"), QStringLiteral("forward"),
            QStringLiteral("addBookmark"), QStringLiteral("developerTools"),
            QStringLiteral("developerToolsF12"),
            QStringLiteral("find")
        };
        const QStringList shortcutLabels = {
            tr("Barre d’adresse"), tr("Nouvel onglet"), tr("Rouvrir l’onglet fermé"),
            tr("Fermer l’onglet"), tr("Onglet suivant"), tr("Onglet précédent"),
            tr("Aller à l’onglet 1"), tr("Aller à l’onglet 2"),
            tr("Aller à l’onglet 3"), tr("Aller à l’onglet 4"),
            tr("Aller à l’onglet 5"), tr("Aller à l’onglet 6"),
            tr("Aller à l’onglet 7"), tr("Aller à l’onglet 8"),
            tr("Dernier onglet"), tr("Fenêtre privée"),
            tr("Nouvelle fenêtre privée (alternative)"),
            tr("Ouvrir un fichier"), tr("Favoris"),
            tr("Historique"), tr("Téléchargements"), tr("Paramètres"),
            tr("Actualiser"), tr("Actualiser sans cache"), tr("Arrêter le chargement"),
            tr("Page d’accueil"), tr("Rechercher depuis la barre d’adresse"),
            tr("Page précédente"), tr("Page suivante"),
            tr("Ajouter aux favoris"), tr("Inspecteur (Ctrl+Maj+I)"),
            tr("Afficher/masquer l’inspecteur"), tr("Rechercher dans la page")
        };
        for (qsizetype index = 0; index < shortcutIds.size(); ++index) {
            const QString id = shortcutIds.at(index);
            auto *editor = new QKeySequenceEdit(shortcutsCard);
            editor->setKeySequence(shortcuts_.value(id)->key());
            shortcutForm->addRow(shortcutLabels.at(index), editor);
            connect(editor, &QKeySequenceEdit::editingFinished, this, [this, editor, id] {
                const QKeySequence proposed = editor->keySequence();
                for (auto it = shortcuts_.cbegin(); it != shortcuts_.cend(); ++it) {
                    if (it.key() != id && !proposed.isEmpty() && it.value()->key() == proposed) {
                        QMessageBox::warning(
                            this, tr("Raccourci déjà utilisé"),
                            tr("Cette combinaison est déjà affectée à une autre action."));
                        editor->setKeySequence(shortcuts_.value(id)->key());
                        return;
                    }
                }
                shortcuts_.value(id)->setKey(proposed);
                QSettings().setValue(
                    QStringLiteral("shortcuts/") + id,
                    proposed.toString(QKeySequence::PortableText));
            });
        }
        shortcutsLayout->addLayout(shortcutForm);
        layout->addWidget(shortcutsCard);

#ifdef Q_OS_WIN
        auto *openWithLayout = static_cast<QVBoxLayout *>(nullptr);
        auto *openWithCard = makeCard(
            content, tr("Intégration Windows"),
            tr("Ajoutez WebAD à la liste des applications proposées par « Ouvrir avec » "
               "pour les fichiers HTML. Cela ne change pas l’application par défaut."),
            &openWithLayout);
        auto *registerOpenWithButton = makeActionButton(
            openWithCard, tr("Ajouter WebAD à « Ouvrir avec »"));
        openWithLayout->addWidget(registerOpenWithButton, 0, Qt::AlignLeft);
        connect(registerOpenWithButton, &QPushButton::clicked,
                this, &BrowserWindow::registerOpenWith);
        layout->addWidget(openWithCard);
#endif
    }

    layout->addStretch(1);
    scroll->setWidget(content);
    outerLayout->addWidget(scroll);
    return outer;
}

void BrowserWindow::closeTab(int index)
{
    if (index < 0 || index >= pages_->count()) {
        return;
    }
    QWidget *page = pages_->widget(index);
    QUrl closedUrl;
    if (auto *view = qobject_cast<QWebEngineView *>(page)) {
        closedUrl = view->url();
    } else {
        const QString pageKey = page->property("internalKey").toString();
        if (!pageKey.isEmpty()) {
            closedUrl = QUrl(internalPageUrl(pageKey));
        }
    }
    if (closedTabs_.size() == 20) {
        closedTabs_.removeFirst();
    }
    closedTabs_.append(closedUrl);
    pages_->removeWidget(page);
    tabBar_->removeTab(index);
    page->deleteLater();

    if (pages_->count() == 0) {
        createTab();
    } else {
        pages_->setCurrentIndex(tabBar_->currentIndex());
        updateNavigationState();
    }
}

QString BrowserWindow::searchUrl(const QString &query) const
{
    QUrl url(searchBaseUrl(searchEngine_));
    QUrlQuery urlQuery;
    urlQuery.addQueryItem(QStringLiteral("q"), query);
    url.setQuery(urlQuery);
    return url.toString();
}

void BrowserWindow::navigate(const QString &input)
{
    const QString text = input.trimmed();
    if (text.isEmpty()) {
        return;
    }

    const QUrl enteredUrl(text);
    if (enteredUrl.scheme().compare(QStringLiteral("webad"), Qt::CaseInsensitive) == 0) {
        const QString pageKey = internalPageKey(enteredUrl);
        if (pageKey.isEmpty()) {
            statusBar()->showMessage(
                tr("Page WebAD inconnue : %1").arg(text), 5000);
            openInternalPage(QStringLiteral("404"));
            return;
        }
        openInternalPage(pageKey);
        return;
    }

    QUrl target;
    const QFileInfo localFile(text);
    static const QRegularExpression windowsPathPattern(QStringLiteral("^[a-zA-Z]:[\\\\/]"));
    static const QRegularExpression schemePattern(QStringLiteral("^[a-zA-Z][a-zA-Z0-9+.-]*:"));
    static const QRegularExpression hostWithPortPattern(QStringLiteral("^[a-zA-Z0-9.-]+:\\d+"));
    if (localFile.exists()) {
        target = QUrl::fromLocalFile(localFile.absoluteFilePath());
    } else if (windowsPathPattern.match(text).hasMatch()) {
        statusBar()->showMessage(tr("Le fichier local est introuvable."), 5000);
        return;
    } else if (text.startsWith(QStringLiteral("file://"), Qt::CaseInsensitive)) {
        target = QUrl::fromUserInput(text);
    } else {
        const bool hasScheme = schemePattern.match(text).hasMatch()
            && !hostWithPortPattern.match(text).hasMatch();
        const bool looksLikeHost = text == QStringLiteral("localhost")
            || text.contains(QLatin1Char('.')) || text.startsWith(QLatin1Char('['));
        if (hasScheme || looksLikeHost) {
            target = QUrl::fromUserInput(hasScheme ? text : QStringLiteral("https://") + text);
        } else {
            target = QUrl(searchUrl(text));
        }
    }

    if (!target.isValid()) {
        statusBar()->showMessage(tr("Adresse ou chemin de fichier invalide."), 5000);
        return;
    }
    if (QWebEngineView *view = currentView()) {
        view->setUrl(target);
    } else {
        const int currentIndex = tabBar_->currentIndex();
        if (currentIndex >= 0
            && !pages_->widget(currentIndex)->property("internalKey").toString().isEmpty()) {
            QWidget *internalPage = pages_->widget(currentIndex);
            pages_->removeWidget(internalPage);
            tabBar_->removeTab(currentIndex);
            internalPage->deleteLater();
        }
        createTab(target);
    }
}

void BrowserWindow::openLocalFile()
{
    const QString path = QFileDialog::getOpenFileName(
        this, tr("Ouvrir un fichier local"),
        QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation),
        tr("Pages web et fichiers (*.html *.htm *.xhtml *.pdf *.txt *.png *.jpg *.jpeg *.svg);;"
           "Tous les fichiers (*.*)"));
    if (path.isEmpty()) {
        return;
    }
    createTab(QUrl::fromLocalFile(path));
}

void BrowserWindow::navigateHome()
{
    if (QWebEngineView *view = currentView()) {
        view->setProperty("webadHomePage", true);
        view->setHtml(homePageHtml(searchEngine_, bookmarks_, isDarkTheme(theme_)));
        addressBar_->clear();
    } else {
        const int currentIndex = tabBar_->currentIndex();
        if (currentIndex >= 0
            && !pages_->widget(currentIndex)->property("internalKey").toString().isEmpty()) {
            QWidget *internalPage = pages_->widget(currentIndex);
            pages_->removeWidget(internalPage);
            tabBar_->removeTab(currentIndex);
            internalPage->deleteLater();
        }
        createTab();
    }
}

void BrowserWindow::addBookmark()
{
    QWebEngineView *view = currentView();
    if (!view || view->url().isEmpty()
        || view->url().scheme() == QStringLiteral("about")
        || view->url().scheme() == QStringLiteral("data")) {
        updateBookmarkButton();
        return;
    }

    addBookmark(view->url());
    updateBookmarkButton();
}

void BrowserWindow::addBookmark(const QUrl &url)
{
    if (!url.isValid() || url.isEmpty()
        || url.scheme() == QStringLiteral("about")
        || url.scheme() == QStringLiteral("data")) {
        return;
    }

    const QString savedUrl = url.toString();
    if (!bookmarks_.contains(savedUrl)) {
        bookmarks_.append(savedUrl);
        if (!privateMode_) {
            QSettings().setValue(QStringLiteral("bookmarks"), bookmarks_);
        }
        refreshBookmarks();
        refreshBookmarksPage();
        refreshHomePages();
    }
}

void BrowserWindow::removeBookmark(const QUrl &url)
{
    bookmarks_.removeAll(url.toString());
    if (!privateMode_) {
        QSettings().setValue(QStringLiteral("bookmarks"), bookmarks_);
    }
    refreshBookmarks();
    refreshBookmarksPage();
    refreshHomePages();
    updateBookmarkButton();
}

void BrowserWindow::refreshBookmarks()
{
    auto *layout = qobject_cast<QHBoxLayout *>(bookmarksBar_->layout());
    if (!layout) {
        layout = new QHBoxLayout(bookmarksBar_);
        layout->setContentsMargins(2, 0, 2, 0);
        layout->setSpacing(5);
    }
    while (QLayoutItem *item = layout->takeAt(0)) {
        if (QWidget *widget = item->widget()) {
            widget->deleteLater();
        }
        delete item;
    }

    auto *label = makeActionButton(bookmarksBar_, tr("Favoris"));
    label->setObjectName(QStringLiteral("bookmarkShortcut"));
    label->setProperty("lineIcon", QStringLiteral("bookmark"));
    label->setFlat(true);
    connect(label, &QPushButton::clicked, this, [this] {
        openInternalPage(QStringLiteral("bookmarks"));
    });
    layout->addWidget(label);
    for (const QString &savedUrl : bookmarks_) {
        const QUrl url(savedUrl);
        if (!url.isValid()) {
            continue;
        }
        const QString title = url.host().isEmpty() ? url.fileName() : url.host();
        auto *button = makeActionButton(bookmarksBar_, title);
        button->setObjectName(QStringLiteral("bookmarkShortcut"));
        button->setFlat(true);
        button->setMaximumWidth(170);
        button->setToolTip(savedUrl);
        connect(button, &QPushButton::clicked, this, [this, url] {
            if (QWebEngineView *view = currentView()) {
                view->setUrl(url);
            } else {
                createTab(url);
            }
        });
        layout->addWidget(button);
    }
    layout->addItem(new QSpacerItem(0, 0, QSizePolicy::Expanding, QSizePolicy::Minimum));
}

void BrowserWindow::refreshBookmarksPage()
{
    if (!bookmarksTable_) {
        return;
    }
    bookmarksTable_->setRowCount(bookmarks_.size());
    for (int row = 0; row < bookmarks_.size(); ++row) {
        const QUrl url(bookmarks_.at(row));
        const QString name = url.host().isEmpty() ? url.fileName() : url.host();
        auto *nameItem = new QTableWidgetItem(name);
        auto *urlItem = new QTableWidgetItem(url.toDisplayString());
        urlItem->setToolTip(url.toString());
        bookmarksTable_->setItem(row, 0, nameItem);
        bookmarksTable_->setItem(row, 1, urlItem);

        auto *actions = new QWidget(bookmarksTable_);
        auto *actionLayout = new QHBoxLayout(actions);
        actionLayout->setContentsMargins(4, 2, 4, 2);
        auto *open = makeActionButton(actions, tr("Ouvrir"));
        auto *remove = makeActionButton(actions, tr("Retirer"));
        actionLayout->addWidget(open);
        actionLayout->addWidget(remove);
        connect(open, &QPushButton::clicked, this, [this, url] {
            if (QWebEngineView *view = currentView()) {
                view->setUrl(url);
            } else {
                createTab(url);
            }
        });
        connect(remove, &QPushButton::clicked, this, [this, url] {
            removeBookmark(url);
        });
        bookmarksTable_->setCellWidget(row, 2, actions);
        bookmarksTable_->setRowHeight(row, 46);
    }
}

void BrowserWindow::recordHistory(QWebEngineView *view, const QUrl &url)
{
    if (privateMode_ || !view || (url.scheme() != QStringLiteral("http")
                  && url.scheme() != QStringLiteral("https")
                  && url.scheme() != QStringLiteral("file"))) {
        return;
    }

    const QString urlString = url.toString();
    int existingIndex = -1;
    for (int index = 0; index < history_.size(); ++index) {
        if (history_.at(index).url == urlString) {
            existingIndex = index;
            break;
        }
    }

    HistoryEntry entry;
    entry.url = urlString;
    entry.title = view->title().trimmed();
    if (entry.title.isEmpty()) {
        entry.title = url.host();
    }
    entry.lastVisited = QDateTime::currentDateTime();
    if (existingIndex >= 0) {
        history_.removeAt(existingIndex);
    }
    history_.prepend(entry);
    constexpr int maxHistoryEntries = 500;
    if (history_.size() > maxHistoryEntries) {
        history_.resize(maxHistoryEntries);
    }

    saveHistory();
    refreshHistoryPage();
}

void BrowserWindow::removeHistoryEntry(const QString &url)
{
    for (int index = history_.size() - 1; index >= 0; --index) {
        if (history_.at(index).url == url) {
            history_.removeAt(index);
        }
    }
    saveHistory();
    refreshHistoryPage();
}

void BrowserWindow::saveHistory()
{
    if (privateMode_) {
        return;
    }

    QVariantList savedHistory;
    for (const HistoryEntry &entry : history_) {
        QVariantMap savedItem;
        savedItem.insert(QStringLiteral("title"), entry.title);
        savedItem.insert(QStringLiteral("url"), entry.url);
        savedItem.insert(QStringLiteral("lastVisited"), entry.lastVisited);
        savedHistory.append(savedItem);
    }
    QSettings().setValue(QStringLiteral("history"), savedHistory);
}

void BrowserWindow::refreshHistoryPage()
{
    if (!historyTable_) {
        return;
    }
    historyTable_->setRowCount(history_.size());
    for (int row = 0; row < history_.size(); ++row) {
        const HistoryEntry entry = history_.at(row);
        const QUrl url(entry.url);
        auto *titleItem = new QTableWidgetItem(
            entry.title.isEmpty() ? url.host() : entry.title);
        auto *urlItem = new QTableWidgetItem(url.toDisplayString());
        urlItem->setToolTip(entry.url);
        auto *timeItem = new QTableWidgetItem(
            entry.lastVisited.toString(QStringLiteral("dd/MM/yyyy HH:mm")));
        historyTable_->setItem(row, 0, titleItem);
        historyTable_->setItem(row, 1, urlItem);
        historyTable_->setItem(row, 2, timeItem);
        historyTable_->setRowHeight(row, 42);
    }
}

void BrowserWindow::refreshDownloadsPage()
{
    if (!downloadsTable_) {
        return;
    }
    downloadsTable_->setRowCount(downloads_.size());
    for (int row = 0; row < downloads_.size(); ++row) {
        updateDownloadRow(row);
    }
}

void BrowserWindow::updateDownloadRow(int index)
{
    if (!downloadsTable_ || index < 0 || index >= downloads_.size()) {
        return;
    }
    const DownloadEntry &entry = downloads_.at(index);
    const auto setCellText = [this, index](int column, const QString &text) {
        QTableWidgetItem *item = downloadsTable_->item(index, column);
        if (item) {
            item->setText(text);
        } else {
            downloadsTable_->setItem(index, column, new QTableWidgetItem(text));
        }
    };
    setCellText(0, entry.fileName);
    setCellText(1, downloadStateLabel(entry.request, entry.state));

    auto *progress = qobject_cast<QProgressBar *>(downloadsTable_->cellWidget(index, 2));
    if (!progress) {
        progress = new QProgressBar(downloadsTable_);
        progress->setTextVisible(true);
        progress->setMinimumWidth(130);
        downloadsTable_->setCellWidget(index, 2, progress);
    }
    if (entry.totalBytes > 0) {
        progress->setRange(0, 100);
        progress->setValue(qBound(
            0, static_cast<int>(100.0 * entry.receivedBytes / entry.totalBytes), 100));
        progress->setFormat(QStringLiteral("%p%"));
    } else if (entry.request) {
        progress->setRange(0, 0);
        progress->setFormat(tr("En cours"));
    } else {
        progress->setRange(0, 100);
        progress->setValue(entry.state == tr("Termin\u00e9") ? 100 : 0);
        progress->setFormat(entry.state == tr("Termin\u00e9") ? tr("Terminé") : QStringLiteral("—"));
    }

    const bool isInProgress = entry.request
        && entry.request->state() == QWebEngineDownloadRequest::DownloadInProgress;
    const bool canOpenFile = entry.state == tr("Termin\u00e9") && QFileInfo::exists(entry.path);
    const QString actionState = isInProgress
        ? QStringLiteral("in-progress")
        : (canOpenFile ? QStringLiteral("completed")
                       : (!entry.path.isEmpty() ? QStringLiteral("history") : QStringLiteral("none")));
    QWidget *actions = downloadsTable_->cellWidget(index, 3);
    if (!actions || actions->property("actionState").toString() != actionState) {
        actions = new QWidget(downloadsTable_);
        actions->setProperty("actionState", actionState);
        auto *actionLayout = new QHBoxLayout(actions);
        actionLayout->setContentsMargins(4, 2, 4, 2);
        if (isInProgress) {
            auto *cancel = makeActionButton(actions, tr("Annuler"));
            QPointer<QWebEngineDownloadRequest> request = entry.request;
            connect(cancel, &QPushButton::clicked, this, [request] {
                if (request) {
                    request->cancel();
                }
            });
            actionLayout->addWidget(cancel);
        } else if (canOpenFile) {
            auto *open = makeActionButton(actions, tr("Ouvrir"));
            const QString path = entry.path;
            connect(open, &QPushButton::clicked, this, [this, path] {
                if (!QDesktopServices::openUrl(QUrl::fromLocalFile(path))) {
                    statusBar()->showMessage(tr("Impossible d’ouvrir ce fichier."), 5000);
                }
            });
            actionLayout->addWidget(open);
        }
        if (!entry.path.isEmpty()) {
            auto *folder = makeActionButton(actions, tr("Dossier"));
            const QString path = QFileInfo(entry.path).absolutePath();
            connect(folder, &QPushButton::clicked, this, [path] {
                QDesktopServices::openUrl(QUrl::fromLocalFile(path));
            });
            actionLayout->addWidget(folder);
        }
        downloadsTable_->setCellWidget(index, 3, actions);
    }
    downloadsTable_->setRowHeight(index, 48);
}

void BrowserWindow::saveDownloadHistory()
{
    if (privateMode_) {
        return;
    }

    QVariantList savedDownloads;
    for (const DownloadEntry &entry : downloads_) {
        QVariantMap savedEntry;
        savedEntry.insert(QStringLiteral("fileName"), entry.fileName);
        savedEntry.insert(QStringLiteral("path"), entry.path);
        savedEntry.insert(QStringLiteral("state"), downloadStateLabel(entry.request, entry.state));
        savedEntry.insert(QStringLiteral("receivedBytes"), entry.receivedBytes);
        savedEntry.insert(QStringLiteral("totalBytes"), entry.totalBytes);
        savedDownloads.append(savedEntry);
    }
    QSettings().setValue(QStringLiteral("downloads/history"), savedDownloads);
}

void BrowserWindow::handleDownload(QWebEngineDownloadRequest *download)
{
    const QString defaultFolder = QSettings().value(
        QStringLiteral("downloadFolder"),
        QStandardPaths::writableLocation(QStandardPaths::DownloadLocation)).toString();
    const QString suggestedPath = QDir(defaultFolder).filePath(download->downloadFileName());
    const QString destination = QFileDialog::getSaveFileName(
        this, tr("Enregistrer le téléchargement"), suggestedPath);
    if (destination.isEmpty()) {
        download->cancel();
        return;
    }

    const QFileInfo fileInfo(destination);
    download->setDownloadDirectory(fileInfo.absolutePath());
    download->setDownloadFileName(fileInfo.fileName());
    download->accept();

    DownloadEntry entry;
    entry.fileName = fileInfo.fileName();
    entry.path = fileInfo.absoluteFilePath();
    entry.state = tr("En cours");
    entry.totalBytes = download->totalBytes();
    entry.request = download;
    downloads_.append(entry);
    const int row = downloads_.size() - 1;
    refreshDownloadsPage();

    const auto updateProgress = [this, download] {
        for (int index = 0; index < downloads_.size(); ++index) {
            if (downloads_.at(index).request == download) {
                downloads_[index].receivedBytes = download->receivedBytes();
                downloads_[index].totalBytes = download->totalBytes();
                updateDownloadRow(index);
                return;
            }
        }
    };
    connect(download, &QWebEngineDownloadRequest::receivedBytesChanged, this, updateProgress);
    connect(download, &QWebEngineDownloadRequest::totalBytesChanged, this, updateProgress);
    connect(download, &QWebEngineDownloadRequest::stateChanged, this,
            [this, download](QWebEngineDownloadRequest::DownloadState state) {
        for (int index = 0; index < downloads_.size(); ++index) {
            DownloadEntry &item = downloads_[index];
            if (item.request != download) {
                continue;
            }
            item.receivedBytes = download->receivedBytes();
            item.totalBytes = download->totalBytes();
            switch (state) {
            case QWebEngineDownloadRequest::DownloadCompleted:
                item.state = tr("Terminé");
                break;
            case QWebEngineDownloadRequest::DownloadCancelled:
                item.state = tr("Annulé");
                break;
            case QWebEngineDownloadRequest::DownloadInterrupted:
                item.state = tr("Interrompu");
                break;
            case QWebEngineDownloadRequest::DownloadInProgress:
                item.state = tr("En cours");
                break;
            }
            updateDownloadRow(index);
            saveDownloadHistory();
            if (state == QWebEngineDownloadRequest::DownloadCompleted) {
                statusBar()->showMessage(tr("Téléchargement terminé : %1").arg(item.fileName), 5000);
            }
            return;
        }
    });
    updateDownloadRow(row);
    saveDownloadHistory();
    statusBar()->showMessage(tr("Téléchargement démarré : %1").arg(fileInfo.fileName()), 4000);
}

void BrowserWindow::updateNavigationState()
{
    QWebEngineView *view = currentView();
    const bool hasView = view != nullptr;
    const int currentIndex = tabBar_->currentIndex();
    QWidget *currentPage = currentIndex >= 0 ? pages_->widget(currentIndex) : nullptr;
    const QString pageKey = currentPage
        ? currentPage->property("internalKey").toString()
        : QString();

    backButton_->setEnabled(hasView && view->history()->canGoBack());
    forwardButton_->setEnabled(hasView && view->history()->canGoForward());
    reloadButton_->setEnabled(hasView);
    if (hasView) {
        const QString scheme = view->url().scheme();
        addressBar_->setText(scheme == QStringLiteral("about") || scheme == QStringLiteral("data")
                                 ? QString()
                                 : view->url().toDisplayString());
        const QString title = tabBar_->tabText(tabBar_->currentIndex());
        setWindowTitle(privateMode_
            ? tr("WebAD — Privée — %1").arg(title)
            : QStringLiteral("WebAD — %1").arg(title));
    } else if (!pageKey.isEmpty()) {
        addressBar_->setText(internalPageUrl(pageKey));
        setWindowTitle(privateMode_
            ? tr("WebAD — Privée — %1").arg(tabBar_->tabText(currentIndex))
            : QStringLiteral("WebAD — %1").arg(tabBar_->tabText(currentIndex)));
        progressBar_->hide();
    } else {
        addressBar_->clear();
        setWindowTitle(currentIndex >= 0
            ? (privateMode_
                ? tr("WebAD — Privée — %1").arg(tabBar_->tabText(currentIndex))
                : QStringLiteral("WebAD — %1").arg(tabBar_->tabText(currentIndex)))
            : (privateMode_ ? tr("WebAD — Navigation privée") : QStringLiteral("WebAD")));
    }
    updateBookmarkButton();
}

void BrowserWindow::updateBookmarkButton()
{
    QWebEngineView *view = currentView();
    const bool isBookmarkable = view && !view->url().isEmpty()
        && view->url().scheme() != QStringLiteral("about")
        && view->url().scheme() != QStringLiteral("data");
    const bool isBookmarked = isBookmarkable && bookmarks_.contains(view->url().toString());
    const QSignalBlocker blocker(bookmarkButton_);
    bookmarkButton_->setChecked(isBookmarked);
    bookmarkButton_->setProperty("bookmarked", isBookmarked);
    bookmarkButton_->setIcon(makeLineIcon(
        QStringLiteral("bookmark"), lineIconColor(theme_, isBookmarked), isBookmarked));
}

void BrowserWindow::refreshHomePages()
{
    for (int index = 0; index < pages_->count(); ++index) {
        auto *view = qobject_cast<QWebEngineView *>(pages_->widget(index));
        if (view && view->property("webadHomePage").toBool()) {
            view->setHtml(homePageHtml(searchEngine_, bookmarks_, isDarkTheme(theme_)));
        }
    }
}

void BrowserWindow::applyTheme()
{
    const bool systemDark = QApplication::palette().color(QPalette::Window).lightness() < 128;
    const bool dark = theme_ == QStringLiteral("Dark")
        || (theme_ == QStringLiteral("System") && systemDark);
    const QString background = dark ? QStringLiteral("#1b1822") : QStringLiteral("#f7f6fb");
    const QString foreground = dark ? QStringLiteral("#f4effa") : QStringLiteral("#302a42");
    const QString toolbarBackground = dark ? QStringLiteral("#25212d") : QStringLiteral("#f0edfa");
    const QString cardBackground = dark ? QStringLiteral("#292531") : QStringLiteral("#ffffff");
    const QString border = dark ? QStringLiteral("#403849") : QStringLiteral("#e7e2ef");
    const QString accent = dark ? QStringLiteral("#c1a3fb") : QStringLiteral("#7953dc");
    const QString secondaryText = dark ? QStringLiteral("#b5acbe") : QStringLiteral("#777184");
    const QString accentWash = dark ? QStringLiteral("#382b49") : QStringLiteral("#eee8fb");
    const QString navigationBackground = dark ? QStringLiteral("#211e29") : QStringLiteral("#faf9fd");
    const QString addressBorder = dark ? QStringLiteral("#625277") : QStringLiteral("#cfc2eb");

    setStyleSheet(QStringLiteral(
        "QMainWindow, QWidget#appRoot, QWidget#pageStack, QWidget#internalPageContent"
        " { background-color: %1; color: %2; }"
        "QWidget#tabsToolbar { background: qlineargradient(x1:0, y1:0, x2:1, y2:0,"
        " stop:0 %3, stop:1 %9); border-bottom: 1px solid %4; }"
        "QWidget#navigationBar { background-color: %10; }"
        "QWidget#bookmarksBar { background-color: %10; border-bottom: 1px solid %4; }"
        "QLabel#brandMark { background: transparent; border: 0; }"
        "QLabel#brandName { color: %2; font-size: 16px; font-weight: 750; letter-spacing: -0.3px; }"
        "QLabel#privateBadge { background: %8; color: %6; border-radius: 9px;"
        " padding: 5px 9px; font-size: 10px; font-weight: 750; letter-spacing: 0.6px; }"
        "QLabel#privacyNotice { background: %8; color: %2; border-bottom: 1px solid %4;"
        " padding: 9px 16px; font-size: 12px; }"
        "QFrame#toolbarDivider { color: %4; }"
        "QLabel#mutedLabel { color: %7; }"
        "QLabel#pageHeading { font-size: 28px; font-weight: 730; letter-spacing: -0.8px; }"
        "QLabel#cardHeading { font-size: 16px; font-weight: 650; }"
        "QLabel#emptyState { color: %7; font-size: 15px; padding: 24px; }"
        "QFrame#settingsCard { background-color: %5; border: 1px solid %4; border-radius: 20px; }"
        "QTabBar#tabBar::tab { background: transparent; color: %7; border: 1px solid transparent;"
        " border-radius: 13px; padding: 9px 15px; margin: 1px 2px 0; min-width: 84px; }"
        "QTabBar#tabBar::tab:selected { background: %10; color: %2; border-color: %4;"
        " border-bottom-color: %10; }"
        "QTabBar#tabBar::tab:hover:!selected { background: %8; color: %2; }"
        "QLineEdit#addressBar { background-color: %5; color: %2;"
        " border: 1px solid %11; border-radius: 20px; padding: 10px 18px; }"
        "QLineEdit#addressBar:focus { background-color: %5; border: 2px solid %6;"
        " padding: 9px 17px; }"
        "QLineEdit { background-color: %5; color: %2;"
        " border: 1px solid %4; border-radius: 14px; padding: 9px 13px; }"
        "QToolButton#iconButton { background-color: transparent; border: 0; border-radius: 13px;"
        " color: %7; font-size: 17px; }"
        "QToolButton#iconButton:hover { background-color: %8; color: %6; }"
        "QToolButton#iconButton:checked { background-color: %8; color: %6; }"
        "QToolButton#tabCloseButton { background: transparent; border: 0; border-radius: 8px; }"
        "QToolButton#tabCloseButton:hover { background: %8; }"
        "QMenu { background: %5; color: %2; border: 1px solid %4; border-radius: 14px; padding: 6px; }"
        "QMenu#webContextMenu { padding: 8px; }"
        "QMenu::item { border-radius: 9px; padding: 9px 30px 9px 13px; }"
        "QMenu::item:selected { background: %8; color: %6; }"
        "QMenu::separator { height: 1px; background: %4; margin: 5px 8px; }"
        "QPushButton { background-color: %5; color: %2; border: 1px solid %4;"
        " border-radius: 13px; padding: 9px 14px; }"
        "QPushButton:hover { background-color: %8; border-color: %6; }"
        "QPushButton#bookmarkShortcut { background: transparent; color: %7;"
        " border: 1px solid transparent; border-radius: 12px; padding: 6px 11px;"
        " text-align: left; }"
        "QPushButton#bookmarkShortcut:hover { background: %8; color: %6; }"
        "QComboBox, QCheckBox { color: %2; }"
        "QComboBox { background-color: %5; border: 1px solid %4; border-radius: 13px;"
        " padding: 8px 12px; min-width: 180px; }"
        "QComboBox QAbstractItemView { background-color: %5; color: %2; selection-background-color: %4; }"
        "QTableWidget { background-color: %5; alternate-background-color: %1; color: %2;"
        " gridline-color: %4; border: 1px solid %4; border-radius: 15px; }"
        "QTableWidget::item { padding: 7px 10px; }"
        "QTableWidget::item:selected { background: %8; color: %2; }"
        "QHeaderView::section { background-color: %1; color: %7; border: 0;"
        " padding: 11px; font-weight: 650; }"
        "QProgressBar#pageProgress { border: 0; background: %4; }"
        "QProgressBar#pageProgress::chunk { background: %6; border-radius: 1px; }"
        "QScrollArea { border: 0; background: transparent; }"
        "QStatusBar { background-color: %1; color: %7; border: 0; }")
        .arg(background, foreground, toolbarBackground, border, cardBackground, accent,
             secondaryText, accentWash, background, navigationBackground, addressBorder));

    for (QAbstractButton *button : findChildren<QAbstractButton *>()) {
        const QString iconName = button->property("lineIcon").toString();
        if (!iconName.isEmpty()) {
            const bool highlighted = button->property("bookmarked").toBool();
            button->setIcon(makeLineIcon(
                iconName, lineIconColor(theme_, highlighted), highlighted));
        }
    }

#if QT_VERSION >= QT_VERSION_CHECK(6, 7, 0)
    profile_->settings()->setAttribute(
        QWebEngineSettings::ForceDarkMode, dark);
#endif
}

void BrowserWindow::closeEvent(QCloseEvent *event)
{
    if (!privateMode_) {
        QSettings().setValue(QStringLiteral("windowGeometry"), saveGeometry());
    }
    saveDownloadHistory();
    event->accept();
}
