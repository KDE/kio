/*
    This file is part of the KDE project
    SPDX-FileCopyrightText: 2007 Kevin Ottens <ervin@kde.org>
    SPDX-FileCopyrightText: 2007 David Faure <faure@kde.org>

    SPDX-License-Identifier: LGPL-2.0-only
*/

#include "kfileplacesmodel.h"
#include "kfileplacesitem_p.h"

#ifdef _WIN32_WCE
#include "Windows.h"
#include "WinBase.h"
#endif

#include <QCoreApplication>
#include <QDir>
#include <QMimeData>
#include <QTimer>
#include <QFile>
#include <QAction>
#include <QMimeDatabase>

#include <kfileitem.h>
#include <KLocalizedString>

#include <QDebug>
#include <KUrlMimeData>

#include <KBookmarkManager>

#include <kio/job.h>
#include <kprotocolinfo.h>
#include <KCoreDirLister>

#include <KConfig>
#include <KConfigGroup>

#include <solid/devicenotifier.h>
#include <solid/storageaccess.h>
#include <solid/storagedrive.h>
#include <solid/storagevolume.h>
#include <solid/opticaldrive.h>
#include <solid/opticaldisc.h>
#include <solid/portablemediaplayer.h>
#include <solid/predicate.h>
#include <QStandardPaths>

namespace {
    QString stateNameForGroupType(KFilePlacesModel::GroupType type)
    {
        switch (type) {
        case KFilePlacesModel::PlacesType:
                return QStringLiteral("GroupState-Places-IsHidden");
        case KFilePlacesModel::RemoteType:
                return QStringLiteral("GroupState-Remote-IsHidden");
        case KFilePlacesModel::RecentlySavedType:
                return QStringLiteral("GroupState-RecentlySaved-IsHidden");
        case KFilePlacesModel::SearchForType:
                return QStringLiteral("GroupState-SearchFor-IsHidden");
        case KFilePlacesModel::DevicesType:
                return QStringLiteral("GroupState-Devices-IsHidden");
        case KFilePlacesModel::RemovableDevicesType:
                return QStringLiteral("GroupState-RemovableDevices-IsHidden");
        case KFilePlacesModel::TagsType:
                return QStringLiteral("GroupState-Tags-IsHidden");
        default:
            Q_UNREACHABLE();
        }
    }

    static bool isFileIndexingEnabled()
    {
        KConfig config(QStringLiteral("baloofilerc"));
        KConfigGroup basicSettings = config.group("Basic Settings");
        return basicSettings.readEntry("Indexing-Enabled", true);
    }

    static QString timelineDateString(int year, int month, int day = 0)
    {
        const QString dateFormat = QStringLiteral("%1-%2");

        QString date = dateFormat.arg(year).arg(month, 2, 10, QLatin1Char('0'));
        if (day > 0) {
            date += QStringLiteral("-%1").arg(day, 2, 10, QLatin1Char('0'));
        }
        return date;
    }

    static QUrl createTimelineUrl(const QUrl &url)
    {
        // based on dolphin urls
        const QString timelinePrefix = QLatin1String("timeline:") + QLatin1Char('/');
        QUrl timelineUrl;

        const QString path = url.toDisplayString(QUrl::PreferLocalFile);
        if (path.endsWith(QLatin1String("/yesterday"))) {
            const QDate date = QDate::currentDate().addDays(-1);
            const int year = date.year();
            const int month = date.month();
            const int day = date.day();

            timelineUrl = QUrl(timelinePrefix + timelineDateString(year, month) +
                  QLatin1Char('/') + timelineDateString(year, month, day));
        } else if (path.endsWith(QLatin1String("/thismonth"))) {
            const QDate date = QDate::currentDate();
            timelineUrl = QUrl(timelinePrefix + timelineDateString(date.year(), date.month()));
        } else if (path.endsWith(QLatin1String("/lastmonth"))) {
            const QDate date = QDate::currentDate().addMonths(-1);
            timelineUrl = QUrl(timelinePrefix + timelineDateString(date.year(), date.month()));
        } else {
            Q_ASSERT(path.endsWith(QLatin1String("/today")));
            timelineUrl = url;
        }

        return timelineUrl;
    }

    static QUrl createSearchUrl(const QUrl &url)
    {
        QUrl searchUrl = url;

        const QString path = url.toDisplayString(QUrl::PreferLocalFile);

        const QStringList validSearchPaths = {
            QStringLiteral("/documents"),
            QStringLiteral("/images"),
            QStringLiteral("/audio"),
            QStringLiteral("/videos")
        };

        for (const QString &validPath : validSearchPaths) {
            if (path.endsWith(validPath)) {
                searchUrl.setScheme(QStringLiteral("baloosearch"));
                return searchUrl;
            }
        }

        qWarning() << "Invalid search url:" << url;

        return searchUrl;
    }
}

class Q_DECL_HIDDEN KFilePlacesModel::Private
{
public:
    explicit Private(KFilePlacesModel *self)
        : q(self),
          bookmarkManager(nullptr),
          fileIndexingEnabled(isFileIndexingEnabled()),
          tags(),
          tagsLister(new KCoreDirLister(q))
    {
        if (KProtocolInfo::isKnownProtocol(QStringLiteral("tags"))) {
            connect(tagsLister, &KCoreDirLister::itemsAdded, q, [this](const QUrl&, const KFileItemList& items) {

                if(tags.isEmpty()) {
                    QList<QUrl> existingBookmarks;

                    KBookmarkGroup root = bookmarkManager->root();
                    KBookmark bookmark = root.first();

                    while (!bookmark.isNull()) {
                        existingBookmarks.append(bookmark.url());
                        bookmark = root.next(bookmark);
                    }

                    if (!existingBookmarks.contains(QUrl(tagsUrlBase))) {
                        KBookmark alltags = KFilePlacesItem::createSystemBookmark(bookmarkManager, "All tags"
                                                                                  , i18n("All tags").toUtf8().data()
                                                                                  , QUrl(tagsUrlBase), QStringLiteral("tag"));
                    }
                }

                for (const KFileItem &item: items) {
                    const QString name = item.name();

                     if (!tags.contains(name)) {
                         tags.append(name);
                    }
                }
                _k_reloadBookmarks();
            });

            connect(tagsLister, &KCoreDirLister::itemsDeleted, q, [this](const KFileItemList& items) {
                for (const KFileItem &item: items) {
                    tags.removeAll(item.name());
                }
                _k_reloadBookmarks();
            });

            tagsLister->openUrl(QUrl(tagsUrlBase), KCoreDirLister::OpenUrlFlag::Reload);
        }
    }

