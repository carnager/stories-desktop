#include "Api.h"

#include <QFile>
#include <QHostInfo>
#include <QJsonDocument>
#include <QNetworkReply>
#include <QRegularExpression>
#include <QSaveFile>
#include <QUrl>

static const int DefaultPort = 8743;

Api::Api(QObject *parent)
    : QObject(parent)
{
    m_net.setRedirectPolicy(QNetworkRequest::ManualRedirectPolicy);
    m_retry.setSingleShot(true);
    m_retry.setInterval(5000);
    connect(&m_retry, &QTimer::timeout, this, &Api::openEvents);
    // The server says something every 25 s; silence means the link is gone.
    m_watchdog.setSingleShot(true);
    m_watchdog.setInterval(70000);
    connect(&m_watchdog, &QTimer::timeout, this, [this] {
        if (m_events)
            m_events->abort();
    });
}

void Api::setServer(const QString &url, const QString &token)
{
    if (url != m_url)
        m_checked.invalidate();
    m_url = url;
    m_token = token;
}

QString Api::normalize(const QString &input)
{
    QString text = input.trimmed();
    while (text.endsWith(QLatin1Char('/')))
        text.chop(1);
    if (text.isEmpty() || text.contains(QRegularExpression(QStringLiteral("\\s"))))
        return {};
    if (!text.contains(QLatin1String("://")))
        text.prepend(QLatin1String("http://"));
    QUrl url(text, QUrl::StrictMode);
    const QString scheme = url.scheme().toLower();
    if (!url.isValid() || url.host().isEmpty() || (scheme != QLatin1String("http") && scheme != QLatin1String("https")))
        return {};
    if (!url.userInfo().isEmpty() || url.hasQuery() || url.hasFragment())
        return {};
    int port = url.port(-1);
    if (port < 0 && scheme == QLatin1String("http"))
        port = DefaultPort;
    QString host = url.host();
    if (host.contains(QLatin1Char(':')))
        host = QLatin1Char('[') + host + QLatin1Char(']');
    QString path = url.path();
    while (path.endsWith(QLatin1Char('/')))
        path.chop(1);
    return scheme + QLatin1String("://") + host + (port > 0 ? QLatin1Char(':') + QString::number(port) : QString()) + path;
}

bool Api::isPrivate(const QHostAddress &input)
{
    QHostAddress address = input;
    bool ok = false;
    const quint32 mapped = address.toIPv4Address(&ok);
    if (ok)
        address = QHostAddress(mapped);
    if (address.isLoopback() || address.isLinkLocal())
        return true;
    if (address.protocol() == QAbstractSocket::IPv4Protocol) {
        const quint32 v = address.toIPv4Address();
        return (v >> 24) == 10 || (v >> 20) == 0xAC1 || (v >> 16) == 0xC0A8 || (v >> 16) == 0xA9FE || (v >> 22) == (0x64400000u >> 22);
    }
    return (address.toIPv6Address()[0] & 0xFE) == 0xFC;
}

void Api::checkAllowed(std::function<void(bool)> done)
{
    const QUrl url(m_url);
    if (url.scheme() == QLatin1String("https"))
        return done(true);
    if (url.scheme() != QLatin1String("http"))
        return done(false);
    if (m_checked.isValid() && m_checked.elapsed() < 60000)
        return done(m_allowed);
    const auto finish = [this, done](bool allowed) {
        m_allowed = allowed;
        m_checked.start();
        done(allowed);
    };
    QHostAddress literal;
    if (literal.setAddress(url.host()))
        return finish(isPrivate(literal));
    QHostInfo::lookupHost(url.host(), this, [finish](const QHostInfo &info) {
        const auto addresses = info.addresses();
        bool allowed = info.error() == QHostInfo::NoError && !addresses.isEmpty();
        for (const QHostAddress &a : addresses)
            allowed = allowed && isPrivate(a);
        finish(allowed);
    });
}

