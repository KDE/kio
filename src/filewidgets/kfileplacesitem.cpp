/*
    This file is part of the KDE project
    SPDX-FileCopyrightText: 2007 Kevin Ottens <ervin@kde.org>

    SPDX-License-Identifier: LGPL-2.0-only
*/

#include "kfileplacesitem_p.h"

#include <QDateTime>
#include <QDir>
#include <QIcon>

#include <KBookmarkManager>
#include <KConfig>
#include <KConfigGroup>
#include <KIconUtils>
#include <KLocalizedString>
#include <KMountPoint>
#include <kprotocolinfo.h>
#include <solid/block.h>
#include <solid/networkshare.h>
#include <solid/opticaldisc.h>
#include <solid/opticaldrive.h>
#include <solid/portablemediaplayer.h>
#include <solid/storageaccess.h>
#include <solid/storagedrive.h>
#include <solid/storagevolume.h>

static bool isTrash(const KBookmark &bk)
{
    return bk.url().toString() == QLatin1String("trash:/");
}

KFilePlacesItem::KFilePlacesItem(KBookmarkManager *manager, const QString &address, const QString &udi, KFilePlacesModel *parent)
    : QObject(static_cast<QObject *>(parent))
    , m_manager(manager)
    , m_folderIsEmpty(true)
    , m_isCdrom(false)
    , m_isAccessible(false)
    , m_isTeardownAllowed(false)
{
    updateDeviceInfo(udi);
    setBookmark(m_manager->findByAddress(address));

    if (udi.isEmpty() && m_bookmark.metaDataItem(QStringLiteral("ID")).isEmpty()) {
        m_bookmark.setMetaDataItem(QStringLiteral("ID"), generateNewId());
    } else if (udi.isEmpty()) {
        if (isTrash(m_bookmark)) {
            KConfig cfg(QStringLiteral("trashrc"), KConfig::SimpleConfig);
            const KConfigGroup group = cfg.group("Status");
            m_folderIsEmpty = group.readEntry("Empty", true);
        }
    }

    // Hide SSHFS network device mounted by kdeconnect, since we already have the kdeconnect:// place.
    if (isDevice() && m_access && device().vendor() == QLatin1String("fuse.sshfs")) {
        const QString storageFilePath = m_access->filePath();
        // Not using findByPath() as it resolves symlinks, potentially blocking,
        // but here we know we query for an existing actual mount point.
        const auto mountPoints = KMountPoint::currentMountPoints();
        auto it = std::find_if(mountPoints.cbegin(), mountPoints.cend(), [&storageFilePath](const KMountPoint::Ptr &mountPoint) {
            return mountPoint->mountPoint() == storageFilePath;
        });
        if (it != mountPoints.cend()) {
            if ((*it)->mountedFrom().startsWith(QLatin1String("kdeconnect@"))) {
                // Hide only if the user never set the "Hide" checkbox on the device.
                if (m_bookmark.metaDataItem(QStringLiteral("IsHidden")).isEmpty()) {
                    setHidden(true);
                }
            }
        }
    }
}

KFilePlacesItem::~KFilePlacesItem()
{
}

QString KFilePlacesItem::id() const
{
    if (isDevice()) {
        return bookmark().metaDataItem(QStringLiteral("UDI"));
    } else {
        return bookmark().metaDataItem(QStringLiteral("ID"));
    }
}

bool KFilePlacesItem::hasSupportedScheme(const QStringList &schemes) const
{
    if (schemes.isEmpty()) {
        return true;
    }

    // StorageAccess is always local, doesn't need to be accessible to know this
    if (m_access && schemes.contains(QLatin1String("file"))) {
        return true;
    }

    if (m_networkShare && schemes.contains(m_networkShare->url().scheme())) {
        return true;
    }

    if (m_player) {
        const QStringList protocols = m_player->supportedProtocols();
        for (const QString &protocol : protocols) {
            if (schemes.contains(protocol)) {
                return true;
            }
        }
    }

    return false;
}

bool KFilePlacesItem::isDevice() const
{
    return !bookmark().metaDataItem(QStringLiteral("UDI")).isEmpty();
}

bool KFilePlacesItem::isTeardownAllowed() const
{
    return m_isTeardownAllowed;
}

