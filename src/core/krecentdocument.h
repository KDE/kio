/* -*- c++ -*-
    SPDX-FileCopyrightText: 2000 Daniel M. Duley <mosfet@kde.org>

    SPDX-License-Identifier: BSD-2-Clause
*/

#ifndef __KRECENTDOCUMENT_H
#define __KRECENTDOCUMENT_H

#include "kiocore_export.h"

#include <QString>
#include <QUrl>

/**
 * @class KRecentDocument krecentdocument.h <KRecentDocument>
 *
 * Manage the "Recent Document Menu" entries displayed by
 * applications such as Kicker and Konqueror.
 *
 * These entries are automatically generated .desktop files pointing
 * to the current application and document.  You should call the
 * static add() method whenever the user opens or saves a new
 * document if you want it to show up in the menu.
 *
 * You don't have to worry about this if you are using
 * QFileDialog to open and save documents, as the KDE implementation
 * (KFileWidget) already calls this class.  User defined limits on the maximum
 * number of documents to save, etc... are all automatically handled.
 *
 * @author Daniel M. Duley <mosfet@kde.org>
 */
class KIOCORE_EXPORT KRecentDocument
{
public:

    /**
     *
     * Return a list of absolute paths to recent document .desktop files,
     * sorted by date.
     *
     */
    static QStringList recentDocuments();

    /**
     * Add a new item to the Recent Document menu.
     *
     * @param url The url to add.
     */
    static void add(const QUrl &url);

    /**
     * Add a new item to the Recent Document menu, specifying the application to open it with.
     * The above add() method uses QCoreApplication::applicationName() for the app name,
     * which isn't always flexible enough.
     * This method is used when an application launches another one to open a document.
     *
     * @param url The url to add.
     * @param desktopEntryName The desktopEntryName of the service to use for opening this document.
     */
    static void add(const QUrl &url, const QString &desktopEntryName);

#if KIOCORE_ENABLE_DEPRECATED_SINCE(5, 0)
    /**
     *
     * Add a new item to the Recent Document menu. Calls add( url ).
     *
     * @param documentStr The full path to the document or URL to add.
     * @param isURL Set to @p true if @p documentStr is an URL and not a local file path.
     * @deprecated Since 5.0, call add(QUrl(str)) if isURL=true, and add(QUrl::fromLocalFile(str)) if isURL=false.
     */
    KIOCORE_DEPRECATED_VERSION(5, 0, "Use KRecentDocument::add(const QUrl &)")
    static void add(const QString &documentStr, bool isUrl = false)
    {
        if (isUrl) {
            add(QUrl(documentStr));
        } else {
            add(QUrl::fromLocalFile(documentStr));
        }
    }
#endif

    /**
     * Clear the recent document menu of all entries.
     */
    static void clear();

    /**
     * Returns the maximum amount of recent document entries allowed.
     */
    static int maximumItems();

    /**
     * Returns the path to the directory where recent document .desktop files
     * are stored.
     */
    static QString recentDocumentDirectory();
};

#endif
