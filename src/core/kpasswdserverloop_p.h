/*
    This file is part of the KDE libraries
    SPDX-FileCopyrightText: 2009 Michael Leupold <lemma@confuego.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/

#ifndef KPASSWDSERVERLOOP_P_H
#define KPASSWDSERVERLOOP_P_H

#include <kio/authinfo.h>
#include <QEventLoop>

// Wait for the result of an asynchronous D-Bus request to KPasswdServer.
// Objects of this class are one-way ie. as soon as they have received
// a result you can't call waitForResult() again.
class KPasswdServerLoop : public QEventLoop
{
    Q_OBJECT

public:
    KPasswdServerLoop();
    virtual ~KPasswdServerLoop();
    bool waitForResult(qlonglong requestId);

    qlonglong seqNr() const;
    const KIO::AuthInfo &authInfo() const;

public Q_SLOTS:
    void slotQueryResult(qlonglong requestId, qlonglong seqNr, const KIO::AuthInfo &authInfo);

private Q_SLOTS:
    void kdedServiceUnregistered();

private:
    qlonglong m_requestId;
    qlonglong m_seqNr;
    KIO::AuthInfo m_authInfo;
};

#endif
