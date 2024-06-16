/*
    This file is part of the KDE libraries
    SPDX-FileCopyrightText: 1999 Torben Weis <weis@kde.org>
    SPDX-FileCopyrightText: 2000 Waldo Bastain <bastain@kde.org>
    SPDX-FileCopyrightText: 2000 Dawit Alemayehu <adawit@kde.org>
    SPDX-FileCopyrightText: 2008 Jarosław Staniek <staniek@kde.org>
    SPDX-FileCopyrightText: 2022 Harald Sitter <sitter@kde.org>

    SPDX-License-Identifier: LGPL-2.0-only
*/

#ifndef KPROTOCOLMANAGER_P_H
#define KPROTOCOLMANAGER_P_H

#include <kiocore_export.h>

#include <QCache>
#include <QHostAddress>
#include <QMutex>
#include <QString>
#include <QUrl>

#include <KSharedConfig>

#include "kprotocolmanager.h"

class KIOCORE_EXPORT KProtocolManagerPrivate
{
public:
    KProtocolManagerPrivate();
    ~KProtocolManagerPrivate();
    void sync();

    QMutex mutex; // protects all member vars
    KSharedConfig::Ptr configPtr;
    QString modifiers;
    QString useragent;

    QMap<QString /*mimetype*/, QString /*protocol*/> protocolForArchiveMimetypes;

    /**
     * Returns the default user-agent value used for web browsing, for example
     * "Mozilla/5.0 (compatible; Konqueror/4.0; Linux; X11; i686; en_US) KHTML/4.0.1 (like Gecko)"
     *
     * @param keys can be any of the following:
     * @li 'o'    Show OS
     * @li 'v'    Show OS Version
     * @li 'p'    Show platform (only for X11)
     * @li 'm'    Show machine architecture
     * @li 'l'    Show language
     * @return the default user-agent value with the given @p keys
     */
    static QString defaultUserAgent(const QString &keys);

    /**
     * Returns system name, version and machine type, for example "Windows", "5.1", "i686".
     * This information can be used for constructing custom user-agent strings.
     *
     * @param systemName system name
     * @param systemVersion system version
     * @param machine machine type

     * @return true if system name, version and machine type has been provided
     */
    static bool getSystemNameVersionAndMachine(QString &systemName, QString &systemVersion, QString &machine);
};

#endif
