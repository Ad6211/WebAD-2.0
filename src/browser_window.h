#pragma once

#include <QMainWindow>
#include <QHash>
#include <QPointer>
#include <QStringList>
#include <QUrl>
#include <QVector>
#include <QDateTime>

class QCloseEvent;
class QLineEdit;
class QShortcut;
class QDockWidget;
class QWebEnginePage;
class QWebEngineProfile;
class QProgressBar;
class QTableWidget;
class QStackedWidget;
class QTabBar;
class QToolButton;
class QWidget;
class QWebEngineDownloadRequest;
class QWebEngineView;

class BrowserWindow final : public QMainWindow
{
    Q_OBJECT

public:
    explicit BrowserWindow(QWidget *parent = nullptr, bool privateMode = false,
                            const QUrl &initialUrl = QUrl());
    ~BrowserWindow() override;

protected:
    void closeEvent(QCloseEvent *event) override;

private:
    struct DownloadEntry
    {
        QString fileName;
        QString path;
        QString state;
        qint64 receivedBytes = 0;
        qint64 totalBytes = 0;
        QPointer<QWebEngineDownloadRequest> request;
    };

    struct HistoryEntry
    {
        QString title;
        QString url;
        QDateTime lastVisited;
    };

    struct ExtensionEntry
    {
        QString id;
        QString name;
        QString version;
        QString scriptPath;
        QStringList matches;
    };

    QWebEngineView *currentView() const;
    void createTab(const QUrl &url = QUrl());
    QWebEngineView *createWebViewTab(const QUrl &url, bool showHomePage);
    QWebEnginePage *createPopupPage();
    void openPrivateWindow();
    void openDeveloperTools();
    void refreshExtensions();
    void refreshExtensionScripts();
    void installExtension(const QString &sourcePath);
    void setExtensionEnabled(const QString &id, bool enabled);
    void registerOpenWith();
    void installTabCloseButton(int index);
    void openInternalPage(const QString &pageKey);
    QWidget *createInternalPage(const QString &pageKey);
    void closeTab(int index);
    void navigate(const QString &input);
    void openLocalFile();
    void navigateHome();
    void addBookmark();
    void addBookmark(const QUrl &url);
    void removeBookmark(const QUrl &url);
    void refreshBookmarks();
    void refreshBookmarksPage();
    void recordHistory(QWebEngineView *view, const QUrl &url);
    void removeHistoryEntry(const QString &url);
    void saveHistory();
    void refreshHistoryPage();
    void refreshDownloadsPage();
    void updateDownloadRow(int index);
    void saveDownloadHistory();
    void handleDownload(QWebEngineDownloadRequest *download);
    void updateNavigationState();
    void updateBookmarkButton();
    void refreshHomePages();
    void applyTheme();
    QString searchUrl(const QString &query) const;

    QTabBar *tabBar_ = nullptr;
    QStackedWidget *pages_ = nullptr;
    QPointer<QWebEngineProfile> profile_;
    QLineEdit *addressBar_ = nullptr;
    QProgressBar *progressBar_ = nullptr;
    QToolButton *backButton_ = nullptr;
    QToolButton *forwardButton_ = nullptr;
    QToolButton *reloadButton_ = nullptr;
    QToolButton *bookmarkButton_ = nullptr;
    QWidget *bookmarksBar_ = nullptr;
    QPointer<QTableWidget> bookmarksTable_;
    QPointer<QTableWidget> historyTable_;
    QPointer<QTableWidget> downloadsTable_;
    QPointer<QWidget> extensionsList_;
    QPointer<QDockWidget> devToolsDock_;
    QPointer<QWebEngineView> devToolsView_;
    QPointer<QWebEnginePage> inspectedPage_;
    QHash<QString, QShortcut *> shortcuts_;
    QVector<QUrl> closedTabs_;
    QStringList bookmarks_;
    QVector<HistoryEntry> history_;
    QVector<DownloadEntry> downloads_;
    QVector<ExtensionEntry> extensions_;
    QString extensionPath_;
    QString theme_;
    QString searchEngine_;
    bool privateMode_ = false;
    bool bookmarksBarVisible_ = true;
    bool blockThirdPartyCookies_ = false;
};
