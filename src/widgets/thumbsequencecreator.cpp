/*
    This file is part of the KDE libraries
    SPDX-FileCopyrightText: 2009 David Nolden <david.nolden.kdevelop@art-master.de>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#include "thumbsequencecreator.h"

class Q_DECL_HIDDEN ThumbSequenceCreator::Private
{
public:
    Private() : sequenceIndex(0)
    {
    }

    float sequenceIndex;
};

float ThumbSequenceCreator::sequenceIndex() const
{
    return d->sequenceIndex;
}

void ThumbSequenceCreator::setSequenceIndex(float index)
{
    d->sequenceIndex = index;
}

ThumbSequenceCreator::ThumbSequenceCreator() : d(new Private)
{

}

ThumbSequenceCreator::~ThumbSequenceCreator()
{
    delete d;
}
