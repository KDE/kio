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

/*!
 * \class KIO::FileUndoManager
 * \inheaderfile KIO/FileUndoManager
 * \inmodule KIOWidgets
 *
 * \brief Makes it possible to undo KIO jobs.
 *
 * This class is a singleton, use self() to access its only instance.
 */
class KIOWIDGETS_EXPORT FileUndoManager : public QObject
{
    Q_OBJECT
public:
    /*!
     * Returns the FileUndoManager instance
     */
    static FileUndoManager *self();

    /*!
     * \class KIO::FileUndoManager::UiInterface
     * \inmodule KIOGui
     *
     * Interface for the gui handling of FileUndoManager.
     * This includes three events currently:
     * \list
     * \li error when undoing a job
     * \li (until KF 5.78) confirm deletion before undoing a copy job
     * \li confirm deletion when the copied file has been modified afterwards
     * \endlist
     *
     * By default UiInterface shows message boxes in all three cases;
     * applications can reimplement this interface to provide different user interfaces.
     */
    class KIOWIDGETS_EXPORT UiInterface
    {
    public:
        UiInterface();
        virtual ~UiInterface();

        /*!
         * Sets whether to show progress info when running the KIO jobs for undoing.
         */
        void setShowProgressInfo(bool b);
        /*!
         * Returns whether progress info dialogs are shown while undoing.
         */
        bool showProgressInfo() const;

        /*!
         * Sets the parent widget to use for message boxes.
         */
        void setParentWidget(QWidget *parentWidget);

        /*!
         * Returns the parent widget passed to the last call to undo(parentWidget), or \c nullptr.
         */
        QWidget *parentWidget() const;

        /*!
         * Called when an undo job errors; default implementation displays a message box.
         */
        virtual void jobError(KIO::Job *job);

        /*!
         * Called when dest was modified since it was copied from src.
         * Note that this is called after confirmDeletion.
         * Return true if we should proceed with deleting dest.
         */
        virtual bool copiedFileWasModified(const QUrl &src, const QUrl &dest, const QDateTime &srcTime, const QDateTime &destTime);

        // TODO KF6 replace hook with virtual AskUserActionInterface* askUserActionInterface(); // (does not take ownership)
        enum {
            HookGetAskUserActionInterface = 1
        };
        /*!
         * \internal, for future extensions
         */
        virtual void virtual_hook(int id, void *data);

    private:
        class UiInterfacePrivate;
        UiInterfacePrivate *d;
    };

    /*!
     * Set a new UiInterface implementation.
     * This deletes the previous one.
     *
     * \a ui the UiInterface instance, which becomes owned by the undo manager.
     */
    void setUiInterface(UiInterface *ui);

    /*!
     * Returns the UiInterface instance passed to setUiInterface.
     * This is useful for calling setParentWidget on it. Never delete it!
     */
    UiInterface *uiInterface() const;

    /*!
     * The type of job.
     *
     * \value Copy
     * \value Move
     * \value Rename
     * \value Link
     * \value Mkdir
     * \value Trash
     * \value[since 4.7] Put Represents the creation of a file from data in memory. Used when pasting data from clipboard or drag-n-drop
     * \value[since 5.4] Mkpath Represents a KIO::mkpath() job
     * \value[since 5.42] BatchRename Represents a KIO::batchRename() job. Used when renaming multiple files
     */
    enum CommandType {
        Copy,
        Move,
        Rename,
        Link,
        Mkdir,
        Trash,
        Put,
        Mkpath,
        BatchRename
    };

    /*!
     * Record this job while it's happening and add a command for it so that the user can undo it.
     * The signal jobRecordingStarted() is emitted.
     *
     * \a op the type of job - which is also the type of command that will be created for it
     *
     * \a src list of source urls. This is empty for Mkdir, Mkpath, Put operations.
     *
     * \a dst destination url
     *
     * \a job the job to record
     */
    void recordJob(CommandType op, const QList<QUrl> &src, const QUrl &dst, KIO::Job *job);

    /*!
     * Record this CopyJob while it's happening and add a command for it so that the user can undo it.
     * The signal jobRecordingStarted() is emitted.
     */
    void recordCopyJob(KIO::CopyJob *copyJob);

    /*!
     * Returns true if undo is possible. Usually used for enabling/disabling the undo action.
     *
     * \since 5.79
     */
    bool isUndoAvailable() const;

    /*!
     * Returns true if redo is possible. Usually used for enabling/disabling the redo action.
     *
     * \since 6.17
     */
    bool isRedoAvailable() const;

    /*!
     * Returns the current text for the undo action.
     */
    QString undoText() const;

    /*!
     * Returns the current text for the redo action.
     *
     * \since 6.17
     */
    QString redoText() const;

    /*!
     * These two functions are useful when wrapping FileUndoManager and adding custom commands.
     * Each command has a unique ID. You can get a new serial number for a custom command
     * with newCommandSerialNumber(), and then when you want to undo, check if the command
     * FileUndoManager would undo is newer or older than your custom command.
     *
     * \sa currentCommandSerialNumber()
     */
    quint64 newCommandSerialNumber();

    /*!
     *
     */
    quint64 currentCommandSerialNumber() const;

public Q_SLOTS:
    /*!
     * Undoes the last command
     * Remember to call uiInterface()->setParentWidget(parentWidget) first,
     * if you have multiple mainwindows.
     *
     * This operation is asynchronous.
     * undoJobFinished will be emitted once the undo is complete.
     */
    void undo(); // TODO pass QWindow*, for askUserInterface->askUserDelete and error handling etc.

    /*!
     * Redoes the last command
     * Remember to call uiInterface()->setParentWidget(parentWidget) first,
     * if you have multiple mainwindows.
     *
     * This operation is asynchronous.
     * undoJobFinished will be emitted once the redo is complete.
     *
     * \since 6.17
     */
    void redo();

Q_SIGNALS:
    /*!
     * Emitted when the value of isUndoAvailable() changes
     */
    void undoAvailable(bool avail);

    /*!
     * Emitted when the value of isRedoAvailable() changes
     *
     * \since 6.17
     */
    void redoAvailable(bool avail);

    /*!
     * Emitted when the value of undoText() changes
     */
    void undoTextChanged(const QString &text);

    /*!
     * Emitted when the value of redoText() changes
     *
     * \since 6.17
     */
    void redoTextChanged(const QString &text);

    /*!
     * Emitted when an undo (or redo) job finishes. Used for unit testing.
     */
    void undoJobFinished();

    /*!
     * Emitted when a job recording has been started by FileUndoManager::recordJob()
     * or FileUndoManager::recordCopyJob(). After the job recording has been finished,
     * the signal jobRecordingFinished() will be emitted.
     */
    void jobRecordingStarted(CommandType op);

    /*!
     * Emitted when a job that has been recorded by FileUndoManager::recordJob()
     * or FileUndoManager::recordCopyJob has been finished. The command
     * is now available for an undo-operation.
     */
    void jobRecordingFinished(FileUndoManager::CommandType op);

private:
    KIOWIDGETS_NO_EXPORT FileUndoManager();
    KIOWIDGETS_NO_EXPORT ~FileUndoManager() override;
    friend class FileUndoManagerSingleton;

    friend class UndoJob;
    friend class CommandRecorder;

    friend class FileUndoManagerPrivate;
    std::unique_ptr<FileUndoManagerPrivate> d;
};

} // namespace

#endif
