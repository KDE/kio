/*
    SPDX-FileCopyrightText: 1997 Torben Weis <weis@stud.uni-frankfurt.de>
    SPDX-FileCopyrightText: 1999 Dirk Mueller <mueller@kde.org>
    Portions SPDX-FileCopyrightText: 1999 Preston Brown <pbrown@kde.org>
    SPDX-FileCopyrightText: 2007 Pino Toscano <pino@kde.org>
    SPDX-FileCopyrightText: 2023 Harald Sitter <sitter@kde.org>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#include "openwith.h"

#include <QFileInfo>
#include <QStandardPaths>

#include <KConfigGroup>
#include <KDesktopFile>
#include <KLocalizedString>
#include <KSharedConfig>

#include "desktopexecparser.h"
#include "kiocoredebug.h"

namespace
{

QString simplifiedExecLineFromService(const QString &fullExec)
{
    QString exec = fullExec;
    exec.remove(QLatin1String("%u"), Qt::CaseInsensitive);
    exec.remove(QLatin1String("%f"), Qt::CaseInsensitive);
    exec.remove(QLatin1String("-caption %c"));
    exec.remove(QLatin1String("-caption \"%c\""));
    exec.remove(QLatin1String("%i"));
    exec.remove(QLatin1String("%m"));
    return exec.simplified();
}

void addToMimeAppsList(const QString &serviceId /*menu id or storage id*/, const QString &qMimeType)
{
    KSharedConfig::Ptr profile = KSharedConfig::openConfig(QStringLiteral("mimeapps.list"), KConfig::NoGlobals, QStandardPaths::GenericConfigLocation);

    // Save the default application according to mime-apps-spec 1.0
    KConfigGroup defaultApp(profile, QStringLiteral("Default Applications"));
    defaultApp.writeXdgListEntry(qMimeType, QStringList(serviceId));

    KConfigGroup addedApps(profile, QStringLiteral("Added Associations"));
    QStringList apps = addedApps.readXdgListEntry(qMimeType);
    apps.removeAll(serviceId);
    apps.prepend(serviceId); // make it the preferred app
    addedApps.writeXdgListEntry(qMimeType, apps);

    profile->sync();

    // Also make sure the "auto embed" setting for this MIME type is off
    KSharedConfig::Ptr fileTypesConfig = KSharedConfig::openConfig(QStringLiteral("filetypesrc"), KConfig::NoGlobals);
    fileTypesConfig->group(QStringLiteral("EmbedSettings")).writeEntry(QStringLiteral("embed-") + qMimeType, false);
    fileTypesConfig->sync();
}

} // namespace

