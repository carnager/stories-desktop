#pragma once

#include "Api.h"
#include "BookModel.h"
#include "MpvPlayer.h"
#include "Types.h"

#include <QHash>
#include <QObject>
#include <QTimer>
#include <QVariantList>
#include <QVariantMap>
#include <optional>

/** The app's state and rules, for the QML interface: library, player and sync. */
class Core : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool ready READ ready NOTIFY signedInChanged)
    Q_PROPERTY(bool signedIn READ signedIn NOTIFY signedInChanged)
    Q_PROPERTY(bool busy READ busy NOTIFY busyChanged)
    Q_PROPERTY(QString error READ error NOTIFY errorChanged)
    Q_PROPERTY(QString serverUrl READ serverUrl NOTIFY signedInChanged)
    Q_PROPERTY(QString shareUrl READ shareUrl NOTIFY shareUrlChanged)
    Q_PROPERTY(QString localLibrary READ localLibrary WRITE setLocalLibrary NOTIFY localLibraryChanged)
    Q_PROPERTY(BookModel *books READ books CONSTANT)
    Q_PROPERTY(BookModel *listening READ listening CONSTANT)
    Q_PROPERTY(QString query READ query WRITE setQuery NOTIFY queryChanged)
    Q_PROPERTY(QString filter READ filter WRITE setFilter NOTIFY filterChanged)
    Q_PROPERTY(int revision READ revision NOTIFY revisionChanged)
    Q_PROPERTY(QVariantMap current READ current NOTIFY currentChanged)
    Q_PROPERTY(bool playing READ playing NOTIFY playingChanged)
    Q_PROPERTY(bool buffering READ buffering NOTIFY bufferingChanged)
    Q_PROPERTY(qint64 position READ position NOTIFY positionChanged)
    Q_PROPERTY(int chapter READ chapter NOTIFY chapterChanged)
    Q_PROPERTY(double speed READ speed WRITE setSpeed NOTIFY speedChanged)
    Q_PROPERTY(qint64 sleepLeft READ sleepLeft NOTIFY sleepChanged)
    Q_PROPERTY(bool sleepAtChapter READ sleepAtChapter NOTIFY sleepChanged)
    Q_PROPERTY(QVariantMap newer READ newer NOTIFY newerChanged)
public:
    explicit Core(QObject *parent = nullptr);
    ~Core() override;

    bool ready() const { return m_ready; }
    bool signedIn() const { return !m_api.token().isEmpty(); }
    bool busy() const { return m_busy; }
    QString error() const { return m_error; }
    QString serverUrl() const { return m_api.url(); }
    QString shareUrl() const { return m_shareUrl; }
    QString localLibrary() const { return m_localLibrary; }
    void setLocalLibrary(const QString &path);
    BookModel *books() { return &m_books; }
    BookModel *listening() { return &m_listening; }
    QString query() const { return m_query; }
    void setQuery(const QString &query);
    QString filter() const { return m_filter; }
    void setFilter(const QString &filter);
    int revision() const { return m_revision; }
    QVariantMap current() const;
    bool playing() const { return m_player.loaded() && !m_player.paused(); }
    bool buffering() const { return m_buffering; }
    qint64 position() const;
    int chapter() const;
    double speed() const { return m_speed; }
    void setSpeed(double speed);
    qint64 sleepLeft() const { return m_sleepLeft; }
    bool sleepAtChapter() const { return m_sleepAtChapter; }
    QVariantMap newer() const;

    Q_INVOKABLE void login(const QString &address, const QString &password);
    Q_INVOKABLE void logout();
    Q_INVOKABLE void clearError();
    Q_INVOKABLE void refresh();
    Q_INVOKABLE QVariantMap book(const QString &id, int revision = 0) const;

    Q_INVOKABLE void play(const QString &id, qint64 atMs = -1);
    Q_INVOKABLE void toggle();
    Q_INVOKABLE void seekTo(qint64 bookMs);
    Q_INVOKABLE void skip(qint64 deltaMs);
    Q_INVOKABLE void nextChapter();
    Q_INVOKABLE void previousChapter();
    Q_INVOKABLE void markFinished(const QString &id, bool finished);

    Q_INVOKABLE void addBookmark();
    Q_INVOKABLE void noteBookmark(const QString &id, const QString &note);
    Q_INVOKABLE void removeBookmark(const QString &id);

    Q_INVOKABLE void sleepIn(int minutes);
    Q_INVOKABLE void sleepAtEndOfChapter();
    Q_INVOKABLE void cancelSleep();

    Q_INVOKABLE void acceptNewer();
    Q_INVOKABLE void declineNewer();

signals:
    void signedInChanged();
    void busyChanged();
    void errorChanged();
    void shareUrlChanged();
    void localLibraryChanged();
    void queryChanged();
    void filterChanged();
    void revisionChanged();
    void currentChanged();
    void playingChanged();
    void bufferingChanged();
    void positionChanged();
    void chapterChanged();
    void speedChanged();
    void sleepChanged();
    void newerChanged();
    void raiseRequested();
    void deviceSignedIn(const QString &device);

private:
    struct Place {
        bool any = false, finished = false;
        int track = 0;
        qint64 trackMs = 0, bookMs = 0, lastPlayed = 0;
        double speed = 1;
        QString path;
    };

    void setBusy(bool busy);
    void setError(const QString &error);
    void connected();
    void forgetSession();
    void readToken();
    void storeToken(const QString &token);
    void deleteToken();

    void fetchServerInfo();
    void fetchLibrary(std::function<void()> then = {});
    void parseLibrary(const QByteArray &json);
    void fetchCovers();
    void pull(std::function<void()> then = {});
    void flush(std::function<void()> then = {});
    void flushBookmarks();

    Place placeOf(const Book &book) const;
    void applyRemote();
    void rebuildRows();
    void bump();
    void recordPlace(bool send);
    void putPosition(const Book &book, int track, qint64 ms, bool finished, double speed, qint64 at);
    QString source(const BookFile &file) const;
    void start(const QString &id, qint64 atMs);
    void onTick();
    void endSleep();
    QString coverPath(const QString &coverId) const;

    void load();
    void save() const;
    QString dataFile(const QString &name) const;

    Api m_api;
    MpvPlayer m_player;
    BookModel m_books, m_listening;
    QList<Book> m_list;
    QHash<QString, int> m_index;
    QHash<QString, Position> m_positions, m_pendingPositions;
    QHash<QString, Bookmark> m_bookmarks, m_pendingBookmarks;
    qint64 m_positionsRev = 0, m_bookmarksRev = 0, m_clockOffset = 0;
    QByteArray m_etag;
    QString m_libraryEtag;

    bool m_ready = false, m_busy = false, m_buffering = false, m_pulling = false, m_pullAgain = false, m_flushing = false;
    QString m_error, m_shareUrl, m_localLibrary, m_query, m_filter = QStringLiteral("all");
    int m_revision = 0;
    QList<std::function<void()>> m_pullWaiters;

    QString m_currentId;
    double m_speed = 1;
    int m_lastChapter = -1;
    qint64 m_seekingTo = -1;
    QHash<QString, QString> m_agreed; // book id -> the place last agreed with the server

    std::optional<Position> m_offer;
    QString m_offerBook;

    QTimer m_tick;
    int m_ticks = 0;
    qint64 m_sleepLeft = 0;
    bool m_sleepAtChapter = false;
};
