#pragma once

#include <QElapsedTimer>
#include <QHash>
#include <QHostAddress>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QObject>
#include <QPointer>
#include <QTimer>
#include <functional>

class QNetworkReply;

/** storiesd's HTTP API. Plain http goes only to private addresses, tokens included. */
class Api : public QObject {
    Q_OBJECT
public:
    using Done = std::function<void(int status, const QByteArray &body, const QByteArray &etag)>;

    explicit Api(QObject *parent = nullptr);

    void setServer(const QString &url, const QString &token);
    QString url() const { return m_url; }
    QString token() const { return m_token; }

    void get(const QString &path, Done done, const QByteArray &etag = {});
    void send(const QByteArray &verb, const QString &path, const QJsonObject &body, Done done);
    void saveTo(const QString &path, const QString &file, std::function<void(bool)> done);

    void openEvents();
    void closeEvents();

    /** Whether a token may be sent to the server now; resolves names. */
    void checkAllowed(std::function<void(bool)> done);

    static QString normalize(const QString &input);
    static bool isPrivate(const QHostAddress &address);

signals:
    void changed(const QJsonObject &revisions);
    void signedIn(const QString &device);
    void unauthorized();

private:
    void start(const QByteArray &verb, const QString &path, const QByteArray &body, const QByteArray &etag, Done done);
    void readEvents();
    void eventsEnded();

    QNetworkAccessManager m_net;
    QString m_url, m_token;
    QPointer<QNetworkReply> m_events;
    QByteArray m_buffer;
    QTimer m_retry, m_watchdog;
    bool m_wantEvents = false;
    bool m_allowed = false;
    QElapsedTimer m_checked;
};
