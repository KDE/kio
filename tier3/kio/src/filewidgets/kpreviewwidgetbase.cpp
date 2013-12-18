/*
 * This file is part of the KDE project.
 * Copyright (C) 2003 Carsten Pfeiffer <pfeiffer@kde.org>
 *
 * You can Freely distribute this program under the GNU Library General Public
 * License. See the file "COPYING" for the exact licensing terms.
 */

#include "kpreviewwidgetbase.h"

class KPreviewWidgetBase::KPreviewWidgetBasePrivate
{
public:
    QStringList supportedMimeTypes;
};

KPreviewWidgetBase::KPreviewWidgetBase( QWidget *parent )
    : QWidget(parent), d(new KPreviewWidgetBasePrivate)
{
}

KPreviewWidgetBase::~KPreviewWidgetBase()
{
    delete d;
}

void KPreviewWidgetBase::setSupportedMimeTypes( const QStringList& mimeTypes )
{
    d->supportedMimeTypes = mimeTypes;
}

QStringList KPreviewWidgetBase::supportedMimeTypes() const
{
    return d->supportedMimeTypes;
}

#include "moc_kpreviewwidgetbase.cpp"
