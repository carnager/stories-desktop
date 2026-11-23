#pragma once

#include <QDBusAbstractAdaptor>
#include <QDBusObjectPath>
#include <QObject>
#include <QVariantMap>

class Core;

/** Media keys, playerctl and desktop widgets, through MPRIS. */
class Mpris : public QObject {
    Q_OBJECT
public:
    explicit Mpris(Core *core);

    Core *core() const { return m_core; }
    QVariantMap metadata() const;

private:
    void notify(const QString &interface, const QVariantMap &changed);

    Core *m_core;
    qint64 m_lastMs = 0;
};

class MprisRoot : public QDBusAbstractAdaptor {
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.mpris.MediaPlayer2")
    Q_PROPERTY(bool CanQuit READ canQuit)
    Q_PROPERTY(bool CanRaise READ canRaise)
    Q_PROPERTY(bool HasTrackList READ hasTrackList)
    Q_PROPERTY(QString Identity READ identity)
    Q_PROPERTY(QString DesktopEntry READ desktopEntry)
    Q_PROPERTY(QStringList SupportedUriSchemes READ schemes)
    Q_PROPERTY(QStringList SupportedMimeTypes READ schemes)
public:
    explicit MprisRoot(Mpris *parent);
    bool canQuit() const { return true; }
    bool canRaise() const { return true; }
    bool hasTrackList() const { return false; }
    QString identity() const { return QStringLiteral("Stories"); }
    QString desktopEntry() const { return QStringLiteral("stories-desktop"); }
    QStringList schemes() const { return {}; }
public slots:
    void Raise();
    void Quit();
private:
    Mpris *m_mpris;
};

class MprisPlayer : public QDBusAbstractAdaptor {
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.mpris.MediaPlayer2.Player")
    Q_PROPERTY(QString PlaybackStatus READ status)
    Q_PROPERTY(double Rate READ rate WRITE setRate)
    Q_PROPERTY(double MinimumRate READ minRate)
    Q_PROPERTY(double MaximumRate READ maxRate)
    Q_PROPERTY(QVariantMap Metadata READ metadata)
    Q_PROPERTY(qlonglong Position READ position)
    Q_PROPERTY(double Volume READ volume)
    Q_PROPERTY(bool CanGoNext READ hasBook)
    Q_PROPERTY(bool CanGoPrevious READ hasBook)
    Q_PROPERTY(bool CanPlay READ hasBook)
    Q_PROPERTY(bool CanPause READ hasBook)
    Q_PROPERTY(bool CanSeek READ hasBook)
    Q_PROPERTY(bool CanControl READ canControl)
public:
    explicit MprisPlayer(Mpris *parent);
    QString status() const;
    double rate() const;
    void setRate(double rate);
    double minRate() const { return 0.5; }
    double maxRate() const { return 3.5; }
    QVariantMap metadata() const { return m_mpris->metadata(); }
    qlonglong position() const;
    double volume() const { return 1.0; }
    bool hasBook() const;
    bool canControl() const { return true; }
public slots:
    void Play();
    void Pause();
    void PlayPause();
    void Stop();
    void Next();
    void Previous();
    void Seek(qlonglong offset);
    void SetPosition(const QDBusObjectPath &track, qlonglong position);
signals:
    void Seeked(qlonglong position);
private:
    Mpris *m_mpris;
    friend class Mpris;
};