    ~Private()
    {
        qDeleteAll(items);
    }

    KFilePlacesModel * const q;

    QList<KFilePlacesItem *> items;
    QVector<QString> availableDevices;
    QMap<QObject *, QPersistentModelIndex> setupInProgress;
    QStringList supportedSchemes;

    Solid::Predicate predicate;
    KBookmarkManager *bookmarkManager;

    const bool fileIndexingEnabled;

    QString alternativeApplicationName;

    void reloadAndSignal();
    QList<KFilePlacesItem *> loadBookmarkList();
    int findNearestPosition(int source, int target);

    QVector<QString> tags;
    const QString tagsUrlBase = QStringLiteral("tags:/");
    KCoreDirLister* tagsLister;

    void _k_initDeviceList();
    void _k_deviceAdded(const QString &udi);
    void _k_deviceRemoved(const QString &udi);
    void _k_itemChanged(const QString &udi);
    void _k_reloadBookmarks();
    void _k_storageSetupDone(Solid::ErrorType error, const QVariant &errorData);
    void _k_storageTeardownDone(Solid::ErrorType error, const QVariant &errorData);

private:
    bool isBalooUrl(const QUrl &url) const;
};

KBookmark KFilePlacesModel::bookmarkForUrl(const QUrl &searchUrl) const
{
    KBookmarkGroup root = d->bookmarkManager->root();
    KBookmark current = root.first();
    while (!current.isNull()) {
        if (current.url() == searchUrl) {
            return current;
        }
        current = root.next(current);
    }
    return KBookmark();
}

static inline QString versionKey() { return QStringLiteral("kde_places_version"); }

