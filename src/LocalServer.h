#pragma once

#include <QObject>
#include <QStringList>
#include <functional>

/**
 * First start, "on this computer": a storiesd config, a password and a
 * systemd user service. An existing config or unit is used, never rewritten.
 */
class LocalServer : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool available READ available CONSTANT)
    Q_PROPERTY(bool hasConfig READ hasConfig NOTIFY stateChanged)
    Q_PROPERTY(QString library READ library NOTIFY stateChanged)
    Q_PROPERTY(bool busy READ busy NOTIFY stateChanged)
    Q_PROPERTY(QString step READ step NOTIFY stateChanged)
    Q_PROPERTY(QString error READ error NOTIFY stateChanged)
public:
    explicit LocalServer(QObject *parent = nullptr);

    /** storiesd and systemctl are both installed. */
    bool available() const { return !m_binary.isEmpty() && !m_systemctl.isEmpty(); }
    bool hasConfig() const;
    QString library() const;
    bool busy() const { return m_busy; }
    QString step() const { return m_step; }
    QString error() const { return m_error; }

    Q_INVOKABLE void setup(const QString &library, const QString &password);
    Q_INVOKABLE QString localPath(const QUrl &url) const;

signals:
    void stateChanged();
    void ready(const QString &url, const QString &password, const QString &library);

private:
    using Next = std::function<void()>;
    void run(const QString &program, const QStringList &args, const QByteArray &input, const QString &failure, Next next);
    void setStep(const QString &step);
    void fail(const QString &message);
    bool writeConfig(const QString &library);
    bool writeUnit();
    void enable();
    void waitForPort(int tries);
    QString configValue(const QString &key) const;
    QString configPath() const;
    QString unitPath() const;
    int port() const;
    int freePort() const;

    QString m_binary, m_systemctl, m_step, m_error, m_password, m_library;
    bool m_busy = false, m_createdConfig = false;
};
