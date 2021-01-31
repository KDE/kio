/*
    SPDX-FileCopyrightText: 2008-2009 Peter Penz <peter.penz@gmx.at>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#ifndef KFILEPREVIEWGENERATOR_H
#define KFILEPREVIEWGENERATOR_H

#include "kiofilewidgets_export.h"

#include <QObject>

#include <memory>

class KAbstractViewAdapter;
class KDirModel;
class QAbstractItemView;
class QAbstractProxyModel;

class KFilePreviewGeneratorPrivate;

/**
 * @class KFilePreviewGenerator kfilepreviewgenerator.h <KFilePreviewGenerator>
 *
 * @brief Generates previews for files of an item view.
 *
 * Per default a preview is generated for each item.
 * Additionally the clipboard is checked for cut items.
 * The icon state for cut items gets dimmed automatically.
 *
 * The following strategy is used when creating previews:
 * - The previews for currently visible items are created before
 *   the previews for invisible items.
 * - If the user changes the visible area by using the scrollbars,
 *   all pending previews get paused. As soon as the user stays
 *   on the same position for a short delay, the previews are
 *   resumed. Also in this case the previews for the visible items
 *   are generated first.
 *
 * @since 4.2
 */
class KIOFILEWIDGETS_EXPORT KFilePreviewGenerator : public QObject
{
    Q_OBJECT

public:
    /**
     * @param parent  Item view containing the file items where previews should
     *                be generated. It is mandatory that the item view specifies
     *                an icon size by QAbstractItemView::setIconSize() and that
     *                the model of the view (or the source model of the proxy model)
     *                is an instance of KDirModel. Otherwise no previews will be generated.
     */
    KFilePreviewGenerator(QAbstractItemView *parent);

    /** @internal */
    KFilePreviewGenerator(KAbstractViewAdapter *parent, QAbstractProxyModel *model);

    virtual ~KFilePreviewGenerator();

    /**
     * If \a show is set to true, a preview is generated for each item. If \a show
     * is false, the MIME type icon of the item is shown instead. Per default showing
     * the preview is turned on. Note that it is mandatory that the item view
     * specifies an icon size by QAbstractItemView::setIconSize(), otherwise
     * KFilePreviewGenerator::isPreviewShown() will always return false.
     */
    void setPreviewShown(bool show);
    bool isPreviewShown() const;

#if KIOFILEWIDGETS_ENABLE_DEPRECATED_SINCE(4, 3)
    /**
     * @deprecated Since 4.3, use KFilePreviewGenerator::updateIcons() instead.
     */
    KIOFILEWIDGETS_DEPRECATED_VERSION(4, 3, "Use KFilePreviewGenerator::updateIcons()")
    void updatePreviews();
#endif

    /**
     * Sets the list of enabled thumbnail plugins.
     * Per default all plugins enabled in the KConfigGroup "PreviewSettings"
     * are used.
     *
     * Note that this method doesn't cause already generated previews
     * to be regenerated.
     *
     * For a list of available plugins, call KServiceTypeTrader::self()->query("ThumbCreator").
     *
     * @see enabledPlugins
     */
    void setEnabledPlugins(const QStringList &list);

    /**
     * Returns the list of enabled thumbnail plugins.
     * @see setEnabledPlugins
     */
    QStringList enabledPlugins() const;

public Q_SLOTS:
    /**
     * Updates the icons for all items. Usually it is only
     * necessary to invoke this method when the icon size of the abstract item view
     * has been changed by QAbstractItemView::setIconSize(). Note that this method
     * should also be invoked if previews have been turned off, as the icons for
     * cut items must be updated when the icon size has changed.
     * @since 4.3
     */
    void updateIcons();

    /** Cancels all pending previews. */
    void cancelPreviews();

private:
    friend class KFilePreviewGeneratorPrivate;
    std::unique_ptr<KFilePreviewGeneratorPrivate> const d;

    Q_DISABLE_COPY(KFilePreviewGenerator)

    Q_PRIVATE_SLOT(d, void pauseIconUpdates())
};

#endif
