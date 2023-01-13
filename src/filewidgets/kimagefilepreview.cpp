/*
    This file is part of the KDE project
    SPDX-FileCopyrightText: 2001 Martin R. Jones <mjones@kde.org>
    SPDX-FileCopyrightText: 2001 Carsten Pfeiffer <pfeiffer@kde.org>
    SPDX-FileCopyrightText: 2008 Rafael Fernández López <ereslibre@kde.org>

    SPDX-License-Identifier: LGPL-2.0-only
*/

#include "kimagefilepreview.h"

#include <QCheckBox>
#include <QLabel>
#include <QPainter>
#include <QResizeEvent>
#include <QStyle>
#include <QTimeLine>
#include <QVBoxLayout>

#include <KConfig>
#include <KConfigGroup>
#include <KIconLoader>
#include <KLocalizedString>
#include <kfileitem.h>
#include <kio/previewjob.h>

/**** KImageFilePreview ****/

class KImageFilePreviewPrivate
{
public:
    KImageFilePreviewPrivate(KImageFilePreview *qq)
        : q(qq)
    {
        if (q->style()->styleHint(QStyle::SH_Widget_Animate, nullptr, q)) {
            m_timeLine = new QTimeLine(150, q);
            m_timeLine->setEasingCurve(QEasingCurve::InCurve);
            m_timeLine->setDirection(QTimeLine::Forward);
            m_timeLine->setFrameRange(0, 100);
        }
    }

    void slotResult(KJob *);
    void slotFailed(const KFileItem &);
    void slotStepAnimation();
    void slotFinished();
    void slotActuallyClear();

    KImageFilePreview *q = nullptr;
    QUrl currentURL;
    QUrl lastShownURL;
    QLabel *imageLabel;
    KIO::PreviewJob *m_job = nullptr;
    QTimeLine *m_timeLine = nullptr;
    QPixmap m_pmCurrent;
    QPixmap m_pmTransition;
    float m_pmCurrentOpacity = 1;
    float m_pmTransitionOpacity = 0;
    bool clear = true;
};

KImageFilePreview::KImageFilePreview(QWidget *parent)
    : KPreviewWidgetBase(parent)
    , d(new KImageFilePreviewPrivate(this))
{
    QVBoxLayout *vb = new QVBoxLayout(this);
    vb->setContentsMargins(0, 0, 0, 0);

    d->imageLabel = new QLabel(this);
    d->imageLabel->setAlignment(Qt::AlignHCenter | Qt::AlignVCenter);
    d->imageLabel->setSizePolicy(QSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding));
    vb->addWidget(d->imageLabel);

    setSupportedMimeTypes(KIO::PreviewJob::supportedMimeTypes());
    setMinimumWidth(50);

    if (d->m_timeLine) {
        connect(d->m_timeLine, &QTimeLine::frameChanged, this, [this]() {
            d->slotStepAnimation();
        });
        connect(d->m_timeLine, &QTimeLine::finished, this, [this]() {
            d->slotFinished();
        });
    }
}

KImageFilePreview::~KImageFilePreview()
{
    if (d->m_job) {
        d->m_job->kill();
    }
}

void KImageFilePreview::showPreview()
{
    // Pass a copy since clearPreview() will clear currentURL
    QUrl url = d->currentURL;
    showPreview(url, true);
}

// called via KPreviewWidgetBase interface
void KImageFilePreview::showPreview(const QUrl &url)
{
    showPreview(url, false);
}

void KImageFilePreview::showPreview(const QUrl &url, bool force)
{
    /* clang-format off */
    if (!url.isValid()
        || (d->lastShownURL.isValid()
            && url.matches(d->lastShownURL, QUrl::StripTrailingSlash)
            && d->currentURL.isValid())) {
        return;
    }
    /* clang-format on*/

    d->clear = false;
    d->currentURL = url;
    d->lastShownURL = url;

    int w = d->imageLabel->contentsRect().width() - 4;
    int h = d->imageLabel->contentsRect().height() - 4;

    if (d->m_job) {
        disconnect(d->m_job, nullptr, this, nullptr);

        d->m_job->kill();
    }

    d->m_job = createJob(url, w, h);
    if (force) { // explicitly requested previews shall always be generated!
        d->m_job->setIgnoreMaximumSize(true);
    }

    connect(d->m_job, &KJob::result, this, [this](KJob *job) {
        d->slotResult(job);
    });
    connect(d->m_job, &KIO::PreviewJob::gotPreview, this, &KImageFilePreview::gotPreview);
    connect(d->m_job, &KIO::PreviewJob::failed, this, [this](const KFileItem &item) {
        d->slotFailed(item);
    });
}

