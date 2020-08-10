/*
    This file is part of the KDE libraries
    SPDX-FileCopyrightText: 2015 David Faure <faure@kde.org>

    SPDX-License-Identifier: LGPL-2.0-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/

#include <QApplication>
#include <QHash>
#include <QDBusMessage>
#include <QDBusConnectionInterface>

#include <KDBusService>
#include <KDEDModule>
#include <KPluginLoader>
#include <KPluginFactory>
#include <KPluginMetaData>

#include <QLoggingCategory>
Q_DECLARE_LOGGING_CATEGORY(KIOD_CATEGORY)
Q_LOGGING_CATEGORY(KIOD_CATEGORY, "kf.kio.kiod")

class KIOD : public QObject
{
    Q_OBJECT
public:
    ~KIOD() override;

public Q_SLOTS:
    void loadModule(const QString &name);

private:
    QHash<QString, KDEDModule *> m_modules;
};

void KIOD::loadModule(const QString &name)
{
    // Make sure this method is only called with valid module names.
    Q_ASSERT(name.indexOf(QLatin1Char('/')) == -1);

    KDEDModule *module = m_modules.value(name, nullptr);
    if (module) {
        return;
    }

    qCDebug(KIOD_CATEGORY) << "loadModule" << name;
    KPluginLoader loader(QLatin1String("kf5/kiod/") + name);
    KPluginFactory *factory = loader.factory();
    if (factory) {
        module = factory->create<KDEDModule>();
    }
    if (!module) {
        qCWarning(KIOD_CATEGORY) << "Error loading plugin:" << loader.errorString();
        return;
    }
    module->setModuleName(name); // makes it register to DBus
    m_modules.insert(name, module);
}

KIOD::~KIOD()
{
    qDeleteAll(m_modules);
}

Q_GLOBAL_STATIC(KIOD, self)

// on-demand module loading
// this function is called by the D-Bus message processing function before
// calls are delivered to objects
static void messageFilter(const QDBusMessage &message)
{
    const QString name = KDEDModule::moduleForMessage(message);
    if (name.isEmpty()) {
        return;
    }

    self()->loadModule(name);
}

extern Q_DBUS_EXPORT void qDBusAddSpyHook(void (*)(const QDBusMessage&));

int main(int argc, char *argv[])
{
#ifdef Q_OS_MACOS
    // do the "early" step to make this an "agent" application:
    // set the LSUIElement InfoDict key programmatically.
    extern void makeAgentApplication();
    makeAgentApplication();
#endif
    qunsetenv("SESSION_MANAGER"); // disable session management

    QApplication app(argc, argv); // GUI needed for kpasswdserver's dialogs
    app.setApplicationName(QStringLiteral("kiod5"));
    app.setOrganizationDomain(QStringLiteral("kde.org"));
    app.setQuitOnLastWindowClosed(false);
    KDBusService service(KDBusService::Unique);

    QDBusConnectionInterface *bus = QDBusConnection::sessionBus().interface();
    // Also register as all the names we should respond to (org.kde.kssld, org.kde.kcookiejar, etc.)
    // so that the calling code is independent from the physical "location" of the service.
    const QVector<KPluginMetaData> plugins = KPluginLoader::findPlugins(QStringLiteral("kf5/kiod"));
    for (const KPluginMetaData &metaData : plugins) {
        const QString serviceName = metaData.rawData().value(QStringLiteral("X-KDE-DBus-ServiceName")).toString();
        if (serviceName.isEmpty()) {
            qCWarning(KIOD_CATEGORY) << "No X-KDE-DBus-ServiceName found in" << metaData.fileName();
            continue;
        }
        if (!bus->registerService(serviceName)) {
            qCWarning(KIOD_CATEGORY) << "Couldn't register name" << serviceName << "with DBUS - another process owns it already!";
        }
    }

    self(); // create it in this thread
    qDBusAddSpyHook(messageFilter);

#ifdef Q_OS_MACOS
    // In the case of kiod5 we need to confirm the agent nature,
    // possibly because of how things have been set up after creating
    // the QApplication instance. Failure to do this will disable
    // text input into dialogs we may post.
    extern void setAgentActivationPolicy();
    setAgentActivationPolicy();
#endif
    return app.exec();
}

#include "kiod_main.moc"
