#include "Core.h"

#include <qt6keychain/keychain.h>

#include <QCollator>
#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QEventLoop>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QSaveFile>
#include <QSettings>
#include <QStandardPaths>
#include <QSysInfo>
#include <QUrl>
#include <QUuid>

using namespace Qt::StringLiterals;

static const auto KeychainService = u"stories-desktop"_s;
static const int FadeMs = 30000;

static qint64 nowMs() { return QDateTime::currentMSecsSinceEpoch(); }

static QString placeKey(const QString &path, qint64 ms, bool finished)
{
    return path + u'\n' + QString::number(ms) + (finished ? u"f"_s : u"p"_s);
}

static QJsonObject toJson(const Position &p, qint64 at)
{
    return {{u"path"_s, p.path}, {u"size"_s, p.size}, {u"ms"_s, p.ms}, {u"finished"_s, p.finished},
            {u"speed"_s, p.speed}, {u"lastPlayed"_s, at}};
}

static Position positionFrom(const QJsonObject &o)
{
    Position p;
    p.path = o[u"path"].toString();
    p.size = o[u"size"].toInteger();
    p.ms = o[u"ms"].toInteger();
    p.finished = o[u"finished"].toBool();
    p.speed = o[u"speed"].toDouble(1);
    p.lastPlayed = o[u"lastPlayed"].toInteger();
    return p;
}

static QJsonObject toJson(const Bookmark &b)
{
    return {{u"id"_s, b.id}, {u"path"_s, b.path}, {u"size"_s, b.size}, {u"ms"_s, b.ms}, {u"note"_s, b.note},
            {u"created"_s, b.created}, {u"updated"_s, b.updated}, {u"deleted"_s, b.deleted}};
}

static Bookmark bookmarkFrom(const QJsonObject &o)
{
    Bookmark b;
    b.id = o[u"id"].toString();
    b.path = o[u"path"].toString();
    b.size = o[u"size"].toInteger();
    b.ms = o[u"ms"].toInteger();
    b.note = o[u"note"].toString();
    b.created = o[u"created"].toInteger();
    b.updated = o[u"updated"].toInteger();
    b.deleted = o[u"deleted"].toBool();
    return b;
}

Core::Core(QObject *parent)
    : QObject(parent)
{
    QDir().mkpath(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation));
    QDir().mkpath(QStandardPaths::writableLocation(QStandardPaths::CacheLocation) + u"/covers"_s);

    connect(&m_api, &Api::changed, this, [this](const QJsonObject &r) {
        pull();
        const QString library = r[u"library"].toString();
        if (!library.isEmpty() && library != m_libraryEtag) {
            m_libraryEtag = library;
            fetchLibrary();
        }
    });
    connect(&m_api, &Api::signedIn, this, &Core::deviceSignedIn);
    connect(&m_api, &Api::unauthorized, this, [this] {
        forgetSession();
        setError(tr("Signed out by the server. Sign in to go on syncing."));
    });

    connect(&m_player, &MpvPlayer::pausedChanged, this, [this] {
        emit playingChanged();
        if (!m_currentId.isEmpty() && m_player.paused())
            recordPlace(true);
    });
    connect(&m_player, &MpvPlayer::positionChanged, this, [this] {
        if (m_seekingTo >= 0 && qAbs(position() - m_seekingTo) < 1500)
            m_seekingTo = -1;
        emit positionChanged();
        const int now = chapter();
        if (now != m_lastChapter) {
            const bool moved = m_lastChapter >= 0;
            if (moved && m_sleepAtChapter && playing() && now == m_lastChapter + 1)
                endSleep();
            m_lastChapter = now;
            emit chapterChanged();
            if (moved)
                recordPlace(true);
        }
    });
    connect(&m_player, &MpvPlayer::trackChanged, this, &Core::positionChanged);
    connect(&m_player, &MpvPlayer::bufferingChanged, this, [this](bool buffering) {
        m_buffering = buffering;
        emit bufferingChanged();
    });
    connect(&m_player, &MpvPlayer::ended, this, [this] {
        if (m_currentId.isEmpty() || !m_index.contains(m_currentId))
            return;
        const Book &b = m_list[m_index[m_currentId]];
        // keep-open also reports the end of a file skipped to; only the last one counts.
        if (m_player.track() != b.files.size() - 1)
            return;
        m_player.setPaused(true);
        if (!m_offer)
            putPosition(b, b.files.size() - 1, b.files.last().durationMs, true, m_speed, nowMs());
        cancelSleep();
        bump();
        flush();
    });
    connect(&m_player, &MpvPlayer::failed, this, [this](const QString &message) {
        setError(tr("Could not play this file: %1").arg(message));
    });

    m_tick.setInterval(1000);
    connect(&m_tick, &QTimer::timeout, this, &Core::onTick);
    m_tick.start();

    connect(qApp, &QCoreApplication::aboutToQuit, this, [this] {
        if (!m_currentId.isEmpty())
            recordPlace(false);
        save();
        if (m_pendingPositions.isEmpty() || !signedIn())
            return;
        // A moment for the last place to reach the server; if not, it goes next time.
        QEventLoop wait;
        QTimer::singleShot(1500, &wait, &QEventLoop::quit);
        flush([&wait] { wait.quit(); });
        wait.exec();
    });

    load();
    readToken();
}

