/*
    This file is part of the KDE libraries
    SPDX-FileCopyrightText: 2000 Matthias Hoelzer-Kluepfel <hoelzer@kde.org>
    SPDX-FileCopyrightText: 2001 Stephan Kulow <coolo@kde.org>
    SPDX-FileCopyrightText: 2003 Cornelius Schumacher <schumacher@kde.org>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#ifndef __help_h__
#define __help_h__

#include <kio/global.h>
#include <kio/slavebase.h>

#include <QString>

#include <qplatformdefs.h>

#include <stdio.h>

class HelpProtocol : public KIO::SlaveBase
{
public:

    HelpProtocol(bool ghelp, const QByteArray &pool, const QByteArray &app);
    virtual ~HelpProtocol() { }

    void get(const QUrl &url) override;

    void mimetype(const QUrl &url) override;

private:

    QString langLookup(const QString &fname);
    void emitFile(const QUrl &url);
    void get_file(const QString &path);
    QString lookupFile(const QString &fname, const QString &query,
                       bool &redirect);

    void sendError(const QString &t);

    QString mParsed;
    bool mGhelp;
};

#endif
