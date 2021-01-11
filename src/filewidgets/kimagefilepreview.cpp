/*
    This file is part of the KDE project
    SPDX-FileCopyrightText: 2001 Martin R. Jones <mjones@kde.org>
    SPDX-FileCopyrightText: 2001 Carsten Pfeiffer <pfeiffer@kde.org>
    SPDX-FileCopyrightText: 2008 Rafael Fernández López <ereslibre@kde.org>

    SPDX-License-Identifier: LGPL-2.0-only
*/

#include "kimagefilepreview.h"

#include <QLabel>
#include <QPainter>
#include <QCheckBox>
#include <QResizeEvent>
#include <QTimeLine>
#include <QVBoxLayout>
#include <QStyle>

#include <KConfig>
#include <KIconLoader>
#include <KLocalizedString>
#include <kfileitem.h>
#include <kio/previewjob.h>
#include <KConfigGroup>

/**** KImageFilePreview ****/

class Q_DECL_HIDDEN KImageFilePreview::KImageFilePreviewPrivate
{
public:
    KImageFilePreviewPrivate()
        : m_job(nullptr)
        , clear(true)
    {
        m_timeLine = new QTimeLine(150);
        m_timeLine->setEasingCurve(QEasingCurve::InCurve);
        m_timeLine->setDirection(QTimeLine::Forward);
        m_timeLine->setFrameRange(0, 100);
    }

    ~KImageFilePreviewPrivate()
    {
        delete m_timeLine;
    }

    void _k_slotResult(KJob *);
    void _k_slotFailed(const KFileItem &);
    void _k_slotStepAnimation(int frame);
    void _k_slotFinished();
    void _k_slotActuallyClear();

    QUrl currentURL;
    QUrl lastShownURL;
    QLabel *imageLabel;
    KIO::PreviewJob *m_job;
    QTimeLine *m_timeLine;
    QPixmap m_pmCurrent;
    QPixmap m_pmTransition;
    float m_pmCurrentOpacity;
    float m_pmTransitionOpacity;
    bool clear;
};

KImageFilePreview::KImageFilePreview(QWidget *parent)
    : KPreviewWidgetBase(parent), d(new KImageFilePreviewPrivate)
{
    QVBoxLayout *vb = new QVBoxLayout(this);
    vb->setContentsMargins(0, 0, 0, 0);

    d->imageLabel = new QLabel(this);
    d->imageLabel->setAlignment(Qt::AlignHCenter | Qt::AlignVCenter);
    d->imageLabel->setSizePolicy(QSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding));
    vb->addWidget(d->imageLabel);

    setSupportedMimeTypes(KIO::PreviewJob::supportedMimeTypes());
    setMinimumWidth(50);

    connect(d->m_timeLine, &QTimeLine::frameChanged, this, [this](int value) { d->_k_slotStepAnimation(value); });
    connect(d->m_timeLine, &QTimeLine::finished, this, [this]() { d->_k_slotFinished(); });
}

KImageFilePreview::~KImageFilePreview()
{
    if (d->m_job) {
        d->m_job->kill();
    }

    delete d;
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
    if (!url.isValid() ||
            (d->lastShownURL.isValid() &&
             url.matches(d->lastShownURL, QUrl::StripTrailingSlash) &&
             d->currentURL.isValid())) {
        return;
    }

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

    connect(d->m_job, &KJob::result, this, [this](KJob *job) { d->_k_slotResult(job); });
    connect(d->m_job, &KIO::PreviewJob::gotPreview, this, &KImageFilePreview::gotPreview);
    connect(d->m_job, &KIO::PreviewJob::failed, this, [this](const KFileItem &item) { d->_k_slotFailed(item); });
}

void KImageFilePreview::resizeEvent(QResizeEvent *)
{
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
    if (url.isValid()) {
        KFileItemList items;
        items.append(KFileItem(url));
        QStringList plugins = KIO::PreviewJob::availablePlugins();

        KIO::PreviewJob *previewJob = KIO::filePreview(items, QSize(w, h), &plugins);
        previewJob->setOverlayIconAlpha(0);
        previewJob->setScaleType(KIO::PreviewJob::Scaled);
        return previewJob;
    } else {
        return nullptr;
    }
}

void KImageFilePreview::gotPreview(const KFileItem &item, const QPixmap &pm)
{
    if (item.url() == d->currentURL) {  // should always be the case
        if (style()->styleHint(QStyle::SH_Widget_Animate, nullptr, this)) {
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
}

void KImageFilePreview::KImageFilePreviewPrivate::_k_slotFailed(const KFileItem &item)
{
    if (item.isDir()) {
        imageLabel->clear();
    } else if (item.url() == currentURL) // should always be the case
        imageLabel->setPixmap(QIcon::fromTheme(QStringLiteral("image-missing"))
            .pixmap(KIconLoader::SizeLarge, QIcon::Disabled));
}

void KImageFilePreview::KImageFilePreviewPrivate::_k_slotResult(KJob *job)
{
    if (job == m_job) {
        m_job = nullptr;
    }
}

void KImageFilePreview::KImageFilePreviewPrivate::_k_slotStepAnimation(int frame)
{
    Q_UNUSED(frame)

    QPixmap pm(QSize(qMax(m_pmCurrent.size().width(), m_pmTransition.size().width()),
                     qMax(m_pmCurrent.size().height(), m_pmTransition.size().height())));
    pm.fill(Qt::transparent);

    QPainter p(&pm);
    p.setOpacity(m_pmCurrentOpacity);

    //If we have a current pixmap
    if (!m_pmCurrent.isNull())
        p.drawPixmap(QPoint(((float) pm.size().width() - m_pmCurrent.size().width()) / 2.0,
                            ((float) pm.size().height() - m_pmCurrent.size().height()) / 2.0), m_pmCurrent);
    if (!m_pmTransition.isNull()) {
        p.setOpacity(m_pmTransitionOpacity);
        p.drawPixmap(QPoint(((float) pm.size().width() - m_pmTransition.size().width()) / 2.0,
                            ((float) pm.size().height() - m_pmTransition.size().height()) / 2.0), m_pmTransition);
    }
    p.end();

    imageLabel->setPixmap(pm);

    m_pmCurrentOpacity = qMax(m_pmCurrentOpacity - 0.4, 0.0); // krazy:exclude=qminmax
    m_pmTransitionOpacity = qMin(m_pmTransitionOpacity + 0.4, 1.0); //krazy:exclude=qminmax
}

void KImageFilePreview::KImageFilePreviewPrivate::_k_slotFinished()
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

    if (d->clear || d->m_timeLine->state() == QTimeLine::Running) {
        return;
    }

    if (style()->styleHint(QStyle::SH_Widget_Animate, nullptr, this)) {
        d->m_pmTransition = QPixmap();
        //If we add a previous preview then we run the animation
        if (!d->m_pmCurrent.isNull()) {
            d->m_timeLine->setCurrentTime(0);
            d->m_timeLine->setDirection(QTimeLine::Backward);
            d->m_timeLine->start();
        }
        d->currentURL = QUrl();
        d->clear = true;
    } else {
        d->imageLabel->clear();
    }
}

#include "moc_kimagefilepreview.cpp"
