/*
    This file is part of the KDE project
    SPDX-FileCopyrightText: 1998, 1999 Torben Weis <weis@kde.org>
    SPDX-FileCopyrightText: 1999, 2000 Preston Brown <pbrown@kde.org>
    SPDX-FileCopyrightText: 2000 Simon Hausmann <hausmann@kde.org>
    SPDX-FileCopyrightText: 2000 David Faure <faure@kde.org>
    SPDX-FileCopyrightText: 2003 Waldo Bastian <bastian@kde.org>
    SPDX-FileCopyrightText: 2021 Ahmad Samir <a.samirh78@gmail.com>
    SPDX-FileCopyrightText: 2022 Harald Sitter <sitter@kde.org>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

/*
 * kpropertiesdialog.cpp
 * View/Edit Properties of files, locally or remotely
 *
 * some FilePermissionsPropsPlugin-changes by
 *  Henner Zeller <zeller@think.de>
 * some layout management by
 *  Bertrand Leconte <B.Leconte@mail.dotcom.fr>
 * the rest of the layout management, bug fixes, adaptation to libkio,
 * template feature by
 *  David Faure <faure@kde.org>
 * More layout, cleanups, and fixes by
 *  Preston Brown <pbrown@kde.org>
 * Plugin capability, cleanups and port to KDialog by
 *  Simon Hausmann <hausmann@kde.org>
 * KDesktopPropsPlugin by
 *  Waldo Bastian <bastian@kde.org>
 */

#include "kpropertiesdialog.h"
#include "../utils_p.h"
#include "kio_widgets_debug.h"
#include "kpropertiesdialogbuiltin_p.h"

#include <config-kiowidgets.h>

#include <kacl.h>
#include <kio/global.h>
#include <kio/statjob.h>
#include <kioglobal_p.h>

#include <KJobWidgets>
#include <KLocalizedString>
#include <KPluginFactory>
#include <KPluginMetaData>

#include <qplatformdefs.h>

#include <QDebug>
#include <QDir>
#include <QLayout>
#include <QList>
#include <QMimeData>
#include <QMimeDatabase>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QUrl>

#include <algorithm>
#include <functional>
#include <vector>

#ifdef Q_OS_WIN
#include <process.h>
#include <qt_windows.h>
#include <shellapi.h>
#ifdef __GNUC__
#warning TODO: port completely to win32
#endif
#endif

using namespace KDEPrivate;

constexpr mode_t KFilePermissionsPropsPlugin::fperm[3][4] = {
    {S_IRUSR, S_IWUSR, S_IXUSR, S_ISUID},
    {S_IRGRP, S_IWGRP, S_IXGRP, S_ISGID},
    {S_IROTH, S_IWOTH, S_IXOTH, S_ISVTX},
};

class KPropertiesDialogPrivate
{
public:
    explicit KPropertiesDialogPrivate(KPropertiesDialog *qq)
        : q(qq)
    {
    }
    ~KPropertiesDialogPrivate()
    {
        // qDeleteAll deletes the pages in order, this prevents crashes when closing the dialog
        qDeleteAll(m_pages);
    }

    /**
     * Common initialization for all constructors
     */
    void init();
    /**
     * Inserts all pages in the dialog.
     */
    void insertPages();

    void insertPlugin(KPropertiesDialogPlugin *plugin)
    {
        q->connect(plugin, &KPropertiesDialogPlugin::changed, plugin, [plugin]() {
            plugin->setDirty();
        });
        m_pages.push_back(plugin);
    }

    KPropertiesDialog *const q;
    bool m_aborted = false;
    KPageWidgetItem *fileSharePageItem = nullptr;
    KFilePropsPlugin *m_filePropsPlugin = nullptr;
    KFilePermissionsPropsPlugin *m_permissionsPropsPlugin = nullptr;
    KDesktopPropsPlugin *m_desktopPropsPlugin = nullptr;
    KUrlPropsPlugin *m_urlPropsPlugin = nullptr;

