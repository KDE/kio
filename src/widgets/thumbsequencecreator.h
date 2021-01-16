/*
    This file is part of the KDE libraries
    SPDX-FileCopyrightText: 2009 David Nolden <david.nolden.kdevelop@art-master.de>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#ifndef _THUMBSEQUENCECREATOR_H_
#define _THUMBSEQUENCECREATOR_H_

#include "thumbcreator.h"

#include <qglobal.h>

#include <memory>

class ThumbSequenceCreatorPrivate;

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
    std::unique_ptr<ThumbSequenceCreatorPrivate> d;
};

typedef ThumbCreator *(*newCreator)();

#endif
