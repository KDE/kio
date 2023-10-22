/*
    fixhosturifilter.h

    This file is part of the KDE project
    SPDX-FileCopyrightText: 2007 Lubos Lunak <llunak@suse.cz>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef FIXHOSTURIFILTER_H
#define FIXHOSTURIFILTER_H

#include "kurifilterplugin_p.h"
#include <KUriFilter>

/**
 This filter tries to automatically prepend www. to http URLs that need it.
*/
class FixHostUriFilter : public KUriFilterPlugin
{
    Q_OBJECT

public:
    using KUriFilterPlugin::KUriFilterPlugin;
    bool filterUri(KUriFilterData &data) const override;

private:
    bool exists(const QString &host) const;
    bool isResolvable(const QString &host) const;
};

#endif