Core::~Core() = default;

QString Core::dataFile(const QString &name) const
{
    return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + u'/' + name;
}

QString Core::coverPath(const QString &coverId) const
{
    return QStandardPaths::writableLocation(QStandardPaths::CacheLocation) + u"/covers/"_s + QFileInfo(coverId).fileName() + u".jpg"_s;
}

void Core::setBusy(bool busy)
{
    if (busy != m_busy) {
        m_busy = busy;
        emit busyChanged();
    }
}

void Core::setError(const QString &error)
{
    if (error != m_error) {
        m_error = error;
        emit errorChanged();
    }
}

void Core::clearError() { setError({}); }

void Core::setLocalLibrary(const QString &path)
{
    const QString clean = path.trimmed().isEmpty() ? QString() : QDir::cleanPath(QDir::fromNativeSeparators(path.trimmed()));
    if (clean == m_localLibrary)
        return;
    m_localLibrary = clean;
    QSettings().setValue(u"localLibrary"_s, clean);
    emit localLibraryChanged();
}

void Core::setQuery(const QString &query)
{
    if (query != m_query) {
        m_query = query;
        emit queryChanged();
        rebuildRows();
    }
}

void Core::setFilter(const QString &filter)
{
    if (filter != m_filter) {
        m_filter = filter;
        emit filterChanged();
        rebuildRows();
    }
}

// --- signing in ---

void Core::readToken()
{
    QSettings settings;
    m_localLibrary = settings.value(u"localLibrary"_s).toString();
    const QString url = settings.value(u"server"_s).toString();
    const auto done = [this, url](const QString &token) {
        m_ready = true;
        if (!url.isEmpty() && !token.isEmpty()) {
            m_api.setServer(url, token);
            connected();
        }
        emit signedInChanged();
    };
    if (url.isEmpty())
        return done({});
    auto *job = new QKeychain::ReadPasswordJob(KeychainService, this);
    job->setKey(url);
    connect(job, &QKeychain::Job::finished, this, [this, job, done] {
        if (job->error() == QKeychain::NoError)
            return done(job->textData());
        // No keyring on this desktop: the token is in a file only this user can read.
        QFile file(dataFile(u"token"_s));
        done(file.open(QIODevice::ReadOnly) ? QString::fromUtf8(file.readAll()).trimmed() : QString());
    });
    job->start();
}

void Core::storeToken(const QString &token)
{
    auto *job = new QKeychain::WritePasswordJob(KeychainService, this);
    job->setKey(m_api.url());
    job->setTextData(token);
    connect(job, &QKeychain::Job::finished, this, [this, job, token] {
        if (job->error() == QKeychain::NoError) {
            QFile::remove(dataFile(u"token"_s));
            return;
        }
        QSaveFile file(dataFile(u"token"_s));
        if (file.open(QIODevice::WriteOnly)) {
            file.setPermissions(QFile::ReadOwner | QFile::WriteOwner);
            file.write(token.toUtf8());
            file.commit();
        }
    });
    job->start();
}

void Core::deleteToken()
{
    auto *job = new QKeychain::DeletePasswordJob(KeychainService, this);
    job->setKey(m_api.url());
    job->start();
    QFile::remove(dataFile(u"token"_s));
}

void Core::login(const QString &address, const QString &password)
{
    const QString url = Api::normalize(address);
    if (url.isEmpty())
        return setError(tr("That doesn't look like a server address."));
    setBusy(true);
    setError({});
    const QString previous = QSettings().value(u"serverId"_s).toString();
    m_api.setServer(url, {});
    const QString device = QSysInfo::machineHostName().isEmpty() ? u"Desktop"_s : QSysInfo::machineHostName();
    m_api.send("POST", u"/v1/login"_s, {{u"password"_s, password}, {u"device"_s, device}},
               [this, url, previous](int status, const QByteArray &body, const QByteArray &) {
        setBusy(false);
        if (status != 200) {
            m_api.setServer({}, {});
            if (status == 401)
                setError(tr("Wrong password."));
            else if (status == 429)
                setError(tr("Too many attempts. Try again in a minute."));
            else if (status == -1)
                setError(tr("This address needs https."));
            else if (status == 0)
                setError(tr("Can't reach the server: %1").arg(QString::fromUtf8(body)));
            else
                setError(tr("The server answered %1.").arg(status));
            return;
        }
        const QJsonObject o = QJsonDocument::fromJson(body).object();
        const QString token = o[u"token"].toString();
        const QString serverId = o[u"serverId"].toString();
        if (token.isEmpty()) {
            m_api.setServer({}, {});
            return setError(tr("That doesn't look like a storiesd server."));
        }
        if (serverId != previous) {
            // What's known of another server says nothing about this one.
            m_positions.clear();
            m_pendingPositions.clear();
            m_bookmarks.clear();
            m_pendingBookmarks.clear();
            m_positionsRev = m_bookmarksRev = 0;
            m_list.clear();
            m_index.clear();
            m_agreed.clear();
            m_etag.clear();
        }
        QSettings settings;
        settings.setValue(u"server"_s, url);
        settings.setValue(u"serverId"_s, serverId);
        m_api.setServer(url, token);
        storeToken(token);
        connected();
        emit signedInChanged();
    });
}

