/*
    This file is part of the KDE libraries
    SPDX-FileCopyrightText: 2009 Andreas Hartmetz <ahartmetz@gmail.com>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#ifndef KSSLERRORUIDATA_P_H
#define KSSLERRORUIDATA_P_H

#include "ksslerroruidata.h"

#include <QString>
#include <QSslError>
#include <QSslCertificate>

class Q_DECL_HIDDEN KSslErrorUiData::Private
{
public:
    static const KSslErrorUiData::Private *get(const KSslErrorUiData *uiData)
    {
        return uiData->d;
    }

    QList<QSslCertificate> certificateChain;
    QList<QSslError> sslErrors;   // parallel list to certificateChain
    QString ip;
    QString host;
    QString sslProtocol;
    QString cipher;
    int usedBits;
    int bits;
};

#endif // KSSLERRORUIDATA_P_H
