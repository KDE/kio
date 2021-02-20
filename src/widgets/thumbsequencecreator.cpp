/*
    This file is part of the KDE libraries
    SPDX-FileCopyrightText: 2009 David Nolden <david.nolden.kdevelop@art-master.de>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#include "thumbsequencecreator.h"

class ThumbSequenceCreatorPrivate
{
public:
    float m_sequenceIndex = 0;
    float m_sequenceIndexWraparoundPoint = -1;
};

float ThumbSequenceCreator::sequenceIndex() const
{
    return d->m_sequenceIndex;
}

void ThumbSequenceCreator::setSequenceIndex(float index)
{
    d->m_sequenceIndex = index;
}

float ThumbSequenceCreator::sequenceIndexWraparoundPoint() const
{
    return d->m_sequenceIndexWraparoundPoint;
}

void ThumbSequenceCreator::setSequenceIndexWraparoundPoint(float wraparoundPoint)
{
    d->m_sequenceIndexWraparoundPoint = wraparoundPoint;
}

ThumbSequenceCreator::ThumbSequenceCreator()
    : d(new ThumbSequenceCreatorPrivate)
{
}

ThumbSequenceCreator::~ThumbSequenceCreator() = default;
