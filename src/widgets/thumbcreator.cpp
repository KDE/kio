/*
    This file is part of the KDE project
    SPDX-FileCopyrightText: 2009 Patrick Spendrin <ps_ml@gmx.de>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#include "thumbcreator.h"

#include <qglobal.h>

ThumbCreator::~ThumbCreator()
{
}

ThumbCreator::Flags ThumbCreator::flags() const
{
    return None;
}

#if KIOWIDGETS_BUILD_DEPRECATED_SINCE(5, 0)
ThumbCreatorV2::~ThumbCreatorV2()
{
}
#endif

QWidget *ThumbCreator::createConfigurationWidget()
{
    return nullptr;
}

void ThumbCreator::writeConfiguration(const QWidget *configurationWidget)
{
    Q_UNUSED(configurationWidget);
}
