#include "LocalServer.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QRegularExpression>
#include <QSaveFile>
#include <QStandardPaths>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTimer>
#include <QUrl>

using namespace Qt::StringLiterals;

static const int MinPassword = 8;

static QString tomlString(QString text)
{
    text.replace(u'\\', u"\\\\"_s);
    text.replace(u'"', u"\\\""_s);
    return u'"' + text + u'"';
}

// systemd's own quoting, with % doubled so a path is never read as a specifier.
static QString unitQuoted(QString text)
{
    text.replace(u'\\', u"\\\\"_s);
    text.replace(u'"', u"\\\""_s);
    text.replace(u'%', u"%%"_s);
    text.replace(u'$', u"$$"_s);
    return u'"' + text + u'"';
}

LocalServer::LocalServer(QObject *parent)
    : QObject(parent)
{
    m_binary = QStandardPaths::findExecutable(u"storiesd"_s);
    m_systemctl = QStandardPaths::findExecutable(u"systemctl"_s);
}

QString LocalServer::configPath() const
{
    return QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation) + u"/storiesd/config.toml"_s;
}

QString LocalServer::unitPath() const
{
    return QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation) + u"/systemd/user/storiesd.service"_s;
}

bool LocalServer::hasConfig() const { return QFile::exists(configPath()); }

