/*
    This file is part of the KDE project
    SPDX-FileCopyrightText: 2000 Simon Hausmann <hausmann@kde.org>
    SPDX-FileCopyrightText: 2006, 2008 David Faure <faure@kde.org>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#ifndef KIO_FILEUNDOMANAGER_H
#define KIO_FILEUNDOMANAGER_H

#include <QObject>
#include <QUrl>

#include "kiowidgets_export.h"

#include <memory>

class QDateTime;

namespace KIO
{

class Job;
class CopyJob;
class FileUndoManagerPrivate;
class FileUndoManagerSingleton;
class CommandRecorder;
class UndoCommand;
class UndoJob;

/**
 * @class KIO::FileUndoManager fileundomanager.h <KIO/FileUndoManager>
 *
 * FileUndoManager: makes it possible to undo kio jobs.
 * This class is a singleton, use self() to access its only instance.
 */
class KIOWIDGETS_EXPORT FileUndoManager : public QObject
{
    Q_OBJECT
public:
    /**
     * @return the FileUndoManager instance
     */
    static FileUndoManager *self();

    /**
     * Interface for the gui handling of FileUndoManager.
     * This includes three events currently:
     * - error when undoing a job
     * - (until KF 5.78) confirm deletion before undoing a copy job
     * - confirm deletion when the copied file has been modified afterwards
     *
     * By default UiInterface shows message boxes in all three cases;
     * applications can reimplement this interface to provide different user interfaces.
     */
    class KIOWIDGETS_EXPORT UiInterface
    {
    public:
        UiInterface();
        virtual ~UiInterface();

        /**
         * Sets whether to show progress info when running the KIO jobs for undoing.
         */
        void setShowProgressInfo(bool b);
        /**
         * @returns whether progress info dialogs are shown while undoing.
         */
        bool showProgressInfo() const;

        /**
         * Sets the parent widget to use for message boxes.
         */
        void setParentWidget(QWidget *parentWidget);

        /**
         * @return the parent widget passed to the last call to undo(parentWidget), or @c nullptr.
         */
        QWidget *parentWidget() const;

        /**
         * Called when an undo job errors; default implementation displays a message box.
         */
        virtual void jobError(KIO::Job *job);

        /**
         * Called when we are about to remove those files.
         * Return true if we should proceed with deleting them.
         * Deprecated since 5.79, no longer called.
         */
        virtual bool confirmDeletion(const QList<QUrl> &files);

        /**
         * Called when dest was modified since it was copied from src.
         * Note that this is called after confirmDeletion.
         * Return true if we should proceed with deleting dest.
         */
        virtual bool copiedFileWasModified(const QUrl &src, const QUrl &dest, const QDateTime &srcTime, const QDateTime &destTime);

        // TODO KF6 replace hook with virtual AskUserActionInterface* askUserActionInterface(); // (does not take ownership)
        enum {
            HookGetAskUserActionInterface = 1
        };
        /**
         * \internal, for future extensions
         */
        virtual void virtual_hook(int id, void *data);

    private:
        class UiInterfacePrivate;
        UiInterfacePrivate *d;
    };

    /**
     * Set a new UiInterface implementation.
     * This deletes the previous one.
     * @param ui the UiInterface instance, which becomes owned by the undo manager.
     */
    void setUiInterface(UiInterface *ui);

    /**
     * @return the UiInterface instance passed to setUiInterface.
     * This is useful for calling setParentWidget on it. Never delete it!
     */
    UiInterface *uiInterface() const;

    /**
     * The type of job.
     *
     * Put: @since 4.7, represents the creation of a file from data in memory.
     *                Used when pasting data from clipboard or drag-n-drop.
     * Mkpath: @since 5.4, represents a KIO::mkpath() job.
     * BatchRename: @since 5.42, represents a KIO::batchRename() job. Used when
     *              renaming multiple files.
     */
    enum CommandType { Copy, Move, Rename, Link, Mkdir, Trash, Put, Mkpath, BatchRename };

    /**
     * Record this job while it's happening and add a command for it so that the user can undo it.
     * The signal jobRecordingStarted() is emitted.
     * @param op the type of job - which is also the type of command that will be created for it
     * @param src list of source urls. This is empty for Mkdir, Mkpath, Put operations.
     * @param dst destination url
     * @param job the job to record
     */
    void recordJob(CommandType op, const QList<QUrl> &src, const QUrl &dst, KIO::Job *job);

    /**
     * Record this CopyJob while it's happening and add a command for it so that the user can undo it.
     * The signal jobRecordingStarted() is emitted.
     */
    void recordCopyJob(KIO::CopyJob *copyJob);

#if KIOWIDGETS_ENABLE_DEPRECATED_SINCE(5, 79)
    /**
     * @return true if undo is possible. Usually used for enabling/disabling the undo action.
     *
     * @deprecated since 5.79, use isUndoAvailable()
     */
    KIOWIDGETS_DEPRECATED_VERSION(5, 79, "Use isUndoAvailable()")
    bool undoAvailable() const;
#endif

    /**
     * @return true if undo is possible. Usually used for enabling/disabling the undo action.
     *
     * @since 5.79
     */
    bool isUndoAvailable() const;

    /**
     * @return the current text for the undo action.
     */
    QString undoText() const;

    /**
     * These two functions are useful when wrapping FileUndoManager and adding custom commands.
     * Each command has a unique ID. You can get a new serial number for a custom command
     * with newCommandSerialNumber(), and then when you want to undo, check if the command
     * FileUndoManager would undo is newer or older than your custom command.
     */
    quint64 newCommandSerialNumber();
    quint64 currentCommandSerialNumber() const;

public Q_SLOTS:
    /**
     * Undoes the last command
     * Remember to call uiInterface()->setParentWidget(parentWidget) first,
     * if you have multiple mainwindows.
     *
     * This operation is asynchronous.
     * undoJobFinished will be emitted once the undo is complete.
     */
    void undo(); // TODO pass QWindow*, for askUserInterface->askUserDelete and error handling etc.

Q_SIGNALS:
    /// Emitted when the value of isUndoAvailable() changes
    void undoAvailable(bool avail);

    /// Emitted when the value of undoText() changes
    void undoTextChanged(const QString &text);

    /// Emitted when an undo job finishes. Used for unit testing.
    void undoJobFinished();

    /**
     * Emitted when a job recording has been started by FileUndoManager::recordJob()
     * or FileUndoManager::recordCopyJob(). After the job recording has been finished,
     * the signal jobRecordingFinished() will be emitted.
     * @since 4.2
     */
    void jobRecordingStarted(CommandType op);

    /**
     * Emitted when a job that has been recorded by FileUndoManager::recordJob()
     * or FileUndoManager::recordCopyJob has been finished. The command
     * is now available for an undo-operation.
     * @since 4.2
     */
    // TODO KF6 signal arg should be fully-qualified
    void jobRecordingFinished(CommandType op); // clazy:exclude=fully-qualified-moc-types

private:
    FileUndoManager();
    virtual ~FileUndoManager();
    friend class FileUndoManagerSingleton;

    friend class UndoJob;
    friend class CommandRecorder;

    friend class FileUndoManagerPrivate;
    std::unique_ptr<FileUndoManagerPrivate> d;
};

} // namespace

#endif
