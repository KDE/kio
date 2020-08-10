//krazy:exclude=copyright (email of Maxim is missing)
/*
    This file is a part of the KDE project

    SPDX-FileCopyrightText: 2006 Zack Rusin <zack@kde.org>
    SPDX-FileCopyrightText: 2006-2007, 2008 Fredrik Höglund <fredrik@kde.org>

    The stack blur algorithm was invented by Mario Klingemann <mario@quasimondo.com>

    This implementation is based on the version in Anti-Grain Geometry Version 2.4,
    SPDX-FileCopyrightText: 2002-2005 Maxim Shemanarev <http://www.antigrain.com>

    SPDX-License-Identifier: BSD-2-Clause
*/

#ifndef KIO_IMAGEFILTER_P_H
#define KIO_IMAGEFILTER_P_H

#include "kiowidgets_export.h"

class QImage;
class QColor;
namespace KIO
{

class KIOWIDGETS_EXPORT ImageFilter
{
public:
    // Blurs the alpha channel of the image and recolors it to the specified color.
    // The image must have transparent padding on all sides, or the shadow will be clipped.
    static void shadowBlur(QImage &image, float radius, const QColor &color);
};

}

#endif

