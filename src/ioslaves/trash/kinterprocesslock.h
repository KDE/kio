/*
    This file is part of the KDE
    SPDX-FileCopyrightText: 2009 Tobias Koenig <tokoe@kde.org>

    SPDX-License-Identifier: GPL-2.0-only
*/

#ifndef KINTERPROCESSLOCK_H
#define KINTERPROCESSLOCK_H

#include <QObject>

class KInterProcessLockPrivate;

/**
 * @short A class for serializing access to a resource that is shared between multiple processes.
 *
 * This class can be used to serialize access to a resource between
 * multiple processes. Instead of using lock files, which could
 * become stale easily, the registration of dummy dbus services is used
 * to allow only one process at a time to access the resource.
 *
 * Example:
 *
 * @code
 *
 * KInterProcessLock *lock = new KInterProcessLock("myresource");
 * connect(lock, KInterProcessLock::lockGranted, this, [this, lock] { doCriticalTask(lock); });
 * lock->lock();
 *
 * ...
 *
 * ... ::doCriticalTask(KInterProcessLock *lock)
 * {
 *    // change common resource
 *
 *    lock->unlock();
 * }
 *
 * @endcode
 *
 * @author Tobias Koenig <tokoe@kde.org>
 */
class KInterProcessLock : public QObject
{
    Q_OBJECT
    Q_DECLARE_PRIVATE(KInterProcessLock)

public:
    /**
     * Creates a new inter process lock object.
     *
     * @param resource The identifier of the resource that shall be locked.
     *                 This identifier can be any string, however it must be unique for
     *                 the resource and every client that wants to access the resource must
     *                 know it.
     */
    explicit KInterProcessLock(const QString &resource);

    /**
     * Destroys the inter process lock object.
     */
    ~KInterProcessLock();

    /**
     * Returns the identifier of the resource the lock is set on.
     */
    QString resource() const;

    /**
     * Requests the lock.
     *
     * The lock is granted as soon as the lockGranted() signal is emitted.
     */
    void lock();

    /**
     * Releases the lock.
     *
     * @note This method should be called as soon as the critical area is left
     *       in your code path and the lock is no longer needed.
     */
    void unlock();

    /**
     * Waits for the granting of a lock by starting an internal event loop.
     */
    void waitForLockGranted();

Q_SIGNALS:
    /**
     * This signal is emitted when the requested lock has been granted.
     *
     * @param lock The lock that has been granted.
     */
    void lockGranted(KInterProcessLock *lock);

private:
    KInterProcessLockPrivate *const d_ptr;

    Q_PRIVATE_SLOT(d_func(), void _k_serviceRegistered(const QString &))
};

#endif
