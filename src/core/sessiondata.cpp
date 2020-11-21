/*
    This file is part of the KDE project
    SPDX-FileCopyrightText: 2000 Dawit Alemayehu <adawit@kde.org>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#include "sessiondata_p.h"

#include <QDir>
#include <QStandardPaths>
#include <QTextCodec>

#include <KConfigGroup>
#include <KLocalizedString>
#include <kprotocolmanager.h>
#include <KSharedConfig>

#include <kio/slaveconfig.h>
#include "http_slave_defaults.h"

namespace KIO
{

/********************************* SessionData ****************************/

class SessionData::SessionDataPrivate
{
public:
    SessionDataPrivate()
    {
        useCookie = true;
        initDone = false;
    }

    bool initDone;
    bool useCookie;
    QString charsets;
    QString language;
};

SessionData::SessionData()
    : d(new SessionDataPrivate)
{
}

SessionData::~SessionData()
{
    delete d;
}

void SessionData::configDataFor(MetaData &configData, const QString &proto,
                                const QString &)
{
    if ((proto.startsWith(QLatin1String("http"), Qt::CaseInsensitive)) ||
            (proto.startsWith(QLatin1String("webdav"), Qt::CaseInsensitive))) {
        if (!d->initDone) {
            reset();
        }

        // These might have already been set so check first
        // to make sure that we do not trumpt settings sent
        // by apps or end-user.
        if (configData[QStringLiteral("Cookies")].isEmpty()) {
            configData[QStringLiteral("Cookies")] = d->useCookie ? QStringLiteral("true") : QStringLiteral("false");
        }
        if (configData[QStringLiteral("Languages")].isEmpty()) {
            configData[QStringLiteral("Languages")] = d->language;
        }
        if (configData[QStringLiteral("Charsets")].isEmpty()) {
            configData[QStringLiteral("Charsets")] = d->charsets;
        }
        if (configData[QStringLiteral("CacheDir")].isEmpty()) {
            const QString httpCacheDir = QStandardPaths::writableLocation(QStandardPaths::GenericCacheLocation) + QLatin1String("/kio_http");
            QDir().mkpath(httpCacheDir);
            configData[QStringLiteral("CacheDir")] = httpCacheDir;
        }
        if (configData[QStringLiteral("UserAgent")].isEmpty()) {
            configData[QStringLiteral("UserAgent")] = KProtocolManager::defaultUserAgent();
        }
    }
}

void SessionData::reset()
{
    d->initDone = true;
    // Get Cookie settings...
    d->useCookie = KSharedConfig::openConfig(QStringLiteral("kcookiejarrc"), KConfig::NoGlobals)->
                   group("Cookie Policy").
                   readEntry("Cookies", true);

    d->language = KProtocolManager::acceptLanguagesHeader();
    d->charsets = QString::fromLatin1(QTextCodec::codecForLocale()->name()).toLower();
    KProtocolManager::reparseConfiguration();
}

}