void Core::connected()
{
    fetchServerInfo();
    fetchLibrary([this] { pull([this] { flush(); flushBookmarks(); }); });
    m_api.openEvents();
}

void Core::forgetSession()
{
    m_api.closeEvents();
    deleteToken();
    m_api.setServer(m_api.url(), {});
    emit signedInChanged();
}

void Core::logout()
{
    if (!m_currentId.isEmpty())
        recordPlace(false);
    m_player.stop();
    m_currentId.clear();
    m_offer.reset();
    m_offerBook.clear();
    cancelSleep();
    emit currentChanged();
    emit newerChanged();
    emit playingChanged();
    const auto leave = [this] {
        forgetSession();
        QSettings().remove(u"server"_s);
        m_api.setServer({}, {});
        save();
        emit signedInChanged();
    };
    flush(leave);
}

void Core::refresh()
{
    if (signedIn()) {
        fetchLibrary();
        pull();
    }
}

void Core::fetchServerInfo()
{
    m_api.get(u"/v1/server"_s, [this](int status, const QByteArray &body, const QByteArray &) {
        if (status != 200)
            return;
        const QJsonObject o = QJsonDocument::fromJson(body).object();
        m_clockOffset = o[u"now"].toInteger() - nowMs();
        const QUrl mine(m_api.url());
        const QHostAddress host(mine.host());
        const bool loopback = host.isLoopback() || mine.host() == u"localhost"_s;
        const QString share = loopback ? o[u"url"].toString() : m_api.url();
        if (share != m_shareUrl) {
            m_shareUrl = share;
            emit shareUrlChanged();
        }
    });
}

// --- library ---

void Core::fetchLibrary(std::function<void()> then)
{
    m_api.get(u"/v1/library"_s, [this, then](int status, const QByteArray &body, const QByteArray &etag) {
        if (status == 200) {
            m_etag = etag;
            parseLibrary(body);
            QSaveFile out(dataFile(u"library.json"_s));
            if (out.open(QIODevice::WriteOnly)) {
                out.write(body);
                out.commit();
            }
            QSettings().setValue(u"libraryEtag"_s, QString::fromLatin1(etag));
            fetchCovers();
        } else if (status == 0) {
            setError(tr("Server not reachable. Showing what's known."));
        }
        if (then)
            then();
    }, m_etag);
}

void Core::parseLibrary(const QByteArray &json)
{
    const QJsonArray array = QJsonDocument::fromJson(json).object()[u"books"].toArray();
    QList<Book> list;
    QHash<QString, int> index;
    for (const QJsonValue &value : array) {
        const QJsonObject o = value.toObject();
        Book b;
        b.id = o[u"id"].toString();
        b.title = o[u"title"].toString();
        b.author = o[u"author"].toString();
        b.narrator = o[u"narrator"].toString();
        b.series = o[u"series"].toString();
        b.year = o[u"year"].toString();
        b.coverId = o[u"coverId"].toString();
        for (const QJsonValue &fv : o[u"files"].toArray()) {
            const QJsonObject fo = fv.toObject();
            BookFile f;
            f.id = fo[u"id"].toString();
            f.path = fo[u"path"].toString();
            f.title = fo[u"title"].toString();
            f.size = fo[u"size"].toInteger();
            f.durationMs = fo[u"durationMs"].toInteger();
            for (const QJsonValue &mv : fo[u"chapters"].toArray())
                f.marks << Mark{mv[u"title"].toString(), mv[u"startMs"].toInteger()};
            b.files << f;
        }
        if (b.id.isEmpty() || b.files.isEmpty())
            continue;
        b.finish();
        index.insert(b.id, int(list.size()));
        list << b;
    }
    m_list = list;
    m_index = index;
    rebuildRows();
    bump();
    emit currentChanged();
}

void Core::fetchCovers()
{
    for (const Book &b : std::as_const(m_list)) {
        if (b.coverId.isEmpty() || QFile::exists(coverPath(b.coverId)))
            continue;
        m_api.saveTo(u"/v1/covers/"_s + b.coverId, coverPath(b.coverId), [this](bool ok) {
            if (ok) {
                rebuildRows();
                bump();
                emit currentChanged();
            }
        });
    }
}

Core::Place Core::placeOf(const Book &book) const
{
    Place best;
    for (int i = 0; i < book.files.size(); ++i) {
        const auto it = m_positions.constFind(fileKey(book.files[i].path, book.files[i].size));
        if (it == m_positions.constEnd() || (best.any && it->lastPlayed <= best.lastPlayed))
            continue;
        best.any = true;
        best.track = i;
        best.trackMs = it->ms;
        best.finished = it->finished;
        best.speed = it->speed;
        best.lastPlayed = it->lastPlayed;
        best.path = it->path;
        best.bookMs = it->finished ? book.durationMs : qMin(book.starts[i] + it->ms, book.durationMs);
    }
    return best;
}

