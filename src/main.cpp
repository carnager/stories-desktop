#include "Core.h"
#include "LocalServer.h"
#include "Mpris.h"
#include "QrProvider.h"

#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QTimer>
#include <clocale>

#ifdef STORIES_DEV
#include "DevDriver.h"
#endif

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);
    QGuiApplication::setApplicationName(QStringLiteral("stories-desktop"));
    QGuiApplication::setApplicationDisplayName(QStringLiteral("Stories"));
    QGuiApplication::setOrganizationName(QStringLiteral("stories"));
    QGuiApplication::setDesktopFileName(QStringLiteral("stories-desktop"));
    // libmpv requires this.
    std::setlocale(LC_NUMERIC, "C");
    QQuickStyle::setStyle(QStringLiteral("Basic"));

    Core core;
    Mpris mpris(&core);
    LocalServer local;
    QObject::connect(&local, &LocalServer::ready, &core, [&core](const QString &url, const QString &password, const QString &library) {
        // On this computer the files can be played directly, without streaming.
        core.setLocalLibrary(library);
        core.login(url, password);
    });
    qmlRegisterSingletonInstance("Stories.Native", 1, 0, "Core", &core);
    qmlRegisterSingletonInstance("Stories.Native", 1, 0, "LocalServer", &local);
    qmlRegisterUncreatableType<BookModel>("Stories.Native", 1, 0, "BookModel", QStringLiteral("From Core"));

    QQmlApplicationEngine engine;
    engine.addImageProvider(QStringLiteral("qr"), new QrProvider);
    QObject::connect(&engine, &QQmlApplicationEngine::objectCreationFailed, &app, [] { QCoreApplication::exit(1); }, Qt::QueuedConnection);
    engine.loadFromModule("Stories", "Main");

#ifdef STORIES_DEV
    const QString script = qEnvironmentVariable("STORIES_DRIVE");
    if (!script.isEmpty() && !engine.rootObjects().isEmpty())
        new DevDriver(&core, &local, qobject_cast<QQuickWindow *>(engine.rootObjects().first()), script);
#endif
    return app.exec();
}
