#pragma once

// Built only with -DSTORIES_DEV=ON. STORIES_DRIVE holds ';'-separated steps
// for testing the real app without a display or a person.

#include "Core.h"
#include "LocalServer.h"

#include <QCoreApplication>
#include <QJsonDocument>
#include <QJsonObject>
#include <QQuickWindow>
#include <QTimer>
#include <cstdio>

class DevDriver : public QObject {
public:
    DevDriver(Core *core, LocalServer *local, QQuickWindow *window, const QString &script)
        : QObject(core), m_core(core), m_local(local), m_window(window), m_steps(script.split(u';', Qt::SkipEmptyParts))
    {
        QTimer::singleShot(200, this, [this] { next(); });
    }

private:
    QString idOf(const QString &title) const
    {
        BookModel *model = m_core->books();
        const int role = model->roleNames().key("title");
        const int idRole = model->roleNames().key("bookId");
        for (int i = 0; i < model->rowCount(); ++i) {
            if (model->data(model->index(i), role).toString() == title)
                return model->data(model->index(i), idRole).toString();
        }
        return {};
    }

    void status() const
    {
        const QVariantMap newer = m_core->newer();
        const QJsonObject o{{QStringLiteral("signedIn"), m_core->signedIn()}, {QStringLiteral("server"), m_core->serverUrl()},
                            {QStringLiteral("books"), m_core->books()->count()}, {QStringLiteral("book"), m_core->current().value(QStringLiteral("title")).toString()},
                            {QStringLiteral("playing"), m_core->playing()}, {QStringLiteral("position"), m_core->position()},
                            {QStringLiteral("chapter"), m_core->chapter()}, {QStringLiteral("speed"), m_core->speed()},
                            {QStringLiteral("newerAt"), newer.isEmpty() ? -1 : newer.value(QStringLiteral("bookMs")).toLongLong()},
                            {QStringLiteral("error"), m_core->error()}, {QStringLiteral("localError"), m_local->error()},
                            {QStringLiteral("share"), m_core->shareUrl()}};
        printf("%s\n", QJsonDocument(o).toJson(QJsonDocument::Compact).constData());
        fflush(stdout);
    }

    void next()
    {
        if (m_steps.isEmpty())
            return QCoreApplication::quit();
        const QString step = m_steps.takeFirst().trimmed();
        const QString word = step.section(u' ', 0, 0);
        const QString rest = step.section(u' ', 1);
        int delay = 60;
        if (word == u"wait")
            delay = rest.toInt();
        else if (word == u"setup")
            m_local->setup(rest.section(u'|', 0, 0), rest.section(u'|', 1, 1), rest.section(u'|', 2, 2));
        else if (word == u"login")
            m_core->login(rest.section(u'|', 0, 0), rest.section(u'|', 1));
        else if (word == u"play")
            m_core->play(idOf(rest));
        else if (word == u"open")
            m_window->setProperty("openBook", idOf(rest));
        else if (word == u"close")
            m_window->setProperty("openBook", QString());
        else if (word == u"expand")
            m_window->setProperty("expanded", rest != u"off");
        else if (word == u"seek")
            m_core->seekTo(rest.toLongLong());
        else if (word == u"toggle")
            m_core->toggle();
        else if (word == u"nextchapter")
            m_core->nextChapter();
        else if (word == u"speed")
            m_core->setSpeed(rest.toDouble());
        else if (word == u"bookmark")
            m_core->addBookmark();
        else if (word == u"finish")
            m_core->markFinished(idOf(rest), true);
        else if (word == u"accept")
            m_core->acceptNewer();
        else if (word == u"decline")
            m_core->declineNewer();
        else if (word == u"logout")
            m_core->logout();
        else if (word == u"dialog") {
            if (QObject *dialog = m_window->findChild<QObject *>(rest))
                QMetaObject::invokeMethod(dialog, "open");
        } else if (word == u"status")
            status();
        else if (word == u"shot")
            m_window->grabWindow().save(rest);
        else if (word == u"quit")
            return QCoreApplication::quit();
        QTimer::singleShot(delay, this, [this] { next(); });
    }

    Core *m_core;
    LocalServer *m_local;
    QQuickWindow *m_window;
    QStringList m_steps;
};
