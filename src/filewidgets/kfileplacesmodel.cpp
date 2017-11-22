/*  This file is part of the KDE project
    Copyright (C) 2007 Kevin Ottens <ervin@kde.org>
    Copyright (C) 2007 David Faure <faure@kde.org>

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License version 2 as published by the Free Software Foundation.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.

*/
#include "kfileplacesmodel.h"
#include "kfileplacesitem_p.h"

#ifdef _WIN32_WCE
#include "Windows.h"
#include "WinBase.h"
#endif

#include <QCoreApplication>
#include <QtCore/QDir>
#include <QtCore/QMimeData>
#include <QtCore/QTimer>
#include <QtCore/QFile>
#include <QColor>
#include <QAction>
#include <QUrlQuery>
#include <qmimedatabase.h>

#include <kfileitem.h>
#include <klocalizedstring.h>

#include <QDebug>
#include <kurlmimedata.h>

#include <kbookmarkmanager.h>
#include <kbookmark.h>

#include <kio/job.h>
#include <kprotocolinfo.h>

#include <kconfig.h>
#include <kconfiggroup.h>

#include <solid/devicenotifier.h>
#include <solid/storageaccess.h>
#include <solid/storagedrive.h>
#include <solid/storagevolume.h>
#include <solid/opticaldrive.h>
#include <solid/opticaldisc.h>
#include <solid/portablemediaplayer.h>
#include <solid/predicate.h>
#include <qstandardpaths.h>

namespace {
    static bool isFileIndexingEnabled()
    {
        KConfig config(QStringLiteral("baloofilerc"));
        KConfigGroup basicSettings = config.group("Basic Settings");
        return basicSettings.readEntry("Indexing-Enabled", true);
    }

    static QString timelineDateString(int year, int month, int day = 0)
    {
        const QString dateFormat("%1-%2");

        QString date = dateFormat.arg(year).arg(month, 2, 10, QChar('0'));
        if (day > 0) {
            date += QString("-%1").arg(day, 2, 10, QChar('0'));
        }
        return date;
    }

