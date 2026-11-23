#include "LocalServer.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QSaveFile>
#include <QStandardPaths>
#include <QTcpSocket>
#include <QTimer>
#include <QUrl>

using namespace Qt::StringLiterals;

static const int MinPassword = 8;

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
    refresh();
}

void LocalServer::refresh()
{
    if (m_binary.isEmpty() || m_busy)
        return;
    QProcess process;
    process.start(m_binary, {u"status"_s, u"--json"_s});
    if (process.waitForFinished(5000) && process.exitCode() == 0) {
        const QJsonObject o = QJsonDocument::fromJson(process.readAllStandardOutput()).object();
        m_config = o[u"config"].toString();
        m_hasConfig = o[u"configured"].toBool();
        m_hasPassword = o[u"hasPassword"].toBool();
        m_library = o[u"library"].toString();
        m_listen = o[u"listen"].toString();
        m_running = o[u"running"].toBool();
        // A service started by an older storiesd hasn't said where it is; its config has.
        if (!m_running && m_hasConfig && !m_systemctl.isEmpty()) {
            QProcess active;
            active.start(m_systemctl, {u"--user"_s, u"is-active"_s, u"--quiet"_s, u"storiesd.service"_s});
            m_running = active.waitForFinished(3000) && active.exitStatus() == QProcess::NormalExit && active.exitCode() == 0;
        }
    } else {
        m_error = tr("storiesd can't read its setup: %1").arg(QString::fromUtf8(process.readAllStandardError()).trimmed());
    }
    emit stateChanged();
}

QString LocalServer::unitPath() const
{
    return QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation) + u"/systemd/user/storiesd.service"_s;
}

QString LocalServer::library() const
{
    if (!m_library.isEmpty())
        return m_library;
    const QString guess = QDir::homePath() + u"/audiobooks"_s;
    return QDir(guess).exists() ? guess : QString();
}

int LocalServer::port() const
{
    const int colon = m_listen.lastIndexOf(u':');
    const int value = colon >= 0 ? m_listen.mid(colon + 1).toInt() : 0;
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
    m_password.clear();
    m_busy = false;
    m_step.clear();
    m_error = message;
    emit stateChanged();
    refresh();
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
        if (!failure.isEmpty() && (status != QProcess::NormalExit || code != 0)) {
            const QString output = QString::fromUtf8(process->readAll()).trimmed().remove(u"storiesd: "_s);
            const bool adds = !output.isEmpty() && output != u"wrong password"_s;
            return fail(adds ? failure + u"\n"_s + output : failure);
        }
        next();
    });
    process->start(program, args);
    if (!input.isEmpty())
        process->write(input);
    process->closeWriteChannel();
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
        + unitQuoted(QFileInfo(m_binary).absoluteFilePath()) + u" -config "_s + unitQuoted(m_config)
        + u"\nRestart=on-failure\n\n[Install]\nWantedBy=default.target\n"_s;
    file.write(text.toUtf8());
    return file.commit();
}

void LocalServer::setup(const QString &library, const QString &password, const QString &repeat)
{
    if (m_busy)
        return;
    m_error.clear();
    if (!available())
        return fail(tr("storiesd or systemctl isn't installed."));
    refresh();
    if (running()) {
        // Already up: only signing in is left, and the server itself checks the password.
        if (password.isEmpty())
            return fail(tr("Enter a password."));
        emit stateChanged();
        emit ready(u"http://127.0.0.1:%1"_s.arg(port()), password, m_library);
        return;
    }
    m_folder = QDir::cleanPath(library.trimmed());
    if (!m_hasConfig && (m_folder.isEmpty() || !QDir(m_folder).exists()))
        return fail(tr("Choose a folder that exists."));
    if (password.isEmpty())
        return fail(tr("Enter a password."));
    if (!m_hasPassword) {
        if (password.size() < MinPassword)
            return fail(tr("The password needs at least %1 characters.").arg(MinPassword));
        if (password != repeat)
            return fail(tr("The passwords don't match."));
    }
    m_busy = true;
    m_password = password;
    if (!m_hasPassword)
        return initialize();
    // Checked before anything changes, so a typo leaves a running server alone.
    setStep(tr("Checking the password"));
    run(m_binary, {u"init"_s, u"--check"_s}, password.toUtf8() + '\n', tr("That isn't this server's password."), [this] {
        if (m_hasConfig)
            startService(false);
        else
            initialize();
    });
}

void LocalServer::initialize()
{
    const auto create = [this] {
        setStep(tr("Creating the configuration"));
        QStringList args{u"init"_s};
        if (!m_hasConfig)
            args << u"--library"_s << m_folder;
        run(m_binary, args, m_password.toUtf8() + '\n', tr("storiesd could not be set up."), [this] {
            m_busy = false;
            refresh();
            m_busy = true;
            startService(true);
        });
    };
    if (m_hasConfig)
        return create();
    // A service still running from a removed config holds the port the new one should get.
    setStep(tr("Stopping the old service"));
    run(m_systemctl, {u"--user"_s, u"stop"_s, u"storiesd.service"_s}, {}, {}, create);
}

void LocalServer::startService(bool restart)
{
    setStep(tr("Starting the service"));
    if (!writeUnit())
        return fail(tr("Could not write %1.").arg(unitPath()));
    run(m_systemctl, {u"--user"_s, u"daemon-reload"_s}, {}, tr("systemd could not reload its units."), [this, restart] {
        run(m_systemctl, {u"--user"_s, u"enable"_s, u"storiesd.service"_s}, {}, tr("Could not enable the storiesd service."), [this, restart] {
            // Already running from before, it wouldn't read a new config by itself.
            run(m_systemctl, {u"--user"_s, restart ? u"restart"_s : u"start"_s, u"storiesd.service"_s}, {},
                tr("Could not start the storiesd service."), [this] {
                setStep(tr("Waiting for the server"));
                waitForPort(40);
            });
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
        m_busy = false;
        m_step.clear();
        refresh();
        emit ready(u"http://127.0.0.1:%1"_s.arg(port()), password, m_library);
    });
    socket->connectToHost(u"127.0.0.1"_s, quint16(port()));
}
