#include <QApplication>
#include <QCommandLineParser>
#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QMessageBox>
#include <QProcess>
#include <QRegularExpression>
#include <QSettings>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QThread>
#include <QVersionNumber>

#ifdef Q_OS_WIN
#include <windows.h>
#else
#include <cerrno>
#include <csignal>
#include <unistd.h>
#endif

namespace
{
constexpr auto repositoryUrl = "https://github.com/Ad6211/WebAD-2.0.git";
constexpr auto repositoryBranch = "main";
constexpr qint64 checkIntervalSeconds = 3 * 24 * 60 * 60;
constexpr qint64 maxArtifactSize = 1024LL * 1024 * 1024;
constexpr int commandTimeoutMs = 5 * 60 * 1000;

struct CommandResult
{
    bool ok = false;
    QString output;
    QString error;
};

QString platformDirectory()
{
#ifdef Q_OS_WIN
    return QStringLiteral("win64");
#elif defined(Q_OS_LINUX)
    return QStringLiteral("deb64");
#else
    return {};
#endif
}

QString findGit()
{
    const QStringList candidates = {
        QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("git/cmd/git.exe")),
        QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("git/bin/git.exe")),
        QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("git.exe"))
    };
    for (const QString &candidate : candidates) {
        if (QFileInfo(candidate).isFile()) {
            return candidate;
        }
    }
#ifdef Q_OS_WIN
    return QStandardPaths::findExecutable(QStringLiteral("git.exe"));
#else
    return QStandardPaths::findExecutable(QStringLiteral("git"));
#endif
}

CommandResult runCommand(const QString &program, const QStringList &arguments,
                         int timeoutMs = commandTimeoutMs)
{
    QProcess process;
    process.setProgram(program);
    process.setArguments(arguments);
    process.setProcessChannelMode(QProcess::MergedChannels);
    process.start();
    if (!process.waitForStarted(10000)) {
        return {false, {}, QObject::tr("Impossible de démarrer %1 : %2")
                             .arg(program, process.errorString())};
    }
    if (!process.waitForFinished(timeoutMs)) {
        process.kill();
        process.waitForFinished(5000);
        return {false, QString::fromLocal8Bit(process.readAll()),
                QObject::tr("La commande %1 a dépassé le délai autorisé.").arg(program)};
    }

    const QString output = QString::fromLocal8Bit(process.readAll()).trimmed();
    if (process.exitStatus() != QProcess::NormalExit || process.exitCode() != 0) {
        return {false, output,
                QObject::tr("La commande %1 a échoué (code %2).")
                    .arg(program)
                    .arg(process.exitCode())};
    }
    return {true, output, {}};
}

bool parseVersion(const QString &text, QVersionNumber *version)
{
    static const QRegularExpression pattern(
        QStringLiteral("^(0|[1-9][0-9]*)\\.(0|[1-9][0-9]*)\\.(0|[1-9][0-9]*)$"));
    const QString value = text.trimmed();
    if (!pattern.match(value).hasMatch()) {
        return false;
    }
    *version = QVersionNumber::fromString(value);
    return !version->isNull();
}

bool sha256File(const QString &path, QByteArray *digest, QString *error);

bool runSelfTest()
{
    QVersionNumber older;
    QVersionNumber newer;
    if (!parseVersion(QStringLiteral("2.0.0"), &older)
        || !parseVersion(QStringLiteral("2.1.0"), &newer)
        || parseVersion(QStringLiteral("2.1"), &newer)
        || QVersionNumber::compare(newer, older) <= 0) {
        return false;
    }

    QTemporaryDir directory;
    if (!directory.isValid()) {
        return false;
    }
    const QString filePath = QDir(directory.path()).filePath(QStringLiteral("payload"));
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly) || file.write("WebAD updater test") < 0) {
        return false;
    }
    file.close();
    QByteArray digest;
    QString error;
    return sha256File(filePath, &digest, &error)
        && digest == QByteArrayLiteral(
                         "b52df194e2866d58b64ae82c5e8c48268646aa864874d676df55c04e3de4da49");
}

