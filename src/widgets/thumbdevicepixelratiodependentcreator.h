/*
    This file is part of the KDE libraries
    SPDX-FileCopyrightText: 2020 Méven Car <meven.car@kdemail.net>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/
#ifndef THUMBDEVICEPIXELRATIODEPENDENTCREATOR_H
#define THUMBDEVICEPIXELRATIODEPENDENTCREATOR_H

#include "thumbcreator.h"

#include <qglobal.h>

namespace KIO
{
/**
 * @class ThumbDevicePixelRatioDependentCreator thumbdevicepixelratiodependentcreator.h <KIO/ThumbDevicePixelratioDependentCreator>
 *
 * @see ThumbCreator
 *
 * This is an extension of ThumbCreator that allows to pass to the thumbCreator
 * a device pixel ratio to use for the output thumbnail.
 *
 * The dimensions are then passed to the thumbnail creator without scaling with the dpr.
 *
 * You also need to set the following key in the thumbcreator .desktop file
 * \code
 * DevicePixelRatioDependent=true;
 * \endcode
 *
 * @since 5.80
 */
class KIOWIDGETS_EXPORT ThumbDevicePixelRatioDependentCreator : public ThumbCreator
{
public:
    Q_DISABLE_COPY(ThumbDevicePixelRatioDependentCreator)
    ThumbDevicePixelRatioDependentCreator();
    ~ThumbDevicePixelRatioDependentCreator() override;

    /**
     * The device Pixel Ratio used for thumbnail creation
     */
    int devicePixelRatio() const;

    /**
     * Sets device Pixel Ratio used for thumbnail creation
     */
    void setDevicePixelRatio(int dpr);

private:
    class Private;
    Private *d;
};

}

typedef ThumbCreator *(*newCreator)();

#endif // THUMBDEVICEPIXELRATIODEPENDENTCREATOR_H
