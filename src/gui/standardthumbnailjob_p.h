/*
    SPDX-FileCopyrightText: 2024 Akseli Lahtinen <akselmo@akselmo.dev>
    SPDX-License-Identifier: LGPL-2.0-or-later
*/
#pragma once

#include <kio/job.h>

#include <QSize>

namespace KIO
{

inline qreal supportedDevicePixelRatio(const QSize &imageSize, const QSize &logicalSize, qreal maxDevicePixelRatio)
{
    const int longerLogical = qMax(logicalSize.width(), logicalSize.height());
    if (longerLogical <= 0) {
        return maxDevicePixelRatio;
    }
    const int longerActual = qMax(imageSize.width(), imageSize.height());
    return qBound(qreal(1), qreal(longerActual) / longerLogical, maxDevicePixelRatio);
}

class StandardThumbnailJob : public KIO::Job
{
    Q_OBJECT

public:
    StandardThumbnailJob(const QString &execString, int logicalWidth, qreal devicePixelRatio, const QString &inputFile, const QString &outputFile);
    ~StandardThumbnailJob() override;

    void start() override;
    bool doKill() override;

Q_SIGNALS:
    void data(KIO::Job *job, const QImage &thumb);

private:
    class Private;
    std::unique_ptr<Private> const d;
    Q_DISABLE_COPY(StandardThumbnailJob)
};

}
