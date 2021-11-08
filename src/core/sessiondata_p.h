/*
    This file is part of the KDE project
    SPDX-FileCopyrightText: 2000 Dawit Alemayehu <adawit@kde.org

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#ifndef KIO_SESSIONDATA_P_H
#define KIO_SESSIONDATA_P_H

#include "kiocore_export.h"
#include <QObject>
#include <kio/metadata.h>

#include <memory>

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
    ~SessionData() override;

    void configDataFor(KIO::MetaData &configData, const QString &proto, const QString &host);
    void reset();

private:
    // TODO: fold private class back into this one, it's internal anyway
    class SessionDataPrivate;
    std::unique_ptr<SessionDataPrivate> const d;
};

} // namespace

#endif
