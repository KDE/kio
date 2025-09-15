/*
    This file is part of the KDE project.
    SPDX-FileCopyrightText: 2003 Carsten Pfeiffer <pfeiffer@kde.org>

    SPDX-License-Identifier: LGPL-2.0-only
*/

#ifndef KFILEMETAPREVIEW_H
#define KFILEMETAPREVIEW_H

#include <QHash>
#include <QMimeType>
#include <QStackedWidget>
#include <kpreviewwidgetbase.h>

// Internal, but exported for KDirOperator (kfile) and KPreviewProps (kdelibs4support)
class KIOFILEWIDGETS_EXPORT KFileMetaPreview : public KPreviewWidgetBase
{
    Q_OBJECT

public:
    explicit KFileMetaPreview(QWidget *parent);
    ~KFileMetaPreview() override;

    virtual void addPreviewProvider(const QString &mimeType, KPreviewWidgetBase *provider);
    virtual void clearPreviewProviders();

public Q_SLOTS:
    void showPreview(const QUrl &url) override;
    void clearPreview() override;

protected:
    virtual KPreviewWidgetBase *previewProviderFor(const QString &mimeType);

private:
    void initPreviewProviders();
    KPreviewWidgetBase *findExistingProvider(const QString &mimeType, const QMimeType &mimeInfo) const;

    QStackedWidget *m_stack;
    QHash<QString, KPreviewWidgetBase *> m_previewProviders;

private:
    class KFileMetaPreviewPrivate;
    KFileMetaPreviewPrivate *d;
};

#endif // KFILEMETAPREVIEW_H