    /**
     * The URL of the props dialog (when shown for only one file)
     */
    QUrl m_singleUrl;
    /**
     * List of items this props dialog is shown for
     */
    KFileItemList m_items;
    /**
     * For templates
     */
    QString m_defaultName;
    QUrl m_currentDir;

    /**
     * List of all plugins inserted ( first one first )
     */
    std::vector<KPropertiesDialogPlugin *> m_pages;
};

KPropertiesDialog::KPropertiesDialog(const KFileItem &item, QWidget *parent)
    : KPageDialog(parent)
    , d(new KPropertiesDialogPrivate(this))
{
    setWindowTitle(i18n("Properties for %1", KIO::decodeFileName(item.name())));

    Q_ASSERT(!item.isNull());
    d->m_items.append(item);

    d->m_singleUrl = item.url();
    Q_ASSERT(!d->m_singleUrl.isEmpty());

    d->init();
}

KPropertiesDialog::KPropertiesDialog(const QString &title, QWidget *parent)
    : KPageDialog(parent)
    , d(new KPropertiesDialogPrivate(this))
{
    setWindowTitle(i18n("Properties for %1", title));

    d->init();
}

KPropertiesDialog::KPropertiesDialog(const KFileItemList &_items, QWidget *parent)
    : KPageDialog(parent)
    , d(new KPropertiesDialogPrivate(this))
{
    if (_items.count() > 1) {
        setWindowTitle(i18np("Properties for 1 item", "Properties for %1 Selected Items", _items.count()));
    } else {
        setWindowTitle(i18n("Properties for %1", KIO::decodeFileName(_items.first().name())));
    }

    Q_ASSERT(!_items.isEmpty());
    d->m_singleUrl = _items.first().url();
    Q_ASSERT(!d->m_singleUrl.isEmpty());

    d->m_items = _items;

    d->init();
}

KPropertiesDialog::KPropertiesDialog(const QUrl &_url, QWidget *parent)
    : KPageDialog(parent)
    , d(new KPropertiesDialogPrivate(this))
{
    d->m_singleUrl = _url.adjusted(QUrl::StripTrailingSlash);

    setWindowTitle(i18n("Properties for %1", KIO::decodeFileName(d->m_singleUrl.fileName())));

    KIO::StatJob *job = KIO::stat(d->m_singleUrl);
    KJobWidgets::setWindow(job, parent);
    job->exec();
    KIO::UDSEntry entry = job->statResult();

    d->m_items.append(KFileItem(entry, d->m_singleUrl));
    d->init();
}

KPropertiesDialog::KPropertiesDialog(const QList<QUrl> &urls, QWidget *parent)
    : KPageDialog(parent)
    , d(new KPropertiesDialogPrivate(this))
{
    if (urls.count() > 1) {
        setWindowTitle(i18np("Properties for 1 item", "Properties for %1 Selected Items", urls.count()));
    } else {
        setWindowTitle(i18n("Properties for %1", KIO::decodeFileName(urls.first().fileName())));
    }

    Q_ASSERT(!urls.isEmpty());
    d->m_singleUrl = urls.first();
    Q_ASSERT(!d->m_singleUrl.isEmpty());

    d->m_items.reserve(urls.size());
    for (const QUrl &url : urls) {
        KIO::StatJob *job = KIO::stat(url);
        KJobWidgets::setWindow(job, parent);
        job->exec();
        KIO::UDSEntry entry = job->statResult();

        d->m_items.append(KFileItem(entry, url));
    }

    d->init();
}

KPropertiesDialog::KPropertiesDialog(const QUrl &_tempUrl, const QUrl &_currentDir, const QString &_defaultName, QWidget *parent)
    : KPageDialog(parent)
    , d(new KPropertiesDialogPrivate(this))
{
    setWindowTitle(i18n("Properties for %1", KIO::decodeFileName(_tempUrl.fileName())));

    d->m_singleUrl = _tempUrl;
    d->m_defaultName = _defaultName;
    d->m_currentDir = _currentDir;
    Q_ASSERT(!d->m_singleUrl.isEmpty());

    // Create the KFileItem for the _template_ file, in order to read from it.
    d->m_items.append(KFileItem(d->m_singleUrl));
    d->init();
}

