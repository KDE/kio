/*
    This file is part of the KDE project
    SPDX-FileCopyrightText: 2009 Patrick Spendrin <ps_ml@gmx.de>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#include "thumbcreator.h"

#if KIOWIDGETS_BUILD_DEPRECATED_SINCE(5, 101)

ThumbCreator::~ThumbCreator()
{
}

ThumbCreator::Flags ThumbCreator::flags() const
{
    return None;
}

#endif