KFilePlacesModel::KFilePlacesModel(const QString &alternativeApplicationName, QObject *parent)
    : QAbstractItemModel(parent), d(new Private(this))
{
    const QString file = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) + QLatin1String("/user-places.xbel");
    d->bookmarkManager = KBookmarkManager::managerForExternalFile(file);
    d->alternativeApplicationName = alternativeApplicationName;

    // Let's put some places in there if it's empty.
    KBookmarkGroup root = d->bookmarkManager->root();

    const auto setDefaultMetadataItemForGroup = [&root](KFilePlacesModel::GroupType type) {
        root.setMetaDataItem(stateNameForGroupType(type), QStringLiteral("false"));
    };

    // Increase this version number and use the following logic to handle the update process for existing installations.
    static const int s_currentVersion = 4;

    const bool newFile = root.first().isNull() || !QFile::exists(file);
    const int fileVersion = root.metaDataItem(versionKey()).toInt();

    if (newFile || fileVersion < s_currentVersion) {
        root.setMetaDataItem(versionKey(), QString::number(s_currentVersion));

        const QList<QUrl> seenUrls = root.groupUrlList();

        auto createSystemBookmark = [this, &seenUrls](const char *translationContext,
                const QByteArray &untranslatedLabel,
                const QUrl &url,
                const QString &iconName,
                const KBookmark &after) {
            if (!seenUrls.contains(url)) {
                return KFilePlacesItem::createSystemBookmark(d->bookmarkManager, translationContext, untranslatedLabel, url, iconName, after);
            }
            return KBookmark();
        };

        if (fileVersion < 2) {
            // NOTE: The context for these I18NC_NOOP calls has to be "KFile System Bookmarks".
            // The real i18nc call is made later, with this context, so the two must match.
            // createSystemBookmark actually does nothing with its second argument, the context.
            createSystemBookmark(I18NC_NOOP("KFile System Bookmarks", "Home"),
                                 QUrl::fromLocalFile(QDir::homePath()), QStringLiteral("user-home"), KBookmark());

            // Some distros may not create various standard XDG folders by default
            // so check for their existence before adding bookmarks for them
            const QString desktopFolder = QStandardPaths::writableLocation(QStandardPaths::DesktopLocation);
            if (QDir(desktopFolder).exists()) {
                createSystemBookmark(I18NC_NOOP("KFile System Bookmarks", "Desktop"),
                                     QUrl::fromLocalFile(desktopFolder), QStringLiteral("user-desktop"), KBookmark());
            }
            const QString documentsFolder = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
            if (QDir(documentsFolder).exists()) {
                createSystemBookmark(I18NC_NOOP("KFile System Bookmarks", "Documents"),
                                     QUrl::fromLocalFile(documentsFolder), QStringLiteral("folder-documents"), KBookmark());
            }
            const QString downloadFolder = QStandardPaths::writableLocation(QStandardPaths::DownloadLocation);
            if (QDir(downloadFolder).exists()) {
                createSystemBookmark(I18NC_NOOP("KFile System Bookmarks", "Downloads"),
                                     QUrl::fromLocalFile(downloadFolder), QStringLiteral("folder-downloads"), KBookmark());
            }
            createSystemBookmark(I18NC_NOOP("KFile System Bookmarks", "Network"),
                                 QUrl(QStringLiteral("remote:/")), QStringLiteral("folder-network"), KBookmark());

            createSystemBookmark(I18NC_NOOP("KFile System Bookmarks", "Trash"),
                                 QUrl(QStringLiteral("trash:/")), QStringLiteral("user-trash"), KBookmark());
        }

        if (!newFile && fileVersion < 3) {
            KBookmarkGroup root = d->bookmarkManager->root();
            KBookmark bItem = root.first();
            while (!bItem.isNull()) {
                KBookmark nextbItem = root.next(bItem);
                const bool isSystemItem = bItem.metaDataItem(QStringLiteral("isSystemItem")) == QLatin1String("true");
                if (isSystemItem) {
                    const QString text = bItem.fullText();
                    // Because of b8a4c2223453932202397d812a0c6b30c6186c70 we need to find the system bookmark named Audio Files
                    // and rename it to Audio, otherwise users are getting untranslated strings
                    if (text == QLatin1String("Audio Files")) {
                        bItem.setFullText(QStringLiteral("Audio"));
                    } else if (text == QLatin1String("Today")) {
                        // Because of 19feef732085b444515da3f6c66f3352bbcb1824 we need to find the system bookmark named Today
                        // and rename it to Modified Today, otherwise users are getting untranslated strings
                        bItem.setFullText(QStringLiteral("Modified Today"));
                    } else if (text == QLatin1String("Yesterday")) {
                        // Because of 19feef732085b444515da3f6c66f3352bbcb1824 we need to find the system bookmark named Yesterday
                        // and rename it to Modified Yesterday, otherwise users are getting untranslated strings
                        bItem.setFullText(QStringLiteral("Modified Yesterday"));
                    } else if (text == QLatin1String("This Month")) {
                        // Because of 7e1d2fb84546506c91684dd222c2485f0783848f we need to find the system bookmark named This Month
                        // and remove it, otherwise users are getting untranslated strings
                        root.deleteBookmark(bItem);
                    } else if (text == QLatin1String("Last Month")) {
                        // Because of 7e1d2fb84546506c91684dd222c2485f0783848f we need to find the system bookmark named Last Month
                        // and remove it, otherwise users are getting untranslated strings
                        root.deleteBookmark(bItem);
                    }
                }

                bItem = nextbItem;
            }
        }
        if (fileVersion < 4) {
            auto findSystemBookmark = [this](const QString &untranslatedText) {
                KBookmarkGroup root = d->bookmarkManager->root();
                KBookmark bItem = root.first();
                while (!bItem.isNull()) {
                    const bool isSystemItem = bItem.metaDataItem(QStringLiteral("isSystemItem")) == QLatin1String("true");
                    if (isSystemItem && bItem.fullText() == untranslatedText) {
                        return bItem;
                    }
                    bItem = root.next(bItem);
                }
                return KBookmark();
            };
            // This variable is used to insert the new bookmarks at the correct place starting after the "Downloads"
            // bookmark. When the user already has some of the bookmarks set up manually, the createSystemBookmark()
            // function returns an empty KBookmark so the following entries will be added at the end of the bookmark
            // section to not mess with the users setup.
            KBookmark after = findSystemBookmark(QLatin1String("Downloads"));

            const QString musicFolder = QStandardPaths::writableLocation(QStandardPaths::MusicLocation);
            if (QDir(musicFolder).exists()) {
                after = createSystemBookmark(I18NC_NOOP("KFile System Bookmarks", "Music"),
                                             QUrl::fromLocalFile(musicFolder), QStringLiteral("folder-music"), after);
            }
            const QString pictureFolder = QStandardPaths::writableLocation(QStandardPaths::PicturesLocation);
            if (QDir(pictureFolder).exists()) {
                after = createSystemBookmark(I18NC_NOOP("KFile System Bookmarks", "Pictures"),
                                             QUrl::fromLocalFile(pictureFolder), QStringLiteral("folder-pictures"), after);
            }
            // Choosing the name "Videos" instead of "Movies", since that is how the folder
            // is called normally on Linux: https://cgit.freedesktop.org/xdg/xdg-user-dirs/tree/user-dirs.defaults
            const QString videoFolder = QStandardPaths::writableLocation(QStandardPaths::MoviesLocation);
            if (QDir(videoFolder).exists()) {
                after = createSystemBookmark(I18NC_NOOP("KFile System Bookmarks", "Videos"),
                                             QUrl::fromLocalFile(videoFolder), QStringLiteral("folder-videos"), after);
            }

        }

        if (newFile) {
            setDefaultMetadataItemForGroup(PlacesType);
            setDefaultMetadataItemForGroup(RemoteType);
            setDefaultMetadataItemForGroup(DevicesType);
            setDefaultMetadataItemForGroup(RemovableDevicesType);
            setDefaultMetadataItemForGroup(TagsType);
        }

        // Force bookmarks to be saved. If on open/save dialog and the bookmarks are not saved, QFile::exists
        // will always return false, which opening/closing all the time the open/save dialog would cause the
        // bookmarks to be added once each time, having lots of times each bookmark. (ereslibre)
        d->bookmarkManager->saveAs(file);
    }

    // Add a Recently Used entry if available (it comes from kio-extras)
    if (qEnvironmentVariableIsSet("KDE_FULL_SESSION") && KProtocolInfo::isKnownProtocol(QStringLiteral("recentlyused")) &&
             root.metaDataItem(QStringLiteral("withRecentlyUsed")) != QLatin1String("true")) {

        root.setMetaDataItem(QStringLiteral("withRecentlyUsed"), QStringLiteral("true"));

        KBookmark recentFilesBookmark = KFilePlacesItem::createSystemBookmark(d->bookmarkManager,
                                            I18NC_NOOP("KFile System Bookmarks", "Recent Files"),
                                            QUrl(QStringLiteral("recentlyused:/files")), QStringLiteral("document-open-recent"));

        KBookmark recentDirectoriesBookmark = KFilePlacesItem::createSystemBookmark(d->bookmarkManager,
                                            I18NC_NOOP("KFile System Bookmarks", "Recent Locations"),
                                            QUrl(QStringLiteral("recentlyused:/locations")), QStringLiteral("folder-open-recent"));

        setDefaultMetadataItemForGroup(RecentlySavedType);

        // Move The recently used bookmarks below the trash, making it the first element in the Recent group
        KBookmark trashBookmark = bookmarkForUrl(QUrl(QStringLiteral("trash:/")));
        if (!trashBookmark.isNull()) {
            root.moveBookmark(recentFilesBookmark, trashBookmark);
            root.moveBookmark(recentDirectoriesBookmark, recentFilesBookmark);
        }

        d->bookmarkManager->save();
    }

    // if baloo is enabled, add new urls even if the bookmark file is not empty
    if (d->fileIndexingEnabled &&
        root.metaDataItem(QStringLiteral("withBaloo")) != QLatin1String("true")) {

        root.setMetaDataItem(QStringLiteral("withBaloo"), QStringLiteral("true"));

        // don't add by default "Modified Today" and "Modified Yesterday" when recentlyused:/ is present
        if (root.metaDataItem(QStringLiteral("withRecentlyUsed")) != QLatin1String("true")) {
            KFilePlacesItem::createSystemBookmark(d->bookmarkManager,
                                                  I18NC_NOOP("KFile System Bookmarks", "Modified Today"),
                                                  QUrl(QStringLiteral("timeline:/today")),  QStringLiteral("go-jump-today"));
            KFilePlacesItem::createSystemBookmark(d->bookmarkManager,
                                                  I18NC_NOOP("KFile System Bookmarks", "Modified Yesterday"),
                                                  QUrl(QStringLiteral("timeline:/yesterday")),  QStringLiteral("view-calendar-day"));
        }

        KFilePlacesItem::createSystemBookmark(d->bookmarkManager,
                                              I18NC_NOOP("KFile System Bookmarks", "Documents"),
                                             QUrl(QStringLiteral("search:/documents")),  QStringLiteral("folder-text"));
        KFilePlacesItem::createSystemBookmark(d->bookmarkManager,
                                              I18NC_NOOP("KFile System Bookmarks", "Images"),
                                              QUrl(QStringLiteral("search:/images")),  QStringLiteral("folder-images"));
        KFilePlacesItem::createSystemBookmark(d->bookmarkManager,
                                              I18NC_NOOP("KFile System Bookmarks", "Audio"),
                                              QUrl(QStringLiteral("search:/audio")),  QStringLiteral("folder-sound"));
        KFilePlacesItem::createSystemBookmark(d->bookmarkManager,
                                              I18NC_NOOP("KFile System Bookmarks", "Videos"),
                                              QUrl(QStringLiteral("search:/videos")),  QStringLiteral("folder-videos"));

        setDefaultMetadataItemForGroup(SearchForType);
        setDefaultMetadataItemForGroup(RecentlySavedType);

        d->bookmarkManager->save();
    }

    QString predicate(QString::fromLatin1("[[[[ StorageVolume.ignored == false AND [ StorageVolume.usage == 'FileSystem' OR StorageVolume.usage == 'Encrypted' ]]"
                      " OR "
                      "[ IS StorageAccess AND StorageDrive.driveType == 'Floppy' ]]"
                      " OR "
                      "OpticalDisc.availableContent & 'Audio' ]"
                      " OR "
                      "StorageAccess.ignored == false ]"));

    if (KProtocolInfo::isKnownProtocol(QStringLiteral("mtp"))) {
        predicate = QLatin1Char('[') + predicate + QLatin1String(" OR PortableMediaPlayer.supportedProtocols == 'mtp']");
    }
    if (KProtocolInfo::isKnownProtocol(QStringLiteral("afc"))) {
        predicate = QLatin1Char('[') + predicate + QLatin1String(" OR PortableMediaPlayer.supportedProtocols == 'afc']");
    }

    d->predicate = Solid::Predicate::fromString(predicate);

    Q_ASSERT(d->predicate.isValid());

    connect(d->bookmarkManager, &KBookmarkManager::changed, this, [this]() { d->_k_reloadBookmarks(); });
    connect(d->bookmarkManager, &KBookmarkManager::bookmarksChanged, this, [this]() { d->_k_reloadBookmarks(); });

    d->_k_reloadBookmarks();
    QTimer::singleShot(0, this, [this]() { d->_k_initDeviceList(); });
}

