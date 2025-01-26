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
#include <solid/genericinterface.h>
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
    , m_isTeardownOverlayRecommended(false)
    , m_isTeardownInProgress(false)
    , m_isSetupInProgress(false)
    , m_isEjectInProgress(false)
    , m_isReadOnly(false)
{
    updateDeviceInfo(udi);

    setBookmark(m_manager->findByAddress(address));

    if (udi.isEmpty() && m_bookmark.metaDataItem(QStringLiteral("ID")).isEmpty()) {
        m_bookmark.setMetaDataItem(QStringLiteral("ID"), generateNewId());
    } else if (udi.isEmpty()) {
        if (isTrash(m_bookmark)) {
            KConfig cfg(QStringLiteral("trashrc"), KConfig::SimpleConfig);
            const KConfigGroup group = cfg.group(QStringLiteral("Status"));
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

KFilePlacesModel::DeviceAccessibility KFilePlacesItem::deviceAccessibility() const
{
    if (m_isTeardownInProgress || m_isEjectInProgress) {
        return KFilePlacesModel::TeardownInProgress;
    } else if (m_isSetupInProgress) {
        return KFilePlacesModel::SetupInProgress;
    } else if (m_isAccessible) {
        return KFilePlacesModel::Accessible;
    } else {
        return KFilePlacesModel::SetupNeeded;
    }
}

bool KFilePlacesItem::isTeardownAllowed() const
{
    return m_isTeardownAllowed;
}

bool KFilePlacesItem::isTeardownOverlayRecommended() const
{
    return m_isTeardownOverlayRecommended;
}

bool KFilePlacesItem::isEjectAllowed() const
{
    return m_isCdrom;
}

KBookmark KFilePlacesItem::bookmark() const
{
    return m_bookmark;
}

void KFilePlacesItem::setBookmark(const KBookmark &bookmark)
{
    m_bookmark = bookmark;

    if (m_device.isValid()) {
        m_bookmark.setMetaDataItem(QStringLiteral("UDI"), m_device.udi());
        if (m_volume && !m_volume->uuid().isEmpty()) {
            m_bookmark.setMetaDataItem(QStringLiteral("uuid"), m_volume->uuid());
        }
    }

    if (bookmark.metaDataItem(QStringLiteral("isSystemItem")) == QLatin1String("true")) {
        // This context must stay as it is - the translated system bookmark names
        // are created with 'KFile System Bookmarks' as their context, so this
        // ensures the right string is picked from the catalog.
        // (coles, 13th May 2009)

        m_text = i18nc("KFile System Bookmarks", bookmark.text().toUtf8().data());
    } else {
        m_text = bookmark.text();
    }

    if (!isDevice()) {
        const QString protocol = bookmark.url().scheme();
        if (protocol == QLatin1String("timeline") || protocol == QLatin1String("recentlyused")) {
            m_groupType = KFilePlacesModel::RecentlySavedType;
        } else if (protocol.contains(QLatin1String("search"))) {
            m_groupType = KFilePlacesModel::SearchForType;
        } else if (protocol == QLatin1String("bluetooth") || protocol == QLatin1String("obexftp") || protocol == QLatin1String("kdeconnect")) {
            m_groupType = KFilePlacesModel::DevicesType;
        } else if (protocol == QLatin1String("tags")) {
            m_groupType = KFilePlacesModel::TagsType;
        } else if (protocol == QLatin1String("remote") || KProtocolInfo::protocolClass(protocol) != QLatin1String(":local")) {
            m_groupType = KFilePlacesModel::RemoteType;
        } else {
            m_groupType = KFilePlacesModel::PlacesType;
        }
    } else {
        if (m_drive && m_drive->isRemovable()) {
            m_groupType = KFilePlacesModel::RemovableDevicesType;
        } else if (m_networkShare) {
            m_groupType = KFilePlacesModel::RemoteType;
        } else {
            m_groupType = KFilePlacesModel::DevicesType;
        }
    }

    switch (m_groupType) {
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
    case KFilePlacesModel::UnknownType:
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
    return m_groupType;
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
    case Qt::ToolTipRole: {
        const KFilePlacesModel::GroupType type = groupType();
        // Don't display technical gibberish in the URL, particularly search.
        if (type != KFilePlacesModel::RecentlySavedType && type != KFilePlacesModel::SearchForType && type != KFilePlacesModel::TagsType) {
            return b.url().toDisplayString(QUrl::PreferLocalFile);
        }
        return QString();
    }
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
            if (m_deviceDisplayName.isEmpty()) {
                m_deviceDisplayName = d.displayName();
            }
            return m_deviceDisplayName;
        case Qt::DecorationRole:
            // qDebug() << "adding emblems" << m_emblems << "to device icon" << m_deviceIconName;
            return KIconUtils::addOverlays(m_deviceIconName, m_emblems);
        case Qt::ToolTipRole: {
            if (m_access && m_isAccessible) {
                // For loop devices, show backing file path rather than /dev/loop123.
                QString mountedFrom = m_backingFile;
                if (mountedFrom.isEmpty() && m_block) {
                    mountedFrom = m_block->device();
                }

                if (!mountedFrom.isEmpty()) {
                    return i18nc("@info:tooltip path (mounted from)", "%1 (from %2)", m_access->filePath(), mountedFrom);
                }
            } else if (!m_backingFile.isEmpty()) {
                return m_backingFile;
            } else if (m_block) {
                return m_block->device();
            }
            return QString();
        }
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
                    const QString protocol = protocols.first();
                    if (protocol == QLatin1String("mtp")) {
                        return QUrl(QStringLiteral("%1:udi=%2").arg(protocol, d.udi()));
                    } else {
                        QUrl url;
                        url.setScheme(protocol);
                        url.setHost(d.udi().section(QLatin1Char('/'), -1));
                        url.setPath(QStringLiteral("/"));
                        return url;
                    }
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

        case KFilePlacesModel::EjectAllowedRole:
            return m_isAccessible && m_isCdrom;

        case KFilePlacesModel::TeardownOverlayRecommendedRole:
            return m_isTeardownOverlayRecommended;

        case KFilePlacesModel::DeviceAccessibilityRole:
            return deviceAccessibility();

        case KFilePlacesModel::FixedDeviceRole: {
            if (m_drive != nullptr) {
                return !m_drive->isRemovable();
            }
            return true;
        }

        case KFilePlacesModel::CapacityBarRecommendedRole:
            return m_isAccessible && !m_isCdrom && !m_networkShare && !m_isReadOnly;

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

KBookmark KFilePlacesItem::createDeviceBookmark(KBookmarkManager *manager, const Solid::Device &device)
{
    KBookmarkGroup root = manager->root();
    if (root.isNull()) {
        return KBookmark();
    }
    KBookmark bookmark = root.createNewSeparator();
    bookmark.setMetaDataItem(QStringLiteral("UDI"), device.udi());
    bookmark.setMetaDataItem(QStringLiteral("isSystemItem"), QStringLiteral("true"));

    const auto storage = device.as<Solid::StorageVolume>();
    if (storage) {
        bookmark.setMetaDataItem(QStringLiteral("uuid"), storage->uuid());
    }
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

    if (m_opticalDrive) {
        m_opticalDrive->disconnect(this);
    }

    m_device = Solid::Device(udi);
    if (m_device.isValid()) {
        m_access = m_device.as<Solid::StorageAccess>();
        m_volume = m_device.as<Solid::StorageVolume>();
        m_block = m_device.as<Solid::Block>();
        m_disc = m_device.as<Solid::OpticalDisc>();
        m_player = m_device.as<Solid::PortableMediaPlayer>();
        m_networkShare = m_device.as<Solid::NetworkShare>();
        m_deviceIconName = m_device.icon();
        m_emblems = m_device.emblems();

        if (auto *genericIface = m_device.as<Solid::GenericInterface>()) {
            m_backingFile = genericIface->property(QStringLiteral("BackingFile")).toString();
        }

        m_drive = nullptr;
        m_opticalDrive = nullptr;

        Solid::Device parentDevice = m_device;
        while (parentDevice.isValid() && !m_drive) {
            m_drive = parentDevice.as<Solid::StorageDrive>();
            m_opticalDrive = parentDevice.as<Solid::OpticalDrive>();
            parentDevice = parentDevice.parent();
        }

        if (m_access) {
            connect(m_access.data(), &Solid::StorageAccess::setupRequested, this, [this] {
                m_isSetupInProgress = true;
                Q_EMIT itemChanged(id(), {KFilePlacesModel::DeviceAccessibilityRole});
            });
            connect(m_access.data(), &Solid::StorageAccess::setupDone, this, [this] {
                m_isSetupInProgress = false;
                Q_EMIT itemChanged(id(), {KFilePlacesModel::DeviceAccessibilityRole});
            });

            connect(m_access.data(), &Solid::StorageAccess::teardownRequested, this, [this] {
                m_isTeardownInProgress = true;
                Q_EMIT itemChanged(id(), {KFilePlacesModel::DeviceAccessibilityRole});
            });
            connect(m_access.data(), &Solid::StorageAccess::teardownDone, this, [this] {
                m_isTeardownInProgress = false;
                Q_EMIT itemChanged(id(), {KFilePlacesModel::DeviceAccessibilityRole});
            });

            connect(m_access.data(), &Solid::StorageAccess::accessibilityChanged, this, &KFilePlacesItem::onAccessibilityChanged);
            onAccessibilityChanged(m_access->isAccessible());
        }

        if (m_opticalDrive) {
            connect(m_opticalDrive.data(), &Solid::OpticalDrive::ejectRequested, this, [this] {
                m_isEjectInProgress = true;
                Q_EMIT itemChanged(id(), {KFilePlacesModel::DeviceAccessibilityRole});
            });
            connect(m_opticalDrive.data(), &Solid::OpticalDrive::ejectDone, this, [this] {
                m_isEjectInProgress = false;
                Q_EMIT itemChanged(id(), {KFilePlacesModel::DeviceAccessibilityRole});
            });
        }
    } else {
        m_access = nullptr;
        m_volume = nullptr;
        m_disc = nullptr;
        m_player = nullptr;
        m_drive = nullptr;
        m_opticalDrive = nullptr;
        m_networkShare = nullptr;
        m_deviceIconName.clear();
        m_emblems.clear();
    }

    return true;
}

void KFilePlacesItem::onAccessibilityChanged(bool isAccessible)
{
    m_isAccessible = isAccessible;
    m_isCdrom = m_device.is<Solid::OpticalDrive>() || m_opticalDrive || (m_volume && m_volume->fsType() == QLatin1String("iso9660"));
    m_emblems = m_device.emblems();

    if (auto generic = m_device.as<Solid::GenericInterface>()) {
        // TODO add Solid API for this.
        m_isReadOnly = generic->property(QStringLiteral("ReadOnly")).toBool();
    }

    m_isTeardownAllowed = isAccessible;
    if (m_isTeardownAllowed) {
        if (m_access->filePath() == QDir::rootPath()) {
            m_isTeardownAllowed = false;
        } else {
            const auto homeDevice = Solid::Device::storageAccessFromPath(QDir::homePath());
            const auto *homeAccess = homeDevice.as<Solid::StorageAccess>();
            if (homeAccess && m_access->filePath() == homeAccess->filePath()) {
                m_isTeardownAllowed = false;
            }
        }
    }

    m_isTeardownOverlayRecommended = m_isTeardownAllowed && !m_networkShare;
    if (m_isTeardownOverlayRecommended) {
        if (m_drive && !m_drive->isRemovable()) {
            m_isTeardownOverlayRecommended = false;
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
