/*
    This file is part of the KDE libraries
    SPDX-FileCopyrightText: 1999 Waldo Bastian <bastian@kde.org>
    SPDX-FileCopyrightText: 2013 David Faure <faure@kde.org>

    SPDX-License-Identifier: LGPL-2.0-only
*/

#ifndef IDLESLAVE_H
#define IDLESLAVE_H

#include "kiocore_export.h"
#include <QObject>
#include <QDateTime>
#include <QUrl>
#include <QScopedPointer>

namespace KIO
{

class IdleSlavePrivate;
class Connection;

/**
 * @internal
 * Represents an idle slave, waiting to be reused.
 * Used by klauncher.
 * Do not use outside KIO and klauncher!
 * @since 5.0
 */
class KIOCORE_EXPORT IdleSlave : public QObject
{
    Q_OBJECT
public:
    explicit IdleSlave(QObject *parent);
    ~IdleSlave();

    bool match(const QString &protocol, const QString &host, bool connected) const;
    void connect(const QString &app_socket);
    qint64 pid() const;
    int age(const QDateTime &now) const;
    void reparseConfiguration();
    bool onHold(const QUrl &url) const;
    QString protocol() const;
    Connection *connection() const;
    bool hasTempAuthorization() const;

Q_SIGNALS:
    // TODO KF6: fix clazy wanring by using fully-qualified signal argument
    void statusUpdate(IdleSlave *); // clazy:exclude=fully-qualified-moc-types

private Q_SLOTS:
    void gotInput();

private:
    QScopedPointer<IdleSlavePrivate> d;
};

} // namespace KIO

#endif // IDLESLAVE_H