namespace KIO
{

OpenWith::AcceptResult OpenWith::accept(KService::Ptr &service,
                                        const QString &typedExec,
                                        bool remember,
                                        const QString &mimeType,
                                        bool openInTerminal,
                                        bool lingerTerminal,
                                        bool saveNewApps)
{
    QString fullExec(typedExec);

    KConfigGroup confGroup(KSharedConfig::openConfig(), QStringLiteral("General"));
    const QString preferredTerminal = confGroup.readPathEntry("TerminalApplication", QStringLiteral("konsole"));

    QString serviceName;
    QString initialServiceName;
    QString configPath;
    QString serviceExec;
    bool rebuildSycoca = false;
    if (!service) {
        // No service selected - check the command line

        // Find out the name of the service from the command line, removing args and paths
        serviceName = KIO::DesktopExecParser::executableName(typedExec);
        if (serviceName.isEmpty()) {
            return {.accept = false, .error = i18n("Could not extract executable name from '%1', please type a valid program name.", serviceName)};
        }
        initialServiceName = serviceName;
        // Also remember the executableName with a path, if any, for the
        // check that the executable exists.
        qCDebug(KIO_CORE) << "initialServiceName=" << initialServiceName;
        int i = 1; // We have app, app-2, app-3... Looks better for the user.
        bool ok = false;
        // Check if there's already a service by that name, with the same Exec line
        do {
            qCDebug(KIO_CORE) << "looking for service" << serviceName;
            KService::Ptr serv = KService::serviceByDesktopName(serviceName);
            ok = !serv; // ok if no such service yet
            // also ok if we find the exact same service (well, "kwrite" == "kwrite %U")
            if (serv && !serv->noDisplay() /* #297720 */) {
                if (serv->isApplication()) {
                    qCDebug(KIO_CORE) << "typedExec=" << typedExec << "serv->exec=" << serv->exec()
                                      << "simplifiedExecLineFromService=" << simplifiedExecLineFromService(fullExec);
                    serviceExec = simplifiedExecLineFromService(serv->exec());
                    if (typedExec == serviceExec) {
                        ok = true;
                        service = serv;
                        qCDebug(KIO_CORE) << "OK, found identical service: " << serv->entryPath();
                    } else {
                        qCDebug(KIO_CORE) << "Exec line differs, service says:" << serviceExec;
                        configPath = serv->entryPath();
                        serviceExec = serv->exec();
                    }
                } else {
                    qCDebug(KIO_CORE) << "Found, but not an application:" << serv->entryPath();
                }
            }
            if (!ok) { // service was found, but it was different -> keep looking
                ++i;
                serviceName = initialServiceName + QLatin1Char('-') + QString::number(i);
            }
        } while (!ok);
    }
    if (service) {
        // Existing service selected
        serviceName = service->name();
        initialServiceName = serviceName;
        fullExec = service->exec();
    } else {
        const QString binaryName = KIO::DesktopExecParser::executablePath(typedExec);
        qCDebug(KIO_CORE) << "binaryName=" << binaryName;
        // Ensure that the typed binary name actually exists (#81190)
        if (QStandardPaths::findExecutable(binaryName).isEmpty()) {
            // QStandardPaths::findExecutable does not find non-executable files.
            // Give a better error message for the case of a existing but non-executable file.
            // https://bugs.kde.org/show_bug.cgi?id=437880
            const QString msg = QFileInfo::exists(binaryName)
                ? xi18nc("@info", "<filename>%1</filename> does not appear to be an executable program.", binaryName)
                : xi18nc("@info", "<filename>%1</filename> was not found; please enter a valid path to an executable program.", binaryName);
            return {.accept = false, .error = msg};
        }
    }

    if (service && openInTerminal != service->terminal()) {
        service = nullptr; // It's not exactly this service we're running
    }

    qCDebug(KIO_CORE) << "bRemember=" << remember << "service found=" << service;
    if (service) {
        if (remember) {
            // Associate this app with qMimeType in mimeapps.list
            Q_ASSERT(!mimeType.isEmpty()); // we don't show the remember checkbox otherwise
            addToMimeAppsList(service->storageId(), mimeType);
            rebuildSycoca = true;
        }
    } else {
        const bool createDesktopFile = remember || saveNewApps;
        if (!createDesktopFile) {
            // Create temp service
            if (configPath.isEmpty()) {
                service = new KService(initialServiceName, fullExec, QString());
            } else {
                if (!typedExec.contains(QLatin1String("%u"), Qt::CaseInsensitive) && !typedExec.contains(QLatin1String("%f"), Qt::CaseInsensitive)) {
                    int index = serviceExec.indexOf(QLatin1String("%u"), 0, Qt::CaseInsensitive);
                    if (index == -1) {
                        index = serviceExec.indexOf(QLatin1String("%f"), 0, Qt::CaseInsensitive);
                    }
                    if (index > -1) {
                        fullExec += QLatin1Char(' ') + QStringView(serviceExec).mid(index, 2);
                    }
                }
                // qDebug() << "Creating service with Exec=" << fullExec;
                service = new KService(configPath);
                service->setExec(fullExec);
            }
            if (openInTerminal) {
                service->setTerminal(true);
                // only add --noclose when we are sure it is konsole we're using
                if (preferredTerminal == QLatin1String("konsole") && lingerTerminal) {
                    service->setTerminalOptions(QStringLiteral("--noclose"));
                }
            }
        } else {
            // If we got here, we can't seem to find a service for what they wanted. Create one.

            QString menuId;
#ifdef Q_OS_WIN32
            // on windows, do not use the complete path, but only the default name.
            serviceName = QFileInfo(serviceName).fileName();
#endif
            QString newPath = KService::newServicePath(false /* ignored argument */, serviceName, &menuId);
            // qDebug() << "Creating new service" << serviceName << "(" << newPath << ")" << "menuId=" << menuId;

            KDesktopFile desktopFile(newPath);
            KConfigGroup cg = desktopFile.desktopGroup();
            cg.writeEntry("Type", "Application");

            // For the user visible name, use the executable name with any
            // arguments appended, but with desktop-file specific expansion
            // arguments removed. This is done to more clearly communicate the
            // actual command used to the user and makes it easier to
            // distinguish things like "qdbus".
            QString name = KIO::DesktopExecParser::executableName(fullExec);
            auto view = QStringView{fullExec}.trimmed();
            int index = view.indexOf(QLatin1Char(' '));
            if (index > 0) {
                name.append(view.mid(index));
            }
            cg.writeEntry("Name", simplifiedExecLineFromService(name));

            // if we select a binary for a scheme handler, then it's safe to assume it can handle URLs
            if (mimeType.startsWith(QLatin1String("x-scheme-handler/"))) {
                if (!typedExec.contains(QLatin1String("%u"), Qt::CaseInsensitive) && !typedExec.contains(QLatin1String("%f"), Qt::CaseInsensitive)) {
                    fullExec += QStringLiteral(" %u");
                }
            }

            cg.writeEntry("Exec", fullExec);
            cg.writeEntry("NoDisplay", true); // don't make it appear in the K menu
            if (openInTerminal) {
                cg.writeEntry("Terminal", true);
                // only add --noclose when we are sure it is konsole we're using
                if (preferredTerminal == QLatin1String("konsole") && lingerTerminal) {
                    cg.writeEntry("TerminalOptions", "--noclose");
                }
            }
            if (!mimeType.isEmpty()) {
                cg.writeXdgListEntry("MimeType", QStringList() << mimeType);
            }
            cg.sync();

            if (!mimeType.isEmpty()) {
                addToMimeAppsList(menuId, mimeType);
                rebuildSycoca = true;
            }
            service = new KService(newPath);
        }
    }

    return {.accept = true, .error = {}, .rebuildSycoca = rebuildSycoca};
}

} // namespace KIO