void KImageFilePreview::resizeEvent(QResizeEvent *)
{
    // Nothing to do, if no current preview
    if (d->imageLabel->pixmap().isNull()) {
        return;
    }

    clearPreview();
    d->currentURL = QUrl(); // force this to actually happen
    showPreview(d->lastShownURL);
}

QSize KImageFilePreview::sizeHint() const
{
    return QSize(100, 200);
}

KIO::PreviewJob *KImageFilePreview::createJob(const QUrl &url, int w, int h)
{
    if (!url.isValid()) {
        return nullptr;
    }

    KFileItemList items;
    items.append(KFileItem(url));
    QStringList plugins = KIO::PreviewJob::availablePlugins();

    KIO::PreviewJob *previewJob = KIO::filePreview(items, QSize(w, h), &plugins);
#if KIOWIDGETS_BUILD_DEPRECATED_SINCE(5, 102)
    previewJob->setOverlayIconAlpha(0);
#endif
    previewJob->setScaleType(KIO::PreviewJob::Scaled);
    return previewJob;
}

void KImageFilePreview::gotPreview(const KFileItem &item, const QPixmap &pm)
{
    if (item.url() != d->currentURL) { // Shouldn't happen
        return;
    }

    if (d->m_timeLine) {
        if (d->m_timeLine->state() == QTimeLine::Running) {
            d->m_timeLine->setCurrentTime(0);
        }

        d->m_pmTransition = pm;
        d->m_pmTransitionOpacity = 0;
        d->m_pmCurrentOpacity = 1;
        d->m_timeLine->setDirection(QTimeLine::Forward);
        d->m_timeLine->start();
    } else {
        d->imageLabel->setPixmap(pm);
    }
}

void KImageFilePreviewPrivate::slotFailed(const KFileItem &item)
{
    if (item.isDir()) {
        imageLabel->clear();
    } else if (item.url() == currentURL) { // should always be the case
        imageLabel->setPixmap(QIcon::fromTheme(QStringLiteral("image-missing")).pixmap(KIconLoader::SizeLarge, QIcon::Disabled));
    }
}

void KImageFilePreviewPrivate::slotResult(KJob *job)
{
    if (job == m_job) {
        m_job = nullptr;
    }
}

void KImageFilePreviewPrivate::slotStepAnimation()
{
    const QSize currSize = m_pmCurrent.size();
    const QSize transitionSize = m_pmTransition.size();
    const int width = std::max(currSize.width(), transitionSize.width());
    const int height = std::max(currSize.height(), transitionSize.height());
    QPixmap pm(QSize(width, height));
    pm.fill(Qt::transparent);

    QPainter p(&pm);
    p.setOpacity(m_pmCurrentOpacity);

    // If we have a current pixmap
    if (!m_pmCurrent.isNull()) {
        p.drawPixmap(QPoint(((float)pm.size().width() - m_pmCurrent.size().width()) / 2.0, ((float)pm.size().height() - m_pmCurrent.size().height()) / 2.0),
                     m_pmCurrent);
    }
    if (!m_pmTransition.isNull()) {
        p.setOpacity(m_pmTransitionOpacity);
        p.drawPixmap(
            QPoint(((float)pm.size().width() - m_pmTransition.size().width()) / 2.0, ((float)pm.size().height() - m_pmTransition.size().height()) / 2.0),
            m_pmTransition);
    }
    p.end();

    imageLabel->setPixmap(pm);

    m_pmCurrentOpacity = qMax(m_pmCurrentOpacity - 0.4, 0.0); // krazy:exclude=qminmax
    m_pmTransitionOpacity = qMin(m_pmTransitionOpacity + 0.4, 1.0); // krazy:exclude=qminmax
}

void KImageFilePreviewPrivate::slotFinished()
{
    m_pmCurrent = m_pmTransition;
    m_pmTransitionOpacity = 0;
    m_pmCurrentOpacity = 1;
    m_pmTransition = QPixmap();
    // The animation might have lost some frames. Be sure that if the last one
    // was dropped, the last image shown is the opaque one.
    imageLabel->setPixmap(m_pmCurrent);
    clear = false;
}

void KImageFilePreview::clearPreview()
{
    if (d->m_job) {
        d->m_job->kill();
        d->m_job = nullptr;
    }

    if (d->clear || (d->m_timeLine && d->m_timeLine->state() == QTimeLine::Running)) {
        return;
    }

    if (d->m_timeLine) {
        d->m_pmTransition = QPixmap();
        // If we add a previous preview then we run the animation
        if (!d->m_pmCurrent.isNull()) {
            d->m_timeLine->setCurrentTime(0);
            d->m_timeLine->setDirection(QTimeLine::Backward);
            d->m_timeLine->start();
        }
        d->currentURL.clear();
        d->clear = true;
    } else {
        d->imageLabel->clear();
    }
}

#include "moc_kimagefilepreview.cpp"
