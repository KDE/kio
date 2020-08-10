/*
    localdomainurifilter.h

    This file is part of the KDE project
    SPDX-FileCopyrightText: 2002 Lubos Lunak <llunak@suse.cz>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef LOCALDOMAINURIFILTER_H
#define LOCALDOMAINURIFILTER_H

#include <KUriFilter>

#include <QRegularExpression>

class QHostInfo;
class QEventLoop;

/**
 This filter takes care of hostnames in the local search domain.
 If you're in domain domain.org which has a host intranet.domain.org
 and the typed URI is just intranet, check if there's a host
 intranet.domain.org and if yes, it's a network URI.
*/
class LocalDomainUriFilter : public KUriFilterPlugin
{
    Q_OBJECT

public:
    LocalDomainUriFilter(QObject *parent, const QVariantList &args);
    bool filterUri(KUriFilterData &data) const override;

private:
    bool exists(const QString &) const;

    QRegularExpression m_hostPortPattern;
};

#endif