static QString spanText(qint64 ms)
{
    const qint64 minutes = qMax<qint64>(ms, 0) / 60000;
    if (minutes < 1)
        return Core::tr("under a minute");
    if (minutes < 60)
        return Core::tr("%1 min").arg(minutes);
    return Core::tr("%1 h %2 min").arg(minutes / 60).arg(minutes % 60);
}

void Core::rebuildRows()
{
    const QString query = m_query.trimmed();
    QList<QVariantMap> rows, going;
    QList<std::pair<qint64, QVariantMap>> recent;
    for (const Book &b : std::as_const(m_list)) {
        const Place p = placeOf(b);
        qint64 at = p.bookMs;
        if (b.id == m_currentId)
            at = position();
        const QString status = p.finished && b.id != m_currentId ? u"finished"_s
            : at > 5000 ? u"listening"_s
            : p.finished ? u"finished"_s : u"new"_s;
        const double fraction = status == u"finished"_s ? 1.0 : b.durationMs > 0 ? double(at) / b.durationMs : 0.0;
        const QString line = status == u"finished"_s ? tr("Finished")
            : status == u"listening"_s ? tr("%1% · %2 left").arg(int(fraction * 100)).arg(spanText(b.durationMs - at))
            : spanText(b.durationMs);
        const QString cover = !b.coverId.isEmpty() && QFile::exists(coverPath(b.coverId))
            ? QUrl::fromLocalFile(coverPath(b.coverId)).toString() : QString();
        const QVariantMap row{{u"bookId"_s, b.id}, {u"title"_s, b.title}, {u"author"_s, b.author}, {u"series"_s, b.series},
                              {u"cover"_s, cover}, {u"status"_s, status}, {u"fraction"_s, fraction}, {u"line"_s, line}};
        if (status == u"listening"_s)
            recent.append({p.lastPlayed, row});
        if (m_filter != u"all"_s && status != m_filter)
            continue;
        if (!query.isEmpty() && !b.title.contains(query, Qt::CaseInsensitive) && !b.author.contains(query, Qt::CaseInsensitive)
            && !b.series.contains(query, Qt::CaseInsensitive) && !b.narrator.contains(query, Qt::CaseInsensitive))
            continue;
        rows << row;
    }
    std::stable_sort(recent.begin(), recent.end(), [](const auto &a, const auto &b) { return a.first > b.first; });
    for (const auto &r : std::as_const(recent))
        going << r.second;
    m_books.update(rows);
    m_listening.update(going);
}

void Core::bump()
{
    ++m_revision;
    emit revisionChanged();
}

QVariantMap Core::book(const QString &id, int) const
{
    const auto it = m_index.constFind(id);
    if (it == m_index.constEnd())
        return {};
    const Book &b = m_list[*it];
    const Place p = placeOf(b);
    const qint64 at = id == m_currentId ? position() : p.bookMs;
    QVariantList chapters;
    const int here = (p.any || id == m_currentId) ? b.chapterAt(at) : -1;
    for (int i = 0; i < b.chapters.size(); ++i) {
        chapters << QVariantMap{{u"title"_s, b.chapters[i].title.isEmpty() ? tr("Chapter %1").arg(i + 1) : b.chapters[i].title},
                                {u"start"_s, b.chapters[i].start}, {u"durationMs"_s, b.chapters[i].durationMs},
                                {u"current"_s, i == here}, {u"heard"_s, p.finished || (here >= 0 && i < here)}};
    }
    QList<Bookmark> own;
    QHash<QString, int> byFile;
    for (int i = 0; i < b.files.size(); ++i)
        byFile.insert(fileKey(b.files[i].path, b.files[i].size), i);
    QList<std::pair<qint64, Bookmark>> marks;
    for (const Bookmark &m : m_bookmarks) {
        const auto f = byFile.constFind(fileKey(m.path, m.size));
        if (!m.deleted && f != byFile.constEnd())
            marks.append({b.starts[*f] + m.ms, m});
    }
    std::sort(marks.begin(), marks.end(), [](const auto &x, const auto &y) { return x.first < y.first; });
    QVariantList bookmarks;
    for (const auto &[ms, m] : std::as_const(marks))
        bookmarks << QVariantMap{{u"id"_s, m.id}, {u"bookMs"_s, ms}, {u"note"_s, m.note},
                                 {u"chapter"_s, b.chapterAt(ms) >= 0 ? b.chapters[b.chapterAt(ms)].title : QString()}};
    const QString cover = !b.coverId.isEmpty() && QFile::exists(coverPath(b.coverId))
        ? QUrl::fromLocalFile(coverPath(b.coverId)).toString() : QString();
    const QString status = p.finished ? u"finished"_s : at > 5000 ? u"listening"_s : u"new"_s;
    return {{u"id"_s, b.id}, {u"title"_s, b.title}, {u"author"_s, b.author}, {u"narrator"_s, b.narrator}, {u"series"_s, b.series},
            {u"year"_s, b.year}, {u"cover"_s, cover}, {u"durationMs"_s, b.durationMs}, {u"status"_s, status}, {u"positionMs"_s, at},
            {u"speed"_s, p.any ? p.speed : 1.0}, {u"chapters"_s, chapters}, {u"bookmarks"_s, bookmarks}};
}

