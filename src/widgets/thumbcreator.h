/*
    This file is part of the KDE libraries
    SPDX-FileCopyrightText: 2000 Malte Starostik <malte@kde.org>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#ifndef _THUMBCREATOR_H_
#define _THUMBCREATOR_H_

#include "kiowidgets_export.h"

class QString;
class QImage;
class QWidget;

#if KIOWIDGETS_ENABLE_DEPRECATED_SINCE(5, 101)
/**
 * @class ThumbCreator thumbcreator.h <KIO/ThumbCreator>
 *
 * Base class for thumbnail generator plugins.
 *
 * KIO::PreviewJob, via the "thumbnail" KIO worker, uses instances of this class
 * to generate the thumbnail previews.
 *
 * To add support for a new document type, subclass ThumbCreator and implement
 * create() to generate a thumbnail for a given path.  Then create a factory
 * method called "new_creator" to return instances of your subclass:
 * \code
 * extern "C"
 * {
 *   Q_DECL_EXPORT ThumbCreator *new_creator()
 *   {
 *     return new FooThumbCreator();
 *   }
 * };
 * \endcode
 *
 * Compile your ThumbCreator as a module; for example, the relevant CMake code
 * for a thumbnailer for the "foo" filetype might look like
 * \code
 * add_library(foothumbnail MODULE foothumbnail.cpp)
 * target_link_libraries(foothumbnail PRIVATE KF5::KIOWidgets)
 *
 * install(TARGETS foothumbnail DESTINATION ${KDE_INSTALL_PLUGINDIR})
 * \endcode
 *
 * You also need to create a desktop file describing the thumbnailer.  For
 * example:
 * \code
 * [Desktop Entry]
 * Type=Service
 * Name=Foo Documents
 * X-KDE-ServiceTypes=ThumbCreator
 * MimeType=application/x-foo;
 * CacheThumbnail=true
 * X-KDE-Library=foothumbcreator
 * \endcode
 *
 * Of course, you will need to install it:
 * \code
 * install(FILES foothumbcreator.desktop DESTINATION ${KDE_INSTALL_KSERVICESDIR})
 * \endcode
 *
 * Note that you can supply a comma-separated list of MIME types to the MimeTypes
 * entry, naming all MIME types your ThumbCreator supports. You can also use
 * simple wildcards, like
 * \htmlonly "text/&#42;".\endhtmlonly\latexonly text/$\ast$.\endlatexonly
 *
 * If the thumbnail creation is cheap (such as text previews), you can set
 * \code
 * CacheThumbnail=false
 * \endcode
 * in the desktop file to prevent your thumbnails from being cached on disk.
 *
 * You can also use the "ThumbnailerVersion" optional property in the .desktop
 * file, like
 * \code
 * ThumbnailerVersion=5
 * \endcode
 * When this is incremented (or defined when it previously was not), all the
 * previously-cached thumbnails for this creator will be discarded.  You should
 * increase the version if and only if old thumbnails need to be regenerated.
 *
 * @deprecated since 5.101, use KIO::ThumbnailCreator instead
 */
class KIOWIDGETS_EXPORT ThumbCreator
{
public:
    /**
     * Flags to provide hints to the user of this plugin.
     * @see flags()
     */
    enum Flags {
        None = 0, /**< No hints. */
        BlendIcon = 2, /**< The MIME type icon should be blended over the preview. */
    };

    /**
     * Destructor.
     */
    virtual ~ThumbCreator();

    /**
     * Creates a thumbnail.
     *
     * Note that this method should not do any scaling.  The @p width and @p
     * height parameters are provided as hints for images that are generated
     * from non-image data (like text).
     *
     * @param path    The path of the file to create a preview for.  This is
     *                always a local path.
     * @param width   The requested preview width (see the note on scaling
     *                above).
     * @param height  The requested preview height (see the note on scaling
     *                above).
     * @param img     The QImage to store the preview in.
     *
     * @return @c true if a preview was successfully generated and store in @p
     *         img, @c false otherwise.
     * @deprecated since 5.101, use KIO::ThumbnailCreator instead.
     */
    KIOWIDGETS_DEPRECATED_VERSION(5, 101, "Use KIO::ThumbnailCreator instead")
    virtual bool create(const QString &path, int width, int height, QImage &img) = 0; // KF6 TODO: turn first arg into QUrl (see thumbnail/htmlcreator.cpp)

    /**
     * Returns the flags for this plugin.
     *
     * @return XOR'd flags values.
     * @see Flags
     */
    virtual Flags flags() const;
};

// KF6 TODO: rename this to something less generic
typedef ThumbCreator *(*newCreator)();

#endif

#endif
