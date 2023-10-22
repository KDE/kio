/*
    localdomainurifilter.h

    This file is part of the KDE project
    SPDX-FileCopyrightText: 2002 Lubos Lunak <llunak@suse.cz>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef LOCALDOMAINURIFILTER_H
#define LOCALDOMAINURIFILTER_H

#include "kurifilterplugin_p.h"
#include <QRegularExpression>

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
    using KUriFilterPlugin::KUriFilterPlugin;
    bool filterUri(KUriFilterData &data) const override;

private:
    bool exists(const QString &) const;

    const QRegularExpression m_hostPortPattern{
        QRegularExpression::anchoredPattern(uR"--([a-zA-Z0-9][a-zA-Z0-9+-]*(?:\:[0-9]{1,5})?(?:/[\w:@&=+$,-.!~*'()]*)*)--"),
    };
};

#endif
