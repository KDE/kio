/*
    This file is part of the KDE libraries
    SPDX-FileCopyrightText: 2000 David Smith <dsmith@algonet.se>

    This class was inspired by a previous KUrlCompletion by
    SPDX-FileContributor: Henner Zeller <zeller@think.de>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#ifndef KURLCOMPLETION_H
#define KURLCOMPLETION_H

#include "kiowidgets_export.h"
#include <KCompletion>
#include <kio/udsentry.h>
#include <QString>

namespace KIO { class Job; }

class QStringList;
class KUrlCompletionPrivate;

/**
 * @class KUrlCompletion kurlcompletion.h <KUrlCompletion>
 *
 * This class does completion of URLs including user directories (~user)
 * and environment variables.  Remote URLs are passed to KIO.
 *
 * @short Completion of a single URL
 * @author David Smith <dsmith@algonet.se>
 */
class KIOWIDGETS_EXPORT KUrlCompletion : public KCompletion
{
    Q_OBJECT

public:
    /**
     * Determines how completion is done.
     * @li ExeCompletion - executables in $PATH or with full path.
     * @li FileCompletion - all files with full path or in dir(), URLs
     * are listed using KIO.
     * @li DirCompletion - Same as FileCompletion but only returns directories.
     */
    enum Mode { ExeCompletion = 1, FileCompletion, DirCompletion };

    /**
     * Constructs a KUrlCompletion object in FileCompletion mode.
     */
    KUrlCompletion();
    /**
     * This overloaded constructor allows you to set the Mode to ExeCompletion
     * or FileCompletion without using setMode. Default is FileCompletion.
     */
    KUrlCompletion(Mode);
    /**
     * Destructs the KUrlCompletion object.
     */
    virtual ~KUrlCompletion();

    /**
     * Finds completions to the given text.
     *
     * Remote URLs are listed with KIO. For performance reasons, local files
     * are listed with KIO only if KURLCOMPLETION_LOCAL_KIO is set.
     * The completion is done asynchronously if KIO is used.
     *
     * Returns the first match for user, environment, and local dir completion
     * and QString() for asynchronous completion (KIO or threaded).
     *
     * @param text the text to complete
     * @return the first match, or QString() if not found
     */
    QString makeCompletion(const QString &text) override;

    /**
     * Sets the current directory (used as base for completion).
     * Default = $HOME.
     * @param dir the current directory, as a URL (use QUrl::fromLocalFile for local paths)
     */
    virtual void setDir(const QUrl &dir);

    /**
     * Returns the current directory, as it was given in setDir
     * @return the current directory, as a URL (use QUrl::toLocalFile for local paths)
     */
    virtual QUrl dir() const;

    /**
     * Check whether asynchronous completion is in progress.
     * @return true if asynchronous completion is in progress
     */
    virtual bool isRunning() const;

    /**
     * Stops asynchronous completion.
     */
    virtual void stop();

    /**
     * Returns the completion mode: exe or file completion (default FileCompletion).
     * @return the completion mode
     */
    virtual Mode mode() const;

    /**
     * Changes the completion mode: exe or file completion
     * @param mode the new completion mode
     */
    virtual void setMode(Mode mode);

    /**
     * Checks whether environment variables are completed and
     * whether they are replaced internally while finding completions.
     * Default is enabled.
     * @return true if environment variables will be replaced
     */
    virtual bool replaceEnv() const;

    /**
     * Enables/disables completion and replacement (internally) of
     * environment variables in URLs. Default is enabled.
     * @param replace true to replace environment variables
     */
    virtual void setReplaceEnv(bool replace);

    /**
     * Returns whether ~username is completed and whether ~username
     * is replaced internally with the user's home directory while
     * finding completions. Default is enabled.
     * @return true to replace tilde with the home directory
     */
    virtual bool replaceHome() const;

    /**
     * Enables/disables completion of ~username and replacement
     * (internally) of ~username with the user's home directory.
     * Default is enabled.
     * @param replace true to replace tilde with the home directory
     */
    virtual void setReplaceHome(bool replace);

    /**
     * Replaces username and/or environment variables, depending on the
     * current settings and returns the filtered url. Only works with
     * local files, i.e. returns back the original string for non-local
     * urls.
     * @param text the text to process
     * @return the path or URL resulting from this operation. If you
     * want to convert it to a QUrl, use QUrl::fromUserInput.
     */
    QString replacedPath(const QString &text) const;

    /**
     * @internal I'll let ossi add a real one to KShell :)
     */
    static QString replacedPath(const QString &text,
                                bool replaceHome, bool replaceEnv = true);

    /**
     * Sets the MIME type filters for the file dialog.
     * @see QFileDialog::setMimeTypeFilters()
     * @since 5.38
     */
    void setMimeTypeFilters(const QStringList &mimeTypes);

    /**
     * Returns the MIME type filters for the file dialog.
     * @see QFileDialog::mimeTypeFilters()
     * @since 5.38
     */
    QStringList mimeTypeFilters() const;

protected:
    // Called by KCompletion, adds '/' to directories
    void postProcessMatch(QString *match) const override;
    void postProcessMatches(QStringList *matches) const override;
    void postProcessMatches(KCompletionMatches *matches) const override;

    void customEvent(QEvent *e) override; // KF6 TODO: remove

private:
    KUrlCompletionPrivate *const d;

    Q_PRIVATE_SLOT(d, void _k_slotEntries(KIO::Job *, const KIO::UDSEntryList &))
    Q_PRIVATE_SLOT(d, void _k_slotIOFinished(KJob *))
};

#endif // KURLCOMPLETION_H
