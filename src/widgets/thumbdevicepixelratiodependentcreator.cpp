/*
    This file is part of the KDE libraries
    SPDX-FileCopyrightText: 2020 Méven Car <meven.car@kdemail.net>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#include "thumbdevicepixelratiodependentcreator.h"

class Q_DECL_HIDDEN KIO::ThumbDevicePixelRatioDependentCreator::Private
{
public:
    Private()
        : devicePixelRatio(1)
    {
    }

    int devicePixelRatio;
};

int KIO::ThumbDevicePixelRatioDependentCreator::devicePixelRatio() const
{
    return d->devicePixelRatio;
}

void KIO::ThumbDevicePixelRatioDependentCreator::setDevicePixelRatio(int dpr)
{
    d->devicePixelRatio = dpr;
}

KIO::ThumbDevicePixelRatioDependentCreator::ThumbDevicePixelRatioDependentCreator()
    : d(new Private)
{
}

KIO::ThumbDevicePixelRatioDependentCreator::~ThumbDevicePixelRatioDependentCreator() = default;
