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

        if (configData[QStringLiteral("UserAgent")].isEmpty()) {
            configData[QStringLiteral("UserAgent")] = KProtocolManagerPrivate::defaultUserAgent();
        }
    }
}

void SessionData::reset()
{
    d->initDone = true;

    KProtocolManager::reparseConfiguration();
}

}

#include "moc_sessiondata_p.cpp"