QVariantMap Core::current() const
{
    return m_currentId.isEmpty() ? QVariantMap() : book(m_currentId);
}

qint64 Core::position() const
{
    const auto it = m_index.constFind(m_currentId);
    if (it == m_index.constEnd())
        return 0;
    const Book &b = m_list[*it];
    const int track = qBound(0, m_player.track(), int(b.starts.size()) - 1);
    return qMin(b.starts[track] + m_player.trackMs(), b.durationMs);
}

int Core::chapter() const
{
    const auto it = m_index.constFind(m_currentId);
    return it == m_index.constEnd() ? -1 : m_list[*it].chapterAt(position());
}

// --- playing ---

QString Core::source(const BookFile &file) const
{
    if (!m_localLibrary.isEmpty()) {
        const QFileInfo local(m_localLibrary + u'/' + file.path);
        if (local.isFile() && local.size() == file.size)
            return local.absoluteFilePath();
    }
    return m_api.url() + u"/v1/files/"_s + file.id;
}

void Core::play(const QString &id, qint64 atMs)
{
    if (!m_index.contains(id))
        return;
    if (id == m_currentId && m_player.loaded()) {
        if (atMs >= 0)
            seekTo(atMs);
        else if (placeOf(m_list[m_index[id]]).finished && m_player.paused())
            seekTo(0);
        m_player.setPaused(false);
        return;
    }
    if (!m_currentId.isEmpty())
        recordPlace(true);
    // Ask first: this book may have been heard further on elsewhere.
    auto *guard = new QObject(this);
    const auto go = [this, id, atMs, guard] {
        if (guard->property("done").toBool())
            return;
        guard->setProperty("done", true);
        guard->deleteLater();
        start(id, atMs);
    };
    QTimer::singleShot(2500, guard, go);
    pull(go);
}

void Core::start(const QString &id, qint64 atMs)
{
    const auto it = m_index.constFind(id);
    if (it == m_index.constEnd())
        return;
    const Book b = m_list[*it];
    const Place p = placeOf(b);
    const qint64 at = atMs >= 0 ? atMs : (p.any && !p.finished ? p.bookMs : 0);
    const auto [track, offset] = b.locate(at);
    m_currentId = id;
    m_offer.reset();
    m_offerBook.clear();
    emit newerChanged();
    m_speed = p.any ? p.speed : 1.0;
    m_player.setSpeed(m_speed);
    m_lastChapter = b.chapterAt(at);
    m_agreed.insert(id, p.any ? placeKey(p.path, p.trackMs, p.finished) : QString());
    QStringList sources;
    for (const BookFile &f : b.files)
        sources << source(f);
    const auto load = [this, sources, track, offset, id] {
        if (id == m_currentId)
            m_player.load(sources, track, offset, true, u"Bearer "_s + m_api.token());
    };
    const bool needsNetwork = std::any_of(sources.begin(), sources.end(), [](const QString &s) { return s.startsWith(u"http"); });
    if (needsNetwork) {
        m_api.checkAllowed([this, load](bool allowed) {
            if (allowed)
                load();
            else
                setError(tr("This address needs https."));
        });
    } else {
        load();
    }
    emit speedChanged();
    emit currentChanged();
    emit chapterChanged();
    emit positionChanged();
    rebuildRows();
}

void Core::toggle()
{
    if (m_currentId.isEmpty())
        return;
    if (!m_player.loaded())
        return play(m_currentId);
    if (m_player.paused() && m_index.contains(m_currentId) && placeOf(m_list[m_index[m_currentId]]).finished)
        seekTo(0);
    m_player.setPaused(!m_player.paused());
}

void Core::seekTo(qint64 bookMs)
{
    const auto it = m_index.constFind(m_currentId);
    if (it == m_index.constEnd())
        return;
    const Book &b = m_list[*it];
    const qint64 target = qBound<qint64>(0, bookMs, qMax<qint64>(b.durationMs - 500, 0));
    const auto [track, offset] = b.locate(target);
    m_seekingTo = target;
    m_player.seek(track, offset);
    if (m_player.paused())
        recordPlace(true);
}

void Core::skip(qint64 deltaMs) { seekTo(position() + deltaMs); }

void Core::nextChapter()
{
    const auto it = m_index.constFind(m_currentId);
    if (it == m_index.constEnd())
        return;
    const Book &b = m_list[*it];
    const int next = b.chapterAt(position()) + 1;
    if (next < b.chapters.size())
        seekTo(b.chapters[next].start);
}

void Core::previousChapter()
{
    const auto it = m_index.constFind(m_currentId);
    if (it == m_index.constEnd())
        return;
    const Book &b = m_list[*it];
    const int at = b.chapterAt(position());
    if (at < 0)
        return;
    const bool justIn = position() - b.chapters[at].start <= 4000;
    seekTo(b.chapters[justIn && at > 0 ? at - 1 : at].start);
}

