#pragma once

#include <QObject>
#include <QStringList>
#include <functional>

/**
 * First start, "on this computer". storiesd itself creates its config and
 * password (storiesd init); this runs it and starts the systemd user service.
 */
class LocalServer : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool available READ available CONSTANT)
    Q_PROPERTY(bool hasConfig READ hasConfig NOTIFY stateChanged)
    Q_PROPERTY(bool hasPassword READ hasPassword NOTIFY stateChanged)
    Q_PROPERTY(bool running READ running NOTIFY stateChanged)
    Q_PROPERTY(QString library READ library NOTIFY stateChanged)
    Q_PROPERTY(bool busy READ busy NOTIFY stateChanged)
    Q_PROPERTY(QString step READ step NOTIFY stateChanged)
    Q_PROPERTY(QString error READ error NOTIFY stateChanged)
public:
    explicit LocalServer(QObject *parent = nullptr);

    /** storiesd and systemctl are both installed. */
    bool available() const { return !m_binary.isEmpty() && !m_systemctl.isEmpty(); }
    bool hasConfig() const { return m_hasConfig; }
    bool hasPassword() const { return m_hasPassword; }
    /** A server is up here and can be signed in to: nothing to set up or start. */
    bool running() const { return m_running && m_hasPassword; }
    QString library() const;
    bool busy() const { return m_busy; }
    QString step() const { return m_step; }
    QString error() const { return m_error; }

    /** Asks storiesd what is set up. */
    Q_INVOKABLE void refresh();
    Q_INVOKABLE void setup(const QString &library, const QString &password, const QString &repeat);
    Q_INVOKABLE QString localPath(const QUrl &url) const;

signals:
    void stateChanged();
    void ready(const QString &url, const QString &password, const QString &library);

private:
    using Next = std::function<void()>;
    void run(const QString &program, const QStringList &args, const QByteArray &input, const QString &failure, Next next);
    void setStep(const QString &step);
    void fail(const QString &message);
    void initialize();
    bool writeUnit();
    void startService(bool restart);
    void waitForPort(int tries);
    QString unitPath() const;
    int port() const;

    QString m_binary, m_systemctl, m_step, m_error, m_password, m_folder;
    QString m_config, m_library, m_listen;
    bool m_busy = false, m_hasConfig = false, m_hasPassword = false, m_running = false;
};
