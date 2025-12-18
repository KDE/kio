/*  This file is part of the KDE libraries
 *    SPDX-FileCopyrightText: 2000 Malte Starostik <malte@kde.org>
 *    SPDX-FileCopyrightText: 2000 Carsten Pfeiffer <pfeiffer@kde.org>
 *
 *    SPDX-License-Identifier: LGPL-2.0-or-later
 */

#include "thumbnail.h"
#include "thumbnail-logsettings.h"

#include <stdlib.h>
#ifdef __FreeBSD__
#include <machine/param.h>
#endif
#include <sys/types.h>
#if defined(Q_OS_WINDOWS)
#include <windows.h>
#else
#include <sys/ipc.h>
#ifndef Q_OS_HAIKU
#include <sys/shm.h>
#endif
#include <unistd.h> // nice()
#endif

#include <QApplication>
#include <QBuffer>
#include <QColorSpace>
#include <QCryptographicHash>
#include <QDebug>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QGuiApplication>
#include <QIcon>
#include <QImage>
#include <QLibrary>
#include <QMimeDatabase>
#include <QMimeType>
#include <QPixmap>
#include <QPluginLoader>
#include <QSaveFile>
#include <QSettings>
#include <QUrl>

#include <KConfigGroup>
#include <KFileItem>
#include <KLocalizedString>
#include <KSharedConfig>

#include <KIO/PreviewJob>
#include <KPluginFactory>

#include <limits>

#include "imagefilter.h"

// Recognized metadata entries:
// mimeType     - the mime type of the file, used for the overlay icon if any
// width        - maximum width for the thumbnail
// height       - maximum height for the thumbnail
// iconSize     - the size of the overlay icon to use if any (deprecated, ignored)
// iconAlpha    - the transparency value used for icon overlays (deprecated, ignored)
// plugin       - the name of the plugin library to be used for thumbnail creation.
//                Provided by the application to save an addition KTrader
//                query here.
// devicePixelRatio - the devicePixelRatio to use for the output,
//                     the dimensions of the output is multiplied by it and output pixmap will have devicePixelRatio
// enabledPlugins - a list of enabled thumbnailer plugins. PreviewJob does not call
//                  this thumbnail worker when a given plugin isn't enabled. However,
//                  for directory thumbnails it doesn't know that the thumbnailer
//                  internally also loads the plugins.
// shmid        - the shared memory segment id to write the image's data to.
//                The segment is assumed to provide enough space for a 32-bit
//                image sized width x height pixels.
//                If this is given, the data returned by the worker will be:
//                    int width
//                    int height
//                    int depth
//                Otherwise, the data returned is the image in PNG format.

using namespace KIO;
using namespace Qt::StringLiterals;

// copied from QGenericUnixThemes

static inline QByteArray detectDesktopEnvironment()
{
    const QByteArray xdgCurrentDesktop = qgetenv("XDG_CURRENT_DESKTOP");
    if (!xdgCurrentDesktop.isEmpty())
        return xdgCurrentDesktop.toUpper(); // KDE, GNOME, UNITY, LXDE, MATE, XFCE...

    // Classic fallbacks
    if (!qEnvironmentVariableIsEmpty("KDE_FULL_SESSION"))
        return QByteArrayLiteral("KDE");
    if (!qEnvironmentVariableIsEmpty("GNOME_DESKTOP_SESSION_ID"))
        return QByteArrayLiteral("GNOME");

    // Fallback to checking $DESKTOP_SESSION (unreliable)
    QByteArray desktopSession = qgetenv("DESKTOP_SESSION");

    // This can be a path in /usr/share/xsessions
    int slash = desktopSession.lastIndexOf('/');
    if (slash != -1) {
#if QT_CONFIG(settings)
        QSettings desktopFile(QFile::decodeName(desktopSession + ".desktop"), QSettings::IniFormat);
        desktopFile.beginGroup(QStringLiteral("Desktop Entry"));
        QByteArray desktopName = desktopFile.value(QStringLiteral("DesktopNames")).toByteArray();
        if (!desktopName.isEmpty())
            return desktopName;
#endif

        // try decoding just the basename
        desktopSession = desktopSession.mid(slash + 1);
    }

    if (desktopSession == "gnome")
        return QByteArrayLiteral("GNOME");
    else if (desktopSession == "xfce")
        return QByteArrayLiteral("XFCE");
    else if (desktopSession == "kde")
        return QByteArrayLiteral("KDE");

    return QByteArrayLiteral("UNKNOWN");
}