KFilePlacesModel::KFilePlacesModel(QObject *parent)
    : KFilePlacesModel({}, parent)
{
}

KFilePlacesModel::~KFilePlacesModel()
{
    delete d;
}

QUrl KFilePlacesModel::url(const QModelIndex &index) const
{
    return data(index, UrlRole).toUrl();
}

bool KFilePlacesModel::setupNeeded(const QModelIndex &index) const
{
    return data(index, SetupNeededRole).toBool();
}

QIcon KFilePlacesModel::icon(const QModelIndex &index) const
{
    return data(index, Qt::DecorationRole).value<QIcon>();
}

QString KFilePlacesModel::text(const QModelIndex &index) const
{
    return data(index, Qt::DisplayRole).toString();
}

bool KFilePlacesModel::isHidden(const QModelIndex &index) const
{
    //Note: we do not want to show an index if its parent is hidden
    return data(index, HiddenRole).toBool() || isGroupHidden(index);
}

bool KFilePlacesModel::isGroupHidden(const GroupType type) const
{
    const QString hidden = d->bookmarkManager->root().metaDataItem(stateNameForGroupType(type));
    return hidden == QLatin1String("true");
}

bool KFilePlacesModel::isGroupHidden(const QModelIndex &index) const
{
    if (!index.isValid()) {
        return false;
    }

    KFilePlacesItem *item = static_cast<KFilePlacesItem *>(index.internalPointer());
    return isGroupHidden(item->groupType());
}

bool KFilePlacesModel::isDevice(const QModelIndex &index) const
{
    if (!index.isValid()) {
        return false;
    }

    KFilePlacesItem *item = static_cast<KFilePlacesItem *>(index.internalPointer());

    return item->isDevice();
}

Solid::Device KFilePlacesModel::deviceForIndex(const QModelIndex &index) const
{
    if (!index.isValid()) {
        return Solid::Device();
    }

    KFilePlacesItem *item = static_cast<KFilePlacesItem *>(index.internalPointer());

    if (item->isDevice()) {
        return item->device();
    } else {
        return Solid::Device();
    }
}

KBookmark KFilePlacesModel::bookmarkForIndex(const QModelIndex &index) const
{
    if (!index.isValid()) {
        return KBookmark();
    }

    KFilePlacesItem *item = static_cast<KFilePlacesItem *>(index.internalPointer());
    return item->bookmark();
}

KFilePlacesModel::GroupType KFilePlacesModel::groupType(const QModelIndex &index) const
{
    if (!index.isValid()) {
        return UnknownType;
    }

    KFilePlacesItem *item = static_cast<KFilePlacesItem *>(index.internalPointer());
    return item->groupType();
}

QModelIndexList KFilePlacesModel::groupIndexes(const KFilePlacesModel::GroupType type) const
{
    if (type == UnknownType) {
        return QModelIndexList();
    }

    QModelIndexList indexes;
    const int rows = rowCount();
    for (int row = 0; row < rows ; ++row) {
        const QModelIndex current = index(row, 0);
        if (groupType(current) == type) {
            indexes << current;
        }
    }

    return indexes;
}

QVariant KFilePlacesModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid()) {
        return QVariant();
    }

    KFilePlacesItem *item = static_cast<KFilePlacesItem *>(index.internalPointer());
    if (role == KFilePlacesModel::GroupHiddenRole) {
        return isGroupHidden(item->groupType());
    } else {
        return item->data(role);
    }
}

QModelIndex KFilePlacesModel::index(int row, int column, const QModelIndex &parent) const
{
    if (row < 0 || column != 0 || row >= d->items.size()) {
        return QModelIndex();
    }

    if (parent.isValid()) {
        return QModelIndex();
    }

    return createIndex(row, column, d->items.at(row));
}

