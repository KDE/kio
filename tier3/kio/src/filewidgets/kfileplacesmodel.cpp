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
#include "kfileplacessharedbookmarks_p.h"

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
#include <qmimedatabase.h>

#include <kfileitem.h>
#include <klocalizedstring.h>

#include <QDebug>
#include <kurlmimedata.h>

#include <kbookmarkmanager.h>
#include <kbookmark.h>

#include <kio/job.h>
#include <kprotocolinfo.h>

#include <solid/devicenotifier.h>
#include <solid/storageaccess.h>
#include <solid/storagedrive.h>
#include <solid/storagevolume.h>
#include <solid/opticaldrive.h>
#include <solid/opticaldisc.h>
#include <solid/portablemediaplayer.h>
#include <solid/predicate.h>
#include <qstandardpaths.h>

class KFilePlacesModel::Private
{
public:
    Private(KFilePlacesModel *self) : q(self), bookmarkManager(0), sharedBookmarks(0) {}
    ~Private()
    {
        delete sharedBookmarks;
        qDeleteAll(items);
    }

    KFilePlacesModel *q;

    QList<KFilePlacesItem*> items;
    QSet<QString> availableDevices;
    QMap<QObject*, QPersistentModelIndex> setupInProgress;

    Solid::Predicate predicate;
    KBookmarkManager *bookmarkManager;
    KFilePlacesSharedBookmarks * sharedBookmarks;

    void reloadAndSignal();
    QList<KFilePlacesItem *> loadBookmarkList();

    void _k_initDeviceList();
    void _k_deviceAdded(const QString &udi);
    void _k_deviceRemoved(const QString &udi);
    void _k_itemChanged(const QString &udi);
    void _k_reloadBookmarks();
    void _k_storageSetupDone(Solid::ErrorType error, QVariant errorData);
    void _k_storageTeardownDone(Solid::ErrorType error, QVariant errorData);
};

KFilePlacesModel::KFilePlacesModel(QObject *parent)
    : QAbstractItemModel(parent), d(new Private(this))
{
    const QString file = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) + QLatin1Char('/') + "kfileplaces/bookmarks.xml";
    d->bookmarkManager = KBookmarkManager::managerForFile(file, "kfilePlaces");

    // Let's put some places in there if it's empty. We have a corner case here:
    // Given you have bookmarked some folders (which have been saved on
    // ~/.local/share/user-places.xbel (according to freedesktop bookmarks spec), and
    // deleted the home directory ~/.kde, the call managerForFile() will return the
    // bookmark manager for the fallback "kfilePlaces", making root.first().isNull() being
    // false (you have your own items bookmarked), resulting on only being added your own
    // bookmarks, and not the default ones too. So, we also check if kfileplaces/bookmarks.xml
    // file exists, and if it doesn't, we also add the default places. (ereslibre)
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
                                              "Home", I18N_NOOP2("KFile System Bookmarks", "Home"),
                                              QUrl::fromLocalFile(QDir::homePath()), "user-home");
        KFilePlacesItem::createSystemBookmark(d->bookmarkManager,
                                              "Network", I18N_NOOP2("KFile System Bookmarks", "Network"),
                                              QUrl("remote:/"), "network-workgroup");
#if defined(_WIN32_WCE)
        // adding drives
        foreach ( const QFileInfo& info, QDir::drives() ) {
            QString driveIcon = "drive-harddisk";
            KFilePlacesItem::createSystemBookmark(d->bookmarkManager,
                                                  info.absoluteFilePath(), info.absoluteFilePath(),
                                                  QUrl::fromLocalFile(info.absoluteFilePath()), driveIcon);
        }
#elif !defined(Q_OS_WIN)
        KFilePlacesItem::createSystemBookmark(d->bookmarkManager,
                                              "Root", I18N_NOOP2("KFile System Bookmarks", "Root"),
                                              QUrl::fromLocalFile("/"), "folder-red");
