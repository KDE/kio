/*
    This file is part of the KDE project
    SPDX-FileCopyrightText: 2000 Simon Hausmann <hausmann@kde.org>
    SPDX-FileCopyrightText: 2006, 2008 David Faure <faure@kde.org>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#ifndef FILEUNDOMANAGER_P_H
#define FILEUNDOMANAGER_P_H

#include "fileundomanager.h"
#include <QStack>
#include <QDateTime>
#include <QQueue>

class KJob;

namespace KIO
{

class FileUndoManagerAdaptor;

struct BasicOperation {
    enum Type { File, Link, Directory };

    // for QDataStream deserialization
    BasicOperation() {}
    BasicOperation(Type type, bool renamed, const QUrl& src, const QUrl &dst, const QDateTime &mtime, const QString &target = {})
        : m_valid(true), m_renamed(renamed), m_type(type), m_src(src), m_dst(dst), m_target(target), m_mtime(mtime) {}

    bool m_valid = false;
    bool m_renamed;

    Type m_type: 2;

    QUrl m_src;
    QUrl m_dst;
    QString m_target;
    QDateTime m_mtime;
};

class UndoCommand
{
public:
    UndoCommand()
        : m_valid(false)
    {}

    UndoCommand(FileUndoManager::CommandType type, const QList<QUrl> &src, const QUrl &dst, qint64 serialNumber)
        : m_valid(true), m_type(type), m_src(src), m_dst(dst), m_serialNumber(serialNumber)
    {}

    // TODO: is ::TRASH missing?
    bool isMoveCommand() const
    {
        return m_type == FileUndoManager::Move || m_type == FileUndoManager::Rename;
    }

    bool m_valid = false;
    FileUndoManager::CommandType m_type;
    QQueue<BasicOperation> m_opQueue;
    QList<QUrl> m_src;
    QUrl m_dst;
    quint64 m_serialNumber;
};

// This class listens to a job, collects info while it's running (for copyjobs)
// and when the job terminates, on success, it calls addCommand in the undomanager.
class CommandRecorder : public QObject
{
    Q_OBJECT
public:
    CommandRecorder(FileUndoManager::CommandType op, const QList<QUrl> &src, const QUrl &dst, KIO::Job *job);

private Q_SLOTS:
    void slotResult(KJob *job);

    void slotCopyingDone(KIO::Job *, const QUrl &from, const QUrl &to, const QDateTime &, bool directory, bool renamed);
    void slotCopyingLinkDone(KIO::Job *, const QUrl &from, const QString &target, const QUrl &to);
    void slotDirectoryCreated(const QUrl &url);
    void slotBatchRenamingDone(const QUrl &from, const QUrl &to);

private:
    UndoCommand m_cmd;
};

enum UndoState { MAKINGDIRS = 0, MOVINGFILES, STATINGFILE, REMOVINGDIRS, REMOVINGLINKS };

// The private class is, exceptionally, a real QObject
// so that it can be the class with the DBUS adaptor forwarding its signals.
class FileUndoManagerPrivate : public QObject
{
    Q_OBJECT
public:
    explicit FileUndoManagerPrivate(FileUndoManager *qq);

    ~FileUndoManagerPrivate()
    {
        delete m_uiInterface;
    }

    void pushCommand(const UndoCommand &cmd);

    void addDirToUpdate(const QUrl &url);

    void startUndo();
    void stepMakingDirectories();
    void stepMovingFiles();
    void stepRemovingLinks();
    void stepRemovingDirectories();

    /// called by FileUndoManagerAdaptor
    QByteArray get() const;

    friend class UndoJob;
    /// called by UndoJob
    void stopUndo(bool step);

    friend class UndoCommandRecorder;
    /// called by UndoCommandRecorder
    void addCommand(const UndoCommand &cmd);

    QStack<UndoCommand> m_commands;

    KIO::Job *m_currentJob = nullptr;
    QStack<QUrl> m_dirStack;
    QStack<QUrl> m_dirCleanupStack;
    QStack<QUrl> m_fileCleanupStack; // files and links
    QList<QUrl> m_dirsToUpdate;
    FileUndoManager::UiInterface *m_uiInterface;

    UndoJob *m_undoJob = nullptr;
    quint64 m_nextCommandIndex;

    FileUndoManager * const q;

    UndoCommand m_current;
    UndoState m_undoState;
    bool m_lock = false;
    bool m_connectedToAskUserInterface = false;

    // DBUS interface
Q_SIGNALS:
    /// DBUS signal
    void push(const QByteArray &command);
    /// DBUS signal
    void pop();
    /// DBUS signal
    void lock();
    /// DBUS signal
    void unlock();

public Q_SLOTS:
    // Those four slots are connected to DBUS signals
    void slotPush(QByteArray);
    void slotPop();
    void slotLock();
    void slotUnlock();

    void undoStep();
    void slotResult(KJob *);
};

} // namespace

#endif /* FILEUNDOMANAGER_P_H */