static QStringList themeNames()
{
    QStringList result;
    if (QGuiApplication::desktopSettingsAware()) {
        const QByteArray desktopEnvironment = detectDesktopEnvironment();
        QList<QByteArray> gtkBasedEnvironments;
        gtkBasedEnvironments << "GNOME"
                             << "X-CINNAMON"
                             << "PANTHEON"
                             << "UNITY"
                             << "MATE"
                             << "XFCE"
                             << "LXDE";
        const QList<QByteArray> desktopNames = desktopEnvironment.split(':');
        for (const QByteArray &desktopName : desktopNames) {
            if (desktopEnvironment == "KDE") {
#if QT_CONFIG(settings)
                result.push_back(QLatin1StringView("kde"));
#endif
            } else if (gtkBasedEnvironments.contains(desktopName)) {
                // prefer the GTK3 theme implementation with native dialogs etc.
                result.push_back(QStringLiteral("gtk3"));
                // fallback to the generic Gnome theme if loading the GTK3 theme fails
                result.push_back(QLatin1StringView("gnome"));
            } else {
                // unknown, but lowercase the name (our standard practice) and
                // remove any "x-" prefix
                QString s = QString::fromLatin1(desktopName.toLower());
                result.push_back(s.startsWith(u"x-") ? s.mid(2) : s);
            }
        }
    } // desktopSettingsAware
    result.append(QLatin1StringView("generic"));
    return result;
}

// End copied from QGenericUnixThemes

// Pseudo plugin class to embed meta data
class KIOPluginForMetaData : public QObject
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID "org.kde.kio.worker.thumbnail" FILE "thumbnail.json")
};

extern "C" Q_DECL_EXPORT int kdemain(int argc, char **argv)
{
#if defined(Q_OS_WINDOWS)
    SetPriorityClass(GetCurrentProcess(), BELOW_NORMAL_PRIORITY_CLASS);
#else
    std::ignore = nice(5);
#endif

    QCoreApplication::setAttribute(Qt::AA_ShareOpenGLContexts);

    // Creating a QApplication in a worker in not a very good idea,
    // as dispatchLoop() doesn't allow it to process its messages,
    // so it for example wouldn't reply to ksmserver - on the other
    // hand, this worker uses QPixmaps for some reason, and they
    // need QGuiApplication
    QCoreApplication::setAttribute(Qt::AA_DisableSessionManager);

    // Some plugins may cause unwanted windows to appear
    // (e.g. Webarchiver, see https://bugs.kde.org/show_bug.cgi?id=500173).
    // This will not let any plugin to create auxilliary windows on the screen
    // while generating thumbnails.
    // Retrieve theme name of the default platform and force using offscreen platform with the default platformtheme
    qputenv("QT_QPA_PLATFORM", "offscreen");
    if (qEnvironmentVariableIsEmpty("QT_QPA_PLATFORMTHEME")) {
        const QString themeName = themeNames().first();
        qputenv("QT_QPA_PLATFORMTHEME", themeName.toUtf8());
    }

    // Some thumbnail plugins use QWidget classes for the rendering,
    // so use QApplication here, not just QGuiApplication
    QApplication app(argc, argv);

    if (argc != 4) {
        qCritical() << "Usage: kio_thumbnail protocol domain-socket1 domain-socket2";
        exit(-1);
    }

    ThumbnailProtocol worker(argv[2], argv[3]);
    worker.dispatchLoop();

    return 0;
}

ThumbnailProtocol::ThumbnailProtocol(const QByteArray &pool, const QByteArray &app)
    : WorkerBase("thumbnail", pool, app)
    , m_width(0)
    , m_height(0)
    , m_devicePixelRatio(1.0)
    , m_maxFileSize(0)
    , m_randomGenerator()
{
}

ThumbnailProtocol::~ThumbnailProtocol()
{
    qDeleteAll(m_creators);
}

/**
 * Scales down the image \p img in a way that it fits into the given maximum width and height
 */
