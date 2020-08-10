/*
    This file is part of the KDE project.
    SPDX-FileCopyrightText: 2003 Carsten Pfeiffer <pfeiffer@kde.org>

    SPDX-License-Identifier: LGPL-2.0-only
*/

#include "kpreviewwidgetbase.h"

class Q_DECL_HIDDEN KPreviewWidgetBase::KPreviewWidgetBasePrivate
{
public:
    QStringList supportedMimeTypes;
};

KPreviewWidgetBase::KPreviewWidgetBase(QWidget *parent)
    : QWidget(parent), d(new KPreviewWidgetBasePrivate)
{
}

KPreviewWidgetBase::~KPreviewWidgetBase()
{
    delete d;
}

void KPreviewWidgetBase::setSupportedMimeTypes(const QStringList &mimeTypes)
{
    d->supportedMimeTypes = mimeTypes;
}

QStringList KPreviewWidgetBase::supportedMimeTypes() const
{
    return d->supportedMimeTypes;
}

#include "moc_kpreviewwidgetbase.cpp"