QString readLatestVersion(const QString &repositoryPath, QString *error)
{
    QFile file(QDir(repositoryPath).filePath(QStringLiteral("version.md")));
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        *error = QObject::tr("Le dépôt ne contient pas de version.md lisible.");
        return {};
    }
    const QString text = QString::fromUtf8(file.readAll()).trimmed();
    const QString versionText = text.section(QLatin1Char('\n'), 0, 0)
                                    .trimmed()
                                    .remove(QLatin1Char('\r'));
    QVersionNumber version;
    if (!parseVersion(versionText, &version)) {
        *error = QObject::tr("version.md doit contenir une version au format X.Y.Z.");
        return {};
    }
    return version.toString();
}

bool sha256File(const QString &path, QByteArray *digest, QString *error)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        *error = QObject::tr("Impossible de lire le fichier à vérifier : %1")
                     .arg(QFileInfo(path).fileName());
        return false;
    }
    QCryptographicHash hash(QCryptographicHash::Sha256);
    if (!hash.addData(&file)) {
        *error = QObject::tr("Impossible de calculer la somme SHA-256 du fichier.");
        return false;
    }
    *digest = hash.result().toHex();
    return true;
}

QString findReleaseArtifact(const QString &releasePath, const QString &version,
                            QString *error)
{
#ifdef Q_OS_WIN
    const QString expectedName =
        QStringLiteral("WebAD-%1-win64-setup.exe").arg(version);
#else
    const QString expectedName = QStringLiteral("webad_%1_amd64.deb").arg(version);
#endif
    const QString artifactPath = QDir(releasePath).filePath(expectedName);
    const QFileInfo artifact(artifactPath);
    if (!artifact.isFile() || artifact.isSymLink()
        || artifact.size() <= 0 || artifact.size() > maxArtifactSize) {
        *error = QObject::tr("La release %1 ne contient pas le paquet attendu : %2")
                     .arg(version, expectedName);
        return {};
    }

    QFile checksums(QDir(releasePath).filePath(QStringLiteral("SHA256SUMS")));
    if (!checksums.open(QIODevice::ReadOnly | QIODevice::Text)) {
        *error = QObject::tr("La release ne contient pas son fichier SHA256SUMS.");
        return {};
    }
    const QRegularExpression checksumPattern(
        QStringLiteral("^([a-fA-F0-9]{64})\\s+\\*?([^\\s]+)$"));
    QByteArray expectedHash;
    while (!checksums.atEnd()) {
        const QString line = QString::fromUtf8(checksums.readLine()).trimmed();
        const auto match = checksumPattern.match(line);
        if (match.hasMatch() && match.captured(2) == expectedName) {
            expectedHash = match.captured(1).toLatin1().toLower();
            break;
        }
    }
    if (expectedHash.isEmpty()) {
        *error = QObject::tr("SHA256SUMS ne contient pas la somme de %1.").arg(expectedName);
        return {};
    }

    QByteArray actualHash;
    if (!sha256File(artifactPath, &actualHash, error)) {
        return {};
    }
    if (actualHash != expectedHash) {
        *error = QObject::tr("La vérification SHA-256 de %1 a échoué.").arg(expectedName);
        return {};
    }
    return artifactPath;
}

bool waitForBrowserExit(qint64 processId, QString *error)
{
    if (processId <= 0) {
        return true;
    }
#ifdef Q_OS_WIN
    HANDLE process = OpenProcess(SYNCHRONIZE, FALSE, static_cast<DWORD>(processId));
    if (!process) {
        if (GetLastError() == ERROR_INVALID_PARAMETER) {
            return true;
        }
        *error = QObject::tr("Impossible d’attendre la fermeture de WebAD (erreur %1).")
                     .arg(GetLastError());
        return false;
    }
    const DWORD result = WaitForSingleObject(process, 5 * 60 * 1000);
    CloseHandle(process);
    if (result != WAIT_OBJECT_0) {
        *error = result == WAIT_TIMEOUT
            ? QObject::tr("WebAD est resté ouvert. Fermez-le puis relancez la mise à jour.")
            : QObject::tr("Impossible de vérifier la fermeture de WebAD.");
        return false;
    }
    return true;
#else
    for (int attempt = 0; attempt < 600; ++attempt) {
        if (kill(static_cast<pid_t>(processId), 0) != 0 && errno == ESRCH) {
            return true;
        }
        QThread::msleep(500);
    }
    *error = QObject::tr("WebAD est resté ouvert. Fermez-le puis relancez la mise à jour.");
    return false;
#endif
}