KBookmark KFilePlacesItem::bookmark() const
{
    return m_bookmark;
}

void KFilePlacesItem::setBookmark(const KBookmark &bookmark)
{
    m_bookmark = bookmark;

    updateDeviceInfo(m_bookmark.metaDataItem(QStringLiteral("UDI")));

    if (bookmark.metaDataItem(QStringLiteral("isSystemItem")) == QLatin1String("true")) {
        // This context must stay as it is - the translated system bookmark names
        // are created with 'KFile System Bookmarks' as their context, so this
        // ensures the right string is picked from the catalog.
        // (coles, 13th May 2009)

        m_text = i18nc("KFile System Bookmarks", bookmark.text().toUtf8().data());
    } else {
        m_text = bookmark.text();
    }

    const KFilePlacesModel::GroupType type = groupType();
    switch (type) {
    case KFilePlacesModel::PlacesType:
        m_groupName = i18nc("@item", "Places");
        break;
    case KFilePlacesModel::RemoteType:
        m_groupName = i18nc("@item", "Remote");
        break;
    case KFilePlacesModel::RecentlySavedType:
        m_groupName = i18nc("@item The place group section name for recent dynamic lists", "Recent");
        break;
    case KFilePlacesModel::SearchForType:
        m_groupName = i18nc("@item", "Search For");
        break;
    case KFilePlacesModel::DevicesType:
        m_groupName = i18nc("@item", "Devices");
        break;
    case KFilePlacesModel::RemovableDevicesType:
        m_groupName = i18nc("@item", "Removable Devices");
        break;
    case KFilePlacesModel::TagsType:
        m_groupName = i18nc("@item", "Tags");
        break;
    default:
        Q_UNREACHABLE();
        break;
    }
}

Solid::Device KFilePlacesItem::device() const
{
    return m_device;
}

QVariant KFilePlacesItem::data(int role) const
{
    if (role == KFilePlacesModel::GroupRole) {
        return QVariant(m_groupName);
    } else if (role != KFilePlacesModel::HiddenRole && role != Qt::BackgroundRole && isDevice()) {
        return deviceData(role);
    } else {
        return bookmarkData(role);
    }
}

KFilePlacesModel::GroupType KFilePlacesItem::groupType() const
{
    if (!isDevice()) {
        const QString protocol = bookmark().url().scheme();
        if (protocol == QLatin1String("timeline") || protocol == QLatin1String("recentlyused")) {
            return KFilePlacesModel::RecentlySavedType;
        }

        if (protocol.contains(QLatin1String("search"))) {
            return KFilePlacesModel::SearchForType;
        }

        if (protocol == QLatin1String("bluetooth") || protocol == QLatin1String("obexftp") || protocol == QLatin1String("kdeconnect")) {
            return KFilePlacesModel::DevicesType;
        }

        if (protocol == QLatin1String("tags")) {
            return KFilePlacesModel::TagsType;
        }

        if (protocol == QLatin1String("remote") || KProtocolInfo::protocolClass(protocol) != QLatin1String(":local")) {
            return KFilePlacesModel::RemoteType;
        } else {
            return KFilePlacesModel::PlacesType;
        }
    }

    if (m_drive && (m_drive->isHotpluggable() || m_drive->isRemovable())) {
        return KFilePlacesModel::RemovableDevicesType;
    } else if (m_networkShare) {
        return KFilePlacesModel::RemoteType;
    } else {
        return KFilePlacesModel::DevicesType;
    }
}

bool KFilePlacesItem::isHidden() const
{
    return m_bookmark.metaDataItem(QStringLiteral("IsHidden")) == QLatin1String("true");
}

void KFilePlacesItem::setHidden(bool hide)
{
    if (m_bookmark.isNull() || isHidden() == hide) {
        return;
    }
    m_bookmark.setMetaDataItem(QStringLiteral("IsHidden"), hide ? QStringLiteral("true") : QStringLiteral("false"));
}