    static QUrl createTimelineUrl(const QUrl &url)
    {
        // based on dolphin urls
        const QString timelinePrefix = QStringLiteral("timeline:") + QLatin1Char('/');
        QUrl timelineUrl;

        const QString path = url.toDisplayString(QUrl::PreferLocalFile);
        if (path.endsWith(QLatin1String("/yesterday"))) {
            const QDate date = QDate::currentDate().addDays(-1);
            const int year = date.year();
            const int month = date.month();
            const int day = date.day();

            timelineUrl = QUrl(timelinePrefix + timelineDateString(year, month) +
                  '/' + timelineDateString(year, month, day));
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

    static QUrl searchUrlForType(const QString &type)
    {
        const QString jsonQuery(QStringLiteral("{\"dayFilter\": 0,\
                                                 \"monthFilter\": 0, \
                                                 \"yearFilter\": 0, \
                                                 \"type\": [ \"%1\"]}"));
        QUrl url;
        url.setScheme(QStringLiteral("baloosearch"));

        QUrlQuery urlQuery;
        urlQuery.addQueryItem(QStringLiteral("json"), jsonQuery.arg(type).simplified());
        url.setQuery(urlQuery);

        return url;
    }

    static QUrl createSearchUrl(const QUrl &url)
    {
        QUrl searchUrl;

        const QString path = url.toDisplayString(QUrl::PreferLocalFile);

        if (path.endsWith(QLatin1String("/documents"))) {
            searchUrl = searchUrlForType(QStringLiteral("Document"));
        } else if (path.endsWith(QLatin1String("/images"))) {
            searchUrl = searchUrlForType(QStringLiteral("Image"));
        } else if (path.endsWith(QLatin1String("/audio"))) {
            searchUrl = searchUrlForType(QStringLiteral("Audio"));
        } else if (path.endsWith(QLatin1String("/videos"))) {
            searchUrl = searchUrlForType(QStringLiteral("Video"));
        } else {
            qWarning() << "Invalid search url:" << url;
            searchUrl = url;
        }

        return searchUrl;
    }
}

class Q_DECL_HIDDEN KFilePlacesModel::Private
{
public:
    Private(KFilePlacesModel *self)
        : q(self),
          bookmarkManager(nullptr),
          fileIndexingEnabled(isFileIndexingEnabled())
    {
    }

    ~Private()
    {
        qDeleteAll(items);
    }

    KFilePlacesModel *q;

    QList<KFilePlacesItem *> items;
    QVector<QString> availableDevices;
    QMap<QObject *, QPersistentModelIndex> setupInProgress;

    Solid::Predicate predicate;
    KBookmarkManager *bookmarkManager;

    const bool fileIndexingEnabled;

    void reloadAndSignal();
    QList<KFilePlacesItem *> loadBookmarkList();
    int findNearestPosition(int source, int target);

    void _k_initDeviceList();
    void _k_deviceAdded(const QString &udi);
    void _k_deviceRemoved(const QString &udi);
    void _k_itemChanged(const QString &udi);
    void _k_reloadBookmarks();
    void _k_storageSetupDone(Solid::ErrorType error, QVariant errorData);
    void _k_storageTeardownDone(Solid::ErrorType error, QVariant errorData);

private:
    bool isBalooUrl(const QUrl &url) const;
};

KFilePlacesModel::KFilePlacesModel(QObject *parent)
    : QAbstractItemModel(parent), d(new Private(this))
{
    const QString file = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) + "/user-places.xbel";
    d->bookmarkManager = KBookmarkManager::managerForExternalFile(file);

    // Let's put some places in there if it's empty.
    KBookmarkGroup root = d->bookmarkManager->root();
    if (root.first().isNull() || !QFile::exists(file)) {

        // NOTE: The context for these I18N_NOOP2 calls has to be "KFile System Bookmarks".
        // The real i18nc call is made later, with this context, so the two must match.
        //
        // createSystemBookmark actually does nothing with its third argument,
        // but we have to give it something so the I18N_NOOP2 calls stay here for now.
        //
        // (coles, 13th May 2009)

        KFilePlacesItem::createSystemBookmark(d->bookmarkManager,
                                              QStringLiteral("Home"), I18N_NOOP2("KFile System Bookmarks", "Home"),
                                              QUrl::fromLocalFile(QDir::homePath()), QStringLiteral("user-home"));
        KFilePlacesItem::createSystemBookmark(d->bookmarkManager,
                                              QStringLiteral("Network"), I18N_NOOP2("KFile System Bookmarks", "Network"),
                                              QUrl(QStringLiteral("remote:/")), QStringLiteral("network-workgroup"));
#if defined(_WIN32_WCE)
        // adding drives
        foreach (const QFileInfo &info, QDir::drives()) {
            QString driveIcon = "drive-harddisk";
            KFilePlacesItem::createSystemBookmark(d->bookmarkManager,
                                                  info.absoluteFilePath(), info.absoluteFilePath(),
                                                  QUrl::fromLocalFile(info.absoluteFilePath()), driveIcon);
        }
#elif !defined(Q_OS_WIN)
        KFilePlacesItem::createSystemBookmark(d->bookmarkManager,
                                              QStringLiteral("Root"), I18N_NOOP2("KFile System Bookmarks", "Root"),
                                              QUrl::fromLocalFile(QStringLiteral("/")), QStringLiteral("folder-red"));
#endif
        KFilePlacesItem::createSystemBookmark(d->bookmarkManager,
                                              QStringLiteral("Trash"), I18N_NOOP2("KFile System Bookmarks", "Trash"),
                                              QUrl(QStringLiteral("trash:/")), QStringLiteral("user-trash"));

        // Force bookmarks to be saved. If on open/save dialog and the bookmarks are not saved, QFile::exists
        // will always return false, which opening/closing all the time the open/save dialog would case the
        // bookmarks to be added once each time, having lots of times each bookmark. (ereslibre)
        d->bookmarkManager->saveAs(file);
    }

    // if baloo is enabled, add new urls even if the bookmark file is not empty
    if (d->fileIndexingEnabled &&
        root.metaDataItem(QStringLiteral("withBaloo")) != QLatin1String("true")) {

        root.setMetaDataItem(QStringLiteral("withBaloo"), QStringLiteral("true"));
        KFilePlacesItem::createSystemBookmark(d->bookmarkManager,
                                              QStringLiteral("Today"), I18N_NOOP2("KFile System Bookmarks", "Today"),
                                              QUrl(QStringLiteral("timeline:/today")),  QStringLiteral("go-jump-today"));
        KFilePlacesItem::createSystemBookmark(d->bookmarkManager,
                                              QStringLiteral("Yesterday"), I18N_NOOP2("KFile System Bookmarks", "Yesterday"),
                                              QUrl(QStringLiteral("timeline:/yesterday")),  QStringLiteral("view-calendar-day"));
        KFilePlacesItem::createSystemBookmark(d->bookmarkManager,
                                              QStringLiteral("This Month"), I18N_NOOP2("KFile System Bookmarks", "This Month"),
                                              QUrl(QStringLiteral("timeline:/thismonth")),  QStringLiteral("view-calendar-month"));
        KFilePlacesItem::createSystemBookmark(d->bookmarkManager,
                                              QStringLiteral("Last Month"), I18N_NOOP2("KFile System Bookmarks", "Last Month"),
                                              QUrl(QStringLiteral("timeline:/lastmonth")),  QStringLiteral("view-calendar-month"));
        KFilePlacesItem::createSystemBookmark(d->bookmarkManager,
                                              QStringLiteral("Documents"), I18N_NOOP2("KFile System Bookmarks", "Documents"),
                                             QUrl(QStringLiteral("search:/documents")),  QStringLiteral("folder-text"));
        KFilePlacesItem::createSystemBookmark(d->bookmarkManager,
                                              QStringLiteral("Images"), I18N_NOOP2("KFile System Bookmarks", "Images"),
                                              QUrl(QStringLiteral("search:/images")),  QStringLiteral("folder-images"));
        KFilePlacesItem::createSystemBookmark(d->bookmarkManager,
                                              QStringLiteral("Audio Files"), I18N_NOOP2("KFile System Bookmarks", "Audio Files"),
                                              QUrl(QStringLiteral("search:/audio")),  QStringLiteral("folder-sound"));
        KFilePlacesItem::createSystemBookmark(d->bookmarkManager,
                                              QStringLiteral("Videos"), I18N_NOOP2("KFile System Bookmarks", "Videos"),
                                              QUrl(QStringLiteral("search:/videos")),  QStringLiteral("folder-videos"));

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
        predicate.prepend("[");
        predicate.append(" OR PortableMediaPlayer.supportedProtocols == 'mtp']");
    }

    d->predicate = Solid::Predicate::fromString(predicate);

    Q_ASSERT(d->predicate.isValid());

    connect(d->bookmarkManager, SIGNAL(changed(QString,QString)),
            this, SLOT(_k_reloadBookmarks()));
    connect(d->bookmarkManager, SIGNAL(bookmarksChanged(QString)),
            this, SLOT(_k_reloadBookmarks()));

    d->_k_reloadBookmarks();
    QTimer::singleShot(0, this, SLOT(_k_initDeviceList()));
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
    return data(index, HiddenRole).toBool();
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

QVariant KFilePlacesModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid()) {
        return QVariant();
    }