#ifdef Q_OS_WIN
bool showWin32FilePropertyDialog(const QString &fileName)
{
    QString path_ = QDir::toNativeSeparators(QFileInfo(fileName).absoluteFilePath());

    SHELLEXECUTEINFOW execInfo;

    memset(&execInfo, 0, sizeof(execInfo));
    execInfo.cbSize = sizeof(execInfo);
    execInfo.fMask = SEE_MASK_INVOKEIDLIST | SEE_MASK_NOCLOSEPROCESS | SEE_MASK_FLAG_NO_UI;

    const QString verb(QLatin1String("properties"));
    execInfo.lpVerb = (LPCWSTR)verb.utf16();
    execInfo.lpFile = (LPCWSTR)path_.utf16();

    return ShellExecuteExW(&execInfo);
}
#endif

bool KPropertiesDialog::showDialog(const KFileItem &item, QWidget *parent, bool modal)
{
    // TODO: do we really want to show the win32 property dialog?
    // This means we lose metainfo, support for .desktop files, etc. (DF)
#ifdef Q_OS_WIN
    QString localPath = item.localPath();
    if (!localPath.isEmpty()) {
        return showWin32FilePropertyDialog(localPath);
    }
#endif
    KPropertiesDialog *dlg = new KPropertiesDialog(item, parent);
    if (modal) {
        dlg->exec();
    } else {
        dlg->show();
    }

    return true;
}

bool KPropertiesDialog::showDialog(const QUrl &_url, QWidget *parent, bool modal)
{
#ifdef Q_OS_WIN
    if (_url.isLocalFile()) {
        return showWin32FilePropertyDialog(_url.toLocalFile());
    }
#endif
    KPropertiesDialog *dlg = new KPropertiesDialog(_url, parent);
    if (modal) {
        dlg->exec();
    } else {
        dlg->show();
    }

    return true;
}

bool KPropertiesDialog::showDialog(const KFileItemList &_items, QWidget *parent, bool modal)
{
    if (_items.count() == 1) {
        const KFileItem &item = _items.first();
        if (item.entry().count() == 0 && item.localPath().isEmpty()) // this remote item wasn't listed by a worker
                                                                     // Let's stat to get more info on the file
        {
            return KPropertiesDialog::showDialog(item.url(), parent, modal);
        } else {
            return KPropertiesDialog::showDialog(_items.first(), parent, modal);
        }
    }
    KPropertiesDialog *dlg = new KPropertiesDialog(_items, parent);
    if (modal) {
        dlg->exec();
    } else {
        dlg->show();
    }
    return true;
}

bool KPropertiesDialog::showDialog(const QList<QUrl> &urls, QWidget *parent, bool modal)
{
    KPropertiesDialog *dlg = new KPropertiesDialog(urls, parent);
    if (modal) {
        dlg->exec();
    } else {
        dlg->show();
    }
    return true;
}

void KPropertiesDialogPrivate::init()
{
    q->setFaceType(KPageDialog::Tabbed);

    insertPages();
    // Ensure users can't make it so small where things break
    q->setMinimumSize(q->sizeHint());
}

void KPropertiesDialog::showFileSharingPage()
{
    if (d->fileSharePageItem) {
        setCurrentPage(d->fileSharePageItem);
    }
}

void KPropertiesDialog::setFileSharingPage(QWidget *page)
{
    d->fileSharePageItem = addPage(page, i18nc("@title:tab", "Share"));
}

void KPropertiesDialog::setFileNameReadOnly(bool ro)
{
    if (d->m_filePropsPlugin) {
        d->m_filePropsPlugin->setFileNameReadOnly(ro);
    }

    if (d->m_urlPropsPlugin) {
        d->m_urlPropsPlugin->setFileNameReadOnly(ro);
    }
}

KPropertiesDialog::~KPropertiesDialog()
{
}

QUrl KPropertiesDialog::url() const
{
    return d->m_singleUrl;
}

KFileItem &KPropertiesDialog::item()
{
    return d->m_items.first();
}

KFileItemList KPropertiesDialog::items() const
{
    return d->m_items;
}

