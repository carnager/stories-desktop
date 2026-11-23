#pragma once

#include <QObject>
#include <QStringList>

struct mpv_handle;

/** libmpv as an audio engine: a book's files as a playlist. */
class MpvPlayer : public QObject {
    Q_OBJECT
public:
    explicit MpvPlayer(QObject *parent = nullptr);
    ~MpvPlayer() override;

    void load(const QStringList &sources, int index, qint64 offsetMs, bool play, const QString &authorization);
    void seek(int index, qint64 offsetMs);
    void setPaused(bool paused);
    void setSpeed(double speed);
    void setVolume(double percent);
    void stop();

    bool paused() const { return m_paused; }
    bool loaded() const { return m_loaded; }
    int track() const { return m_track; }
    qint64 trackMs() const { return m_trackMs; }

signals:
    void positionChanged();
    void trackChanged();
    void pausedChanged();
    void bufferingChanged(bool buffering);
    void ended();
    void failed(const QString &message);

private slots:
    void drain();

private:
    void command(const QStringList &args);
    void setStart(qint64 offsetMs);

    mpv_handle *m_mpv = nullptr;
    bool m_paused = true, m_loaded = false;
    int m_track = 0;
    qint64 m_trackMs = 0;
};
