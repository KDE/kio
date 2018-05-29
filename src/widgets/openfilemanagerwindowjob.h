/* This file is part of the KDE libraries
    Copyright (C) 2016 Kai Uwe Broulik <kde@privat.broulik.de>

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.
*/

#ifndef OPENFILEMANAGERWINDOWJOB_H
#define OPENFILEMANAGERWINDOWJOB_H

#include "kiowidgets_export.h"

#include <KJob>

#include <QList>
#include <QUrl>

namespace KIO
{

class OpenFileManagerWindowJobPrivate;

/**
 * @class KIO::OpenFileManagerWindowJob openfilemanagerwindowjob.h <KIO/OpenFileManagerWindowJob>
 *
 * @brief Open a File Manager Window
 *
 * Using this job you can open a file manager window and highlight specific
 * files within a folder. This can be useful if you downloaded a file and want
 * to present it to the user without the user having to manually search the
 * file in its parent folder. This can also be used for a "Show in Parent Folder"
 * functionality.
 *
 * On Linux, this job will use the org.freedesktop.FileManager1 interface to highlight
 * the files and/or folders. If this fails, the parent directory of the first URL
 * will be opened in the default file manager instead.
 *
 * Note that this job is really only about highlighting certain items
 * which means if you, for example, pass it just a URL to a folder it will
 * not open this particular folder but instead highlight it within its parent folder.
 *
 * If you just want to open a folder, use KRun instead.
 *
 * @since 5.24
 */
class KIOWIDGETS_EXPORT OpenFileManagerWindowJob : public KJob
{
    Q_OBJECT

public:
    /**
     * Creates an OpenFileManagerWindowJob
     */
    explicit OpenFileManagerWindowJob(QObject *parent = nullptr);

    /**
     * Destroys the OpenFileManagerWindowJob
     */
    virtual ~OpenFileManagerWindowJob();

    /**
     * Errors the job may emit
     */
    enum Errors {
        NoValidUrlsError = KJob::UserDefinedError, ///< No valid URLs to highlight have been specified
        LaunchFailedError, ///< Failed to launch the file manager
    };

    /**
     * The files and/or folders to highlight
     */
    QList<QUrl> highlightUrls() const;

    /**
     * Set the files and/or folders to highlight
     */
    void setHighlightUrls(const QList<QUrl> &highlightUrls);

    /**
     * The Startup ID
     */
    QByteArray startupId() const;

    /**
     * Set the Startup ID
     */
    void setStartupId(const QByteArray &startupId);

    /**
     * Starts the job
     */
    void start() override;

private:
    friend class AbstractOpenFileManagerWindowStrategy;

    OpenFileManagerWindowJobPrivate *const d;

};

/**
 * Convenience method for creating a job to highlight a certain file or folder.
 *
 * It will create a job for a given URL(s) and automatically start it.
 *
 * @since 5.24
 */
KIOWIDGETS_EXPORT OpenFileManagerWindowJob *highlightInFileManager(const QList<QUrl> &urls, const QByteArray &asn = QByteArray());

} // namespace KIO

#endif // OPENFILEMANAGERWINDOWJOB_H
