/*
    This file is part of the KDE libraries
    SPDX-FileCopyrightText: 2000 Malte Starostik <malte@kde.org>
    SPDX-FileCopyrightText: 2022 Nicolas Fella <nicolas.fella@gmx.de>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#include "thumbnailcreator.h"

#include <QImage>

namespace KIO
{

class ThumbnailRequestPrivate
{
public:
    QUrl url;
    QSize targetSize;
    QString mimeType;
    qreal dpr;
    float sequenceIndex;
};

ThumbnailRequest::~ThumbnailRequest() = default;

ThumbnailRequest::ThumbnailRequest(const ThumbnailRequest &other)
{
    d = std::make_unique<ThumbnailRequestPrivate>(*other.d);
}

ThumbnailRequest &ThumbnailRequest::operator=(const ThumbnailRequest &other)
{
    ThumbnailRequest temp(other);
    std::swap(*d, *temp.d);
    return *this;
}

ThumbnailRequest::ThumbnailRequest(const QUrl &url, const QSize &targetSize, const QString &mimeType, qreal dpr, float sequenceIndex)
    : d(new ThumbnailRequestPrivate)
{
    d->url = url;
    d->targetSize = targetSize;
    d->mimeType = mimeType;
    d->dpr = dpr;
    d->sequenceIndex = sequenceIndex;
}

QUrl ThumbnailRequest::url() const
{
    return d->url;
}

QSize ThumbnailRequest::targetSize() const
{
    return d->targetSize;
}

QString ThumbnailRequest::mimeType() const
{
    return d->mimeType;
}

qreal ThumbnailRequest::devicePixelRatio() const
{
    return d->dpr;
}

float ThumbnailRequest::sequenceIndex() const
{
    return d->sequenceIndex;
}

class ThumbnailResultPrivate
{
public:
    QImage image;
    float sequenceIndexWraparoundPoint = -1;
};

ThumbnailResult::ThumbnailResult()
    : d(new ThumbnailResultPrivate)
{
}

ThumbnailResult::~ThumbnailResult() = default;

ThumbnailResult::ThumbnailResult(const ThumbnailResult &other)
{
    d = std::make_unique<ThumbnailResultPrivate>(*other.d);
}

ThumbnailResult &ThumbnailResult::operator=(const ThumbnailResult &other)
{
    ThumbnailResult temp(other);
    std::swap(*d, *temp.d);
    return *this;
}

ThumbnailResult ThumbnailResult::pass(const QImage &image)
{
    ThumbnailResult response;
    response.d->image = image;

    return response;
}

ThumbnailResult ThumbnailResult::fail()
{
    ThumbnailResult response;
    return response;
}

float ThumbnailResult::sequenceIndexWraparoundPoint() const
{
    return d->sequenceIndexWraparoundPoint;
}

void ThumbnailResult::setSequenceIndexWraparoundPoint(float wraparoundPoint)
{
    d->sequenceIndexWraparoundPoint = wraparoundPoint;
}

QImage ThumbnailResult::image() const
{
    return d->image;
}

bool ThumbnailResult::isValid() const
{
    return !d->image.isNull();
}

ThumbnailCreator::ThumbnailCreator(QObject *parent, const QVariantList &args)
    : QObject(parent)
{
    Q_UNUSED(args);
}

ThumbnailCreator::~ThumbnailCreator() = default;
};