QModelIndex KFilePlacesModel::parent(const QModelIndex &child) const
{
    Q_UNUSED(child);
    return QModelIndex();
}

int KFilePlacesModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid()) {
        return 0;
    } else {
        return d->items.size();
    }
}

int KFilePlacesModel::columnCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent)
    // We only know 1 piece of information for a particular entry
    return 1;
}

QModelIndex KFilePlacesModel::closestItem(const QUrl &url) const
{
    int foundRow = -1;
    int maxLength = 0;

    // Search the item which is equal to the URL or at least is a parent URL.
    // If there are more than one possible item URL candidates, choose the item
    // which covers the bigger range of the URL.
    for (int row = 0; row < d->items.size(); ++row) {
        KFilePlacesItem *item = d->items[row];

        if (item->isHidden()) {
            continue;
        }

        const QUrl itemUrl(item->data(UrlRole).toUrl());

        if (itemUrl.matches(url, QUrl::StripTrailingSlash) || itemUrl.isParentOf(url)) {
            const int length = itemUrl.toString().length();
            if (length > maxLength) {
                foundRow = row;
                maxLength = length;
            }
        }
    }

    if (foundRow == -1) {
        return QModelIndex();
    } else {
        return createIndex(foundRow, 0, d->items[foundRow]);
    }
}

void KFilePlacesModel::Private::_k_initDeviceList()
{
    Solid::DeviceNotifier *notifier = Solid::DeviceNotifier::instance();

    connect(notifier, &Solid::DeviceNotifier::deviceAdded, q, [this](const QString &device) { _k_deviceAdded(device); });
    connect(notifier, &Solid::DeviceNotifier::deviceRemoved, q, [this](const QString &device) { _k_deviceRemoved(device); });

    const QList<Solid::Device> &deviceList = Solid::Device::listFromQuery(predicate);

    availableDevices.reserve(deviceList.size());
    for (const Solid::Device &device : deviceList) {
        availableDevices << device.udi();
    }

    _k_reloadBookmarks();
}

void KFilePlacesModel::Private::_k_deviceAdded(const QString &udi)
{
    Solid::Device d(udi);

    if (predicate.matches(d)) {
        availableDevices << udi;
        _k_reloadBookmarks();
    }
}

void KFilePlacesModel::Private::_k_deviceRemoved(const QString &udi)
{
    auto it = std::find(availableDevices.begin(), availableDevices.end(), udi);
    if (it != availableDevices.end()) {
        availableDevices.erase(it);
        _k_reloadBookmarks();
    }
}

void KFilePlacesModel::Private::_k_itemChanged(const QString &id)
{
    for (int row = 0; row < items.size(); ++row) {
        if (items.at(row)->id() == id) {
            QModelIndex index = q->index(row, 0);
            Q_EMIT q->dataChanged(index, index);
        }
    }
}

void KFilePlacesModel::Private::_k_reloadBookmarks()
{
    QList<KFilePlacesItem *> currentItems = loadBookmarkList();

    QList<KFilePlacesItem *>::Iterator it_i = items.begin();
    QList<KFilePlacesItem *>::Iterator it_c = currentItems.begin();

    QList<KFilePlacesItem *>::Iterator end_i = items.end();
    QList<KFilePlacesItem *>::Iterator end_c = currentItems.end();

    while (it_i != end_i || it_c != end_c) {
        if (it_i == end_i && it_c != end_c) {
            int row = items.count();

            q->beginInsertRows(QModelIndex(), row, row);
            it_i = items.insert(it_i, *it_c);
            ++it_i;
            it_c = currentItems.erase(it_c);

            end_i = items.end();
            end_c = currentItems.end();
            q->endInsertRows();

        } else if (it_i != end_i && it_c == end_c) {
            int row = items.indexOf(*it_i);

            q->beginRemoveRows(QModelIndex(), row, row);
            delete *it_i;
            it_i = items.erase(it_i);

            end_i = items.end();
            end_c = currentItems.end();
            q->endRemoveRows();

        } else if ((*it_i)->id() == (*it_c)->id()) {
            bool shouldEmit = !((*it_i)->bookmark() == (*it_c)->bookmark());
            (*it_i)->setBookmark((*it_c)->bookmark());
            if (shouldEmit) {
                int row = items.indexOf(*it_i);
                QModelIndex idx = q->index(row, 0);
                Q_EMIT q->dataChanged(idx, idx);
            }
            ++it_i;
            ++it_c;
        } else {
            int row = items.indexOf(*it_i);

            if (it_i + 1 != end_i && (*(it_i + 1))->id() == (*it_c)->id()) { // if the next one matches, it's a remove
                q->beginRemoveRows(QModelIndex(), row, row);
                delete *it_i;
                it_i = items.erase(it_i);

                end_i = items.end();
                end_c = currentItems.end();
                q->endRemoveRows();
            } else {
                q->beginInsertRows(QModelIndex(), row, row);
                it_i = items.insert(it_i, *it_c);
                ++it_i;
                it_c = currentItems.erase(it_c);

                end_i = items.end();
                end_c = currentItems.end();
                q->endInsertRows();
            }
        }
    }

    qDeleteAll(currentItems);
    currentItems.clear();

    Q_EMIT q->reloaded();
}

bool KFilePlacesModel::Private::isBalooUrl(const QUrl &url) const
{
    const QString scheme = url.scheme();
    return ((scheme == QLatin1String("timeline")) ||
            (scheme == QLatin1String("search")));
}