    KFilePlacesItem *item = static_cast<KFilePlacesItem *>(index.internalPointer());
    return item->data(role);
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

    connect(notifier, SIGNAL(deviceAdded(QString)),
            q, SLOT(_k_deviceAdded(QString)));
    connect(notifier, SIGNAL(deviceRemoved(QString)),
            q, SLOT(_k_deviceRemoved(QString)));

    const QList<Solid::Device> &deviceList = Solid::Device::listFromQuery(predicate);

    foreach (const Solid::Device &device, deviceList) {
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
            emit q->dataChanged(index, index);
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
                emit q->dataChanged(idx, idx);
            }
            ++it_i;
            ++it_c;
        } else if ((*it_i)->id() != (*it_c)->id()) {
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

    while (!bookmark.isNull()) {
        QString udi = bookmark.metaDataItem(QStringLiteral("UDI"));
        QString appName = bookmark.metaDataItem(QStringLiteral("OnlyInApp"));
        QUrl url = bookmark.url();
        auto it = std::find(devices.begin(), devices.end(), udi);
        bool deviceAvailable = (it != devices.end());
        if (it != devices.end()) {
            devices.erase(it);
        }

        bool allowedHere = appName.isEmpty() || (appName == QCoreApplication::instance()->applicationName());        
        bool isSupportedUrl = isBalooUrl(url) ? fileIndexingEnabled : true;

        if ((isSupportedUrl && udi.isEmpty() && allowedHere) || deviceAvailable) {

            KFilePlacesItem *item;
            if (deviceAvailable) {
                item = new KFilePlacesItem(bookmarkManager, bookmark.address(), udi);
                // TODO: Update bookmark internal element
            } else {
                item = new KFilePlacesItem(bookmarkManager, bookmark.address());
            }
            connect(item, SIGNAL(itemChanged(QString)),
                    q, SLOT(_k_itemChanged(QString)));
            items << item;
        }

        bookmark = root.next(bookmark);
    }

    // Add bookmarks for the remaining devices, they were previously unknown
    foreach (const QString &udi, devices) {
        bookmark = KFilePlacesItem::createDeviceBookmark(bookmarkManager, udi);
        if (!bookmark.isNull()) {
            KFilePlacesItem *item = new KFilePlacesItem(bookmarkManager,
                    bookmark.address(), udi);
            connect(item, SIGNAL(itemChanged(QString)),
                    q, SLOT(_k_itemChanged(QString)));
            // TODO: Update bookmark internal element
            items << item;
        }
    }

    // return a sorted list based on groups
    qStableSort(items.begin(), items.end(),
                [](KFilePlacesItem *itemA, KFilePlacesItem *itemB) {
       return (itemA->groupType() < itemB->groupType());
    });

    return items;
}

