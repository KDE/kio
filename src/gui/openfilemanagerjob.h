/*
    This file is part of the KDE libraries
    SPDX-FileCopyrightText: 2016 Kai Uwe Broulik <kde@privat.broulik.de>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#ifndef OPENFILEMANAGERJOB_H
#define OPENFILEMANAGERJOB_H

#include "kiogui_export.h"

#include <KJob>

#include <QList>
#include <QUrl>

#include <memory>

namespace KIO
{
class OpenFileManagerJobPrivate;

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
 * If you just want to open a folder, use OpenUrlJob instead.
 *
 * @since 5.110
 */
class KIOGUI_EXPORT OpenFileManagerJob : public KJob
{
    Q_OBJECT

public:
    /**
     * Creates an OpenFileManagerWindowJob
     */
    explicit OpenFileManagerJob(QObject *parent = nullptr);

    /**
     * Destroys the OpenFileManagerWindowJob
     */
    ~OpenFileManagerJob() override;

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
     * Sets the platform-specific startup id of the file manager launch.
     * @param startupId startup id, if any (otherwise "").
     * For X11, this would be the id for the Startup Notification protocol.
     * For Wayland, this would be the token for the XDG Activation protocol.
     */
    void setStartupId(const QByteArray &startupId);

    /**
     * Starts the job
     */
    void start() override;

private:
    friend class AbstractOpenFileManagerJobStrategy;
    friend class OpenFileManagerDBusStrategy;
    friend class OpenFileManagerKRunStrategy;

    std::unique_ptr<OpenFileManagerJobPrivate> const d;
};

/**
 * Convenience method for creating a job to highlight a certain file or folder.
 *
 * It will create a job for a given URL(s) and automatically start it.
 *
 * @since 5.110
 */
KIOGUI_EXPORT OpenFileManagerJob *highlightInFileManager(const QList<QUrl> &urls, const QByteArray &asn = QByteArray(), KJobUiDelegate *delegate = nullptr);

} // namespace KIO

#endif // OPENFILEMANAGERJOB_H