QList<KFilePlacesItem *> KFilePlacesModel::Private::loadBookmarkList()
{
    QList<KFilePlacesItem *> items;

    KBookmarkGroup root = bookmarkManager->root();
    KBookmark bookmark = root.first();
    QVector<QString> devices = availableDevices;
    QVector<QString> tagsList = tags;

    while (!bookmark.isNull()) {
        const QString udi = bookmark.metaDataItem(QStringLiteral("UDI"));
        const QUrl url = bookmark.url();
        const QString tag = bookmark.metaDataItem(QStringLiteral("tag"));
        if (!udi.isEmpty() || url.isValid()) {
            QString appName = bookmark.metaDataItem(QStringLiteral("OnlyInApp"));

            // If it's not a tag it's a device
            if (tag.isEmpty()) {
                auto it = std::find(devices.begin(), devices.end(), udi);
                bool deviceAvailable = (it != devices.end());
                if (deviceAvailable) {
                    devices.erase(it);
                }

                bool allowedHere = appName.isEmpty() ||
                        ((appName == QCoreApplication::instance()->applicationName()) ||
                        (appName == alternativeApplicationName));
                bool isSupportedUrl = isBalooUrl(url) ? fileIndexingEnabled : true;
                bool isSupportedScheme = supportedSchemes.isEmpty() || supportedSchemes.contains(url.scheme());

                KFilePlacesItem *item = nullptr;
                if (deviceAvailable) {
                    item = new KFilePlacesItem(bookmarkManager, bookmark.address(), udi);
                    if (!item->hasSupportedScheme(supportedSchemes)) {
                        delete item;
                        item = nullptr;
                    }
                } else if (isSupportedScheme && isSupportedUrl && udi.isEmpty() && allowedHere) {
                    // TODO: Update bookmark internal element
                    item = new KFilePlacesItem(bookmarkManager, bookmark.address());
                }

                if (item) {
                    connect(item, &KFilePlacesItem::itemChanged,
                            q, [this](const QString &id) { _k_itemChanged(id); });

                    items << item;
                }
            } else {
                auto it = std::find(tagsList.begin(), tagsList.end(), tag);
                if (it != tagsList.end()) {
                    tagsList.removeAll(tag);
                    KFilePlacesItem *item = new KFilePlacesItem(bookmarkManager, bookmark.address());
                    items << item;
                    connect(item, &KFilePlacesItem::itemChanged,
                            q, [this](const QString &id) { _k_itemChanged(id); });
                }
            }
        }

        bookmark = root.next(bookmark);
    }

    // Add bookmarks for the remaining devices, they were previously unknown
    for (const QString &udi : qAsConst(devices)) {
        bookmark = KFilePlacesItem::createDeviceBookmark(bookmarkManager, udi);
        if (!bookmark.isNull()) {
            KFilePlacesItem *item = new KFilePlacesItem(bookmarkManager,
                    bookmark.address(), udi);
            connect(item, &KFilePlacesItem::itemChanged,
                    q, [this](const QString &id) { _k_itemChanged(id); });
            // TODO: Update bookmark internal element
            items << item;
        }
    }

    for (const QString& tag: tagsList) {
        bookmark = KFilePlacesItem::createTagBookmark(bookmarkManager, tag);
        if (!bookmark.isNull()) {
            KFilePlacesItem *item = new KFilePlacesItem(bookmarkManager,
                    bookmark.address(), tag);
            connect(item, &KFilePlacesItem::itemChanged,
                    q, [this](const QString &id) { _k_itemChanged(id); });
            items << item;
        }
    }

    // return a sorted list based on groups
    std::stable_sort(items.begin(), items.end(),
                [](KFilePlacesItem *itemA, KFilePlacesItem *itemB) {
       return (itemA->groupType() < itemB->groupType());
    });

    return items;
}

int KFilePlacesModel::Private::findNearestPosition(int source, int target)
{
    const KFilePlacesItem *item = items.at(source);
    const KFilePlacesModel::GroupType groupType = item->groupType();
    int newTarget = qMin(target, items.count() - 1);

    // moving inside the same group is ok
    if ((items.at(newTarget)->groupType() == groupType)) {
        return target;
    }

    if (target > source) { // moving down, move it to the end of the group
        int groupFooter = source;
        while (items.at(groupFooter)->groupType() == groupType) {
            groupFooter++;
            // end of the list move it there
            if (groupFooter == items.count()) {
                break;
            }
        }
        target = groupFooter;
    } else { // moving up, move it to beginning of the group
        int groupHead = source;
        while (items.at(groupHead)->groupType() == groupType) {
            groupHead--;
            // beginning of the list move it there
            if (groupHead == 0) {
                break;
            }
        }
        target = groupHead;
    }
    return target;
}

void KFilePlacesModel::Private::reloadAndSignal()
{
    bookmarkManager->emitChanged(bookmarkManager->root()); // ... we'll get relisted anyway
}

Qt::DropActions KFilePlacesModel::supportedDropActions() const
{
    return Qt::ActionMask;
}

Qt::ItemFlags KFilePlacesModel::flags(const QModelIndex &index) const
{
    Qt::ItemFlags res;

    if (index.isValid()) {
        res |= Qt::ItemIsDragEnabled | Qt::ItemIsSelectable | Qt::ItemIsEnabled;
    }

    if (!index.isValid()) {
        res |= Qt::ItemIsDropEnabled;
    }

    return res;
}

static QString _k_internalMimetype(const KFilePlacesModel *const self)
{
    return QStringLiteral("application/x-kfileplacesmodel-") + QString::number(reinterpret_cast<qptrdiff>(self));
}

QStringList KFilePlacesModel::mimeTypes() const
{
    QStringList types;

    types << _k_internalMimetype(this) << QStringLiteral("text/uri-list");

    return types;
}

QMimeData *KFilePlacesModel::mimeData(const QModelIndexList &indexes) const
{
    QList<QUrl> urls;
    QByteArray itemData;

    QDataStream stream(&itemData, QIODevice::WriteOnly);

    for (const QModelIndex &index : qAsConst(indexes)) {
        QUrl itemUrl = url(index);
        if (itemUrl.isValid()) {
            urls << itemUrl;
        }
        stream << index.row();
    }

    QMimeData *mimeData = new QMimeData();

    if (!urls.isEmpty()) {
        mimeData->setUrls(urls);
    }

    mimeData->setData(_k_internalMimetype(this), itemData);

    return mimeData;
}