QString LocalServer::configValue(const QString &key) const
{
    QFile file(configPath());
    if (!file.open(QIODevice::ReadOnly))
        return {};
    const QRegularExpression line(u"^\\s*"_s + key + uR"re(\s*=\s*"((?:[^"\\]|\\.)*)")re"_s, QRegularExpression::MultilineOption);
    QString value = line.match(QString::fromUtf8(file.readAll())).captured(1);
    value.replace(u"\\\""_s, u"\""_s);
    value.replace(u"\\\\"_s, u"\\"_s);
    return value;
}

QString LocalServer::library() const
{
    const QString configured = configValue(u"library"_s);
    if (!configured.isEmpty())
        return configured;
    const QString guess = QDir::homePath() + u"/audiobooks"_s;
    return QDir(guess).exists() ? guess : QString();
}

int LocalServer::port() const
{
    const QString listen = configValue(u"listen"_s);
    const int colon = listen.lastIndexOf(u':');
    const int value = colon >= 0 ? listen.mid(colon + 1).toInt() : 0;
    return value > 0 ? value : 8743;
}

QString LocalServer::localPath(const QUrl &url) const { return url.toLocalFile(); }

void LocalServer::setStep(const QString &step)
{
    m_step = step;
    emit stateChanged();
}

void LocalServer::fail(const QString &message)
{
    // A config made in this attempt goes, so another attempt starts clean.
    if (m_createdConfig)
        QFile::remove(configPath());
    m_createdConfig = false;
    m_password.clear();
    m_busy = false;
    m_step.clear();
    m_error = message;
    emit stateChanged();
}

void LocalServer::run(const QString &program, const QStringList &args, const QByteArray &input, const QString &failure, Next next)
{
    auto *process = new QProcess(this);
    process->setProcessChannelMode(QProcess::MergedChannels);
    connect(process, &QProcess::errorOccurred, this, [this, process, failure](QProcess::ProcessError error) {
        if (error != QProcess::FailedToStart)
            return;
        process->deleteLater();
        fail(failure);
    });
    connect(process, &QProcess::finished, this, [this, process, failure, next](int code, QProcess::ExitStatus status) {
        process->deleteLater();
        if (status != QProcess::NormalExit || code != 0) {
            const QString output = QString::fromUtf8(process->readAll()).trimmed();
            return fail(output.isEmpty() ? failure : failure + u"\n"_s + output);
        }
        next();
    });
    process->start(program, args);
    if (!input.isEmpty())
        process->write(input);
    process->closeWriteChannel();
}

bool LocalServer::writeConfig(const QString &library)
{
    QDir().mkpath(QFileInfo(configPath()).path());
    QSaveFile file(configPath());
    if (!file.open(QIODevice::WriteOnly))
        return false;
    const int free = freePort();
    if (free == 0)
        return false;
    const QString data = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) + u"/storiesd"_s;
    const QString text = u"library = "_s + tomlString(library) + u"\nlisten = \":%1\"\ndata = "_s.arg(free) + tomlString(data) + u"\n"_s;
    file.write(text.toUtf8());
    return file.commit();
}

// The usual port, or the next free one: something else answering there must
// not be mistaken for the server just started.
int LocalServer::freePort() const
{
    for (int candidate = 8743; candidate < 8793; ++candidate) {
        QTcpServer probe;
        if (probe.listen(QHostAddress::Any, quint16(candidate)))
            return candidate;
    }
    return 0;
}

bool LocalServer::writeUnit()
{
    // Installed as a package, storiesd brings its own unit.
    if (QFile::exists(unitPath()) || QFile::exists(u"/usr/lib/systemd/user/storiesd.service"_s))
        return true;
    QDir().mkpath(QFileInfo(unitPath()).path());
    QSaveFile file(unitPath());
    if (!file.open(QIODevice::WriteOnly))
        return false;
    const QString text =
        u"[Unit]\nDescription=Stories audiobook server\nAfter=network.target\n\n[Service]\nExecStart="_s
        + unitQuoted(QFileInfo(m_binary).absoluteFilePath()) + u" -config "_s + unitQuoted(configPath())
        + u"\nRestart=on-failure\n\n[Install]\nWantedBy=default.target\n"_s;
    file.write(text.toUtf8());
    return file.commit();
}

void LocalServer::setup(const QString &library, const QString &password)
{
    if (m_busy)
        return;
    m_error.clear();
    if (!available())
        return fail(tr("storiesd or systemctl isn't installed."));
    const bool existing = hasConfig();
    const QString folder = existing ? this->library() : QDir::cleanPath(library.trimmed());
    if (!existing && (folder.isEmpty() || !QDir(folder).exists()))
        return fail(tr("Choose a folder that exists."));
    if (password.size() < MinPassword)
        return fail(existing ? tr("Enter this server's password.") : tr("The password needs at least %1 characters.").arg(MinPassword));
    m_busy = true;
    m_password = password;
    m_library = folder;
    m_createdConfig = false;
    if (existing) {
        enable();
        return;
    }
    setStep(tr("Writing the configuration"));
    if (!writeConfig(folder))
        return fail(tr("Could not write %1.").arg(configPath()));
    m_createdConfig = true;
    setStep(tr("Setting the password"));
    // On stdin: a command line can be read by other users' processes.
    run(m_binary, {u"-config"_s, configPath(), u"passwd"_s}, password.toUtf8() + '\n', tr("Could not set the password."), [this] { enable(); });
}

void LocalServer::enable()
{
    setStep(tr("Starting the service"));
    if (!writeUnit())
        return fail(tr("Could not write %1.").arg(unitPath()));
    run(m_systemctl, {u"--user"_s, u"daemon-reload"_s}, {}, tr("systemd could not reload its units."), [this] {
        run(m_systemctl, {u"--user"_s, u"enable"_s, u"--now"_s, u"storiesd.service"_s}, {}, tr("Could not start the storiesd service."), [this] {
            setStep(tr("Waiting for the server"));
            waitForPort(40);
        });
    });
}

void LocalServer::waitForPort(int tries)
{
    auto *socket = new QTcpSocket(this);
    const auto retry = [this, socket, tries] {
        socket->deleteLater();
        if (tries <= 1)
            return fail(tr("The service started but isn't answering. See: journalctl --user -u storiesd"));
        QTimer::singleShot(250, this, [this, tries] { waitForPort(tries - 1); });
    };
    connect(socket, &QTcpSocket::errorOccurred, this, retry);
    connect(socket, &QTcpSocket::connected, this, [this, socket] {
        disconnect(socket, nullptr, this, nullptr);
        socket->abort();
        socket->deleteLater();
        const QString password = std::exchange(m_password, {});
        m_createdConfig = false;
        m_busy = false;
        m_step.clear();
        emit stateChanged();
        emit ready(u"http://127.0.0.1:%1"_s.arg(port()), password, m_library);
    });
    socket->connectToHost(u"127.0.0.1"_s, quint16(port()));
}