void scaleDownImage(QImage &img, int maxWidth, int maxHeight)
{
    if (img.width() > maxWidth || img.height() > maxHeight) {
        img = img.scaled(maxWidth, maxHeight, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    }
}

/**
 * @brief convertToStandardRgb
 * Convert preview to sRGB for proper viewing on most monitors.
 */
void convertToStandardRgb(QImage &img)
{
    auto cs = img.colorSpace();
    if (!cs.isValid()) {
        return;
    }
    if (cs.transferFunction() != QColorSpace::TransferFunction::SRgb || cs.primaries() != QColorSpace::Primaries::SRgb) {
        img.convertToColorSpace(QColorSpace(QColorSpace::SRgb));
    }
}

KIO::WorkerResult ThumbnailProtocol::get(const QUrl &url)
{
    m_mimeType = metaData(u"mimeType"_s);
    m_enabledPlugins = metaData(u"enabledPlugins"_s).split(QLatin1Char(','), Qt::SkipEmptyParts);
    if (m_enabledPlugins.isEmpty()) {
        const KConfigGroup globalConfig(KSharedConfig::openConfig(), QStringLiteral("PreviewSettings"));
        m_enabledPlugins = globalConfig.readEntry("Plugins", KIO::PreviewJob::defaultPlugins());
    }

    Q_ASSERT(url.scheme() == u"thumbnail"_s);
    QFileInfo info(url.path());
    Q_ASSERT_X(info.isAbsolute(), "ThumbnailProtocol::get", qPrintable(u"path is not absolute: "_s + info.filePath()));

    if (!info.exists()) {
        // The file does not exist
        return KIO::WorkerResult::fail(KIO::ERR_DOES_NOT_EXIST, url.path());
    } else if (!info.isReadable()) {
        // The file is not readable!
        return KIO::WorkerResult::fail(KIO::ERR_CANNOT_READ, url.path());
    }

    // qDebug() << "Wanting MIME Type:" << m_mimeType;
    bool direct = false;
    if (m_mimeType.isEmpty()) {
        // qDebug() << "PATH: " << url.path() << "isDir:" << info.isDir();
        if (info.isDir()) {
            m_mimeType = u"inode/directory"_s;
        } else {
            const QMimeDatabase db;

            m_mimeType = db.mimeTypeForFile(info).name();
        }

        // qDebug() << "Guessing MIME Type:" << m_mimeType;
        direct = true; // thumbnail: URL was probably typed in Konqueror
    }

    if (m_mimeType.isEmpty()) {
        return KIO::WorkerResult::fail(KIO::ERR_INTERNAL, i18n("No MIME Type specified."));
    }

    m_width = metaData(u"width"_s).toInt();
    m_height = metaData(u"height"_s).toInt();

    if (m_width < 0 || m_height < 0) {
        return KIO::WorkerResult::fail(KIO::ERR_INTERNAL, i18n("No or invalid size specified."));
    } else if (!m_width || !m_height) {
        // qDebug() << "Guessing height, width, icon size!";
        m_width = 128;
        m_height = 128;
    }
    bool ok;
    m_devicePixelRatio = metaData(u"devicePixelRatio"_s).toFloat(&ok);
    if (!ok || qFuzzyIsNull(m_devicePixelRatio)) {
        m_devicePixelRatio = 1.0;
    } else {
        m_width *= m_devicePixelRatio;
        m_height *= m_devicePixelRatio;
    }

    QImage img;
    QString plugin = metaData(u"plugin"_s);

    if ((plugin.isEmpty() || plugin.contains(u"directorythumbnail"_s)) && m_mimeType == u"inode/directory"_s) {
        img = thumbForDirectory(info.canonicalFilePath());
        if (img.isNull()) {
            return KIO::WorkerResult::fail(KIO::ERR_INTERNAL, i18n("Cannot create thumbnail for directory"));
        }
    } else {
        if (plugin.isEmpty()) {
            plugin = pluginForMimeType(m_mimeType).fileName();
        }

        // qDebug() << "Guess plugin: " << plugin;
        if (plugin.isEmpty()) {
            return KIO::WorkerResult::fail(KIO::ERR_INTERNAL, i18n("No plugin specified."));
        }

        ThumbCreatorWithMetadata *creator = getThumbCreator(plugin);
        if (!creator) {
            return KIO::WorkerResult::fail(KIO::ERR_INTERNAL, i18n("Cannot load ThumbCreator %1", plugin));
        }

        if (creator->handleSequences) {
            setMetaData(u"handlesSequences"_s, QStringLiteral("1"));
        }

        if (!createThumbnail(creator, info.canonicalFilePath(), m_width, m_height, img)) {
            return KIO::WorkerResult::fail(KIO::ERR_INTERNAL, i18n("Cannot create thumbnail for %1", info.canonicalFilePath()));
        }

        // We MUST do this after calling create(), because the create() call itself might change it.
        if (creator->handleSequences) {
            setMetaData(u"sequenceIndexWraparoundPoint"_s, QString::number(m_sequenceIndexWrapAroundPoint));
        }
    }

    if (img.isNull()) {
        return KIO::WorkerResult::fail(KIO::ERR_INTERNAL, i18n("Failed to create a thumbnail."));
    }

    // image quality and size corrections
    scaleDownImage(img, m_width, m_height);

    convertToStandardRgb(img);

    if (img.colorCount() > 0 || img.depth() > 32) {
        // images using indexed color format, are not loaded properly by QImage ctor using in shm code path
        // convert the format to regular RGB
        // Also limit the bits per pixel to 32 since PreviewJob only allocates as much shared memory
        img = img.convertToFormat(img.hasAlphaChannel() ? QImage::Format_ARGB32 : QImage::Format_RGB32);
    }

    if (direct) {
        // If thumbnail was called directly from Konqueror, then the image needs to be raw
        // qDebug() << "RAW IMAGE TO STREAM";
        QBuffer buf;
        if (!buf.open(QIODevice::WriteOnly)) {
            return KIO::WorkerResult::fail(KIO::ERR_INTERNAL, i18n("Could not write image."));
        }
        img.save(&buf, "PNG");
        buf.close();
        mimeType(u"image/png"_s);
        data(buf.buffer());
        return KIO::WorkerResult::pass();
    }

    QByteArray imgData;
    QDataStream stream(&imgData, QIODevice::WriteOnly);

    // Keep in sync with kio/src/previewjob.cpp
    stream << img.width() << img.height() << img.format() << img.devicePixelRatio();

#ifndef Q_OS_WIN
    const QString shmid = metaData(u"shmid"_s);
    if (shmid.isEmpty())
#endif
    {
        // qDebug() << "IMAGE TO STREAM";
        stream << img;
    }
#if !defined(Q_OS_WIN) && !defined(Q_OS_HAIKU)
    else {
        // qDebug() << "IMAGE TO SHMID";
        void *shmaddr = shmat(shmid.toInt(), nullptr, 0);
        if (shmaddr == (void *)-1) {
            return KIO::WorkerResult::fail(KIO::ERR_INTERNAL, i18n("Failed to attach to shared memory segment %1", shmid));
        }
        struct shmid_ds shmStat;
        if (shmctl(shmid.toInt(), IPC_STAT, &shmStat) == -1 || shmStat.shm_segsz < (uint)img.sizeInBytes()) {
            return KIO::WorkerResult::fail(KIO::ERR_INTERNAL, i18n("Image is too big for the shared memory segment"));
            shmdt((char *)shmaddr);
        }
        memcpy(shmaddr, img.constBits(), img.sizeInBytes());
        shmdt((char *)shmaddr);
    }
#endif
    mimeType(u"application/octet-stream"_s);
    data(imgData);

    return KIO::WorkerResult::pass();
}

KPluginMetaData ThumbnailProtocol::pluginForMimeType(const QString &mimeType)
{
    static const QList<KPluginMetaData> plugins = KPluginMetaData::findPlugins(QStringLiteral("kf6/thumbcreator"));
    for (const KPluginMetaData &plugin : plugins) {
        if (plugin.supportsMimeType(mimeType)) {
            return plugin;
        }
    }
    for (const auto &plugin : plugins) {
        const QStringList mimeTypes = plugin.mimeTypes();
        for (const QString &mime : mimeTypes) {
            if (mime.endsWith(u"*"_s)) {
                const auto mimeGroup = QStringView(mime).left(mime.length() - 1);
                if (mimeType.startsWith(mimeGroup)) {
                    return plugin;
                }
            }
        }
    }

    return {};
}

float ThumbnailProtocol::sequenceIndex() const
{
    return metaData(u"sequence-index"_s).toFloat();
}

bool ThumbnailProtocol::isOpaque(const QImage &image) const
{
    // Test the corner pixels
    return qAlpha(image.pixel(QPoint(0, 0))) == 255 && qAlpha(image.pixel(QPoint(image.width() - 1, 0))) == 255
        && qAlpha(image.pixel(QPoint(0, image.height() - 1))) == 255 && qAlpha(image.pixel(QPoint(image.width() - 1, image.height() - 1))) == 255;
}

void ThumbnailProtocol::drawPictureFrame(QPainter *painter,
                                         const QPoint &centerPos,
                                         const QImage &image,
                                         int borderStrokeWidth,
                                         QSize imageTargetSize,
                                         int rotationAngle) const
{
    // Scale the image down so it matches the aspect ratio
    float scaling = 1.0;

    const bool landscapeDimension = image.width() > image.height();
    const bool hasTargetSizeWidth = imageTargetSize.width() != 0;
    const bool hasTargetSizeHeight = imageTargetSize.height() != 0;
    const int widthWithFrames = image.width() + (2 * borderStrokeWidth);
    const int heightWithFrames = image.height() + (2 * borderStrokeWidth);
    if (landscapeDimension && (widthWithFrames > imageTargetSize.width()) && hasTargetSizeWidth) {
        scaling = float(imageTargetSize.width()) / float(widthWithFrames);
    } else if ((heightWithFrames > imageTargetSize.height()) && hasTargetSizeHeight) {
        scaling = float(imageTargetSize.height()) / float(heightWithFrames);
    }

    const float scaledFrameWidth = borderStrokeWidth / scaling;

    QTransform m;
    m.rotate(rotationAngle);
    m.scale(scaling, scaling);

    const QRectF frameRect(
        QPointF(0, 0),
        QPointF(image.width() / image.devicePixelRatio() + scaledFrameWidth * 2, image.height() / image.devicePixelRatio() + scaledFrameWidth * 2));

    QRect r = m.mapRect(QRectF(frameRect)).toAlignedRect();

    QImage transformed(r.size(), QImage::Format_ARGB32);
    transformed.fill(0);
    QPainter p(&transformed);
    p.setRenderHint(QPainter::SmoothPixmapTransform);
    p.setRenderHint(QPainter::Antialiasing);
    p.setCompositionMode(QPainter::CompositionMode_Source);

    p.translate(-r.topLeft());
    p.setWorldTransform(m, true);

    if (isOpaque(image)) {
        p.setPen(Qt::NoPen);
        p.setBrush(Qt::white);
        p.drawRoundedRect(frameRect, scaledFrameWidth / 2, scaledFrameWidth / 2);
    }
    p.drawImage(scaledFrameWidth, scaledFrameWidth, image);
    p.end();

    int radius = qMax(borderStrokeWidth, 1);

    QImage shadow(r.size() + QSize(radius * 2, radius * 2), QImage::Format_ARGB32);
    shadow.fill(0);

    p.begin(&shadow);
    p.setCompositionMode(QPainter::CompositionMode_Source);
    p.drawImage(radius, radius, transformed);
    p.end();

    ImageFilter::shadowBlur(shadow, radius, QColor(0, 0, 0, 128));

    r.moveCenter(centerPos);

    painter->drawImage(r.topLeft() - QPoint(radius / 2, radius / 2), shadow);
    painter->drawImage(r.topLeft(), transformed);
}

QImage ThumbnailProtocol::thumbForDirectory(const QString &directory)
{
    QImage img;
    KFileItem item(QUrl::fromLocalFile(directory));

    const KConfigGroup globalConfig(KSharedConfig::openConfig(), QStringLiteral("PreviewSettings"));
    m_maxFileSize = !item.isSlow() ? globalConfig.readEntry("MaximumSize", std::numeric_limits<KIO::filesize_t>::max())
                                   : globalConfig.readEntry<KIO::filesize_t>("MaximumRemoteSize", 0);

    if (m_propagationDirectories.isEmpty()) {
        // Directories that the directory preview will be propagated into if there is no direct sub-directories
        const QStringList propagationDirectoriesList = globalConfig.readEntry("PropagationDirectories", QStringList() << u"VIDEO_TS"_s);
        m_propagationDirectories = QSet<QString>(propagationDirectoriesList.begin(), propagationDirectoriesList.end());
    }

    const int tiles = 2; // Count of items shown on each dimension
    const int spacing = 1 * m_devicePixelRatio;
    const int visibleCount = tiles * tiles;

    // TODO: the margins are optimized for the Oxygen iconset
    // Provide a fallback solution for other iconsets (e. g. draw folder
    // only as small overlay, use no margins)

    const int extent = qMin(m_width, m_height);
    QPixmap folder = QIcon::fromTheme(item.iconName()).pixmap(extent);
    folder.setDevicePixelRatio(m_devicePixelRatio);

    // Scale up base icon to ensure overlays are rendered with
    // the best quality possible even for low-res custom folder icons
    if (qMax(folder.width(), folder.height()) < extent) {
        folder = folder.scaled(extent, extent, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    }

    const int folderWidth = folder.width();
    const int folderHeight = folder.height();

    const int topMargin = folderHeight * 30 / 100;
    const int bottomMargin = folderHeight / 6;
    const int leftMargin = folderWidth / 13;
    const int rightMargin = leftMargin;
    // the picture border stroke width 1/170 rounded up
    // (i.e for each 170px the folder width increases those border increase by 1 px)
    const int borderStrokeWidth = qRound(folderWidth / 170.);

    const int segmentWidth = (folderWidth - leftMargin - rightMargin + spacing) / tiles - spacing;
    const int segmentHeight = (folderHeight - topMargin - bottomMargin + spacing) / tiles - spacing;
    if ((segmentWidth < 5 * m_devicePixelRatio) || (segmentHeight < 5 * m_devicePixelRatio)) {
        // the segment size is too small for a useful preview
        return img;
    }

    // Advance to the next tile page each second
    int skipValidItems = ((int)sequenceIndex()) * visibleCount;

    img = QImage(QSize(folderWidth, folderHeight), QImage::Format_ARGB32);
    img.setDevicePixelRatio(m_devicePixelRatio);
    img.fill(0);

    QPainter p;
    p.begin(&img);

    p.setCompositionMode(QPainter::CompositionMode_Source);
    p.drawPixmap(0, 0, folder);
    p.setCompositionMode(QPainter::CompositionMode_SourceOver);

    int xPos = leftMargin;
    int yPos = topMargin;

    int iterations = 0;
    QString hadFirstThumbnail;
    QImage firstThumbnail;

    int validThumbnails = 0;
    int totalValidThumbs = -1;

    while (true) {
        QDirIterator dir(directory, QDir::Files | QDir::Readable);
        int skipped = 0;

        // Seed the random number generator so that it always returns the same result
        // for the same directory and sequence-item
        m_randomGenerator.seed(qHash(directory) + skipValidItems);
        while (dir.hasNext()) {
            ++iterations;
            if (iterations > 500) {
                skipValidItems = skipped = 0;
                break;
            }

            dir.next();

            if (dir.fileInfo().isSymbolicLink()) {
                // Skip symbolic links, as these may point to e.g. network file
                // systems or other slow storage. The calling code already
                // checks for the directory itself, and if it is fine any
                // contained plain file is fine as well.
                continue;
            }

            const auto fileSize = KIO::filesize_t(dir.fileInfo().size());
            if ((fileSize == 0) || (fileSize > m_maxFileSize)) {
                // don't create thumbnails for files that exceed
                // the maximum set file size or are empty
                continue;
            }

            QImage subThumbnail;
            if (!createSubThumbnail(subThumbnail, dir.filePath(), segmentWidth, segmentHeight)) {
                continue;
            }

            if (skipped < skipValidItems) {
                ++skipped;
                continue;
            }

            drawSubThumbnail(p, subThumbnail, segmentWidth, segmentHeight, xPos, yPos, borderStrokeWidth);

            if (hadFirstThumbnail.isEmpty()) {
                hadFirstThumbnail = dir.filePath();
                firstThumbnail = subThumbnail;
            }

            ++validThumbnails;
            if (validThumbnails >= visibleCount) {
                break;
            }

            xPos += segmentWidth + spacing;
            if (xPos > folderWidth - rightMargin - segmentWidth) {
                xPos = leftMargin;
                yPos += segmentHeight + spacing;
            }
        }

        if (!dir.hasNext() && totalValidThumbs < 0) {
            // We iterated over the entire directory for the first time, so now we know how many thumbs
            // were actually created.
            totalValidThumbs = skipped + validThumbnails;
        }

        if (validThumbnails > 0) {
            break;
        }

        if (skipped == 0) {
            break; // No valid items were found
        }

        // Calculate number of (partial) pages for all valid items in the directory
        auto skippedPages = (skipped + visibleCount - 1) / visibleCount;

        // The sequence is continously repeated after all valid items, calculate remainder
        skipValidItems = (((int)sequenceIndex()) % skippedPages) * visibleCount;
    }

    p.end();

    if (totalValidThumbs >= 0) {
        // We only know this once we've iterated over the entire directory, so this will only be
        // set for large enough sequence indices.
        const int wraparoundPoint = (totalValidThumbs - 1) / visibleCount + 1;
        setMetaData(u"sequenceIndexWraparoundPoint"_s, QString().setNum(wraparoundPoint));
    }
    setMetaData(u"handlesSequences"_s, QStringLiteral("1"));

    if (validThumbnails == 0) {
        // Eventually propagate the contained items from a sub-directory
        QDirIterator dir(directory, QDir::Dirs);
        int max = 50;
        while (dir.hasNext() && max > 0) {
            --max;
            dir.next();
            if (m_propagationDirectories.contains(dir.fileName())) {
                return thumbForDirectory(dir.filePath());
            }
        }

        // If no thumbnail could be found, return an empty image which indicates
        // that no preview for the directory is available.
        img = QImage();
    }

    // If only for one file a thumbnail could be generated then paint an image with only one tile
    if (validThumbnails == 1) {
        QImage oneTileImg(folder.size(), QImage::Format_ARGB32);
        oneTileImg.setDevicePixelRatio(m_devicePixelRatio);
        oneTileImg.fill(0);

        QPainter oneTilePainter(&oneTileImg);
        oneTilePainter.setCompositionMode(QPainter::CompositionMode_Source);
        oneTilePainter.drawPixmap(0, 0, folder);
        oneTilePainter.setCompositionMode(QPainter::CompositionMode_SourceOver);

        const int oneTileWidth = folderWidth - leftMargin - rightMargin;
        const int oneTileHeight = folderHeight - topMargin - bottomMargin;

        if (firstThumbnail.width() < oneTileWidth && firstThumbnail.height() < oneTileHeight) {
            createSubThumbnail(firstThumbnail, hadFirstThumbnail, oneTileWidth, oneTileHeight);
        }
        drawSubThumbnail(oneTilePainter, firstThumbnail, oneTileWidth, oneTileHeight, leftMargin, topMargin, borderStrokeWidth);
        return oneTileImg;
    }

    return img;
}

ThumbCreatorWithMetadata *ThumbnailProtocol::getThumbCreator(const QString &plugin)
{
    auto it = m_creators.constFind(plugin);
    if (it != m_creators.constEnd()) {
        return *it;
    }

    const KPluginMetaData md(plugin);
    const KPluginFactory::Result result = KPluginFactory::instantiatePlugin<KIO::ThumbnailCreator>(md);

    if (result) {
        auto creator = new ThumbCreatorWithMetadata{
            std::unique_ptr<ThumbnailCreator>(result.plugin),
            md.value(u"CacheThumbnail"_s, true),
            true, // KIO::ThumbnailCreator are always dpr-aware
            md.value(u"HandleSequences"_s, false),
        };

        m_creators.insert(plugin, creator);
        return creator;
    }

    return nullptr;
}

void ThumbnailProtocol::ensureDirsCreated()
{
    if (m_thumbBasePath.isEmpty()) {
        m_thumbBasePath = QStandardPaths::writableLocation(QStandardPaths::GenericCacheLocation) + QLatin1String("/thumbnails/");
        QDir basePath(m_thumbBasePath);
        basePath.mkpath(u"normal/"_s);
        QFile::setPermissions(basePath.absoluteFilePath(u"normal"_s), QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner);
        basePath.mkpath(u"large/"_s);
        QFile::setPermissions(basePath.absoluteFilePath(u"large"_s), QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner);
        if (m_devicePixelRatio > 1) {
            basePath.mkpath(u"x-large/"_s);
            QFile::setPermissions(basePath.absoluteFilePath(u"x-large"_s), QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner);
            basePath.mkpath(u"xx-large/"_s);
            QFile::setPermissions(basePath.absoluteFilePath(u"xx-large"_s), QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner);
        }
    }
}

bool ThumbnailProtocol::createSubThumbnail(QImage &thumbnail, const QString &filePath, int segmentWidth, int segmentHeight)
{
    const QMimeDatabase db;
    const KPluginMetaData subPlugin = pluginForMimeType(db.mimeTypeForFile(filePath).name());

    if (!subPlugin.isValid() || !m_enabledPlugins.contains(subPlugin.pluginId())) {
        return false;
    }

    ThumbCreatorWithMetadata *subCreator = getThumbCreator(subPlugin.fileName());
    if (!subCreator) {
        return false;
    }

    const auto maxDimension = qMin(1024.0, 512.0 * m_devicePixelRatio);
    if ((segmentWidth <= maxDimension) && (segmentHeight <= maxDimension)) {
        // check whether a cached version of the file is available for
        // 128 x 128, 256 x 256 pixels or 512 x 512 pixels taking into account devicePixelRatio
        int cacheSize = 0;
        QCryptographicHash md5(QCryptographicHash::Md5);
        const QByteArray fileUrl = QUrl::fromLocalFile(filePath).toEncoded();
        md5.addData(fileUrl);
        const QString thumbName = QString::fromLatin1(md5.result().toHex()).append(u".png"_s);

        ensureDirsCreated();

        struct CachePool {
            QString path;
            int minSize;
        };

        static const auto pools = {
            CachePool{QStringLiteral("normal/"), 128},
            CachePool{QStringLiteral("large/"), 256},
            CachePool{QStringLiteral("x-large/"), 512},
            CachePool{QStringLiteral("xx-large/"), 1024},
        };

        const int wants = std::max(segmentWidth, segmentHeight);
        for (const auto &pool : pools) {
            if (pool.minSize < wants) {
                continue;
            } else if (cacheSize == 0) {
                // the lowest cache size the thumbnail could be at
                cacheSize = pool.minSize;
            }
            // try in folders with higher image quality as well
            if (thumbnail.load(m_thumbBasePath + pool.path + thumbName, "png")) {
                thumbnail.setDevicePixelRatio(m_devicePixelRatio);
                break;
            }
        }

        // no cached version is available, a new thumbnail must be created
        if (thumbnail.isNull()) {
            if (createThumbnail(subCreator, filePath, cacheSize, cacheSize, thumbnail)) {
                scaleDownImage(thumbnail, cacheSize, cacheSize);

                // The thumbnail has been created successfully. Check if we can store
                // the thumbnail to the cache for future access.
                if (subCreator->cacheThumbnail && metaData(u"cache"_s).toInt() && !thumbnail.isNull()) {
                    QString thumbPath;
                    const int wants = std::max(thumbnail.width(), thumbnail.height());
                    for (const auto &pool : pools) {
                        if (pool.minSize < wants) {
                            continue;
                        } else if (thumbPath.isEmpty()) {
                            // that's the appropriate path for this thumbnail
                            thumbPath = m_thumbBasePath + pool.path;
                        }
                    }

                    // The thumbnail has been created successfully. Store the thumbnail
                    // to the cache for future access.
                    QSaveFile thumbnailfile(QDir(thumbPath).absoluteFilePath(thumbName));
                    if (thumbnailfile.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
                        QFileInfo fi(filePath);
                        thumbnail.setText(QStringLiteral("Thumb::URI"), QString::fromUtf8(fileUrl));
                        thumbnail.setText(QStringLiteral("Thumb::MTime"), QString::number(fi.lastModified().toSecsSinceEpoch()));
                        thumbnail.setText(QStringLiteral("Thumb::Size"), QString::number(fi.size()));

                        if (thumbnail.save(&thumbnailfile, "png")) {
                            thumbnailfile.commit();
                        }
                    }
                }
            }
        }

        if (thumbnail.isNull()) {
            return false;
        }

    } else {
        // image requested is too big to be stored in the cache
        // create an image on demand
        if (!createThumbnail(subCreator, filePath, segmentWidth, segmentHeight, thumbnail)) {
            return false;
        }
    }

    // Make sure the image fits in the segments
    // Some thumbnail creators do not respect the width / height parameters
    scaleDownImage(thumbnail, segmentWidth, segmentHeight);
    return true;
}

bool ThumbnailProtocol::createThumbnail(ThumbCreatorWithMetadata *thumbCreator, const QString &filePath, int width, int height, QImage &thumbnail)
{
    bool success = false;

    auto result = thumbCreator->creator->create(
        KIO::ThumbnailRequest(QUrl::fromLocalFile(filePath), QSize(width, height), m_mimeType, m_devicePixelRatio, sequenceIndex()));

    success = result.isValid();
    thumbnail = result.image();
    m_sequenceIndexWrapAroundPoint = result.sequenceIndexWraparoundPoint();

    if (!success) {
        return false;
    }

    // make sure the image is not bigger than the expected size
    scaleDownImage(thumbnail, width, height);

    thumbnail.setDevicePixelRatio(m_devicePixelRatio);
    convertToStandardRgb(thumbnail);

    return true;
}

void ThumbnailProtocol::drawSubThumbnail(QPainter &p, QImage subThumbnail, int width, int height, int xPos, int yPos, int borderStrokeWidth)
{
    scaleDownImage(subThumbnail, width, height);

    // center the image inside the segment boundaries
    const QPoint centerPos((xPos + width / 2) / m_devicePixelRatio, (yPos + height / 2) / m_devicePixelRatio);
    const int rotationAngle = m_randomGenerator.bounded(-8, 9); // Random rotation ±8°
    drawPictureFrame(&p, centerPos, subThumbnail, borderStrokeWidth, QSize(width, height), rotationAngle);
}

#include "thumbnail.moc"