#endif
        KFilePlacesItem::createSystemBookmark(d->bookmarkManager,
                                              "Trash", I18N_NOOP2("KFile System Bookmarks", "Trash"),
                                              QUrl("trash:/"), "user-trash");

        // Force bookmarks to be saved. If on open/save dialog and the bookmarks are not saved, QFile::exists
        // will always return false, which opening/closing all the time the open/save dialog would case the
        // bookmarks to be added once each time, having lots of times each bookmark. This forces the defaults
        // to be saved on the bookmarks.xml file. Of course, the complete list of bookmarks (those that come from
        // user-places.xbel will be filled later). (ereslibre)
        d->bookmarkManager->saveAs(file);
    }

    // create after, so if we have own places, they are added afterwards, in case of equal priorities
    d->sharedBookmarks = new KFilePlacesSharedBookmarks(d->bookmarkManager);

    QString predicate("[[[[ StorageVolume.ignored == false AND [ StorageVolume.usage == 'FileSystem' OR StorageVolume.usage == 'Encrypted' ]]"
        " OR "
        "[ IS StorageAccess AND StorageDrive.driveType == 'Floppy' ]]"
        " OR "
        "OpticalDisc.availableContent & 'Audio' ]"
        " OR "
        "StorageAccess.ignored == false ]");

    if (KProtocolInfo::isKnownProtocol("mtp")) {
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
    return QUrl(data(index, UrlRole).toUrl());
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
    if (!index.isValid())
        return false;

    KFilePlacesItem *item = static_cast<KFilePlacesItem*>(index.internalPointer());

    return item->isDevice();
}

Solid::Device KFilePlacesModel::deviceForIndex(const QModelIndex &index) const
{
    if (!index.isValid())
        return Solid::Device();

    KFilePlacesItem *item = static_cast<KFilePlacesItem*>(index.internalPointer());

    if (item->isDevice()) {
        return item->device();
    } else {
        return Solid::Device();
    }
}

KBookmark KFilePlacesModel::bookmarkForIndex(const QModelIndex &index) const
{
    if (!index.isValid())
        return KBookmark();

    KFilePlacesItem *item = static_cast<KFilePlacesItem*>(index.internalPointer());

    if (!item->isDevice()) {
        return item->bookmark();
    } else {
        return KBookmark();
    }
}

QVariant KFilePlacesModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid())
        return QVariant();

    KFilePlacesItem *item = static_cast<KFilePlacesItem*>(index.internalPointer());
    return item->data(role);
}

QModelIndex KFilePlacesModel::index(int row, int column, const QModelIndex &parent) const
{
    if (row<0 || column!=0 || row>=d->items.size())
        return QModelIndex();

    if (parent.isValid())
        return QModelIndex();

    return createIndex(row, column, d->items.at(row));
}

QModelIndex KFilePlacesModel::parent(const QModelIndex &child) const
{
    Q_UNUSED(child);
    return QModelIndex();
}

int KFilePlacesModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid())
        return 0;
    else
        return d->items.size();
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
    for (int row = 0; row<d->items.size(); ++row) {
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

    if (foundRow==-1)
        return QModelIndex();
    else
        return createIndex(foundRow, 0, d->items[foundRow]);
}