bool KFilePlacesModel::dropMimeData(const QMimeData *data, Qt::DropAction action,
                                    int row, int column, const QModelIndex &parent)
{
    if (action == Qt::IgnoreAction) {
        return true;
    }

    if (column > 0) {
        return false;
    }

    if (row == -1 && parent.isValid()) {
        return false; // Don't allow to move an item onto another one,
        // too easy for the user to mess something up
        // If we really really want to allow copying files this way,
        // let's do it in the views to get the good old drop menu
    }

    if (data->hasFormat(_k_internalMimetype(this))) {
        // The operation is an internal move
        QByteArray itemData = data->data(_k_internalMimetype(this));
        QDataStream stream(&itemData, QIODevice::ReadOnly);
        int itemRow;

        stream >> itemRow;

        if (!movePlace(itemRow, row)) {
            return false;
        }

    } else if (data->hasFormat(QStringLiteral("text/uri-list"))) {
        // The operation is an add

        QMimeDatabase db;
        KBookmark afterBookmark;

        if (row == -1) {
            // The dropped item is moved or added to the last position

            KFilePlacesItem *lastItem = d->items.last();
            afterBookmark = lastItem->bookmark();

        } else {
            // The dropped item is moved or added before position 'row', ie after position 'row-1'

            if (row > 0) {
                KFilePlacesItem *afterItem = d->items[row - 1];
                afterBookmark = afterItem->bookmark();
            }
        }

        const QList<QUrl> urls = KUrlMimeData::urlsFromMimeData(data);

        KBookmarkGroup group = d->bookmarkManager->root();

        for (const QUrl &url : urls) {
            // TODO: use KIO::stat in order to get the UDS_DISPLAY_NAME too
            KIO::MimetypeJob *job = KIO::mimetype(url);

            QString mimeString;
            if (!job->exec()) {
                mimeString = QStringLiteral("unknown");
            } else {
                mimeString = job->mimetype();
            }

            QMimeType mimetype = db.mimeTypeForName(mimeString);

            if (!mimetype.isValid()) {
                qWarning() << "URL not added to Places as MIME type could not be determined!";
                continue;
            }

            if (!mimetype.inherits(QStringLiteral("inode/directory"))) {
                // Only directories are allowed
                continue;
            }

            KFileItem item(url, mimetype.name(), S_IFDIR);

            KBookmark bookmark = KFilePlacesItem::createBookmark(d->bookmarkManager,
                                 url.fileName(), url,
                                 item.iconName());
            group.moveBookmark(bookmark, afterBookmark);
            afterBookmark = bookmark;
        }

    } else {
        // Oops, shouldn't happen thanks to mimeTypes()
        qWarning() << ": received wrong mimedata, " << data->formats();
        return false;
    }

    refresh();

    return true;
}

void KFilePlacesModel::refresh() const
{
    d->reloadAndSignal();
}

QUrl KFilePlacesModel::convertedUrl(const QUrl &url)
{
    QUrl newUrl = url;
    if (url.scheme() == QLatin1String("timeline")) {
        newUrl = createTimelineUrl(url);
    } else if (url.scheme() == QLatin1String("search")) {
        newUrl = createSearchUrl(url);
    }

    return newUrl;
}

void KFilePlacesModel::addPlace(const QString &text, const QUrl &url,
                                const QString &iconName, const QString &appName)
{
    addPlace(text, url, iconName, appName, QModelIndex());
}

void KFilePlacesModel::addPlace(const QString &text, const QUrl &url,
                                const QString &iconName, const QString &appName,
                                const QModelIndex &after)
{
    KBookmark bookmark = KFilePlacesItem::createBookmark(d->bookmarkManager,
                         text, url, iconName);

    if (!appName.isEmpty()) {
        bookmark.setMetaDataItem(QStringLiteral("OnlyInApp"), appName);
    }

    if (after.isValid()) {
        KFilePlacesItem *item = static_cast<KFilePlacesItem *>(after.internalPointer());
        d->bookmarkManager->root().moveBookmark(bookmark, item->bookmark());
    }

    refresh();
}

void KFilePlacesModel::editPlace(const QModelIndex &index, const QString &text, const QUrl &url,
                                 const QString &iconName, const QString &appName)
{
    if (!index.isValid()) {
        return;
    }

    KFilePlacesItem *item = static_cast<KFilePlacesItem *>(index.internalPointer());

    if (item->isDevice()) {
        return;
    }

    KBookmark bookmark = item->bookmark();

    if (bookmark.isNull()) {
        return;
    }

    bool changed = false;
    if (text != bookmark.fullText()) {
        bookmark.setFullText(text);
        changed = true;
    }

    if (url != bookmark.url()) {
        bookmark.setUrl(url);
        changed = true;
    }

    if (iconName != bookmark.icon()) {
        bookmark.setIcon(iconName);
        changed = true;
    }

    const QString onlyInApp = bookmark.metaDataItem(QStringLiteral("OnlyInApp"));
    if (appName != onlyInApp) {
        bookmark.setMetaDataItem(QStringLiteral("OnlyInApp"), appName);
        changed = true;
    }

    if (changed) {
        refresh();
        Q_EMIT dataChanged(index, index);
    }
}

void KFilePlacesModel::removePlace(const QModelIndex &index) const
{
    if (!index.isValid()) {
        return;
    }

    KFilePlacesItem *item = static_cast<KFilePlacesItem *>(index.internalPointer());

    if (item->isDevice()) {
        return;
    }

    KBookmark bookmark = item->bookmark();

    if (bookmark.isNull()) {
        return;
    }

    d->bookmarkManager->root().deleteBookmark(bookmark);
    refresh();
}

void KFilePlacesModel::setPlaceHidden(const QModelIndex &index, bool hidden)
{
    if (!index.isValid()) {
        return;
    }

    KFilePlacesItem *item = static_cast<KFilePlacesItem *>(index.internalPointer());

    if (item->bookmark().isNull() || item->isHidden() == hidden) {
        return;
    }

    const bool groupHidden = isGroupHidden(item->groupType());
    const bool hidingChildOnShownParent = hidden && !groupHidden;
    const bool showingChildOnShownParent = !hidden && !groupHidden;

    if (hidingChildOnShownParent || showingChildOnShownParent) {
        item->setHidden(hidden);

        d->reloadAndSignal();
        Q_EMIT dataChanged(index, index);
    }
}

void KFilePlacesModel::setGroupHidden(const GroupType type, bool hidden)
{
    if (isGroupHidden(type) == hidden)
        return;

    d->bookmarkManager->root().setMetaDataItem(stateNameForGroupType(type), (hidden ? QStringLiteral("true") : QStringLiteral("false")));
    d->reloadAndSignal();
    Q_EMIT groupHiddenChanged(type, hidden);
}

