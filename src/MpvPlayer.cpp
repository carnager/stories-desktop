#include "MpvPlayer.h"

#include <mpv/client.h>

#include <QByteArrayList>
#include <QtGlobal>
#include <cmath>

namespace {
enum Observed : uint64_t { TimePos = 1, Pause, PlaylistPos, EofReached, Buffering };
}

MpvPlayer::MpvPlayer(QObject *parent)
    : QObject(parent)
{
    m_mpv = mpv_create();
    if (!m_mpv)
        qFatal("Could not start mpv");
    const char *options[][2] = {
        {"config", "no"}, {"terminal", "no"}, {"video", "no"}, {"vo", "null"}, {"audio-display", "no"},
        {"ytdl", "no"}, {"load-scripts", "no"}, {"input-default-bindings", "no"}, {"input-vo-keyboard", "no"},
        {"idle", "yes"}, {"pause", "yes"}, {"keep-open", "yes"}, {"gapless-audio", "weak"},
        {"audio-client-name", "Stories"}, {"hr-seek", "yes"}, {"cache", "yes"}, {"resume-playback", "no"},
        {"save-position-on-quit", "no"}, {"replaygain", "no"},
    };
    for (const auto &option : options)
        mpv_set_option_string(m_mpv, option[0], option[1]);
#ifdef STORIES_DEV
    if (qEnvironmentVariableIsSet("STORIES_NULL_AUDIO"))
        mpv_set_option_string(m_mpv, "ao", "null");
#endif
    if (mpv_initialize(m_mpv) < 0)
        qFatal("Could not initialize mpv");
    mpv_observe_property(m_mpv, TimePos, "time-pos", MPV_FORMAT_DOUBLE);
    mpv_observe_property(m_mpv, Pause, "pause", MPV_FORMAT_FLAG);
    mpv_observe_property(m_mpv, PlaylistPos, "playlist-pos", MPV_FORMAT_INT64);
    mpv_observe_property(m_mpv, EofReached, "eof-reached", MPV_FORMAT_FLAG);
    mpv_observe_property(m_mpv, Buffering, "paused-for-cache", MPV_FORMAT_FLAG);
    mpv_set_wakeup_callback(m_mpv, [](void *self) {
        QMetaObject::invokeMethod(static_cast<MpvPlayer *>(self), "drain", Qt::QueuedConnection);
    }, this);
}

MpvPlayer::~MpvPlayer()
{
    mpv_set_wakeup_callback(m_mpv, nullptr, nullptr);
    mpv_terminate_destroy(m_mpv);
}

void MpvPlayer::command(const QStringList &args)
{
    QByteArrayList bytes;
    QList<const char *> pointers;
    for (const QString &arg : args)
        bytes << arg.toUtf8();
    for (const QByteArray &b : std::as_const(bytes))
        pointers << b.constData();
    pointers << nullptr;
    mpv_command_async(m_mpv, 0, pointers.data());
}

void MpvPlayer::setStart(qint64 offsetMs)
{
    const QByteArray start = offsetMs > 0 ? QByteArray::number(offsetMs / 1000.0, 'f', 3) : QByteArray("none");
    mpv_set_property_string(m_mpv, "start", start.constData());
}

void MpvPlayer::load(const QStringList &sources, int index, qint64 offsetMs, bool play, const QString &authorization)
{
    const QByteArray header = authorization.isEmpty() ? QByteArray() : "Authorization: " + authorization.toUtf8();
    mpv_set_property_string(m_mpv, "http-header-fields", header.constData());
    setPaused(!play);
    command({QStringLiteral("stop")});
    for (const QString &source : sources)
        command({QStringLiteral("loadfile"), source, QStringLiteral("append")});
    setStart(offsetMs);
    m_track = index;
    m_trackMs = offsetMs;
    m_loaded = true;
    command({QStringLiteral("playlist-play-index"), QString::number(index)});
    emit trackChanged();
    emit positionChanged();
}

void MpvPlayer::seek(int index, qint64 offsetMs)
{
    if (index != m_track) {
        setStart(offsetMs);
        m_track = index;
        command({QStringLiteral("playlist-play-index"), QString::number(index)});
        emit trackChanged();
    } else {
        command({QStringLiteral("seek"), QString::number(offsetMs / 1000.0, 'f', 3), QStringLiteral("absolute+exact")});
    }
    m_trackMs = offsetMs;
    emit positionChanged();
}

void MpvPlayer::setPaused(bool paused)
{
    int flag = paused ? 1 : 0;
    mpv_set_property(m_mpv, "pause", MPV_FORMAT_FLAG, &flag);
}

void MpvPlayer::setSpeed(double speed)
{
    mpv_set_property(m_mpv, "speed", MPV_FORMAT_DOUBLE, &speed);
}

void MpvPlayer::setVolume(double percent)
{
    mpv_set_property(m_mpv, "volume", MPV_FORMAT_DOUBLE, &percent);
}

void MpvPlayer::stop()
{
    setPaused(true);
    command({QStringLiteral("stop")});
    m_loaded = false;
}

void MpvPlayer::drain()
{
    while (m_mpv) {
        mpv_event *event = mpv_wait_event(m_mpv, 0);
        if (event->event_id == MPV_EVENT_NONE)
            return;
        switch (event->event_id) {
        case MPV_EVENT_FILE_LOADED:
            // Only the file it was meant for starts part-way in.
            mpv_set_property_string(m_mpv, "start", "none");
            break;
        case MPV_EVENT_END_FILE: {
            const auto *end = static_cast<mpv_event_end_file *>(event->data);
            if (end->reason == MPV_END_FILE_REASON_ERROR)
                emit failed(QString::fromUtf8(mpv_error_string(end->error)));
            break;
        }
        case MPV_EVENT_PROPERTY_CHANGE: {
            const auto *property = static_cast<mpv_event_property *>(event->data);
            switch (event->reply_userdata) {
            case TimePos:
                if (property->format == MPV_FORMAT_DOUBLE) {
                    const auto ms = qint64(std::llround(*static_cast<double *>(property->data) * 1000));
                    if (qAbs(ms - m_trackMs) >= 200) {
                        m_trackMs = qMax<qint64>(ms, 0);
                        emit positionChanged();
                    }
                }
                break;
            case Pause:
                if (property->format == MPV_FORMAT_FLAG) {
                    m_paused = *static_cast<int *>(property->data);
                    emit pausedChanged();
                }
                break;
            case PlaylistPos:
                if (property->format == MPV_FORMAT_INT64) {
                    const int track = int(*static_cast<int64_t *>(property->data));
                    if (track >= 0 && track != m_track) {
                        m_track = track;
                        emit trackChanged();
                    }
                }
                break;
            case EofReached:
                if (property->format == MPV_FORMAT_FLAG && *static_cast<int *>(property->data))
                    emit ended();
                break;
            case Buffering:
                if (property->format == MPV_FORMAT_FLAG)
                    emit bufferingChanged(*static_cast<int *>(property->data));
                break;
            }
            break;
        }
        default:
            break;
        }
    }
}
