/*
    This file is part of the KDE project.
    SPDX-FileCopyrightText: 2001 Martin R. Jones <mjones@kde.org>
    SPDX-FileCopyrightText: 2001 Carsten Pfeiffer <pfeiffer@kde.org>
    SPDX-FileCopyrightText: 2008 Rafael Fernández López <ereslibre@kde.org>

    SPDX-License-Identifier: LGPL-2.0-only
*/

#ifndef KIMAGEFILEPREVIEW_H
#define KIMAGEFILEPREVIEW_H

#include <QPixmap>
#include <QUrl>
#include <kpreviewwidgetbase.h>

class KFileItem;
class KJob;
namespace KIO
{
class PreviewJob;
}

/**
 * @class KImageFilePreview kimagefilepreview.h <KImageFilePreview>
 *
 * Image preview widget for the file dialog.
 */
class KIOFILEWIDGETS_EXPORT KImageFilePreview : public KPreviewWidgetBase
{
    Q_OBJECT

public:
    /**
     * Creates a new image file preview.
     *
     * @param parent The parent widget.
     */
    explicit KImageFilePreview(QWidget *parent = nullptr);

    /**
     * Destroys the image file preview.
     */
    ~KImageFilePreview() override;

    /**
     * Returns the size hint for this widget.
     */
    QSize sizeHint() const override;

public Q_SLOTS:
    /**
     * Shows a preview for the given @p url.
     */
    void showPreview(const QUrl &url) override;

    /**
     * Clears the preview.
     */
    void clearPreview() override;

protected Q_SLOTS:
    void showPreview();
    void showPreview(const QUrl &url, bool force);

    virtual void gotPreview(const KFileItem &, const QPixmap &);

protected:
    void resizeEvent(QResizeEvent *event) override;
    virtual KIO::PreviewJob *createJob(const QUrl &url, int width, int height);

private:
    class KImageFilePreviewPrivate;
    KImageFilePreviewPrivate *const d;

    Q_DISABLE_COPY(KImageFilePreview)

    Q_PRIVATE_SLOT(d, void _k_slotResult(KJob *))
    Q_PRIVATE_SLOT(d, void _k_slotFailed(const KFileItem &))
    Q_PRIVATE_SLOT(d, void _k_slotStepAnimation(int frame))
    Q_PRIVATE_SLOT(d, void _k_slotFinished())
};

#endif // KIMAGEFILEPREVIEW_H
