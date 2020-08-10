/*
    This file is part of the KDE libraries
    SPDX-FileCopyrightText: 2000 Carsten Pfeiffer <pfeiffer@kde.org>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#ifndef KURLPIXMAPPROVIDER_H
#define KURLPIXMAPPROVIDER_H

#include "kiowidgets_export.h"
#include <KPixmapProvider>

/**
 * @class KUrlPixmapProvider kurlpixmapprovider.h <KUrlPixmapProvider>
 *
 * Implementation of KPixmapProvider.
 *
 * Uses KMimeType::pixmapForURL() to resolve icons.
 *
 * Instantiate this class and supply it to the desired class, e.g.
 * \code
 * KHistoryComboBox *combo = new KHistoryComboBox(this);
 * combo->setPixmapProvider(new KUrlPixmapProvider);
 * [...]
 * \endcode
 *
 * @short Resolves pixmaps for URLs
 * @author Carsten Pfeiffer <pfeiffer@kde.org>
 *
 * @deprecated since 5.66, use KIO::iconNameForUrl to get the icon name and use QIcon::fromTheme
 */
#if KIOWIDGETS_ENABLE_DEPRECATED_SINCE(5, 66) && KCOMPLETION_ENABLE_DEPRECATED_SINCE(5, 66)

class KIOWIDGETS_EXPORT KUrlPixmapProvider : public KPixmapProvider
{
public:
    /**
     * Creates a new url pixmap provider.
     */
    KIOWIDGETS_DEPRECATED_VERSION(5, 66, "Use KIO::iconNameForUrl to get the icon name and use QIcon::fromTheme")
    KUrlPixmapProvider();

    /**
     * Destroys the url pixmap provider.
     */
    KIOWIDGETS_DEPRECATED_VERSION(5, 66, "Use KIO::iconNameForUrl to get the icon name and use QIcon::fromTheme")
    ~KUrlPixmapProvider();

    /**
     * Returns a pixmap for @p url with size @p size.
     *
     * Uses KMimeType::pixmapForURL().
     *
     * @param url the URL to fetch a pixmap for
     * @param size the size of the pixmap in pixels, or 0 for default.
     * @return the resulting pixmap
     * @see KIconLoader::StdSizes
     */
    KIOWIDGETS_DEPRECATED_VERSION(5, 66, "Use KIO::iconNameForUrl to get the icon name and use QIcon::fromTheme")
    QPixmap pixmapFor(const QString &url, int size = 0) override;
protected:
    void virtual_hook(int id, void *data) override;

private:
    class Private;
    Private *const d;
};

#endif

#endif // KURLPIXMAPPROVIDER_H