bool installArtifact(const QString &artifactPath, QString *error)
{
#ifdef Q_OS_WIN
    const QString program = artifactPath;
    const QStringList arguments;
#else
    const QString program = QStandardPaths::findExecutable(QStringLiteral("pkexec"));
    if (program.isEmpty()) {
        *error = QObject::tr("pkexec est requis pour installer le paquet Debian. "
                             "Installez PolicyKit puis relancez WebAD.");
        return false;
    }
    const QStringList arguments = {
        QStringLiteral("dpkg"), QStringLiteral("-i"), artifactPath
    };
#endif
    QProcess installer;
    installer.setProgram(program);
    installer.setArguments(arguments);
    installer.start();
    if (!installer.waitForStarted(15000)) {
        *error = QObject::tr("Impossible de démarrer l’installateur : %1")
                     .arg(installer.errorString());
        return false;
    }
    if (!installer.waitForFinished(-1)) {
        *error = QObject::tr("L’installateur n’a pas pu terminer : %1")
                     .arg(installer.errorString());
        return false;
    }
    if (installer.exitStatus() != QProcess::NormalExit || installer.exitCode() != 0) {
        *error = QObject::tr("L’installation a échoué (code %1).")
                     .arg(installer.exitCode());
        return false;
    }
    return true;
}

int runUpdater(const QString &installedVersionText, qint64 parentPid, bool forceCheck)
{
    QVersionNumber installedVersion;
    if (!parseVersion(installedVersionText, &installedVersion)) {
        QMessageBox::critical(nullptr, QObject::tr("WebAD — Mise à jour"),
                              QObject::tr("La version installée est invalide."));
        return 1;
    }
    if (platformDirectory().isEmpty()) {
        QMessageBox::warning(nullptr, QObject::tr("WebAD — Mise à jour"),
                             QObject::tr("Les mises à jour ne sont pas disponibles sur ce système."));
        return 1;
    }

    QSettings settings;
    const QDateTime now = QDateTime::currentDateTimeUtc();
    const QDateTime lastCheck =
        settings.value(QStringLiteral("updates/lastCheckUtc")).toDateTime();
    const qint64 secondsSinceLastCheck = lastCheck.secsTo(now);
    if (!forceCheck && lastCheck.isValid() && secondsSinceLastCheck >= 0
        && secondsSinceLastCheck < checkIntervalSeconds) {
        return 0;
    }
    settings.setValue(QStringLiteral("updates/lastCheckUtc"), now);

    const QString git = findGit();
    if (git.isEmpty()) {
        QMessageBox::warning(nullptr, QObject::tr("WebAD — Mise à jour"),
                             QObject::tr("Git est introuvable. Installez Git ou placez une "
                                         "version portable à côté de WebADUpdater."));
        return 1;
    }

    QTemporaryDir temporaryDirectory;
    if (!temporaryDirectory.isValid()) {
        QMessageBox::warning(nullptr, QObject::tr("WebAD — Mise à jour"),
                             QObject::tr("Impossible de créer un dossier temporaire."));
        return 1;
    }
    const QString repositoryPath =
        QDir(temporaryDirectory.path()).filePath(QStringLiteral("repository"));
    const CommandResult clone = runCommand(
        git, {QStringLiteral("clone"), QStringLiteral("--depth=1"),
              QStringLiteral("--filter=blob:none"), QStringLiteral("--sparse"),
              QStringLiteral("--branch"), QString::fromLatin1(repositoryBranch),
              QString::fromLatin1(repositoryUrl), repositoryPath});
    if (!clone.ok) {
        QMessageBox::warning(nullptr, QObject::tr("WebAD — Mise à jour"),
                             QObject::tr("Impossible de consulter le dépôt GitHub.\n%1")
                                 .arg(clone.output.isEmpty() ? clone.error : clone.output));
        return 1;
    }

    QString error;
    const QString latestVersionText = readLatestVersion(repositoryPath, &error);
    if (latestVersionText.isEmpty()) {
        QMessageBox::warning(nullptr, QObject::tr("WebAD — Mise à jour"), error);
        return 1;
    }
    QVersionNumber latestVersion;
    if (!parseVersion(latestVersionText, &latestVersion)) {
        QMessageBox::warning(nullptr, QObject::tr("WebAD — Mise à jour"),
                             QObject::tr("La version distante n’est pas valide."));
        return 1;
    }
    if (QVersionNumber::compare(latestVersion, installedVersion) <= 0) {
        return 0;
    }

    const QString releasePath = QStringLiteral("releases/%1/%2")
                                    .arg(latestVersionText, platformDirectory());
    const CommandResult sparseCheckout = runCommand(
        git, {QStringLiteral("-C"), repositoryPath,
              QStringLiteral("sparse-checkout"), QStringLiteral("set"),
              QStringLiteral("version.md"), releasePath});
    if (!sparseCheckout.ok) {
        QMessageBox::warning(nullptr, QObject::tr("WebAD — Mise à jour"),
                             QObject::tr("Impossible de récupérer les fichiers de la release.\n%1")
                                 .arg(sparseCheckout.output.isEmpty()
                                          ? sparseCheckout.error
                                          : sparseCheckout.output));
        return 1;
    }

    const QString absoluteReleasePath = QDir(repositoryPath).filePath(releasePath);
    const QString artifactPath =
        findReleaseArtifact(absoluteReleasePath, latestVersionText, &error);
    if (artifactPath.isEmpty()) {
        QMessageBox::warning(nullptr, QObject::tr("WebAD — Mise à jour"), error);
        return 1;
    }

#ifdef Q_OS_WIN
    const QString action = QObject::tr("lancer l’installateur Windows");
#else
    const QString action = QObject::tr("installer le paquet Debian (une autorisation "
                                       "administrateur sera demandée)");
#endif
    const auto answer = QMessageBox::question(
        nullptr, QObject::tr("Mise à jour WebAD disponible"),
        QObject::tr("La version %1 est disponible (version installée : %2).\n"
                    "Le paquet a été vérifié par SHA-256. Voulez-vous %3 ?")
            .arg(latestVersionText, installedVersionText, action),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes);
    if (answer != QMessageBox::Yes) {
        return 0;
    }

    QMessageBox::information(
        nullptr, QObject::tr("Mise à jour WebAD"),
        QObject::tr("Fermez WebAD pour continuer. La mise à jour démarrera après la fermeture."));
    if (!waitForBrowserExit(parentPid, &error)) {
        QMessageBox::warning(nullptr, QObject::tr("WebAD — Mise à jour"), error);
        return 1;
    }

    if (!installArtifact(artifactPath, &error)) {
        QMessageBox::critical(nullptr, QObject::tr("Échec de la mise à jour"), error);
        return 1;
    }
    QMessageBox::information(nullptr, QObject::tr("WebAD mis à jour"),
                             QObject::tr("WebAD %1 a été installé.").arg(latestVersionText));
    return 0;
}
} // namespace

