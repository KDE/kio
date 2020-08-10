/*
    This file is part of the KDE libraries
    SPDX-FileCopyrightText: 2013 Dawit Alemayehu <adawit@kde.org>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#ifndef KIO_CLIPBOARDUPDATER_P_H
#define KIO_CLIPBOARDUPDATER_P_H

#include <QObject>
#include <jobuidelegateextension.h>

class KJob;
class QUrl;

namespace KIO
{
class Job;
class JobUiDelegate;

/**
 * Updates the clipboard when it is affected by KIO operations.
 *
 * UpdateContent updates clipboard urls that were modified. This mode should
 * be the one preferred by default because it will not change the contents
 * of the clipboard if the urls modified by the job are not found in the
 * clipboard.
 *
 * OverwriteContent blindly replaces all urls in the clipboard with the ones
 * from the job. This mode should not be used unless you are 100% certain that
 * the urls in the clipboard are actually there for the purposes of carrying
 * out the specified job. This mode for example is used by the KIO::pasteClipboard
 * job when a user performs a cut+paste operation.
 *
 * This class also sets @ref job as its parent object. As such, when @ref job
 * is deleted the instance of ClipboardUpdater you create will also be deleted
 * as well.
 */
class ClipboardUpdater : public QObject
{
    Q_OBJECT

public:
    /**
     * Convenience function that allows renaming of a single url in the clipboard.
     */
    static void update(const QUrl &srcUrl, const QUrl &destUrl);

    /**
     * Sets the mode.
     */
    void setMode(JobUiDelegateExtension::ClipboardUpdaterMode m);

private Q_SLOTS:
    void slotResult(KJob *job);

private:
    explicit ClipboardUpdater(Job *job, JobUiDelegateExtension::ClipboardUpdaterMode mode);
    friend class JobUiDelegate;
    JobUiDelegateExtension::ClipboardUpdaterMode m_mode;
};
}

#endif