void Api::start(const QByteArray &verb, const QString &path, const QByteArray &body, const QByteArray &etag, Done done)
{
    checkAllowed([=, this](bool allowed) {
        if (!allowed)
            return done(-1, QByteArrayLiteral("This address needs https"), {});
        QNetworkRequest request(QUrl(m_url + path));
        request.setTransferTimeout(20000);
        if (!m_token.isEmpty())
            request.setRawHeader("Authorization", "Bearer " + m_token.toUtf8());
        if (!etag.isEmpty())
            request.setRawHeader("If-None-Match", etag);
        if (!body.isEmpty())
            request.setHeader(QNetworkRequest::ContentTypeHeader, QByteArrayLiteral("application/json"));
        QNetworkReply *reply = m_net.sendCustomRequest(request, verb, body);
        connect(reply, &QNetworkReply::finished, this, [this, reply, done] {
            reply->deleteLater();
            const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
            if (status == 401 && !m_token.isEmpty())
                emit unauthorized();
            done(status, status ? reply->readAll() : reply->errorString().toUtf8(), reply->rawHeader("ETag"));
        });
    });
}

void Api::get(const QString &path, Done done, const QByteArray &etag)
{
    start("GET", path, {}, etag, std::move(done));
}

void Api::send(const QByteArray &verb, const QString &path, const QJsonObject &body, Done done)
{
    start(verb, path, QJsonDocument(body).toJson(QJsonDocument::Compact), {}, std::move(done));
}

void Api::saveTo(const QString &path, const QString &file, std::function<void(bool)> done)
{
    get(path, [file, done](int status, const QByteArray &body, const QByteArray &) {
        if (status != 200)
            return done(false);
        QSaveFile out(file);
        done(out.open(QIODevice::WriteOnly) && out.write(body) == body.size() && out.commit());
    });
}

void Api::openEvents()
{
    m_wantEvents = true;
    if (m_events || m_url.isEmpty() || m_token.isEmpty())
        return;
    checkAllowed([this](bool allowed) {
        if (!m_wantEvents || m_events)
            return;
        if (!allowed) {
            m_retry.start();
            return;
        }
        QNetworkRequest request(QUrl(m_url + QLatin1String("/v1/events")));
        request.setRawHeader("Authorization", "Bearer " + m_token.toUtf8());
        request.setRawHeader("Accept", "text/event-stream");
        request.setTransferTimeout(0);
        m_buffer.clear();
        m_events = m_net.get(request);
        m_watchdog.start();
        connect(m_events, &QNetworkReply::readyRead, this, &Api::readEvents);
        connect(m_events, &QNetworkReply::finished, this, &Api::eventsEnded);
    });
}

void Api::closeEvents()
{
    m_wantEvents = false;
    m_retry.stop();
    m_watchdog.stop();
    if (m_events) {
        disconnect(m_events, nullptr, this, nullptr);
        m_events->abort();
        m_events->deleteLater();
    }
}

void Api::readEvents()
{
    if (!m_events)
        return;
    m_watchdog.start();
    m_buffer += m_events->readAll();
    qsizetype end;
    while ((end = m_buffer.indexOf("\n\n")) >= 0) {
        const QByteArray block = m_buffer.left(end);
        m_buffer.remove(0, end + 2);
        QByteArray name, data;
        for (const QByteArray &line : block.split('\n')) {
            if (line.startsWith("event:"))
                name = line.mid(6).trimmed();
            else if (line.startsWith("data:"))
                data = line.mid(5).trimmed();
        }
        const QJsonObject object = QJsonDocument::fromJson(data).object();
        if (name == "changed")
            emit changed(object);
        else if (name == "signedin")
            emit signedIn(object.value(QLatin1String("device")).toString());
    }
}

void Api::eventsEnded()
{
    if (!m_events)
        return;
    const int status = m_events->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    m_events->deleteLater();
    m_events = nullptr;
    m_watchdog.stop();
    if (status == 401) {
        emit unauthorized();
        return;
    }
    if (m_wantEvents)
        m_retry.start();
}
