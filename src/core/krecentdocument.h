/* -*- c++ -*-
    SPDX-FileCopyrightText: 2000 Daniel M. Duley <mosfet@kde.org>
    SPDX-FileCopyrightText: 2022 Méven Car <meven.car@kdemail.net>

    SPDX-License-Identifier: BSD-2-Clause
*/

#ifndef __KRECENTDOCUMENT_H
#define __KRECENTDOCUMENT_H

#include "kiocore_export.h"

#include <QDateTime>
#include <QString>
#include <QUrl>

/*!
 * \class KRecentDocument
 * \inmodule KIOCore
 *
 * \brief Manage the "Recent Document Menu" entries displayed by
 * applications such as Kicker and Konqueror.
 *
 * These entries are automatically generated .desktop files pointing
 * to the current application and document.  You should call the
 * static add() method whenever the user opens or saves a new
 * document if you want it to show up in the menu.
 *
 * It also stores history following xdg specification.
 * Ref: https://www.freedesktop.org/wiki/Specifications/desktop-bookmark-spec
 * This allows cross-framework file history sharing.
 * I.e Gtk Apps can access files recently opened by KDE Apps.
 *
 * You don't have to worry about this if you are using
 * QFileDialog to open and save documents, as the KDE implementation
 * (KFileWidget) already calls this class.  User defined limits on the maximum
 * number of documents to save, etc... are all automatically handled.
 */
class KIOCORE_EXPORT KRecentDocument
{
public:
    /*!
     * Usage group for a file to bookmark in recently-used.xbel file
     *
     * From spec https://www.freedesktop.org/wiki/Specifications/desktop-bookmark-spec/#appendixb:registeredgroupnames
     *
     * \value Development A bookmark related to a development environment
     * \value Office A bookmark related to an office type document or folder
     * \value Database A bookmark related to a database application; Office; relates to Development
     * \value Email A bookmark related to an email application relates to Office
     * \value Presentation A bookmark related to a presentation application  relates to Office
     * \value Spreadsheet A bookmark related to a spreadsheet application relates to Office
     * \value WordProcessor A bookmark related to a word processing application  relates to Office
     * \value Graphics A bookmark related to a graphical application
     * \value TextEditor A bookmark related to a text editor
     * \value Viewer A bookmark related to any kind of file viewer
     * \value Archive A bookmark related to an archive file
     * \value Multimedia A bookmark related to a multimedia file or application
     * \value Audio A bookmark related to an audio file or application relates to Multimedia
     * \value Video A bookmark related to a video file or application relates to Multimedia
     * \value Photo A bookmark related to a digital photography file or application relates to Multimedia; Graphics; Viewer
     * \value Application Special bookmark for application launchers
     * \since 5.93
     */
    enum RecentDocumentGroup {
        Development,
        Office,
        Database,
        Email,
        Presentation,
        Spreadsheet,
        WordProcessor,
        Graphics,
        TextEditor,
        Viewer,
        Archive,
        Multimedia,
        Audio,
        Video,
        Photo,
        Application,
    };

    // TODO qdoc?
    typedef QList<KRecentDocument::RecentDocumentGroup> RecentDocumentGroups;

    /*!
     * Returns a list of recent URLs. This includes all the URLs from
     * recentDocuments() as well as URLs from other applications conforming to
     * the XDG desktop-bookmark-spec (e. g. the GTK file dialog).
     *
     * \since 5.93
     */
    static QList<QUrl> recentUrls();

    /*!
     * Add a new item to the Recent Document menu.
     *
     * \a url The url to add.
     */
    static void add(const QUrl &url);

    /*!
     * \since 5.93
     */
    static void add(const QUrl &url, KRecentDocument::RecentDocumentGroups groups);

    /*!
     * Add a new item to the Recent Document menu, specifying the application to open it with.
     * The above add() method uses QCoreApplication::applicationName() for the app name,
     * which isn't always flexible enough.
     * This method is used when an application launches another one to open a document.
     *
     * \a url The url to add.
     *
     * \a desktopEntryName The desktopEntryName of the service to use for opening this document.
     */
    static void add(const QUrl &url, const QString &desktopEntryName);

    /*!
     * \since 5.93
     */
    static void add(const QUrl &url, const QString &desktopEntryName, KRecentDocument::RecentDocumentGroups groups);

    /*!
     *
     */
    static bool clearEntriesOldestEntries(int maxEntries);

    /*!
     * \since 6.6
     */
    static void removeFile(const QUrl &url);

    /*!
     * \since 6.6
     */
    static void removeApplication(const QString &desktopEntryName);

    /*!
     * Remove bookmarks whose modification date is after \a since parameter.
     *
     * \since 6.6
     */
    static void removeBookmarksModifiedSince(const QDateTime &since);

    /*!
     * Clear the recent document menu of all entries.
     */
    static void clear();

    /*!
     * Returns the maximum amount of recent document entries allowed.
     */
    static int maximumItems();
};

#endif
