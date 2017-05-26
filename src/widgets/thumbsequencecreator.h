/*  This file is part of the KDE libraries
    Copyright (C) 2009 David Nolden <david.nolden.kdevelop@art-master.de>

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.
*/

#ifndef _THUMBSEQUENCECREATOR_H_
#define _THUMBSEQUENCECREATOR_H_

#include "thumbcreator.h"

#include <qglobal.h>

/**
 * @class ThumbSequenceCreator thumbsequencecreator.h <KIO/ThumbSequenceCreator>
 *
 * @see ThumbCreator
 *
 * This is an extension of ThumbCreator that allows creating a thumbnail sequence for
 * a file. If your thumbnail plugin can create a thumbnail sequence, you should base it
 * on ThumbSequenceCreator instead of ThumbCreator, and should use sequenceIndex()
 * to decide what thumbnail you generate.
 *
 * You also need to set the following key in the thumbcreator .desktop file
 * \code
 * HandleSequences=true;
 * \endcode
 *
 * @since 4.3
 */
// KF6 TODO: put this in the KIO namespace
class KIOWIDGETS_EXPORT ThumbSequenceCreator : public ThumbCreator
{
public:
    Q_DISABLE_COPY(ThumbSequenceCreator)
    ThumbSequenceCreator();
    virtual ~ThumbSequenceCreator();

    /**
     * If this thumb-creator can create a sequence of thumbnails,
     * it should use this to decide what sequence item to use.
     *
     * If the value is zero, the standard thumbnail should be created.
     *
     * This can be used for example to create thumbnails for different
     * timeframes in videos(For example 0m, 10m, 20m, ...).
     *
     * If your thumb-creator supports a high granularity, like a video,
     * you can respect the sub-integer precision coming from the float.
     * Else, just round the index to an integer.
     *
     * If the end of your sequence is reached, the sequence should start
     * from the beginning, or continue in some other way.
     */
    float sequenceIndex() const;

    /**
     * Sets the sequence-index for this thumb creator.
     * @see sequenceIndex
     */
    void setSequenceIndex(float index);

private:
    class Private;
    Private *d;
};

typedef ThumbCreator *(*newCreator)();

#endif
