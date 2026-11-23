#include "Mpris.h"

#include "Core.h"

#include <QCoreApplication>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QUrl>

static const QString ObjectPath = QStringLiteral("/org/mpris/MediaPlayer2");
static const QString PlayerInterface = QStringLiteral("org.mpris.MediaPlayer2.Player");

Mpris::Mpris(Core *core)
    : QObject(core)
    , m_core(core)
{
    new MprisRoot(this);
    auto *player = new MprisPlayer(this);
    auto bus = QDBusConnection::sessionBus();
    if (!bus.isConnected())
        return;
    bus.registerObject(ObjectPath, this);
    bus.registerService(QStringLiteral("org.mpris.MediaPlayer2.stories"));

    connect(core, &Core::playingChanged, this, [this, player] {
        notify(PlayerInterface, {{QStringLiteral("PlaybackStatus"), player->status()}});
    });
    connect(core, &Core::currentChanged, this, [this, player] {
        notify(PlayerInterface, {{QStringLiteral("Metadata"), metadata()},
                                 {QStringLiteral("PlaybackStatus"), player->status()},
                                 {QStringLiteral("CanPlay"), player->hasBook()},
                                 {QStringLiteral("CanPause"), player->hasBook()},
                                 {QStringLiteral("CanSeek"), player->hasBook()},
                                 {QStringLiteral("CanGoNext"), player->hasBook()},
                                 {QStringLiteral("CanGoPrevious"), player->hasBook()}});
    });
    connect(core, &Core::speedChanged, this, [this, player] {
        notify(PlayerInterface, {{QStringLiteral("Rate"), player->rate()}});
    });
    connect(core, &Core::positionChanged, this, [this, player] {
        // Clients extrapolate from the rate; they're only told of real jumps.
        const qint64 now = m_core->position();
        if (qAbs(now - m_lastMs) > 3000)
            emit player->Seeked(now * 1000);
        m_lastMs = now;
    });
}

QVariantMap Mpris::metadata() const
{
    const QVariantMap book = m_core->current();
    if (book.isEmpty())
        return {{QStringLiteral("mpris:trackid"), QVariant::fromValue(QDBusObjectPath(QStringLiteral("/org/mpris/MediaPlayer2/TrackList/NoTrack")))}};
    const QString id = book.value(QStringLiteral("id")).toString();
    QVariantMap map{
        {QStringLiteral("mpris:trackid"), QVariant::fromValue(QDBusObjectPath(QStringLiteral("/stories/book/") + id))},
        {QStringLiteral("mpris:length"), book.value(QStringLiteral("durationMs")).toLongLong() * 1000},
        {QStringLiteral("xesam:title"), book.value(QStringLiteral("title"))},
        {QStringLiteral("xesam:album"), book.value(QStringLiteral("title"))},
        {QStringLiteral("xesam:artist"), QStringList{book.value(QStringLiteral("author")).toString()}},
    };
    const QString cover = book.value(QStringLiteral("cover")).toString();
    if (!cover.isEmpty())
        map.insert(QStringLiteral("mpris:artUrl"), cover);
    return map;
}

void Mpris::notify(const QString &interface, const QVariantMap &changed)
{
    QDBusMessage signal = QDBusMessage::createSignal(ObjectPath, QStringLiteral("org.freedesktop.DBus.Properties"),
                                                     QStringLiteral("PropertiesChanged"));
    signal << interface << changed << QStringList();
    QDBusConnection::sessionBus().send(signal);
}

MprisRoot::MprisRoot(Mpris *parent)
    : QDBusAbstractAdaptor(parent)
    , m_mpris(parent)
{
}

void MprisRoot::Raise()
{
    emit m_mpris->core()->raiseRequested();
}

void MprisRoot::Quit()
{
    QCoreApplication::quit();
}

MprisPlayer::MprisPlayer(Mpris *parent)
    : QDBusAbstractAdaptor(parent)
    , m_mpris(parent)
{
    setAutoRelaySignals(false);
}

QString MprisPlayer::status() const
{
    if (!hasBook())
        return QStringLiteral("Stopped");
    return m_mpris->core()->playing() ? QStringLiteral("Playing") : QStringLiteral("Paused");
}

double MprisPlayer::rate() const { return m_mpris->core()->speed(); }
void MprisPlayer::setRate(double rate) { m_mpris->core()->setSpeed(rate); }
qlonglong MprisPlayer::position() const { return m_mpris->core()->position() * 1000; }
bool MprisPlayer::hasBook() const { return !m_mpris->core()->current().isEmpty(); }

void MprisPlayer::Play()
{
    if (!m_mpris->core()->playing())
        m_mpris->core()->toggle();
}

void MprisPlayer::Pause()
{
    if (m_mpris->core()->playing())
        m_mpris->core()->toggle();
}

void MprisPlayer::PlayPause() { m_mpris->core()->toggle(); }
void MprisPlayer::Stop() { Pause(); }
void MprisPlayer::Next() { m_mpris->core()->nextChapter(); }
void MprisPlayer::Previous() { m_mpris->core()->previousChapter(); }
void MprisPlayer::Seek(qlonglong offset) { m_mpris->core()->seekTo(m_mpris->core()->position() + offset / 1000); }

void MprisPlayer::SetPosition(const QDBusObjectPath &track, qlonglong position)
{
    if (track.path().endsWith(m_mpris->core()->current().value(QStringLiteral("id")).toString()))
        m_mpris->core()->seekTo(position / 1000);
}
