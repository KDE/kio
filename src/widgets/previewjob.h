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

#include "kiowidgets_export.h"
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
class KIOWIDGETS_EXPORT PreviewJob : public KIO::Job
{
    Q_OBJECT
public:
    /**
     * Specifies the type of scaling that is applied to the generated preview.
     * For HiDPI, pixel density scaling, @see setDevicePixelRatio
     *
     * @since 4.7
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

#if KIOWIDGETS_ENABLE_DEPRECATED_SINCE(4, 7)
    /**
     * Creates a new PreviewJob.
     * @param items a list of files to create previews for
     * @param width the desired width
     * @param height the desired height, 0 to use the @p width
     * @param iconSize the size of the MIME type icon to overlay over the
     * preview or zero to not overlay an icon. This has no effect if the
     * preview plugin that will be used doesn't use icon overlays.
     * @param iconAlpha transparency to use for the icon overlay
     * @param scale if the image is to be scaled to the requested size or
     * returned in its original size
     * @param save if the image should be cached for later use
     * @param enabledPlugins If non-zero, this points to a list containing
     * the names of the plugins that may be used. If enabledPlugins is zero
     * all available plugins are used.
     *
     * @deprecated Since 4.7, use PreviewJob(const KFileItemList&, const QSize&, const QStringList*) in combination
     *             with the setter-methods instead. Note that the semantics of
     *             \p enabledPlugins has been slightly changed.
     */
    KIOWIDGETS_DEPRECATED_VERSION(4, 7, "Use PreviewJob(const KFileItemList&, const QSize&, const QStringList*)")
    PreviewJob(const KFileItemList &items, int width, int height, int iconSize, int iconAlpha, bool scale, bool save, const QStringList *enabledPlugins);
#endif

    /**
     * @param items          List of files to create previews for.
     * @param size           Desired size of the preview.
     * @param enabledPlugins If non-zero it defines the list of plugins that
     *                       are considered for generating the preview. If
     *                       enabledPlugins is zero the plugins specified in the
     *                       KConfigGroup "PreviewSettings" are used.
     * @since 4.7
     */
    PreviewJob(const KFileItemList &items, const QSize &size, const QStringList *enabledPlugins = nullptr);

    ~PreviewJob() override;

    /**
     * Sets the size of the MIME-type icon which overlays the preview. If zero
     * is passed no overlay will be shown at all. The setting has no effect if
     * the preview plugin that will be used does not use icon overlays. Per
     * default the size is set to 0.
     * @since 4.7
     */
    void setOverlayIconSize(int size);

    /**
     * @return The size of the MIME-type icon which overlays the preview.
     * @see PreviewJob::setOverlayIconSize()
     * @since 4.7
     */
    int overlayIconSize() const;

    /**
     * Sets the alpha-value for the MIME-type icon which overlays the preview.
     * The alpha-value may range from 0 (= fully transparent) to 255 (= opaque).
     * Per default the value is set to 70.
     * @see PreviewJob::setOverlayIconSize()
     * @since 4.7
     */
    void setOverlayIconAlpha(int alpha);

    /**
     * @return The alpha-value for the MIME-type icon which overlays the preview.
     *         Per default 70 is returned.
     * @see PreviewJob::setOverlayIconAlpha()
     * @since 4.7
     */
    int overlayIconAlpha() const;

    /**
     * Sets the scale type for the generated preview. Per default
     * PreviewJob::ScaledAndCached is set.
     * @see PreviewJob::ScaleType
     * @since 4.7
     */
    void setScaleType(ScaleType type);

    /**
     * @return The scale type for the generated preview.
     * @see PreviewJob::ScaleType
     * @since 4.7
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
     * @since 4.3
     **/
    void setSequenceIndex(int index);