void Core::setSpeed(double speed)
{
    const double s = qBound(0.5, qRound(speed * 20) / 20.0, 3.5);
    if (qFuzzyCompare(s, m_speed))
        return;
    m_speed = s;
    m_player.setSpeed(s);
    emit speedChanged();
    if (!m_currentId.isEmpty())
        recordPlace(true);
}

void Core::putPosition(const Book &book, int track, qint64 ms, bool finished, double speed, qint64 at)
{
    Position p;
    p.path = book.files[track].path;
    p.size = book.files[track].size;
    p.ms = ms;
    p.finished = finished;
    p.speed = speed;
    p.lastPlayed = at;
    const QString key = fileKey(p.path, p.size);
    m_positions.insert(key, p);
    m_pendingPositions.insert(key, p);
    m_agreed.insert(book.id, placeKey(p.path, p.ms, p.finished));
}

void Core::recordPlace(bool send)
{
    // Waiting for an answer about a newer place: it isn't overwritten meanwhile.
    if (m_offer || !m_player.loaded() || m_seekingTo >= 0)
        return;
    const auto it = m_index.constFind(m_currentId);
    if (it == m_index.constEnd())
        return;
    const Book &b = m_list[*it];
    const int track = qBound(0, m_player.track(), int(b.files.size()) - 1);
    const Place p = placeOf(b);
    const qint64 ms = m_player.trackMs();
    const bool unchanged = p.any && p.track == track && qAbs(p.trackMs - ms) < 500 && !p.finished && qFuzzyCompare(p.speed, m_speed);
    if (!unchanged)
        putPosition(b, track, ms, false, m_speed, nowMs());
    rebuildRows();
    bump();
    if (send)
        flush();
}

void Core::markFinished(const QString &id, bool finished)
{
    const auto it = m_index.constFind(id);
    if (it == m_index.constEnd())
        return;
    const Book &b = m_list[*it];
    const double speed = placeOf(b).any ? placeOf(b).speed : 1.0;
    if (id == m_currentId) {
        m_offer.reset();
        m_offerBook.clear();
        emit newerChanged();
        m_player.setPaused(true);
    }
    if (finished)
        putPosition(b, b.files.size() - 1, b.files.last().durationMs, true, speed, nowMs());
    else
        putPosition(b, 0, 0, false, speed, nowMs());
    if (id == m_currentId) {
        const auto [track, offset] = b.locate(finished ? b.durationMs : 0);
        m_player.seek(track, offset);
    }
    rebuildRows();
    bump();
    emit currentChanged();
    flush();
}

// --- sync ---

void Core::pull(std::function<void()> then)
{
    if (then)
        m_pullWaiters << then;
    if (!signedIn()) {
        const auto waiters = std::exchange(m_pullWaiters, {});
        for (const auto &w : waiters)
            w();
        return;
    }
    if (m_pulling) {
        m_pullAgain = true;
        return;
    }
    m_pulling = true;
    const auto finish = [this] {
        m_pulling = false;
        const auto waiters = std::exchange(m_pullWaiters, {});
        applyRemote();
        for (const auto &w : waiters)
            w();
        if (std::exchange(m_pullAgain, false))
            pull();
    };
    m_api.get(u"/v1/server"_s, [this, finish](int status, const QByteArray &body, const QByteArray &) {
        if (status != 200)
            return finish();
        m_clockOffset = QJsonDocument::fromJson(body).object()[u"now"].toInteger() - nowMs();
        m_api.get(u"/v1/positions?since=%1"_s.arg(m_positionsRev), [this, finish](int status, const QByteArray &body, const QByteArray &) {
            if (status != 200)
                return finish();
            const QJsonObject o = QJsonDocument::fromJson(body).object();
            for (const QJsonValue &v : o[u"positions"].toArray()) {
                Position p = positionFrom(v.toObject());
                p.lastPlayed = qMin(p.lastPlayed - m_clockOffset, nowMs());
                const QString key = fileKey(p.path, p.size);
                const auto known = m_positions.constFind(key);
                if (m_pendingPositions.contains(key) && known != m_positions.constEnd() && known->lastPlayed >= p.lastPlayed)
                    continue;
                m_pendingPositions.remove(key);
                m_positions.insert(key, p);
            }
            m_positionsRev = o[u"rev"].toInteger();
            m_api.get(u"/v1/bookmarks?since=%1"_s.arg(m_bookmarksRev), [this, finish](int status, const QByteArray &body, const QByteArray &) {
                if (status == 200) {
                    const QJsonObject o = QJsonDocument::fromJson(body).object();
                    for (const QJsonValue &v : o[u"bookmarks"].toArray()) {
                        Bookmark b = bookmarkFrom(v.toObject());
                        if (m_pendingBookmarks.contains(b.id))
                            continue;
                        m_bookmarks.insert(b.id, b);
                    }
                    m_bookmarksRev = o[u"rev"].toInteger();
                }
                finish();
            });
        });
    });
}

