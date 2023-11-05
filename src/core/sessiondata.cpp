/*
    This file is part of the KDE project
    SPDX-FileCopyrightText: 2000 Dawit Alemayehu <adawit@kde.org>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#include "sessiondata_p.h"

#include <QDir>
#include <QStandardPaths>

#include <KConfigGroup>
#include <KLocalizedString>
#include <KSharedConfig>
#include <kprotocolmanager_p.h>

namespace KIO
{
/********************************* SessionData ****************************/

class SessionData::SessionDataPrivate
{
public:
    SessionDataPrivate()
    {
        initDone = false;
    }

    bool initDone;
    QString charsets;
    QString language;
};

SessionData::SessionData()
    : d(new SessionDataPrivate)
{
}

SessionData::~SessionData() = default;

void SessionData::configDataFor(MetaData &configData, const QString &proto, const QString &)
{
    if ((proto.startsWith(QLatin1String("http"), Qt::CaseInsensitive)) || (proto.startsWith(QLatin1String("webdav"), Qt::CaseInsensitive))) {
        if (!d->initDone) {
            reset();
        }

        // These might have already been set so check first
        // to make sure that we do not trumpt settings sent
        // by apps or end-user.
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
            configData[QStringLiteral("UserAgent")] = KProtocolManagerPrivate::defaultUserAgent(QString());
        }
    }
}

void SessionData::reset()
{
    d->initDone = true;

    d->language = KProtocolManager::acceptLanguagesHeader();
    d->charsets = QStringLiteral("utf-8");
    KProtocolManager::reparseConfiguration();
}

}

#include "moc_sessiondata_p.cpp"