    /**
     * Returns the currently set sequence index
     *
     * @since 4.3
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

#if KIOWIDGETS_ENABLE_DEPRECATED_SINCE(5, 86)
    /**
     * Request preview to use the device pixel ratio @p dpr.
     * The returned thumbnail may not respect the device pixel ratio requested.
     * Use QPixmap::devicePixelRatio to check, or paint as necessary.
     *
     * @since 5.80
     * @deprecated Since 5.86, use setDevicePixelRatio(qreal dpr) instead
     */
    KIOWIDGETS_DEPRECATED_VERSION(5, 86, "Use setDevicePixelRatio(qreal dpr)")
    void setDevicePixelRatio(int dpr);
#endif

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
     * @since 5.90
     */
    static QVector<KPluginMetaData> availableThumbnailerPlugins();

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

#if KIOWIDGETS_ENABLE_DEPRECATED_SINCE(4, 5)
    /**
     * Returns the default "maximum file size", in bytes, used by PreviewJob.
     * This is useful for applications providing a GUI for letting the user change the size.
     * @since 4.1
     * @deprecated Since 4.5, PreviewJob uses different maximum file sizes dependent on the URL.
     *             The returned file size is only valid for local URLs.
     */
    KIOWIDGETS_DEPRECATED_VERSION(4, 5, "See API dox")
    static KIO::filesize_t maximumFileSize();
#endif

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
    Q_PRIVATE_SLOT(d_func(), void startPreview())
    Q_PRIVATE_SLOT(d_func(), void slotThumbData(KIO::Job *, const QByteArray &))
    Q_DECLARE_PRIVATE(PreviewJob)

public:
#if KIOWIDGETS_ENABLE_DEPRECATED_SINCE(5, 86)
    /**
     * Sets a default device Pixel Ratio used for Previews
     * @see setDevicePixelRatio
     *
     * Defaults to 1
     *
     * @since 5.80
     * @deprecated Since 5.86, use setDefaultDevicePixelRatio(qreal dpr) instead
     */
    KIOWIDGETS_DEPRECATED_VERSION(5, 86, "Use setDefaultDevicePixelRatio(qreal dpr)")
    static void setDefaultDevicePixelRatio(int devicePixelRatio);
#endif

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

#if KIOWIDGETS_ENABLE_DEPRECATED_SINCE(4, 7)
/**
 * Creates a PreviewJob to generate or retrieve a preview image
 * for the given URL.
 *
 * @param items files to get previews for
 * @param width the maximum width to use
 * @param height the maximum height to use, if this is 0, the same
 * value as width is used.
 * @param iconSize the size of the MIME type icon to overlay over the
 * preview or zero to not overlay an icon. This has no effect if the
 * preview plugin that will be used doesn't use icon overlays.
 * @param iconAlpha transparency to use for the icon overlay
 * @param scale if the image is to be scaled to the requested size or
 * returned in its original size
 * @param save if the image should be cached for later use
 * @param enabledPlugins if non-zero, this points to a list containing
 * the names of the plugins that may be used.
 * @return the new PreviewJob
 * @see PreviewJob::availablePlugins()
 * @deprecated Since 4.7, use KIO::filePreview(const KFileItemList&, const QSize&, const QStringList*) in combination
 *             with the setter-methods instead. Note that the semantics of
 *             \p enabledPlugins has been slightly changed.
 */
KIOWIDGETS_EXPORT
KIOWIDGETS_DEPRECATED_VERSION(4, 7, "Use KIO::filePreview(const KFileItemList &, const QSize &, const QStringList *")
PreviewJob *filePreview(const KFileItemList &items,
                        int width,
                        int height = 0,
                        int iconSize = 0,
                        int iconAlpha = 70,
                        bool scale = true,
                        bool save = true,
                        const QStringList *enabledPlugins = nullptr); // KDE5: use enums instead of bool scale + bool save
#endif

#if KIOWIDGETS_ENABLE_DEPRECATED_SINCE(4, 7)
/**
 * Creates a PreviewJob to generate or retrieve a preview image
 * for the given URL.
 *
 * @param items files to get previews for
 * @param width the maximum width to use
 * @param height the maximum height to use, if this is 0, the same
 * value as width is used.
 * @param iconSize the size of the MIME type icon to overlay over the
 * preview or zero to not overlay an icon. This has no effect if the
 * preview plugin that will be used doesn't use icon overlays.
 * @param iconAlpha transparency to use for the icon overlay
 * @param scale if the image is to be scaled to the requested size or
 * returned in its original size
 * @param save if the image should be cached for later use
 * @param enabledPlugins if non-zero, this points to a list containing
 * the names of the plugins that may be used.
 * @return the new PreviewJob
 * @see PreviewJob::availablePlugins()
 * @deprecated Since 4.7, use KIO::filePreview(const KFileItemList&, const QSize&, const QStringList*) in combination
 *             with the setter-methods instead. Note that the semantics of
 *             \p enabledPlugins has been slightly changed.
 */
KIOWIDGETS_EXPORT
KIOWIDGETS_DEPRECATED_VERSION(4, 7, "Use KIO::filePreview(const KFileItemList &, const QSize &, const QStringList *")
PreviewJob *filePreview(const QList<QUrl> &items,
                        int width,
                        int height = 0,
                        int iconSize = 0,
                        int iconAlpha = 70,
                        bool scale = true,
                        bool save = true,
                        const QStringList *enabledPlugins = nullptr);
#endif

/**
 * Creates a PreviewJob to generate a preview image for the given items.
 * @param items          List of files to create previews for.
 * @param size           Desired size of the preview.
 * @param enabledPlugins If non-zero it defines the list of plugins that
 *                       are considered for generating the preview. If
 *                       enabledPlugins is zero the plugins specified in the
 *                       KConfigGroup "PreviewSettings" are used.
 * @since 4.7
 */
KIOWIDGETS_EXPORT PreviewJob *filePreview(const KFileItemList &items, const QSize &size, const QStringList *enabledPlugins = nullptr);
}

#endif