QVariant KFilePlacesItem::bookmarkData(int role) const
{
    KBookmark b = bookmark();

    if (b.isNull()) {
        return QVariant();
    }

    switch (role) {
    case Qt::DisplayRole:
        return m_text;
    case Qt::DecorationRole:
        return QIcon::fromTheme(iconNameForBookmark(b));
    case Qt::BackgroundRole:
        if (isHidden()) {
            return QColor(Qt::lightGray);
        } else {
            return QVariant();
        }
    case KFilePlacesModel::UrlRole:
        return b.url();
    case KFilePlacesModel::SetupNeededRole:
        return false;
    case KFilePlacesModel::HiddenRole:
        return isHidden();
    case KFilePlacesModel::IconNameRole:
        return iconNameForBookmark(b);
    default:
        return QVariant();
    }
}

QVariant KFilePlacesItem::deviceData(int role) const
{
    Solid::Device d = device();

    if (d.isValid()) {
        switch (role) {
        case Qt::DisplayRole:
            return d.displayName();
        case Qt::DecorationRole:
            // qDebug() << "adding emblems" << m_emblems << "to device icon" << m_deviceIconName;
            return KIconUtils::addOverlays(m_deviceIconName, m_emblems);
        case KFilePlacesModel::UrlRole:
            if (m_access) {
                const QString path = m_access->filePath();
                return path.isEmpty() ? QUrl() : QUrl::fromLocalFile(path);
            } else if (m_disc && (m_disc->availableContent() & Solid::OpticalDisc::Audio) != 0) {
                Solid::Block *block = d.as<Solid::Block>();
                if (block) {
                    QString device = block->device();
                    return QUrl(QStringLiteral("audiocd:/?device=%1").arg(device));
                }
                // We failed to get the block device. Assume audiocd:/ can
                // figure it out, but cannot handle multiple disc drives.
                // See https://bugs.kde.org/show_bug.cgi?id=314544#c40
                return QUrl(QStringLiteral("audiocd:/"));
            } else if (m_player) {
                const QStringList protocols = m_player->supportedProtocols();
                if (!protocols.isEmpty()) {
                    return QUrl(QStringLiteral("%1:udi=%2").arg(protocols.first(), d.udi()));
                }
                return QVariant();
            } else {
                return QVariant();
            }
        case KFilePlacesModel::SetupNeededRole:
            if (m_access) {
                return !m_isAccessible;
            } else {
                return QVariant();
            }

        case KFilePlacesModel::TeardownAllowedRole:
            if (m_access) {
                return m_isTeardownAllowed;
            } else {
                return QVariant();
            }

        case KFilePlacesModel::FixedDeviceRole: {
            if (m_drive != nullptr) {
                return !m_drive->isHotpluggable() && !m_drive->isRemovable();
            }
            return true;
        }

        case KFilePlacesModel::CapacityBarRecommendedRole:
            return m_isAccessible && !m_isCdrom;

        case KFilePlacesModel::IconNameRole:
            return m_deviceIconName;

        default:
            return QVariant();
        }
    } else {
        return QVariant();
    }
}

KBookmark KFilePlacesItem::createBookmark(KBookmarkManager *manager, const QString &label, const QUrl &url, const QString &iconName, KFilePlacesItem *after)
{
    KBookmarkGroup root = manager->root();
    if (root.isNull()) {
        return KBookmark();
    }
    QString empty_icon = iconName;
    if (url.toString() == QLatin1String("trash:/")) {
        if (empty_icon.endsWith(QLatin1String("-full"))) {
            empty_icon.chop(5);
        } else if (empty_icon.isEmpty()) {
            empty_icon = QStringLiteral("user-trash");
        }
    }
    KBookmark bookmark = root.addBookmark(label, url, empty_icon);
    bookmark.setMetaDataItem(QStringLiteral("ID"), generateNewId());

    if (after) {
        root.moveBookmark(bookmark, after->bookmark());
    }

    return bookmark;
}

KBookmark KFilePlacesItem::createSystemBookmark(KBookmarkManager *manager,
                                                const char *untranslatedLabel,
                                                const QUrl &url,
                                                const QString &iconName,
                                                const KBookmark &after)
{
    KBookmark bookmark = createBookmark(manager, QString::fromUtf8(untranslatedLabel), url, iconName);
    if (!bookmark.isNull()) {
        bookmark.setMetaDataItem(QStringLiteral("isSystemItem"), QStringLiteral("true"));
    }
    if (!after.isNull()) {
        manager->root().moveBookmark(bookmark, after);
    }
    return bookmark;
}