int KFilePlacesModel::Private::findNearestPosition(int source, int target)
{
    const KFilePlacesItem *item = items.at(source);
    const KFilePlacesItem::GroupType groupType = item->groupType();
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
    Qt::ItemFlags res = Qt::ItemIsSelectable | Qt::ItemIsEnabled;

    if (index.isValid()) {
        res |= Qt::ItemIsDragEnabled;
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

    foreach (const QModelIndex &index, indexes) {
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

        foreach (const QUrl &url, urls) {
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
                qWarning() << "URL not added to Places as mimetype could not be determined!";
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
        emit dataChanged(index, index);
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

    KBookmark bookmark = item->bookmark();

    if (bookmark.isNull()) {
        return;
    }

    bookmark.setMetaDataItem(QStringLiteral("IsHidden"), (hidden ? QStringLiteral("true") : QStringLiteral("false")));

    refresh();
    emit dataChanged(index, index);
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
        QString label = data(index, Qt::DisplayRole).toString().replace('&', QLatin1String("&&"));

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

        QString label = data(index, Qt::DisplayRole).toString().replace('&', QLatin1String("&&"));
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
        connect(access, SIGNAL(teardownDone(Solid::ErrorType,QVariant,QString)),
                this, SLOT(_k_storageTeardownDone(Solid::ErrorType,QVariant)));

        access->teardown();
    }
}

void KFilePlacesModel::requestEject(const QModelIndex &index)
{
    Solid::Device device = deviceForIndex(index);

    Solid::OpticalDrive *drive = device.parent().as<Solid::OpticalDrive>();

    if (drive != nullptr) {
        connect(drive, SIGNAL(ejectDone(Solid::ErrorType,QVariant,QString)),
                this, SLOT(_k_storageTeardownDone(Solid::ErrorType,QVariant)));

        drive->eject();
    } else {
        QString label = data(index, Qt::DisplayRole).toString().replace('&', QLatin1String("&&"));
        QString message = i18n("The device '%1' is not a disk and cannot be ejected.", label);
        emit errorMessage(message);
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

        connect(access, SIGNAL(setupDone(Solid::ErrorType,QVariant,QString)),
                this, SLOT(_k_storageSetupDone(Solid::ErrorType,QVariant)));

        access->setup();
    }
}

void KFilePlacesModel::Private::_k_storageSetupDone(Solid::ErrorType error, QVariant errorData)
{
    QPersistentModelIndex index = setupInProgress.take(q->sender());

    if (!index.isValid()) {
        return;
    }

    if (!error) {
        emit q->setupDone(index, true);
    } else {
        if (errorData.isValid()) {
            emit q->errorMessage(i18n("An error occurred while accessing '%1', the system responded: %2",
                                      q->text(index),
                                      errorData.toString()));
        } else {
            emit q->errorMessage(i18n("An error occurred while accessing '%1'",
                                      q->text(index)));
        }
        emit q->setupDone(index, false);
    }

}

void KFilePlacesModel::Private::_k_storageTeardownDone(Solid::ErrorType error, QVariant errorData)
{
    if (error && errorData.isValid()) {
        emit q->errorMessage(errorData.toString());
    }
}

#include "moc_kfileplacesmodel.cpp"
