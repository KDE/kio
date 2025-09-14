/*
    This file is part of the KDE project.
    SPDX-FileCopyrightText: 2003 Carsten Pfeiffer <pfeiffer@kde.org>

    SPDX-License-Identifier: LGPL-2.0-only
*/

#include "kfilemetapreview_p.h"

#include <QLayout>
#include <QMimeDatabase>

#include <KConfigGroup>
#include <KPluginFactory>
#include <KSharedConfig>
#include <QDebug>
#include <kimagefilepreview.h>
#include <kio/previewjob.h>

KFileMetaPreview::KFileMetaPreview(QWidget *parent)
    : KPreviewWidgetBase(parent)
{
    QHBoxLayout *layout = new QHBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    m_stack = new QStackedWidget(this);
    layout->addWidget(m_stack);

    // ###
    //     m_previewProviders.setAutoDelete( true );
    initPreviewProviders();

    // clean up old config from kfileaudiopreview
    auto config = KSharedConfig::openConfig();
    config->deleteGroup(QStringLiteral("Audio Preview Settings"));
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
    (void)m_stack->addWidget(imagePreview);
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
        provider = m_previewProviders.value(QStringView(mimeType).left(index + 1) + QLatin1Char('*'));
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

    return findExistingProvider(mimeType, mimeInfo);
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

void KFileMetaPreview::addPreviewProvider(const QString &mimeType, KPreviewWidgetBase *provider)
{
    m_previewProviders.insert(mimeType, provider);
}

void KFileMetaPreview::clearPreviewProviders()
{
    for (auto it = m_previewProviders.cbegin(); it != m_previewProviders.cend(); ++it) {
        m_stack->removeWidget(it.value());
    }
    qDeleteAll(m_previewProviders);
    m_previewProviders.clear();
}

#include "moc_kfilemetapreview_p.cpp"