int main(int argc, char *argv[])
{
    QApplication application(argc, argv);
    QCoreApplication::setOrganizationName(QStringLiteral("WebAD"));
    QCoreApplication::setApplicationName(QStringLiteral("WebAD"));
    QCoreApplication::setApplicationVersion(QStringLiteral(WEBAD_VERSION));

    QCommandLineParser parser;
    parser.addHelpOption();
    parser.addOption({QStringLiteral("installed-version"),
                      QObject::tr("Version WebAD actuellement installée."),
                      QStringLiteral("version")});
    parser.addOption({QStringLiteral("parent-pid"),
                      QObject::tr("PID de WebAD à attendre avant l’installation."),
                      QStringLiteral("pid")});
    parser.addOption({QStringLiteral("force"),
                      QObject::tr("Vérifier sans attendre le délai de trois jours.")});
    parser.addOption({QStringLiteral("self-test"),
                      QObject::tr("Exécuter les vérifications internes puis quitter.")});
    parser.process(application);
    if (parser.isSet(QStringLiteral("self-test"))) {
        return runSelfTest() ? 0 : 1;
    }

    bool pidOk = false;
    const qint64 parentPid = parser.value(QStringLiteral("parent-pid")).toLongLong(&pidOk);
    if (!parser.value(QStringLiteral("parent-pid")).isEmpty() && !pidOk) {
        QMessageBox::critical(nullptr, QObject::tr("WebAD — Mise à jour"),
                              QObject::tr("L’identifiant du processus WebAD est invalide."));
        return 1;
    }
    const QString installedVersion =
        parser.value(QStringLiteral("installed-version")).isEmpty()
            ? QCoreApplication::applicationVersion()
            : parser.value(QStringLiteral("installed-version"));
    return runUpdater(installedVersion, parentPid, parser.isSet(QStringLiteral("force")));
}
