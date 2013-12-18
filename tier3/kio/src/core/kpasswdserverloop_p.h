/*
 *  This file is part of the KDE libraries
 *  Copyright (c) 2009 Michael Leupold <lemma@confuego.org>
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2.1 of the License, or (at your option) version 3, or any
 *  later version accepted by the membership of KDE e.V. (or its
 *  successor approved by the membership of KDE e.V.), which shall
 *  act as a proxy defined in Section 6 of version 3 of the license.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef KPASSWDSERVERLOOP_P_H
#define KPASSWDSERVERLOOP_P_H

#include <kio/authinfo.h>
#include <QtCore/QByteArray>
#include <QtCore/QEventLoop>

namespace KIO {

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
    const AuthInfo &authInfo() const;

public Q_SLOTS:
    void slotQueryResult(qlonglong requestId, qlonglong seqNr, const KIO::AuthInfo &authInfo);

private Q_SLOTS:
    void kdedServiceUnregistered();

private:
    qlonglong m_requestId;
    qlonglong m_seqNr;
    AuthInfo m_authInfo;
};

}

#endif