void KFilePlacesModel::Private::_k_initDeviceList()
{
    Solid::DeviceNotifier *notifier = Solid::DeviceNotifier::instance();

    connect(notifier, SIGNAL(deviceAdded(QString)),
            q, SLOT(_k_deviceAdded(QString)));
    connect(notifier, SIGNAL(deviceRemoved(QString)),
            q, SLOT(_k_deviceRemoved(QString)));

    const QList<Solid::Device> &deviceList = Solid::Device::listFromQuery(predicate);

    foreach(const Solid::Device &device, deviceList) {
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
    if (availableDevices.contains(udi)) {
        availableDevices.remove(udi);
        _k_reloadBookmarks();
    }
}

void KFilePlacesModel::Private::_k_itemChanged(const QString &id)
{
    for (int row = 0; row<items.size(); ++row) {
        if (items.at(row)->id()==id) {
            QModelIndex index = q->index(row, 0);
            emit q->dataChanged(index, index);
        }
    }
}

void KFilePlacesModel::Private::_k_reloadBookmarks()
{
    QList<KFilePlacesItem*> currentItems = loadBookmarkList();

    QList<KFilePlacesItem*>::Iterator it_i = items.begin();
    QList<KFilePlacesItem*>::Iterator it_c = currentItems.begin();

    QList<KFilePlacesItem*>::Iterator end_i = items.end();
    QList<KFilePlacesItem*>::Iterator end_c = currentItems.end();

    while (it_i!=end_i || it_c!=end_c) {
        if (it_i==end_i && it_c!=end_c) {
            int row = items.count();

            q->beginInsertRows(QModelIndex(), row, row);
            it_i = items.insert(it_i, *it_c);
            ++it_i;
            it_c = currentItems.erase(it_c);

            end_i = items.end();
            end_c = currentItems.end();
            q->endInsertRows();

        } else if (it_i!=end_i && it_c==end_c) {
            int row = items.indexOf(*it_i);

            q->beginRemoveRows(QModelIndex(), row, row);
            delete *it_i;
            it_i = items.erase(it_i);

            end_i = items.end();
            end_c = currentItems.end();
            q->endRemoveRows();

        } else if ((*it_i)->id()==(*it_c)->id()) {
            bool shouldEmit = !((*it_i)->bookmark()==(*it_c)->bookmark());
            (*it_i)->setBookmark((*it_c)->bookmark());
            if (shouldEmit) {
                int row = items.indexOf(*it_i);
                QModelIndex idx = q->index(row, 0);
                emit q->dataChanged(idx, idx);
            }
            ++it_i;
            ++it_c;
        } else if ((*it_i)->id()!=(*it_c)->id()) {
            int row = items.indexOf(*it_i);

            if (it_i+1!=end_i && (*(it_i+1))->id()==(*it_c)->id()) { // if the next one matches, it's a remove
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

QList<KFilePlacesItem *> KFilePlacesModel::Private::loadBookmarkList()
{
    QList<KFilePlacesItem*> items;

    KBookmarkGroup root = bookmarkManager->root();
    KBookmark bookmark = root.first();
    QSet<QString> devices = availableDevices;

    while (!bookmark.isNull()) {
        QString udi = bookmark.metaDataItem("UDI");
        QString appName = bookmark.metaDataItem("OnlyInApp");
        bool deviceAvailable = devices.remove(udi);

        bool allowedHere = appName.isEmpty() || (appName==QCoreApplication::instance()->applicationName());

        if ((udi.isEmpty() && allowedHere) || deviceAvailable) {
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

    return items;
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
    Qt::ItemFlags res = Qt::ItemIsSelectable|Qt::ItemIsEnabled;

    if (index.isValid())
        res|= Qt::ItemIsDragEnabled;

    if (!index.isValid())
        res|= Qt::ItemIsDropEnabled;

    return res;
}

static QString _k_internalMimetype(const KFilePlacesModel * const self)
{
    return QString("application/x-kfileplacesmodel-")+QString::number((qptrdiff)self);
}

QStringList KFilePlacesModel::mimeTypes() const
{
    QStringList types;

    types << _k_internalMimetype(this) << "text/uri-list";

    return types;
}

QMimeData *KFilePlacesModel::mimeData(const QModelIndexList &indexes) const
{
    QList<QUrl> urls;
    QByteArray itemData;

    QDataStream stream(&itemData, QIODevice::WriteOnly);

    foreach (const QModelIndex &index, indexes) {
        QUrl itemUrl = url(index);
        if (itemUrl.isValid())
            urls << itemUrl;
        stream << index.row();
    }

    QMimeData *mimeData = new QMimeData();

    if (!urls.isEmpty())
        mimeData->setUrls(urls);

    mimeData->setData(_k_internalMimetype(this), itemData);

    return mimeData;
}

bool KFilePlacesModel::dropMimeData(const QMimeData *data, Qt::DropAction action,
                                    int row, int column, const QModelIndex &parent)
{
    if (action == Qt::IgnoreAction)
        return true;

    if (column > 0)
        return false;

    if (row==-1 && parent.isValid()) {
        return false; // Don't allow to move an item onto another one,
                      // too easy for the user to mess something up
                      // If we really really want to allow copying files this way,
                      // let's do it in the views to get the good old drop menu
    }


    QMimeDatabase db;
    KBookmark afterBookmark;

    if (row==-1) {
        // The dropped item is moved or added to the last position

        KFilePlacesItem *lastItem = d->items.last();
        afterBookmark = lastItem->bookmark();

    } else {
        // The dropped item is moved or added before position 'row', ie after position 'row-1'

        if (row>0) {
            KFilePlacesItem *afterItem = d->items[row-1];
            afterBookmark = afterItem->bookmark();
        }
    }

    if (data->hasFormat(_k_internalMimetype(this))) {
        // The operation is an internal move
        QByteArray itemData = data->data(_k_internalMimetype(this));
        QDataStream stream(&itemData, QIODevice::ReadOnly);
        int itemRow;

        stream >> itemRow;

        KFilePlacesItem *item = d->items[itemRow];
        KBookmark bookmark = item->bookmark();

        int destRow = row == -1 ? d->items.count() : row;
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
    } else if (data->hasFormat("text/uri-list")) {
        // The operation is an add
        const QList<QUrl> urls = KUrlMimeData::urlsFromMimeData(data);

        KBookmarkGroup group = d->bookmarkManager->root();

        foreach (const QUrl &url, urls) {
            // TODO: use KIO::stat in order to get the UDS_DISPLAY_NAME too
            KIO::MimetypeJob *job = KIO::mimetype(url);

            QString mimeString;
            if (!job->exec()) {
                mimeString = QLatin1String("unknown");
            } else {
                mimeString = job->mimetype();
            }

            QMimeType mimetype = db.mimeTypeForName(mimeString);

            if (!mimetype.isValid()) {
                qWarning() << "URL not added to Places as mimetype could not be determined!";
                continue;
            }

            if (!mimetype.inherits("inode/directory")) {
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

    d->reloadAndSignal();

    return true;
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
        bookmark.setMetaDataItem("OnlyInApp", appName);
    }

    if (after.isValid()) {
        KFilePlacesItem *item = static_cast<KFilePlacesItem*>(after.internalPointer());
        d->bookmarkManager->root().moveBookmark(bookmark, item->bookmark());
    }

    d->reloadAndSignal();
}

void KFilePlacesModel::editPlace(const QModelIndex &index, const QString &text, const QUrl &url,
                                 const QString &iconName, const QString &appName)
{
    if (!index.isValid()) return;

    KFilePlacesItem *item = static_cast<KFilePlacesItem*>(index.internalPointer());

    if (item->isDevice()) return;

    KBookmark bookmark = item->bookmark();

    if (bookmark.isNull()) return;

    bookmark.setFullText(text);
    bookmark.setUrl(url);
    bookmark.setIcon(iconName);
    bookmark.setMetaDataItem("OnlyInApp", appName);

    d->reloadAndSignal();
    emit dataChanged(index, index);
}

void KFilePlacesModel::removePlace(const QModelIndex &index) const
{
    if (!index.isValid()) return;

    KFilePlacesItem *item = static_cast<KFilePlacesItem*>(index.internalPointer());

    if (item->isDevice()) return;

    KBookmark bookmark = item->bookmark();

    if (bookmark.isNull()) return;

    d->bookmarkManager->root().deleteBookmark(bookmark);
    d->reloadAndSignal();
}

void KFilePlacesModel::setPlaceHidden(const QModelIndex &index, bool hidden)
{
    if (!index.isValid()) return;

    KFilePlacesItem *item = static_cast<KFilePlacesItem*>(index.internalPointer());

    KBookmark bookmark = item->bookmark();

    if (bookmark.isNull()) return;

    bookmark.setMetaDataItem("IsHidden", (hidden ? "true" : "false"));

    d->reloadAndSignal();
    emit dataChanged(index, index);
}

int KFilePlacesModel::hiddenCount() const
{
    int rows = rowCount();
    int hidden = 0;

    for (int i=0; i<rows; ++i) {
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

        if (drive==0) {
            drive = device.parent().as<Solid::StorageDrive>();
        }

        bool hotpluggable = false;
        bool removable = false;

        if (drive!=0) {
            hotpluggable = drive->isHotpluggable();
            removable = drive->isRemovable();
        }

        QString iconName;
        QString text;
        QString label = data(index, Qt::DisplayRole).toString().replace('&',"&&");

        if (device.is<Solid::OpticalDisc>()) {
            text = i18n("&Release '%1'", label);
        } else if (removable || hotpluggable) {
            text = i18n("&Safely Remove '%1'", label);
            iconName = "media-eject";
        } else {
            text = i18n("&Unmount '%1'", label);
            iconName = "media-eject";
        }

        if (!iconName.isEmpty()) {
            return new QAction(QIcon::fromTheme(iconName), text, 0);
        } else {
            return new QAction(text, 0);
        }
    }

    return 0;
}

QAction *KFilePlacesModel::ejectActionForIndex(const QModelIndex &index) const
{
    Solid::Device device = deviceForIndex(index);

    if (device.is<Solid::OpticalDisc>()) {

        QString label = data(index, Qt::DisplayRole).toString().replace('&',"&&");
        QString text = i18n("&Eject '%1'", label);

        return new QAction(QIcon::fromTheme("media-eject"), text, 0);
    }

    return 0;
}

void KFilePlacesModel::requestTeardown(const QModelIndex &index)
{
    Solid::Device device = deviceForIndex(index);
    Solid::StorageAccess *access = device.as<Solid::StorageAccess>();

    if (access!=0) {
        connect(access, SIGNAL(teardownDone(Solid::ErrorType,QVariant,QString)),
                this, SLOT(_k_storageTeardownDone(Solid::ErrorType,QVariant)));

        access->teardown();
    }
}

void KFilePlacesModel::requestEject(const QModelIndex &index)
{
    Solid::Device device = deviceForIndex(index);

    Solid::OpticalDrive *drive = device.parent().as<Solid::OpticalDrive>();

    if (drive!=0) {
        connect(drive, SIGNAL(ejectDone(Solid::ErrorType,QVariant,QString)),
                this, SLOT(_k_storageTeardownDone(Solid::ErrorType,QVariant)));

        drive->eject();
    } else {
        QString label = data(index, Qt::DisplayRole).toString().replace('&',"&&");
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