KBookmark KFilePlacesItem::createDeviceBookmark(KBookmarkManager *manager, const QString &udi)
{
    KBookmarkGroup root = manager->root();
    if (root.isNull()) {
        return KBookmark();
    }
    KBookmark bookmark = root.createNewSeparator();
    bookmark.setMetaDataItem(QStringLiteral("UDI"), udi);
    bookmark.setMetaDataItem(QStringLiteral("isSystemItem"), QStringLiteral("true"));
    return bookmark;
}

KBookmark KFilePlacesItem::createTagBookmark(KBookmarkManager *manager, const QString &tag)
{
    // TODO: Currently KFilePlacesItem::setBookmark() only decides by the "isSystemItem" property
    // if the label text should be looked up for translation. So there is a small risk that
    // labelTexts which match existing untranslated system labels accidentally get translated.
    KBookmark bookmark = createBookmark(manager, tag, QUrl(QLatin1String("tags:/") + tag), QStringLiteral("tag"));
    if (!bookmark.isNull()) {
        bookmark.setMetaDataItem(QStringLiteral("tag"), tag);
        bookmark.setMetaDataItem(QStringLiteral("isSystemItem"), QStringLiteral("true"));
    }

    return bookmark;
}

QString KFilePlacesItem::generateNewId()
{
    static int count = 0;

    //    return QString::number(count++);

    return QString::number(QDateTime::currentSecsSinceEpoch()) + QLatin1Char('/') + QString::number(count++);

    //    return QString::number(QDateTime::currentSecsSinceEpoch())
    //         + '/' + QString::number(qrand());
}

bool KFilePlacesItem::updateDeviceInfo(const QString &udi)
{
    if (m_device.udi() == udi) {
        return false;
    }

    if (m_access) {
        m_access->disconnect(this);
    }

    m_device = Solid::Device(udi);
    if (m_device.isValid()) {
        m_access = m_device.as<Solid::StorageAccess>();
        m_volume = m_device.as<Solid::StorageVolume>();
        m_disc = m_device.as<Solid::OpticalDisc>();
        m_player = m_device.as<Solid::PortableMediaPlayer>();
        m_networkShare = m_device.as<Solid::NetworkShare>();
        m_deviceIconName = m_device.icon();
        m_emblems = m_device.emblems();

        m_drive = nullptr;
        Solid::Device parentDevice = m_device;
        while (parentDevice.isValid() && !m_drive) {
            m_drive = parentDevice.as<Solid::StorageDrive>();
            parentDevice = parentDevice.parent();
        }

        if (m_access) {
            connect(m_access.data(), &Solid::StorageAccess::accessibilityChanged, this, &KFilePlacesItem::onAccessibilityChanged);
            onAccessibilityChanged(m_access->isAccessible());
        }
    } else {
        m_access = nullptr;
        m_volume = nullptr;
        m_disc = nullptr;
        m_player = nullptr;
        m_drive = nullptr;
        m_networkShare = nullptr;
        m_deviceIconName.clear();
        m_emblems.clear();
    }

    return true;
}

void KFilePlacesItem::onAccessibilityChanged(bool isAccessible)
{
    m_isAccessible = isAccessible;
    m_isCdrom =
        m_device.is<Solid::OpticalDrive>() || m_device.parent().is<Solid::OpticalDrive>() || (m_volume && m_volume->fsType() == QLatin1String("iso9660"));
    m_emblems = m_device.emblems();

    m_isTeardownAllowed = isAccessible;
    if (m_isTeardownAllowed) {
        if (m_access->filePath() == QDir::rootPath()) {
            m_isTeardownAllowed = false;
        } else {
            KMountPoint::Ptr mountPoint = KMountPoint::currentMountPoints().findByPath(QDir::homePath());
            if (mountPoint && m_access->filePath() == mountPoint->mountPoint()) {
                m_isTeardownAllowed = false;
            }
        }
    }

    Q_EMIT itemChanged(id());
}

QString KFilePlacesItem::iconNameForBookmark(const KBookmark &bookmark) const
{
    if (!m_folderIsEmpty && isTrash(bookmark)) {
        return bookmark.icon() + QLatin1String("-full");
    } else {
        return bookmark.icon();
    }
}

#include "moc_kfileplacesitem_p.cpp"