QUrl KPropertiesDialog::currentDir() const
{
    return d->m_currentDir;
}

QString KPropertiesDialog::defaultName() const
{
    return d->m_defaultName;
}

bool KPropertiesDialog::canDisplay(const KFileItemList &_items)
{
    // TODO: cache the result of those calls. Currently we parse .desktop files far too many times
    /* clang-format off */
    return KFilePropsPlugin::supports(_items)
        || KFilePermissionsPropsPlugin::supports(_items)
        || KDesktopPropsPlugin::supports(_items)
        || KUrlPropsPlugin::supports(_items);
    /* clang-format on */
}

void KPropertiesDialog::accept()
{
    d->m_aborted = false;

    auto acceptAndClose = [this]() {
        Q_EMIT applied();
        Q_EMIT propertiesClosed();
        deleteLater(); // Somewhat like Qt::WA_DeleteOnClose would do.
        KPageDialog::accept();
    };

    const bool isAnyDirty = std::any_of(d->m_pages.cbegin(), d->m_pages.cend(), [](const KPropertiesDialogPlugin *page) {
        return page->isDirty();
    });

    if (!isAnyDirty) { // No point going further
        acceptAndClose();
        return;
    }

    // If any page is dirty, then set the main one (KFilePropsPlugin) as
    // dirty too. This is what makes it possible to save changes to a global
    // desktop file into a local one. In other cases, it doesn't hurt.
    if (d->m_filePropsPlugin) {
        d->m_filePropsPlugin->setDirty(true);
    }

    // Changes are applied in the following order:
    // - KFilePropsPlugin changes, this is because in case of renaming an item or saving changes
    //   of a template or a .desktop file, the renaming or copying respectively, must be finished
    //   first, before applying the rest of the changes
    // - KFilePermissionsPropsPlugin changes, e.g. if the item was read-only and was changed to
    //   read/write, this must be applied first for other changes to work
    // - The rest of the changes from the other plugins/tabs
    // - KFilePropsPlugin::postApplyChanges()

    auto applyOtherChanges = [this, acceptAndClose]() {
        Q_ASSERT(!d->m_filePropsPlugin->isDirty());
        Q_ASSERT(!d->m_permissionsPropsPlugin->isDirty());

        // Apply the changes for the rest of the plugins
        for (auto *page : d->m_pages) {
            if (d->m_aborted) {
                break;
            }

            if (page->isDirty()) {
                // qDebug() << "applying changes for " << page->metaObject()->className();
                page->applyChanges();
            }
            /* else {
                qDebug() << "skipping page " << page->metaObject()->className();
            } */
        }

        if (!d->m_aborted && d->m_filePropsPlugin) {
            d->m_filePropsPlugin->postApplyChanges();
        }

        if (!d->m_aborted) {
            acceptAndClose();
        } // Else, keep dialog open for user to fix the problem.
    };

    auto applyPermissionsChanges = [this, applyOtherChanges]() {
        connect(d->m_permissionsPropsPlugin, &KFilePermissionsPropsPlugin::changesApplied, this, [applyOtherChanges]() {
            applyOtherChanges();
        });

        d->m_permissionsPropsPlugin->applyChanges();
    };

    if (d->m_filePropsPlugin && d->m_filePropsPlugin->isDirty()) {
        // changesApplied() is _not_ emitted if applying the changes was aborted
        connect(d->m_filePropsPlugin, &KFilePropsPlugin::changesApplied, this, [this, applyPermissionsChanges, applyOtherChanges]() {
            if (d->m_permissionsPropsPlugin && d->m_permissionsPropsPlugin->isDirty()) {
                applyPermissionsChanges();
            } else {
                applyOtherChanges();
            }
        });

        d->m_filePropsPlugin->applyChanges();
    }
}

void KPropertiesDialog::reject()
{
    Q_EMIT canceled();
    Q_EMIT propertiesClosed();

    deleteLater();
    KPageDialog::reject();
}

