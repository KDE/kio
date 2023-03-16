/*
    This file is part of the KDE libraries
    SPDX-FileCopyrightText: 2000 Malte Starostik <malte@kde.org>
    SPDX-FileCopyrightText: 2022 Nicolas Fella <nicolas.fella@gmx.de>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#ifndef _THUMBNAILCREATOR_H_
#define _THUMBNAILCREATOR_H_

#include "kiogui_export.h"

#include <QObject>
#include <QUrl>

#include <memory>

class QString;
class QImage;

namespace KIO
{

class ThumbnailCreatorPrivate;
class ThumbnailRequestPrivate;
class ThumbnailResultPrivate;

/*
 * Encapsulates the input data for a thumbnail request.
 * This includes the URL of the target file as well as additional
 * data such as the target size
 *
 * @since 5.100
 *
 */
class KIOGUI_EXPORT ThumbnailRequest
{
public:
    /**
     * Contruct a new ThumbnailRequest for a given file.
     *
     * @param url URL of the relevant file.
     * @param targetSize A size hint for the result image.
     * The actual result size may be different. This already
     * accounts for highdpi scaling, i.e. if a 500x500px thumbnail
     * with a DPR of 2 is requested 1000x1000 is passed here.
     * @param mimeType The MIME type of the target file.
     * @param dpr The device pixle ratio for this request. This can
     * be used to adjust the level of detail rendered. For example
     * a thumbnail for text of size 1000x1000 and DPR 1 should have
     * the name number of text lines as for a request of size 2000x2000
     * and DPR 2.
     * @param sequenceIndex If the thumbnailer supports sequences this
     * determines which sequence frame is used. Pass 0 otherwise.
     *
     */
    explicit ThumbnailRequest(const QUrl &url, const QSize &targetSize, const QString &mimeType, qreal dpr, float sequenceIndex);
    ThumbnailRequest(const ThumbnailRequest &);
    ThumbnailRequest &operator=(const ThumbnailRequest &);
    ~ThumbnailRequest();

    /**
     * URL of the relevant file
     */
    QUrl url() const;

    /**
     * The target thumbnail size
     */
    QSize targetSize() const;

    /**
     * The target file's MIME type
     */
    QString mimeType() const;

    /**
     * The device Pixel Ratio used for thumbnail creation
     */
    qreal devicePixelRatio() const;

    /**
     * If the thumb-creator can create a sequence of thumbnails,
     * it should use this to decide what sequence item to use.
     *
     * If the value is zero, the standard thumbnail should be created.
     *
     * This can be used for example to create thumbnails for different
     * timeframes in videos(For example 0m, 10m, 20m, ...).
     *
     * If the thumb-creator supports a high granularity, like a video,
     * the sub-integer precision coming from the float should be respected.
     *
     * If the end of the sequence is reached, the sequence should start
     * from the beginning.
     */
    float sequenceIndex() const;

private:
    std::unique_ptr<ThumbnailRequestPrivate> d;
};

/**
 * Encapsulates the output of a thumbnail request.
 * It contains information on whether the request was successful and,
 * if successful, the requested thumbnail
 *
 * To create a result use KIO::ThumbnailResult::pass(image) or KIO::ThumbnailResult::fail()
 *
 * @since 5.100
 */
class KIOGUI_EXPORT ThumbnailResult
{
public:
    ThumbnailResult(const ThumbnailResult &);
    ThumbnailResult &operator=(const ThumbnailResult &);
    ~ThumbnailResult();

    /**
     * The requested thumbnail.
     *
     * If the request failed the image is null
     */
    QImage image() const;

    /**
     * Whether the request was successful.
     */
    bool isValid() const;

    /**
     * Returns the point at which this thumb-creator's sequence indices
     * will wrap around (loop).
     *
     * Usually, the frontend will call setSequenceIndex() with indices
     * that increase indefinitely with time, e.g. as long as the user
     * keeps hovering a video file. Most thumb-creators however only
     * want to display a finite sequence of thumbs, after which their
     * sequence repeats.
     *
     * This method can return the sequence index at which this
     * thumb-creator's sequence starts wrapping around to the start
     * again ("looping"). The frontend may use this to generate only
     * thumbs up to this index, and then use cached versions for the
     * repeating sequence instead.
     *
     * Like sequenceIndex(), fractional values can be used if the
     * wraparound does not happen at an integer position, but
     * frontends handling only integer sequence indices may choose
     * to round it down.
     *
     * By default, this method returns a negative index, which signals
     * the frontend that it can't rely on this fixed-length sequence.
     *
     */
    float sequenceIndexWraparoundPoint() const;

    /**
     * Sets the point at which this thumb-creator's sequence indices
     * will wrap around.
     *
     * @see sequenceIndexWraparoundPoint()
     */
    void setSequenceIndexWraparoundPoint(float wraparoundPoint);

    /**
     * Create a successful result with a given image
     */
    static ThumbnailResult pass(const QImage &image);

    /**
     * Create an error result, i.e. the thumbnail creation failed
     */
    static ThumbnailResult fail();

private:
    KIOGUI_NO_EXPORT ThumbnailResult();
    std::unique_ptr<ThumbnailResultPrivate> d;
};

/**
 * @class ThumbnailCreator thumbnailcreator.h <KIO/ThumbnailCreator>
 *
 * Base class for thumbnail generator plugins.
 *
 * KIO::PreviewJob, via the "thumbnail" KIO worker, uses instances of this class
 * to generate the thumbnail previews.
 *
 * To add support for a new document type, subclass KIO::ThumbnailCreator and implement
 * create() to generate a thumbnail for a given path.
 *
 * Compile your ThumbCreator as a plugin; for example, the relevant CMake code
 * for a thumbnailer for the "foo" filetype might look like
 * \code
 * kcoreaddons_add_plugin(foothumbnail SOURCES foothumbnail.cpp INSTALL_NAMESPACE "kf${QT_MAJOR_VERSION}/thumbcreator")
 * target_link_libraries(foothumbnail PRIVATE KF5::KIOGui)
 * \endcode
 *
 * You also need a JSON file containing the metadata:
 * \code
 * {
 *   "CacheThumbnail": true,
 *   "KPlugin": {
 *       "MimeTypes": [
 *           "image/x-foo"
 *       ],
 *       "Name": "Foo Documents"
 *   }
 * }
 * \endcode
 *
 * MIME types can also use
 * simple wildcards, like
 * \htmlonly "text/&#42;".\endhtmlonly\latexonly text/$\ast$.\endlatexonly
 *
 * If the thumbnail creation is cheap (such as text previews), you can set
 * \code
 * "CacheThumbnail": false
 * \endcode
 * in metadata to prevent your thumbnails from being cached on disk.
 *
 * You can also use the "ThumbnailerVersion" optional property in the .desktop
 * file, like
 * \code
 * "ThumbnailerVersion": 5
 * \endcode
 * When this is incremented (or defined when it previously was not), all the
 * previously-cached thumbnails for this creator will be discarded.  You should
 * increase the version if and only if old thumbnails need to be regenerated.
 *
 * @since 5.100
 */
class KIOGUI_EXPORT ThumbnailCreator : public QObject
{
    Q_OBJECT
public:
    explicit ThumbnailCreator(QObject *parent, const QVariantList &args);
    virtual ~ThumbnailCreator();

    /**
     * Creates a thumbnail for a given request
     */
    virtual ThumbnailResult create(const ThumbnailRequest &request) = 0;

private:
    void *d; // Placeholder
};
}
#endif
