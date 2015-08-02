/* This file is part of the KDE libraries
    Copyright (C) 2015 David Faure <faure@kde.org>

    This library is free software; you can redistribute it and/or modify
    it under the terms of the GNU Lesser General Public License as published by
    the Free Software Foundation; either version 2 of the License or (at
    your option) version 3 or, at the discretion of KDE e.V. (which shall
    act as a proxy as in section 14 of the GPLv3), any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Lesser General Public License
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.
*/

#include <QCoreApplication>
#include <QHash>
#include <QDBusMessage>
#include <QDBusConnectionInterface>

#include <KDBusService>
#include <KDEDModule>
#include <KPluginLoader>
#include <KPluginFactory>
#include <KPluginMetaData>

#include <QLoggingCategory>

Q_DECLARE_LOGGING_CATEGORY(KIOD_CATEGORY);

#if QT_VERSION >= QT_VERSION_CHECK(5, 4, 0)
// logging category for this framework, default: log stuff >= warning
Q_LOGGING_CATEGORY(KIOD_CATEGORY, "kf5.kiod", QtWarningMsg)
#else
Q_LOGGING_CATEGORY(KIOD_CATEGORY, "kf5.kiod")
#endif

class KIOD
{
public:
    KDEDModule *loadModule(const QString &name);

private:
    QHash<QString, KDEDModule *> m_modules;
};

KDEDModule *KIOD::loadModule(const QString &name)
{
    // Make sure this method is only called with valid module names.
    Q_ASSERT(name.indexOf('/') == -1);

    KDEDModule *module = m_modules.value(name, 0);
    if (module) {
        return module;
    }

    KPluginLoader loader("kf5/kiod/" + name);
    KPluginFactory *factory = loader.factory();
    if (factory) {
        module = factory->create<KDEDModule>();
    }
    if (!module) {
        qCWarning(KIOD_CATEGORY) << "Error loading plugin:" << loader.errorString();
        return module;
    }
    module->setModuleName(name); // makes it register to DBus
    m_modules.insert(name, module);

    return module;
}

Q_GLOBAL_STATIC(KIOD, self);

// on-demand module loading
// this function is called by the D-Bus message processing function before
// calls are delivered to objects
void messageFilter(const QDBusMessage &message)
{
    const QString name = KDEDModule::moduleForMessage(message);
    if (name.isEmpty()) {
        return;
    }

    KDEDModule *module = self()->loadModule(name);
    if (!module) {
        qCWarning(KIOD_CATEGORY) << "Failed to load module for" << name;
    }
    Q_UNUSED(module);
}

extern Q_DBUS_EXPORT void qDBusAddSpyHook(void (*)(const QDBusMessage&));

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    app.setApplicationName("kiod5");
    app.setOrganizationDomain("kde.org");
    KDBusService service(KDBusService::Unique);

    QDBusConnectionInterface *bus = QDBusConnection::sessionBus().interface();
    // Also register as all the names we should respond to (org.kde.kssld, org.kde.kcookiejar, etc.)
    // so that the calling code is independent from the physical "location" of the service.
    const QVector<KPluginMetaData> plugins = KPluginLoader::findPlugins("kf5/kiod");
    foreach (const KPluginMetaData &metaData, plugins) {
        const QString serviceName = metaData.rawData().value("X-KDE-DBus-ServiceName").toString();
        if (serviceName.isEmpty()) {
            qCWarning(KIOD_CATEGORY) << "No X-KDE-DBus-ServiceName found in" << metaData.fileName();
            continue;
        }
        if (!bus->registerService(serviceName)) {
            qCWarning(KIOD_CATEGORY) << "Couldn't register name" << serviceName << "with DBUS - another process owns it already!";
        }
    }

    qDBusAddSpyHook(messageFilter);

    return app.exec();
}

