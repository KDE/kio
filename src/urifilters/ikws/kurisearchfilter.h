/*
    This file is part of the KDE project
    SPDX-FileCopyrightText: 1999 Simon Hausmann <hausmann@kde.org>
    SPDX-FileCopyrightText: 2000 Yves Arrouye <yves@realnames.com>
    SPDX-FileCopyrightText: 2002, 2003 Dawit Alemayehu <adawit@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef KURISEARCHFILTER_H
#define KURISEARCHFILTER_H

#include "kurifilterplugin_p.h"

class KUriSearchFilter : public KUriFilterPlugin
{
    Q_OBJECT
public:
    using KUriFilterPlugin::KUriFilterPlugin;
    ~KUriSearchFilter() override;

    bool filterUri(KUriFilterData &) const override;
};

#endif
