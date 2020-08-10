/*
    This file is part of the KDE project
    SPDX-FileCopyrightText: 2000 Dawit Alemayehu <adawit@kde.org

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#ifndef KIO_SESSIONDATA_P_H
#define KIO_SESSIONDATA_P_H

#include <QObject>
#include "kiocore_export.h"
#include <kio/metadata.h>

namespace KIO
{

/**
 * @internal
 */
class SessionData : public QObject
{
    Q_OBJECT

public:
    SessionData();
    ~SessionData();

    void configDataFor(KIO::MetaData &configData, const QString &proto, const QString &host);
    void reset();

private:
    // TODO: fold private class back into this one, it's internal anyway
    class SessionDataPrivate;
    SessionDataPrivate *const d;
};

} // namespace

#endif