void Core::applyRemote()
{
    if (!m_currentId.isEmpty() && m_index.contains(m_currentId) && !m_offer) {
        const Book &b = m_list[m_index[m_currentId]];
        const Place p = placeOf(b);
        if (p.any && placeKey(p.path, p.trackMs, p.finished) != m_agreed.value(m_currentId)) {
            const auto key = fileKey(b.files[p.track].path, b.files[p.track].size);
            if (!m_pendingPositions.contains(key)) {
                if (playing()) {
                    m_offer = m_positions.value(key);
                    m_offerBook = b.id;
                    emit newerChanged();
                } else {
                    m_agreed.insert(b.id, placeKey(p.path, p.trackMs, p.finished));
                    const auto [track, offset] = p.finished ? b.locate(b.durationMs) : std::pair<int, qint64>{p.track, p.trackMs};
                    if (m_player.loaded()) {
                        m_seekingTo = -1;
                        m_player.seek(track, offset);
                    }
                    if (!qFuzzyCompare(p.speed, m_speed)) {
                        m_speed = p.speed;
                        m_player.setSpeed(m_speed);
                        emit speedChanged();
                    }
                    emit positionChanged();
                    emit chapterChanged();
                }
            }
        }
    }
    rebuildRows();
    bump();
    emit currentChanged();
    save();
}

void Core::flush(std::function<void()> then)
{
    if (m_flushing || m_pendingPositions.isEmpty() || !signedIn()) {
        if (then)
            then();
        return;
    }
    m_flushing = true;
    const auto sending = m_pendingPositions;
    QJsonArray array;
    for (const Position &p : sending)
        array << toJson(p, p.lastPlayed);
    m_api.send("PUT", u"/v1/positions"_s, {{u"now"_s, nowMs()}, {u"positions"_s, array}},
               [this, sending, then](int status, const QByteArray &, const QByteArray &) {
        m_flushing = false;
        if (status == 200) {
            for (auto it = sending.constBegin(); it != sending.constEnd(); ++it) {
                const auto now = m_pendingPositions.constFind(it.key());
                if (now != m_pendingPositions.constEnd() && now->lastPlayed == it->lastPlayed && now->ms == it->ms)
                    m_pendingPositions.remove(it.key());
            }
        }
        save();
        if (then)
            then();
    });
}

void Core::flushBookmarks()
{
    if (m_pendingBookmarks.isEmpty() || !signedIn())
        return;
    const auto sending = m_pendingBookmarks;
    QJsonArray array;
    for (const Bookmark &b : sending)
        array << toJson(b);
    m_api.send("PUT", u"/v1/bookmarks"_s, {{u"now"_s, nowMs()}, {u"bookmarks"_s, array}},
               [this, sending](int status, const QByteArray &, const QByteArray &) {
        if (status == 200) {
            for (auto it = sending.constBegin(); it != sending.constEnd(); ++it) {
                const auto now = m_pendingBookmarks.constFind(it.key());
                if (now != m_pendingBookmarks.constEnd() && now->updated == it->updated)
                    m_pendingBookmarks.remove(it.key());
            }
        }
        save();
    });
}

void Core::addBookmark()
{
    const auto it = m_index.constFind(m_currentId);
    if (it == m_index.constEnd())
        return;
    const Book &b = m_list[*it];
    const int track = qBound(0, m_player.track(), int(b.files.size()) - 1);
    Bookmark m;
    m.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    m.path = b.files[track].path;
    m.size = b.files[track].size;
    m.ms = m_player.trackMs();
    m.created = m.updated = nowMs();
    m_bookmarks.insert(m.id, m);
    m_pendingBookmarks.insert(m.id, m);
    bump();
    emit currentChanged();
    flushBookmarks();
}

void Core::noteBookmark(const QString &id, const QString &note)
{
    auto it = m_bookmarks.find(id);
    if (it == m_bookmarks.end() || it->note == note.trimmed())
        return;
    it->note = note.trimmed();
    it->updated = nowMs();
    m_pendingBookmarks.insert(id, *it);
    bump();
    emit currentChanged();
    flushBookmarks();
}

void Core::removeBookmark(const QString &id)
{
    auto it = m_bookmarks.find(id);
    if (it == m_bookmarks.end())
        return;
    it->deleted = true;
    it->updated = nowMs();
    m_pendingBookmarks.insert(id, *it);
    bump();
    emit currentChanged();
    flushBookmarks();
}

QVariantMap Core::newer() const
{
    if (!m_offer || !m_index.contains(m_offerBook))
        return {};
    const Book &b = m_list[m_index[m_offerBook]];
    qint64 at = b.durationMs;
    for (int i = 0; i < b.files.size(); ++i) {
        if (b.files[i].path == m_offer->path && b.files[i].size == m_offer->size && !m_offer->finished)
            at = b.starts[i] + m_offer->ms;
    }
    const int chapter = b.chapterAt(at);
    return {{u"title"_s, b.title}, {u"bookMs"_s, at}, {u"finished"_s, m_offer->finished},
            {u"chapter"_s, chapter >= 0 ? b.chapters[chapter].title : QString()}};
}

