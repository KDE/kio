// -*- c++ -*-
/*
    This file is part of the KDE libraries
    SPDX-FileCopyrightText: 2000 David Faure <faure@kde.org>
    SPDX-FileCopyrightText: 2000 Carsten Pfeiffer <pfeiffer@kde.org>
    SPDX-FileCopyrightText: 2001 Malte Starostik <malte.starostik@t-online.de>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#ifndef KIO_PREVIEWJOB_H
#define KIO_PREVIEWJOB_H

#include "kiogui_export.h"
#include <kfileitem.h>
#include <kio/job.h>

class QPixmap;
class KPluginMetaData;

namespace KIO
{
class PreviewJobPrivate;
/*!
 * @class KIO::PreviewJob previewjob.h <KIO/PreviewJob>
 *
 * This class catches a preview (thumbnail) for files.
 * @short KIO Job to get a thumbnail picture
 */
class KIOGUI_EXPORT PreviewJob : public KIO::Job
{
    Q_OBJECT
public:
    /**
     * Specifies the type of scaling that is applied to the generated preview.
     * For HiDPI, pixel density scaling, @see setDevicePixelRatio
     *
     */
    enum ScaleType {
        /**
         * The original size of the preview will be returned. Most previews
         * will return a size of 256 x 256 pixels.
         */
        Unscaled,
        /**
         * The preview will be scaled to the size specified when constructing
         * the PreviewJob. The aspect ratio will be kept.
         */
        Scaled,
        /**
         * The preview will be scaled to the size specified when constructing
         * the PreviewJob. The result will be cached for later use. Per default
         * ScaledAndCached is set.
         */
        ScaledAndCached,
    };

    /**
     * @param items          List of files to create previews for.
     * @param size           Desired size of the preview.
     * @param enabledPlugins If non-zero it defines the list of plugins that
     *                       are considered for generating the preview. If
     *                       enabledPlugins is zero the plugins specified in the
     *                       KConfigGroup "PreviewSettings" are used.
     */
    PreviewJob(const KFileItemList &items, const QSize &size, const QStringList *enabledPlugins = nullptr);

    ~PreviewJob() override;

    /**
     * Sets the scale type for the generated preview. Per default
     * PreviewJob::ScaledAndCached is set.
     * @see PreviewJob::ScaleType
     */
    void setScaleType(ScaleType type);

    /**
     * @return The scale type for the generated preview.
     * @see PreviewJob::ScaleType
     */
    ScaleType scaleType() const;

    /**
     * Removes an item from preview processing. Use this if you passed
     * an item to filePreview and want to delete it now.
     *
     * @param url the url of the item that should be removed from the preview queue
     */
    void removeItem(const QUrl &url);

    /**
     * If @p ignoreSize is true, then the preview is always
     * generated regardless of the settings
     **/
    void setIgnoreMaximumSize(bool ignoreSize = true);

    /**
     * Sets the sequence index given to the thumb creators.
     * Use the sequence index, it is possible to create alternative
     * icons for the same item. For example it may allow iterating through
     * the items of a directory, or the frames of a video.
     *
     **/
    void setSequenceIndex(int index);

    /**
     * Returns the currently set sequence index
     *
     **/
    int sequenceIndex() const;

    /**
     * Returns the index at which the thumbs of a ThumbSequenceCreator start
     * wrapping around ("looping"). Fractional values may be returned if the
     * ThumbSequenceCreator supports sub-integer precision, but frontends
     * supporting only integer sequence indices may choose to round it down.
     *
     * @see ThumbSequenceCreator::sequenceIndexWraparoundPoint()
     * @since 5.80
     */
    float sequenceIndexWraparoundPoint() const;

    /**
     * Determines whether the ThumbCreator in use is a ThumbSequenceCreator.
     *
     * @since 5.80
     */
    bool handlesSequences() const;

    /**
     * Request preview to use the device pixel ratio @p dpr.
     * The returned thumbnail may not respect the device pixel ratio requested.
     * Use QPixmap::devicePixelRatio to check, or paint as necessary.
     *
     * @since 5.84
     */
    void setDevicePixelRatio(qreal dpr);

    /**
     * Returns a list of all available preview plugins. The list
     * contains the basenames of the plugins' .desktop files (no path,
     * no .desktop).
     * @return the list of all available plugins
     */
    static QStringList availablePlugins();

    /**
     * Returns all plugins that are considered when a preview is generated
     * The result is internally cached, meaning any further method call will not reload the plugins
     * @since 5.90
     */
    static QList<KPluginMetaData> availableThumbnailerPlugins();

    /**
     * Returns a list of plugins that should be enabled by default, which is all plugins
     * Minus the plugins specified in an internal blacklist
     * @return the list of plugins that should be enabled by default
     * @since 5.40
     */
    static QStringList defaultPlugins();

    /**
     * Returns a list of all supported MIME types. The list can
     * contain entries like text/ * (without the space).
     * @return the list of MIME types
     */
    static QStringList supportedMimeTypes();

Q_SIGNALS:
    /**
     * Emitted when a thumbnail picture for @p item has been successfully
     * retrieved.
     * @param item the file of the preview
     * @param preview the preview image
     */
    void gotPreview(const KFileItem &item, const QPixmap &preview);
    /**
     * Emitted when a thumbnail for @p item could not be created,
     * either because a ThumbCreator for its MIME type does not
     * exist, or because something went wrong.
     * @param item the file that failed
     */
    void failed(const KFileItem &item);

protected Q_SLOTS:
    void slotResult(KJob *job) override;

private:
    Q_DECLARE_PRIVATE(PreviewJob)

public:
    /**
     * Sets a default device Pixel Ratio used for Previews
     * @see setDevicePixelRatio
     *
     * Defaults to 1
     *
     * @since 5.84
     */
    static void setDefaultDevicePixelRatio(qreal devicePixelRatio);
};

/**
 * Creates a PreviewJob to generate a preview image for the given items.
 * @param items          List of files to create previews for.
 * @param size           Desired size of the preview.
 * @param enabledPlugins If non-zero it defines the list of plugins that
 *                       are considered for generating the preview. If
 *                       enabledPlugins is zero the plugins specified in the
 *                       KConfigGroup "PreviewSettings" are used.
 */
KIOGUI_EXPORT PreviewJob *filePreview(const KFileItemList &items, const QSize &size, const QStringList *enabledPlugins = nullptr);
}

#endif
