/*
    This file is part of the KDE libraries
    SPDX-FileCopyrightText: 2020 Ahmad Samir <a.samirh78@gmail.com>

    SPDX-License-Identifier: LGPL-2.0-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/

#ifndef KIO_NAMEFINDERJOB_H
#define KIO_NAMEFINDERJOB_H

#include "kiocore_export.h"
#include <KCompositeJob>

#include <memory>

namespace KIO
{

class NameFinderJobPrivate;
/**
 * @class KIO::NameFinderJob namefinderjob.h <KIO/NameFinderJob>
 *
 *
 * @brief NameFinderJob finds a valid "New Folder" name.
 *
 * This job is useful when suggesting a new folder/file name, e.g. in KNewFileMenu,
 * the text box is pre-filled with a suggested name, typically in the form "New Folder";
 * to offer a valid name (i.e. one that doesn't already exist), you can use a NameFinderJob.
 *
 * Internally it uses a KIO::StatJob to determine if e.g. "New Folder" already exists,
 * and in such a case it will use KFileUtils::makeSuggestedName() to make a new name,
 * e.g. "New Folder (1)", then if the latter exists, KFileUtils::makeSuggestedName() will
 * be called again... etc, until it finds a name that doesn't already exist.
 *
 * Since NameFinderJob uses a KIO::StatJob, the code is asynchronous, which means it should
 * work for both local and remote filesystems without blocking I/O calls (this is important
 * when interacting with network mounts (e.g. SMB, NFS), from the upstream QFile perspective
 * these network mounts are "local" files even though they could actually reside on a server
 * halfway across the world).
 *
 * Note that KIO::StatJob will resolve URLs such as "desktop:/" to the most local URL, hence
 * it's advisable to always use baseUrl() (or finalUrl()) to get the actual URL.
 *
 * If the job fails for any reason targerUrl() will return an empty URL.
 *
 * @note You must call start() to start the job.
 *
 * @code
 *    // Create the job
 *    auto nameJob = new KIO::NameFinderJob(baseUrl, name, this);
 *    // Connect to the result() slot, and after making sure there were no errors call
 *    // finalUrl(), baseUrl() or finalName() to get the new url (base
 *    // url + suggested name), base url or suggested name, respectively
 *    connect(nameJob, &KJob::result, this, []() {
 *        if (!nameJob->error()) {
 *            const QUrl newBaseUrl = nameJob->baseUrl();
 *            const QUrl newName = nameJob->finalName();
 *            .....
 *            // Create the new dir "newName" in "newBaseUrl"
 *        }
 *    });
 *
 *    // Start the job
 *    nameJob->start();
 * @endcode
 *
 * @since 5.76
 */

class KIOCORE_EXPORT NameFinderJob : public KCompositeJob
{
    Q_OBJECT

public:
    /**
     * @brief Creates a NameFinderJob to get a "New Folder" (or "Text File.txt") name that doesn't
     * already exist.
     *
     * @param baseUrl URL of the directory where a new folder/file is going to be created
     * @param name the initially proposed name of the new folder/file
     */
    explicit NameFinderJob(const QUrl &baseUrl, const QString &name, QObject *parent);

    /**
     * Destructor
     *
     * Note that by default jobs auto-delete themselves after emitting result.
     */
    ~NameFinderJob() override;

    /**
     * Starts the job.
     */
    void start() override;

    /**
     * Call this to get the full target URL (basically the baseUrl() + "/" + finalName()).
     * Typically you should call this in a slot connected to the result() signal, and after
     * making sure no errors occurred (if there were an error this method will return an empty URL).
     */
    QUrl finalUrl() const;

    /**
     * Call this to get the base URL (i.e. the URL of the folder where a new folder/file
     * is going to be created). Note that this could return a different URL from the one
     * the job was initially called on, since the StatJob (which is used internally) will
     * resolve the URL to the most local one. See KIO::StatJob::mostLocalUrl() for more details.
     *
     * Typically you should call this in a slot connected to the result() signal, and after
     * making sure no errors occurred.
     */
    QUrl baseUrl() const;

    /**
     * Call this to get the suggested new folder/file name. Typically you should call this in a
     * slot connected to the result() signal and after making sure no errors occured.
     */
    QString finalName() const;

private:
    friend class NameFinderJobPrivate;
    std::unique_ptr<NameFinderJobPrivate> d;
};

} // namespace KIO

#endif
