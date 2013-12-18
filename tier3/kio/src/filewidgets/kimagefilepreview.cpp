/*
 * This file is part of the KDE project
 * Copyright (C) 2001 Martin R. Jones <mjones@kde.org>
 *               2001 Carsten Pfeiffer <pfeiffer@kde.org>
 *               2008 Rafael Fernández López <ereslibre@kde.org>
 *
 * You can Freely distribute this program under the GNU Library General Public
 * License. See the file "COPYING" for the exact licensing terms.
 */

#include "kimagefilepreview.h"

#include <QLayout>
#include <QLabel>
#include <QPainter>
#include <QComboBox>
#include <QCheckBox>
#include <QResizeEvent>
#include <QtCore/QTimer>
#include <QtCore/QTimeLine>

#include <kconfig.h>
#include <kiconloader.h>
#include <QDebug>
#include <klocalizedstring.h>
#include <kfileitem.h>
#include <kio/previewjob.h>
#include <kconfiggroup.h>

/**** KImageFilePreview ****/

class KImageFilePreview::KImageFilePreviewPrivate
{
public:
    KImageFilePreviewPrivate()
        : m_job(0)
        , clear(true)
    {
        m_timeLine = new QTimeLine(150);
        m_timeLine->setCurveShape(QTimeLine::EaseInCurve);
        m_timeLine->setDirection(QTimeLine::Forward);
        m_timeLine->setFrameRange(0, 100);
    }

    ~KImageFilePreviewPrivate()
    {
        delete m_timeLine;
    }

    void _k_slotResult( KJob* );
    void _k_slotFailed( const KFileItem& );
    void _k_slotStepAnimation( int frame );
    void _k_slotFinished( );
    void _k_slotActuallyClear( );

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

KImageFilePreview::KImageFilePreview( QWidget *parent )
    : KPreviewWidgetBase(parent), d(new KImageFilePreviewPrivate)
{
    QVBoxLayout *vb = new QVBoxLayout( this );
    vb->setMargin( 0 );

    d->imageLabel = new QLabel(this);
    d->imageLabel->setAlignment(Qt::AlignHCenter | Qt::AlignVCenter);
    d->imageLabel->setSizePolicy(QSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding));
    vb->addWidget(d->imageLabel);

    setSupportedMimeTypes( KIO::PreviewJob::supportedMimeTypes() );
    setMinimumWidth( 50 );

    connect(d->m_timeLine, SIGNAL(frameChanged(int)), this, SLOT(_k_slotStepAnimation(int)));
    connect(d->m_timeLine, SIGNAL(finished()), this, SLOT(_k_slotFinished()));
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
    showPreview( url, true );
}

// called via KPreviewWidgetBase interface
void KImageFilePreview::showPreview( const QUrl& url )
{
    showPreview( url, false );
}

void KImageFilePreview::showPreview( const QUrl &url, bool force )
{
    if (!url.isValid() ||
        (d->lastShownURL.isValid() &&
         url.matches(d->lastShownURL, QUrl::StripTrailingSlash) &&
         d->currentURL.isValid()))
        return;

    d->clear = false;
    d->currentURL = url;
    d->lastShownURL = url;

    int w = d->imageLabel->contentsRect().width() - 4;
    int h = d->imageLabel->contentsRect().height() - 4;

    if (d->m_job) {
        disconnect(d->m_job, SIGNAL(result(KJob*)),
                    this, SLOT(_k_slotResult(KJob*)));
        disconnect(d->m_job, SIGNAL(gotPreview(const KFileItem&,
                                                const QPixmap& )), this,
                SLOT(gotPreview(KFileItem,QPixmap)));

        disconnect(d->m_job, SIGNAL(failed(KFileItem)),
                    this, SLOT(_k_slotFailed(KFileItem)));

        d->m_job->kill();
    }

    d->m_job = createJob(url, w, h);
    if ( force ) // explicitly requested previews shall always be generated!
        d->m_job->setIgnoreMaximumSize(true);

    connect(d->m_job, SIGNAL(result(KJob*)),
                this, SLOT(_k_slotResult(KJob*)));
    connect(d->m_job, SIGNAL(gotPreview(const KFileItem&,
                                        const QPixmap& )),
                SLOT(gotPreview(KFileItem,QPixmap)));

    connect(d->m_job, SIGNAL(failed(KFileItem)),
                this, SLOT(_k_slotFailed(KFileItem)));
}

void KImageFilePreview::resizeEvent( QResizeEvent * )
{
    clearPreview();
    d->currentURL = QUrl(); // force this to actually happen
    showPreview( d->lastShownURL );
}

QSize KImageFilePreview::sizeHint() const
{
    return QSize( 100, 200 );
}

KIO::PreviewJob * KImageFilePreview::createJob( const QUrl& url, int w, int h )
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
        return 0;
    }
}

void KImageFilePreview::gotPreview( const KFileItem& item, const QPixmap& pm )
{
    if (item.url() == d->currentURL) {  // should always be the case
        if (style()->styleHint(QStyle::SH_Widget_Animate, 0, this)) {
            if (d->m_timeLine->state() == QTimeLine::Running) {
                d->m_timeLine->setCurrentTime(0);
            }

            d->m_pmTransition = pm;
            d->m_pmTransitionOpacity = 0;
            d->m_pmCurrentOpacity = 1;
            d->m_timeLine->setDirection(QTimeLine::Forward);
            d->m_timeLine->start();
        }
        else
        {
            d->imageLabel->setPixmap(pm);
        }
    }
}

void KImageFilePreview::KImageFilePreviewPrivate::_k_slotFailed( const KFileItem& item )
{
    if ( item.isDir() )
        imageLabel->clear();
    else if (item.url() == currentURL) // should always be the case
        imageLabel->setPixmap(SmallIcon( "image-missing", KIconLoader::SizeLarge,
                                         KIconLoader::DisabledState ));
}

void KImageFilePreview::KImageFilePreviewPrivate::_k_slotResult( KJob *job )
{
    if (job == m_job) {
        m_job = 0L;
    }
}

void KImageFilePreview::KImageFilePreviewPrivate::_k_slotStepAnimation( int frame )
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
        d->m_job = 0L;
    }

    if (d->clear || d->m_timeLine->state() == QTimeLine::Running) {
        return;
    }

    if (style()->styleHint(QStyle::SH_Widget_Animate, 0, this)) {
        d->m_pmTransition = QPixmap();
        //If we add a previous preview then we run the animation
        if (!d->m_pmCurrent.isNull()) {
            d->m_timeLine->setCurrentTime(0);
            d->m_timeLine->setDirection(QTimeLine::Backward);
            d->m_timeLine->start();
        }
        d->currentURL = QUrl();
        d->clear = true;
    }
    else
    {
        d->imageLabel->clear();
    }
}

#include "moc_kimagefilepreview.cpp"
