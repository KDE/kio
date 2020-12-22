/*
    This file is part of the KDE project.
    SPDX-FileCopyrightText: 2003 Carsten Pfeiffer <pfeiffer@kde.org>

    SPDX-License-Identifier: LGPL-2.0-only
*/

#include "kfilemetapreview_p.h"

#include <QLayout>
#include <QMimeDatabase>

#include <QDebug>
#include <kio/previewjob.h>
#include <KPluginLoader>
#include <KPluginFactory>
#include <kimagefilepreview.h>

bool KFileMetaPreview::s_tryAudioPreview = true;

KFileMetaPreview::KFileMetaPreview(QWidget *parent)
    : KPreviewWidgetBase(parent),
      haveAudioPreview(false)
{
    QHBoxLayout *layout = new QHBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    m_stack = new QStackedWidget(this);
    layout->addWidget(m_stack);

    // ###
//     m_previewProviders.setAutoDelete( true );
    initPreviewProviders();
}

KFileMetaPreview::~KFileMetaPreview()
{
}

void KFileMetaPreview::initPreviewProviders()
{
    qDeleteAll(m_previewProviders);
    m_previewProviders.clear();
    // hardcoded so far

    // image previews
    KImageFilePreview *imagePreview = new KImageFilePreview(m_stack);
    (void) m_stack->addWidget(imagePreview);
    m_stack->setCurrentWidget(imagePreview);
    resize(imagePreview->sizeHint());

    const QStringList mimeTypes = imagePreview->supportedMimeTypes();
    QStringList::ConstIterator it = mimeTypes.begin();
    for (; it != mimeTypes.end(); ++it) {
//         qDebug(".... %s", (*it).toLatin1().constData());
        m_previewProviders.insert(*it, imagePreview);
    }
}

KPreviewWidgetBase *KFileMetaPreview::findExistingProvider(const QString &mimeType, const QMimeType &mimeInfo) const
{
    KPreviewWidgetBase *provider = m_previewProviders.value(mimeType);
    if (provider) {
        return provider;
    }

    if (mimeInfo.isValid()) {
        // check MIME type inheritance
        const QStringList parentMimeTypes = mimeInfo.allAncestors();
        for (const QString &parentMimeType : parentMimeTypes) {
            provider = m_previewProviders.value(parentMimeType);
            if (provider) {
                return provider;
            }
        }
    }

    // ### MIME type may be image/* for example, try that
    const int index = mimeType.indexOf(QLatin1Char('/'));
    if (index > 0) {
        provider = m_previewProviders.value(mimeType.leftRef(index + 1) + QLatin1Char('*'));
        if (provider) {
            return provider;
        }
    }

    return nullptr;
}

KPreviewWidgetBase *KFileMetaPreview::previewProviderFor(const QString &mimeType)
{
    QMimeDatabase db;
    QMimeType mimeInfo = db.mimeTypeForName(mimeType);

    //     qDebug("### looking for: %s", mimeType.toLatin1().constData());
    // often the first highlighted item, where we can be sure, there is no plugin
    // (this "folders reflect icons" is a konq-specific thing, right?)
    if (mimeInfo.inherits(QStringLiteral("inode/directory"))) {
        return nullptr;
    }

    KPreviewWidgetBase *provider = findExistingProvider(mimeType, mimeInfo);
    if (provider) {
        return provider;
    }

//qDebug("#### didn't find anything for: %s", mimeType.toLatin1().constData());

    if (s_tryAudioPreview &&
            !mimeType.startsWith(QLatin1String("text/")) &&
            !mimeType.startsWith(QLatin1String("image/"))) {
        if (!haveAudioPreview) {
            KPreviewWidgetBase *audioPreview = createAudioPreview(m_stack);
            if (audioPreview) {
                haveAudioPreview = true;
                (void) m_stack->addWidget(audioPreview);
                const QStringList mimeTypes = audioPreview->supportedMimeTypes();
                QStringList::ConstIterator it = mimeTypes.begin();
                for (; it != mimeTypes.end(); ++it) {
                    // only add non already handled MIME types
                    if (m_previewProviders.constFind(*it) == m_previewProviders.constEnd()) {
                        m_previewProviders.insert(*it, audioPreview);
                    }
                }
            }
        }
    }

    // with the new MIME types from the audio-preview, try again
    provider = findExistingProvider(mimeType, mimeInfo);
    if (provider) {
        return provider;
    }

    // The logic in this code duplicates the logic in PreviewJob.
    // But why do we need multiple KPreviewWidgetBase instances anyway?

    return nullptr;
}

void KFileMetaPreview::showPreview(const QUrl &url)
{
    QMimeDatabase db;
    QMimeType mt = db.mimeTypeForUrl(url);
    KPreviewWidgetBase *provider = previewProviderFor(mt.name());
    if (provider) {
        if (provider != m_stack->currentWidget()) { // stop the previous preview
            clearPreview();
        }

        m_stack->setEnabled(true);
        m_stack->setCurrentWidget(provider);
        provider->showPreview(url);
    } else {
        clearPreview();
        m_stack->setEnabled(false);
    }
}

void KFileMetaPreview::clearPreview()
{
    if (m_stack->currentWidget()) {
        static_cast<KPreviewWidgetBase *>(m_stack->currentWidget())->clearPreview();
    }
}

void KFileMetaPreview::addPreviewProvider(const QString &mimeType,
        KPreviewWidgetBase *provider)
{
    m_previewProviders.insert(mimeType, provider);
}

void KFileMetaPreview::clearPreviewProviders()
{
    QHash<QString, KPreviewWidgetBase *>::const_iterator i = m_previewProviders.constBegin();
    while (i != m_previewProviders.constEnd()) {
        m_stack->removeWidget(i.value());
        ++i;
    }
    qDeleteAll(m_previewProviders);
    m_previewProviders.clear();
}

// static
KPreviewWidgetBase *KFileMetaPreview::createAudioPreview(QWidget *parent)
{
    KPluginLoader loader(QStringLiteral("kfileaudiopreview"));
    KPluginFactory *factory = loader.factory();
    if (!factory) {
        qWarning() << "Couldn't load kfileaudiopreview" << loader.errorString();
        s_tryAudioPreview = false;
        return nullptr;
    }
    KPreviewWidgetBase *w = factory->create<KPreviewWidgetBase>(parent);
    if (w) {
        w->setObjectName(QStringLiteral("kfileaudiopreview"));
    }
    return w;
}