void Core::acceptNewer()
{
    if (!m_offer || !m_index.contains(m_offerBook))
        return;
    const Book b = m_list[m_index[m_offerBook]];
    const Position p = *m_offer;
    m_offer.reset();
    m_offerBook.clear();
    m_agreed.insert(b.id, placeKey(p.path, p.ms, p.finished));
    qint64 at = 0;
    for (int i = 0; i < b.files.size(); ++i) {
        if (b.files[i].path == p.path && b.files[i].size == p.size)
            at = b.starts[i] + p.ms;
    }
    const auto [track, offset] = b.locate(p.finished ? 0 : at);
    m_lastChapter = b.chapterAt(p.finished ? 0 : at);
    m_player.seek(track, offset);
    if (!qFuzzyCompare(p.speed, m_speed)) {
        m_speed = p.speed;
        m_player.setSpeed(m_speed);
        emit speedChanged();
    }
    emit newerChanged();
    emit positionChanged();
    emit chapterChanged();
}

void Core::declineNewer()
{
    if (!m_offer)
        return;
    m_offer.reset();
    m_offerBook.clear();
    emit newerChanged();
    const auto it = m_index.constFind(m_currentId);
    if (it == m_index.constEnd())
        return;
    const Book &b = m_list[*it];
    // This place goes up as the newest, replacing the other device's.
    putPosition(b, qBound(0, m_player.track(), int(b.files.size()) - 1), m_player.trackMs(), false, m_speed, nowMs());
    flush();
}

// --- clock ---

void Core::onTick()
{
    if (playing()) {
        ++m_ticks;
        if (m_ticks % 5 == 0)
            recordPlace(m_ticks % 30 == 0);
        if (m_sleepLeft > 0) {
            m_sleepLeft = qMax<qint64>(m_sleepLeft - qint64(1000), 0);
            emit sleepChanged();
            if (m_sleepLeft < FadeMs)
                m_player.setVolume(100.0 * m_sleepLeft / FadeMs);
            if (m_sleepLeft == 0)
                endSleep();
        }
    }
    if (m_ticks % 60 == 59 && signedIn()) {
        flush();
        flushBookmarks();
    }
}

void Core::sleepIn(int minutes)
{
    m_sleepAtChapter = false;
    m_sleepLeft = minutes * 60000LL;
    m_player.setVolume(100);
    emit sleepChanged();
}

void Core::sleepAtEndOfChapter()
{
    m_sleepLeft = 0;
    m_sleepAtChapter = true;
    m_player.setVolume(100);
    emit sleepChanged();
}

void Core::cancelSleep()
{
    m_sleepLeft = 0;
    m_sleepAtChapter = false;
    m_player.setVolume(100);
    emit sleepChanged();
}

void Core::endSleep()
{
    m_player.setPaused(true);
    cancelSleep();
}

// --- kept between runs ---

void Core::save() const
{
    QJsonArray positions, pending, bookmarks, pendingMarks;
    for (const Position &p : m_positions)
        positions << toJson(p, p.lastPlayed);
    for (const Position &p : m_pendingPositions)
        pending << toJson(p, p.lastPlayed);
    for (const Bookmark &b : m_bookmarks)
        bookmarks << toJson(b);
    for (const Bookmark &b : m_pendingBookmarks)
        pendingMarks << toJson(b);
    const QJsonObject root{{u"positionsRev"_s, m_positionsRev}, {u"bookmarksRev"_s, m_bookmarksRev},
                           {u"positions"_s, positions}, {u"pendingPositions"_s, pending},
                           {u"bookmarks"_s, bookmarks}, {u"pendingBookmarks"_s, pendingMarks}};
    QSaveFile out(dataFile(u"state.json"_s));
    if (out.open(QIODevice::WriteOnly)) {
        out.write(QJsonDocument(root).toJson(QJsonDocument::Compact));
        out.commit();
    }
}

void Core::load()
{
    QFile state(dataFile(u"state.json"_s));
    if (state.open(QIODevice::ReadOnly)) {
        const QJsonObject o = QJsonDocument::fromJson(state.readAll()).object();
        m_positionsRev = o[u"positionsRev"].toInteger();
        m_bookmarksRev = o[u"bookmarksRev"].toInteger();
        for (const QJsonValue &v : o[u"positions"].toArray()) {
            const Position p = positionFrom(v.toObject());
            m_positions.insert(fileKey(p.path, p.size), p);
        }
        for (const QJsonValue &v : o[u"pendingPositions"].toArray()) {
            const Position p = positionFrom(v.toObject());
            m_pendingPositions.insert(fileKey(p.path, p.size), p);
        }
        for (const QJsonValue &v : o[u"bookmarks"].toArray()) {
            const Bookmark b = bookmarkFrom(v.toObject());
            m_bookmarks.insert(b.id, b);
        }
        for (const QJsonValue &v : o[u"pendingBookmarks"].toArray()) {
            const Bookmark b = bookmarkFrom(v.toObject());
            m_pendingBookmarks.insert(b.id, b);
        }
    }
    QFile library(dataFile(u"library.json"_s));
    if (library.open(QIODevice::ReadOnly)) {
        parseLibrary(library.readAll());
        m_etag = QSettings().value(u"libraryEtag"_s).toString().toLatin1();
    }
}