bool KFilePlacesModel::movePlace(int itemRow, int row)
{
    KBookmark afterBookmark;

    if ((itemRow < 0) || (itemRow >= d->items.count())) {
        return false;
    }

    if (row >= d->items.count()) {
        row = -1;
    }

    if (row == -1) {
        // The dropped item is moved or added to the last position

        KFilePlacesItem *lastItem = d->items.last();
        afterBookmark = lastItem->bookmark();

    } else {
        // The dropped item is moved or added before position 'row', ie after position 'row-1'

        if (row > 0) {
            KFilePlacesItem *afterItem = d->items[row - 1];
            afterBookmark = afterItem->bookmark();
        }
    }

    KFilePlacesItem *item = d->items[itemRow];
    KBookmark bookmark = item->bookmark();

    int destRow = row == -1 ? d->items.count() : row;

    // avoid move item away from its group
    destRow = d->findNearestPosition(itemRow, destRow);

    // The item is not moved when the drop indicator is on either item edge
    if (itemRow == destRow || itemRow + 1 == destRow) {
        return false;
    }

    beginMoveRows(QModelIndex(), itemRow, itemRow, QModelIndex(), destRow);
    d->bookmarkManager->root().moveBookmark(bookmark, afterBookmark);
    // Move item ourselves so that _k_reloadBookmarks() does not consider
    // the move as a remove + insert.
    //
    // 2nd argument of QList::move() expects the final destination index,
    // but 'row' is the value of the destination index before the moved
    // item has been removed from its original position. That is why we
    // adjust if necessary.
    d->items.move(itemRow, itemRow < destRow ? (destRow - 1) : destRow);
    endMoveRows();

    return true;
}

int KFilePlacesModel::hiddenCount() const
{
    int rows = rowCount();
    int hidden = 0;

    for (int i = 0; i < rows; ++i) {
        if (isHidden(index(i, 0))) {
            hidden++;
        }
    }

    return hidden;
}

QAction *KFilePlacesModel::teardownActionForIndex(const QModelIndex &index) const
{
    Solid::Device device = deviceForIndex(index);

    if (device.is<Solid::StorageAccess>() && device.as<Solid::StorageAccess>()->isAccessible()) {

        Solid::StorageDrive *drive = device.as<Solid::StorageDrive>();

        if (drive == nullptr) {
            drive = device.parent().as<Solid::StorageDrive>();
        }

        bool hotpluggable = false;
        bool removable = false;

        if (drive != nullptr) {
            hotpluggable = drive->isHotpluggable();
            removable = drive->isRemovable();
        }

        QString iconName;
        QString text;
        QString label = data(index, Qt::DisplayRole).toString().replace(QLatin1Char('&'), QLatin1String("&&"));

        if (device.is<Solid::OpticalDisc>()) {
            text = i18n("&Release '%1'", label);
        } else if (removable || hotpluggable) {
            text = i18n("&Safely Remove '%1'", label);
            iconName = QStringLiteral("media-eject");
        } else {
            text = i18n("&Unmount '%1'", label);
            iconName = QStringLiteral("media-eject");
        }

        if (!iconName.isEmpty()) {
            return new QAction(QIcon::fromTheme(iconName), text, nullptr);
        } else {
            return new QAction(text, nullptr);
        }
    }

    return nullptr;
}

QAction *KFilePlacesModel::ejectActionForIndex(const QModelIndex &index) const
{
    Solid::Device device = deviceForIndex(index);

    if (device.is<Solid::OpticalDisc>()) {

        QString label = data(index, Qt::DisplayRole).toString().replace(QLatin1Char('&'), QLatin1String("&&"));
        QString text = i18n("&Eject '%1'", label);

        return new QAction(QIcon::fromTheme(QStringLiteral("media-eject")), text, nullptr);
    }

    return nullptr;
}

void KFilePlacesModel::requestTeardown(const QModelIndex &index)
{
    Solid::Device device = deviceForIndex(index);
    Solid::StorageAccess *access = device.as<Solid::StorageAccess>();

    if (access != nullptr) {
        connect(access, &Solid::StorageAccess::teardownDone,
                this, [this](Solid::ErrorType error, QVariant errorData) {
            d->_k_storageTeardownDone(error, errorData);
        });

        access->teardown();
    }
}

void KFilePlacesModel::requestEject(const QModelIndex &index)
{
    Solid::Device device = deviceForIndex(index);

    Solid::OpticalDrive *drive = device.parent().as<Solid::OpticalDrive>();

    if (drive != nullptr) {
        connect(drive, &Solid::OpticalDrive::ejectDone,
                this, [this](Solid::ErrorType error, QVariant errorData) {
            d->_k_storageTeardownDone(error, errorData);
        });

        drive->eject();
    } else {
        QString label = data(index, Qt::DisplayRole).toString().replace(QLatin1Char('&'), QLatin1String("&&"));
        QString message = i18n("The device '%1' is not a disk and cannot be ejected.", label);
        Q_EMIT errorMessage(message);
    }
}

void KFilePlacesModel::requestSetup(const QModelIndex &index)
{
    Solid::Device device = deviceForIndex(index);

    if (device.is<Solid::StorageAccess>()
            && !d->setupInProgress.contains(device.as<Solid::StorageAccess>())
            && !device.as<Solid::StorageAccess>()->isAccessible()) {

        Solid::StorageAccess *access = device.as<Solid::StorageAccess>();

        d->setupInProgress[access] = index;

        connect(access, &Solid::StorageAccess::setupDone,
                this, [this](Solid::ErrorType error, QVariant errorData) {
            d->_k_storageSetupDone(error, errorData);
        });

        access->setup();
    }
}

void KFilePlacesModel::Private::_k_storageSetupDone(Solid::ErrorType error, const QVariant &errorData)
{
    QPersistentModelIndex index = setupInProgress.take(q->sender());

    if (!index.isValid()) {
        return;
    }

    if (!error) {
        Q_EMIT q->setupDone(index, true);
    } else {
        if (errorData.isValid()) {
            Q_EMIT q->errorMessage(i18n("An error occurred while accessing '%1', the system responded: %2",
                                      q->text(index),
                                      errorData.toString()));
        } else {
            Q_EMIT q->errorMessage(i18n("An error occurred while accessing '%1'",
                                      q->text(index)));
        }
        Q_EMIT q->setupDone(index, false);
    }

}

void KFilePlacesModel::Private::_k_storageTeardownDone(Solid::ErrorType error, const QVariant &errorData)
{
    if (error && errorData.isValid()) {
        Q_EMIT q->errorMessage(errorData.toString());
    }
}

void KFilePlacesModel::setSupportedSchemes(const QStringList &schemes)
{
    d->supportedSchemes = schemes;
    d->_k_reloadBookmarks();
}

QStringList KFilePlacesModel::supportedSchemes() const
{
    return d->supportedSchemes;
}

#include "moc_kfileplacesmodel.cpp"