void KPropertiesDialogPrivate::insertPages()
{
    if (m_items.isEmpty()) {
        return;
    }

    if (KFilePropsPlugin::supports(m_items)) {
        m_filePropsPlugin = new KFilePropsPlugin(q);
        insertPlugin(m_filePropsPlugin);
    }

    if (KFilePermissionsPropsPlugin::supports(m_items)) {
        m_permissionsPropsPlugin = new KFilePermissionsPropsPlugin(q);
        insertPlugin(m_permissionsPropsPlugin);
    }

    if (KChecksumsPlugin::supports(m_items)) {
        KPropertiesDialogPlugin *p = new KChecksumsPlugin(q);
        insertPlugin(p);
    }

    if (KDesktopPropsPlugin::supports(m_items)) {
        m_desktopPropsPlugin = new KDesktopPropsPlugin(q);
        insertPlugin(m_desktopPropsPlugin);
    }

    if (KUrlPropsPlugin::supports(m_items)) {
        m_urlPropsPlugin = new KUrlPropsPlugin(q);
        insertPlugin(m_urlPropsPlugin);
    }

    if (m_items.count() != 1) {
        return;
    }

    const KFileItem item = m_items.first();
    const QString mimetype = item.mimetype();

    if (mimetype.isEmpty()) {
        return;
    }

    const auto scheme = item.url().scheme();
    const auto filter = [mimetype, scheme](const KPluginMetaData &metaData) {
        const auto supportedProtocols = metaData.value(QStringLiteral("X-KDE-Protocols"), QStringList());
        if (!supportedProtocols.isEmpty()) {
            const auto none = std::none_of(supportedProtocols.cbegin(), supportedProtocols.cend(), [scheme](const auto &protocol) {
                return !protocol.isEmpty() && protocol == scheme;
            });
            if (none) {
                return false;
            }
        }

        return metaData.mimeTypes().isEmpty() || metaData.supportsMimeType(mimetype);
    };
    const auto jsonPlugins = KPluginMetaData::findPlugins(QStringLiteral("kf6/propertiesdialog"), filter);
    for (const auto &jsonMetadata : jsonPlugins) {
        if (auto plugin = KPluginFactory::instantiatePlugin<KPropertiesDialogPlugin>(jsonMetadata, q).plugin) {
            insertPlugin(plugin);
        }
    }
}

void KPropertiesDialog::updateUrl(const QUrl &_newUrl)
{
    Q_ASSERT(d->m_items.count() == 1);
    // qDebug() << "KPropertiesDialog::updateUrl (pre)" << _newUrl;
    QUrl newUrl = _newUrl;
    Q_EMIT saveAs(d->m_singleUrl, newUrl);
    // qDebug() << "KPropertiesDialog::updateUrl (post)" << newUrl;

    d->m_singleUrl = newUrl;
    d->m_items.first().setUrl(newUrl);
    Q_ASSERT(!d->m_singleUrl.isEmpty());
    // If we have an Desktop page, set it dirty, so that a full file is saved locally
    // Same for a URL page (because of the Name= hack)
    if (d->m_urlPropsPlugin) {
        d->m_urlPropsPlugin->setDirty();
    } else if (d->m_desktopPropsPlugin) {
        d->m_desktopPropsPlugin->setDirty();
    }
}

void KPropertiesDialog::rename(const QString &_name)
{
    Q_ASSERT(d->m_items.count() == 1);
    // qDebug() << "KPropertiesDialog::rename " << _name;
    QUrl newUrl;
    // if we're creating from a template : use currentdir
    if (!d->m_currentDir.isEmpty()) {
        newUrl = d->m_currentDir;
        newUrl.setPath(Utils::concatPaths(newUrl.path(), _name));
    } else {
        // It's a directory, so strip the trailing slash first
        newUrl = d->m_singleUrl.adjusted(QUrl::StripTrailingSlash);
        // Now change the filename
        newUrl = newUrl.adjusted(QUrl::RemoveFilename); // keep trailing slash
        newUrl.setPath(Utils::concatPaths(newUrl.path(), _name));
    }
    updateUrl(newUrl);
}

void KPropertiesDialog::abortApplying()
{
    d->m_aborted = true;
}

#include "moc_kpropertiesdialog.cpp"
