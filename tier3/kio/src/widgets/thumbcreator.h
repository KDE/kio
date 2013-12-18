/*  This file is part of the KDE libraries
    Copyright (C) 2000 Malte Starostik <malte@kde.org>

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.
*/

#ifndef _THUMBCREATOR_H_
#define _THUMBCREATOR_H_

#include <kio/kiowidgets_export.h>

class QString;
class QImage;
class QWidget;

/**
 * This is the baseclass for "thumbnail-plugins" in KDE. Using the class
 * KIO::PreviewJob allows you to generate small images (thumbnails)
 * for any kind of file, where a "ThumbCreator" is available. Have a look
 * at kdebase/kioslave/thumbnail/ for existing ThumbCreators. Use ThumbCreatorV2
 * if the thumbnail-plugin should be configurable by the user.
 *
 * What you need to do to create and register a ThumbCreator:
 * @li Inherit from this class and reimplement the create() method to
 *     generate a thumbnail for the given file-path.
 * @li Provide a factory method in your implementation file to instantiate
 * your plugin, e.g.:
 * \code
 * extern "C"
 * {
 *   Q_DECL_EXPORT ThumbCreator *new_creator()
 *   {
 *     return new YourThumbCreator();
 *   }
 * };
 * \endcode
 *
 * Compile your ThumbCreator as a module. The contents of CMakeLists.txt
 * should look something like this, with "filetype" replaced by the type of
 * file this plugin creates thumbnails for:
 * \code
 * project(filetypethumbcreator)
 *
 * find_package(KDE4 REQUIRED)
 * include (KDE4Defaults)
 * include (ECMOptionalAddSubdirectory)
 *
 * set(filetypethumbnail_SRCS filetypethumbnail.cpp)
 *
 *
 * kde4_add_ui_files(filetypethumbnail_SRCS config.ui )
 *
 * add_library(filetypethumbnail MODULE ${filetypethumbnail_SRCS})
 * target_link_libraries(filetypethumbnail ${KDE4_KIO_LIBS})
 *
 * install(TARGETS filetypethumbnail DESTINATION ${PLUGIN_INSTALL_DIR})
 * install(FILES filetypethumbcreator.desktop DESTINATION ${SERVICES_INSTALL_DIR})
 *
 * \endcode
 *
 * @li Create a file filetypethumbcreator.desktop with the following contents:
 * \code
 * [Desktop Entry]
 * Encoding=UTF-8
 * Type=Service
 * Name=Name of the type of files your ThumbCreator supports
 * ServiceTypes=ThumbCreator
 * MimeType=application/x-somemimetype;
 * CacheThumbnail=true
 * X-KDE-Library=yourthumbcreator
 * \endcode
 *
 * You can supply a comma-separated list of mimetypes to the MimeTypes entry,
 * naming all mimetypes your ThumbCreator supports. You can also use simple
 * wildcards, like (where you see [slash], put a /)
 * \code
 *              text[slash]* or image[slash]*.
 * \endcode
 *
 * If your plugin is rather inexpensive (e.g. like the text preview ThumbCreator),
 * you can set CacheThumbnail=false to prevent your thumbnails from being cached
 * on disk.
 *
 * The following optional property can also be added to the .desktop file:
 * \code
 * ThumbnailerVersion=N
 * \endcode
 * where N is some nonnegative integer. If a cached thumbnail has been created with a
 * previous version of the thumbnailer, then the cached thumbnail will be discarded and
 * a new one will be regenerated. Increase (or define) the version number if and only if
 * old thumbnails need to be regenerated.
 * If no version number is provided, then the version is assumed to be <0.
 *
 * @short Baseclass for thumbnail-generating plugins.
 */
class KIOWIDGETS_EXPORT ThumbCreator
{
public:
  /**
   * The flags of this plugin.
   * @see flags()
   */
    enum Flags { None = 0, DrawFrame = 1, BlendIcon = 2 };
    virtual ~ThumbCreator();

    /**
     * Creates a thumbnail
     * Note that the width and height parameters should not be used
     * for scaling. Only plugins that create an image "from scratch",
     * like the TextCreator should directly use the specified size.
     * If the resulting preview is larger than width x height, it will be
     * scaled down.
     *
     * @param path the (always local) file path to create a preview for
     * @param width maximum width for the preview
     * @param height maximum height for the preview
     * @param img this image will contain the preview on success
     *
     * @return true if preview generation succeeded
     */
    virtual bool create(const QString &path, int width, int height, QImage &img) = 0;

    /**
     * The flags of this plugin:
     * @li None nothing special
     * @li DrawFrame a frame should be painted around the preview
     * @li BlendIcon the mimetype icon should be blended over the preview
     *
     * @return flags for this plugin
     */
    virtual Flags flags() const;
};

/**
 * @since 4.7
 */
class KIOWIDGETS_EXPORT ThumbCreatorV2 : public ThumbCreator
{
public:
    virtual ~ThumbCreatorV2();

    /**
     * Creates a widget that allows to configure the
     * thumbcreator by the user. The caller of this method is defined
     * as owner of the returned instance and must take care to delete it.
     * The default implementation returns 0.
     *
     * The following key in the thumbcreator .desktop file must be set to
     * mark the plugin as configurable:
     * \code
     * Configurable=true
     * \endcode
     */
    virtual QWidget *createConfigurationWidget();

    /**
     * Writes the configuration that is specified by \p configurationWidget.
     * The passed configuration widget is the instance created by
     * ThumbCreatorV2::createConfigurationWidget().
     */
    virtual void writeConfiguration(const QWidget* configurationWidget);
};

typedef ThumbCreator *(*newCreator)();

#endif
