/*
    This file is part of the KDE project
    SPDX-FileCopyrightText: 1998, 1999 Torben Weis <weis@kde.org>
    SPDX-FileCopyrightText: 1999, 2000 Preston Brown <pbrown@kde.org>
    SPDX-FileCopyrightText: 2000 Simon Hausmann <hausmann@kde.org>
    SPDX-FileCopyrightText: 2000 David Faure <faure@kde.org>
    SPDX-FileCopyrightText: 2003 Waldo Bastian <bastian@kde.org>

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
#include "kpropertiesdialog_p.h"
#include "kio_widgets_debug.h"
#include "../pathhelpers_p.h"

#include <config-kiowidgets.h>

#include <qplatformdefs.h>
#include <algorithm>
#include <functional>

#include <QCheckBox>
#include <QClipboard>
#include <QDebug>
#include <QDBusConnection>
#include <QDBusInterface>
#include <QDBusReply>
#include <QDir>
#include <QDialogButtonBox>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFileSystemWatcher>
#include <QFutureWatcher>
#include <QLabel>
#include <QLayout>
#include <QLocale>
#include <QMimeDatabase>
#include <QProgressBar>
#include <QPushButton>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QStyle>
#include <QUrl>
#include <QVector>
#include <QtConcurrent>

#if HAVE_POSIX_ACL
extern "C" {
#  include <errno.h>
#  include <sys/xattr.h>
}
#endif

#include <KAuthorized>
#include <KColorScheme>
#include <KDialogJobUiDelegate>
#include <kdirnotify.h>
#include <kdiskfreespaceinfo.h>
#include <KDesktopFile>
#include <KIconButton>
#include <kurlrequester.h>
#include <KLocalizedString>
#include <KJobUiDelegate>
#include <kio/global.h>
#include <kio/job.h>
#include <kio/copyjob.h>
#include <kio/chmodjob.h>
#include <kio/directorysizejob.h>
#include <KIO/FileSystemFreeSpaceJob>
#include <KIO/OpenFileManagerWindowJob>
#include <kio/renamedialog.h>
#include <kio/jobuidelegate.h>
#include <KJobWidgets>
#include <kmountpoint.h>
#include <KMessageBox>
#include <KMessageWidget>
#include <KService>
#include <KSharedConfig>
#include <KComboBox>
#include <KCompletion>
#include <KLineEdit>
#include <KSeparator>
#include <KSqueezedTextLabel>
#include <KMimeTypeTrader>
#include <KIO/ApplicationLauncherJob>
#include <kio/desktopexecparser.h>
#include <kacl.h>
#include <KConfigGroup>
#include <KMimeTypeEditor>
#include <KShell>
#include <KCapacityBar>
#include <kfileitemlistproperties.h>
#include <KWindowConfig>
#include <kioglobal_p.h>
#include <kprotocolinfo.h>

#include "ui_checksumswidget.h"
#include "ui_kpropertiesdesktopbase.h"
#include "ui_kpropertiesdesktopadvbase.h"
#if HAVE_POSIX_ACL
#include "kacleditwidget.h"
#endif

#include <kbuildsycocaprogressdialog.h>
#include <KMimeTypeChooser>

#ifdef Q_OS_WIN
#include <qt_windows.h>
#include <shellapi.h>
#include <process.h>
#ifdef __GNUC__
# warning TODO: port completely to win32
#endif
#endif

using namespace KDEPrivate;

static QString nameFromFileName(QString nameStr)
{
    if (nameStr.endsWith(QLatin1String(".desktop"))) {
        nameStr.chop(8);
    }
    if (nameStr.endsWith(QLatin1String(".kdelnk"))) {
        nameStr.chop(7);
    }
    // Make it human-readable (%2F => '/', ...)
    nameStr = KIO::decodeFileName(nameStr);
    return nameStr;
}

const mode_t KFilePermissionsPropsPlugin::fperm[3][4] = {
    {S_IRUSR, S_IWUSR, S_IXUSR, S_ISUID},
    {S_IRGRP, S_IWGRP, S_IXGRP, S_ISGID},
    {S_IROTH, S_IWOTH, S_IXOTH, S_ISVTX}
};

class Q_DECL_HIDDEN KPropertiesDialog::KPropertiesDialogPrivate
{
public:
    explicit KPropertiesDialogPrivate(KPropertiesDialog *qq)
        : q(qq)
        , m_aborted(false)
        , fileSharePage(nullptr)
    {
    }
    ~KPropertiesDialogPrivate()
    {
    }

    /**
     * Common initialization for all constructors
     */
    void init();
    /**
     * Inserts all pages in the dialog.
     */
    void insertPages();
    /**
     * Apply change, returns if the changes are applied successfully.
     */
    bool applyChanges();

    KPropertiesDialog * const q;
    bool m_aborted;
    QWidget *fileSharePage;
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
    QList<KPropertiesDialogPlugin *> m_pageList;
    QAbstractButton *m_applyButton;
};

KPropertiesDialog::KPropertiesDialog(const KFileItem &item,
                                     QWidget *parent)
    : KPageDialog(parent), d(new KPropertiesDialogPrivate(this))
{
    setWindowTitle(i18n("Properties for %1", KIO::decodeFileName(item.name())));

    Q_ASSERT(!item.isNull());
    d->m_items.append(item);

    d->m_singleUrl = item.url();
    Q_ASSERT(!d->m_singleUrl.isEmpty());

    d->init();
}

KPropertiesDialog::KPropertiesDialog(const QString &title,
                                     QWidget *parent)
    : KPageDialog(parent), d(new KPropertiesDialogPrivate(this))
{
    setWindowTitle(i18n("Properties for %1", title));

    d->init();
}

KPropertiesDialog::KPropertiesDialog(const KFileItemList &_items,
                                     QWidget *parent)
    : KPageDialog(parent), d(new KPropertiesDialogPrivate(this))
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

KPropertiesDialog::KPropertiesDialog(const QUrl &_url,
                                     QWidget *parent)
    : KPageDialog(parent), d(new KPropertiesDialogPrivate(this))
{
    setWindowTitle(i18n("Properties for %1", KIO::decodeFileName(_url.fileName())));

    d->m_singleUrl = _url;

    KIO::StatJob *job = KIO::stat(_url);
    KJobWidgets::setWindow(job, parent);
    job->exec();
    KIO::UDSEntry entry = job->statResult();

    d->m_items.append(KFileItem(entry, _url));
    d->init();
}

KPropertiesDialog::KPropertiesDialog(const QList<QUrl>& urls,
                                     QWidget* parent)
    : KPageDialog(parent), d(new KPropertiesDialogPrivate(this))
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
    for (const QUrl& url : urls) {
        KIO::StatJob *job = KIO::stat(url);
        KJobWidgets::setWindow(job, parent);
        job->exec();
        KIO::UDSEntry entry = job->statResult();

        d->m_items.append(KFileItem(entry, url));
    }

    d->init();
}

KPropertiesDialog::KPropertiesDialog(const QUrl &_tempUrl, const QUrl &_currentDir,
                                     const QString &_defaultName,
                                     QWidget *parent)
    : KPageDialog(parent), d(new KPropertiesDialogPrivate(this))
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

#ifndef _WIN32_WCE
    SHELLEXECUTEINFOW execInfo;
#else
    SHELLEXECUTEINFO execInfo;
#endif
    memset(&execInfo, 0, sizeof(execInfo));
    execInfo.cbSize = sizeof(execInfo);
#ifndef _WIN32_WCE
    execInfo.fMask = SEE_MASK_INVOKEIDLIST | SEE_MASK_NOCLOSEPROCESS | SEE_MASK_FLAG_NO_UI;
#else
    execInfo.fMask = SEE_MASK_NOCLOSEPROCESS | SEE_MASK_FLAG_NO_UI;
#endif
    const QString verb(QLatin1String("properties"));
    execInfo.lpVerb = (LPCWSTR)verb.utf16();
    execInfo.lpFile = (LPCWSTR)path_.utf16();
#ifndef _WIN32_WCE
    return ShellExecuteExW(&execInfo);
#else
    return ShellExecuteEx(&execInfo);
    //There is no native file property dialog in wince
    // return false;
#endif
}
#endif

bool KPropertiesDialog::showDialog(const KFileItem &item, QWidget *parent,
                                   bool modal)
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

bool KPropertiesDialog::showDialog(const QUrl &_url, QWidget *parent,
                                   bool modal)
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

bool KPropertiesDialog::showDialog(const KFileItemList &_items, QWidget *parent,
                                   bool modal)
{
    if (_items.count() == 1) {
        const KFileItem &item = _items.first();
        if (item.entry().count() == 0 && item.localPath().isEmpty()) // this remote item wasn't listed by a slave
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

bool KPropertiesDialog::showDialog(const QList<QUrl> &urls, QWidget* parent,
                                   bool modal)
{
    KPropertiesDialog *dlg = new KPropertiesDialog(urls, parent);
    if (modal) {
        dlg->exec();
    } else {
        dlg->show();
    }
    return true;
}

void KPropertiesDialog::KPropertiesDialogPrivate::init()
{
    q->setFaceType(KPageDialog::Tabbed);
    q->setStandardButtons(QDialogButtonBox::Ok | QDialogButtonBox::Apply | QDialogButtonBox::Cancel);
    m_applyButton = q->button(QDialogButtonBox::Apply);
    m_applyButton->setEnabled(false);
    q->connect(m_applyButton, &QAbstractButton::clicked, q, [=] { applyChanges(); });

    insertPages();

    KConfigGroup group(KSharedConfig::openConfig(), "KPropertiesDialog");
    KWindowConfig::restoreWindowSize(q->windowHandle(), group);
}

void KPropertiesDialog::showFileSharingPage()
{
    if (d->fileSharePage) {
        // FIXME: this showFileSharingPage thingy looks broken! (tokoe)
        // showPage( pageIndex( d->fileSharePage));
    }
}

void KPropertiesDialog::setFileSharingPage(QWidget *page)
{
    d->fileSharePage = page;
}

void KPropertiesDialog::setFileNameReadOnly(bool ro)
{
    for (KPropertiesDialogPlugin *it : qAsConst(d->m_pageList)) {
        if (auto *filePropsPlugin = qobject_cast<KFilePropsPlugin *>(it)) {
            filePropsPlugin->setFileNameReadOnly(ro);
        } else if (auto *urlPropsPlugin = qobject_cast<KUrlPropsPlugin *>(it)) {
            urlPropsPlugin->setFileNameReadOnly(ro);
        }
    }
}

KPropertiesDialog::~KPropertiesDialog()
{
    qDeleteAll(d->m_pageList);
    delete d;

    KConfigGroup group(KSharedConfig::openConfig(), "KPropertiesDialog");
    KWindowConfig::saveWindowSize(windowHandle(), group, KConfigBase::Persistent);
}

void KPropertiesDialog::insertPlugin(KPropertiesDialogPlugin *plugin)
{
    connect(plugin, &KPropertiesDialogPlugin::changed,
            plugin, QOverload<>::of(&KPropertiesDialogPlugin::setDirty));
    connect(plugin, &KPropertiesDialogPlugin::changed,
            this, [=] { d->m_applyButton->setEnabled(true); });

    d->m_pageList.append(plugin);
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
    return KFilePropsPlugin::supports(_items) ||
           KFilePermissionsPropsPlugin::supports(_items) ||
           KDesktopPropsPlugin::supports(_items) ||
           KUrlPropsPlugin::supports(_items) ||
           KDevicePropsPlugin::supports(_items) /* ||
            KPreviewPropsPlugin::supports( _items )*/;
}

void KPropertiesDialog::slotOk()
{
    accept();
}

bool KPropertiesDialog::KPropertiesDialogPrivate::applyChanges()
{
    QList<KPropertiesDialogPlugin *>::const_iterator pageListIt;
    m_aborted = false;

    KFilePropsPlugin *filePropsPlugin = qobject_cast<KFilePropsPlugin *>(m_pageList.first());

    // If any page is dirty, then set the main one (KFilePropsPlugin) as
    // dirty too. This is what makes it possible to save changes to a global
    // desktop file into a local one. In other cases, it doesn't hurt.
    for (pageListIt = m_pageList.constBegin(); pageListIt != m_pageList.constEnd(); ++pageListIt) {
        if ((*pageListIt)->isDirty() && filePropsPlugin) {
            filePropsPlugin->setDirty();
            break;
        }
    }

    // Apply the changes in the _normal_ order of the tabs now
    // This is because in case of renaming a file, KFilePropsPlugin will call
    // KPropertiesDialog::rename, so other tab will be ok with whatever order
    // BUT for file copied from templates, we need to do the renaming first !
    for (pageListIt = m_pageList.constBegin(); pageListIt != m_pageList.constEnd() && !m_aborted; ++pageListIt) {
        if ((*pageListIt)->isDirty()) {
            // qDebug() << "applying changes for " << (*pageListIt)->metaObject()->className();
            (*pageListIt)->applyChanges();
            // applyChanges may change d->m_aborted.
        } else {
            // qDebug() << "skipping page " << (*pageListIt)->metaObject()->className();
        }
    }

    if (!m_aborted && filePropsPlugin) {
        filePropsPlugin->postApplyChanges();
    }

    if (!m_aborted) {
        m_applyButton->setEnabled(false);
        emit q->applied();
    }
    return !m_aborted;
}

void KPropertiesDialog::accept()
{
    if (d->applyChanges()) {
        // For OK button, close the dialog.
        emit propertiesClosed();
        deleteLater(); // somewhat like Qt::WA_DeleteOnClose would do.
        KPageDialog::accept();
    } // else, keep dialog open for user to fix the problem.
}

void KPropertiesDialog::slotCancel()
{
    reject();
}

void KPropertiesDialog::reject()
{
    emit canceled();
    emit propertiesClosed();

    deleteLater();
    KPageDialog::reject();
}

void KPropertiesDialog::KPropertiesDialogPrivate::insertPages()
{
    if (m_items.isEmpty()) {
        return;
    }

    if (KFilePropsPlugin::supports(m_items)) {
        KPropertiesDialogPlugin *p = new KFilePropsPlugin(q);
        q->insertPlugin(p);
    }

    if (KFilePermissionsPropsPlugin::supports(m_items)) {
        KPropertiesDialogPlugin *p = new KFilePermissionsPropsPlugin(q);
        q->insertPlugin(p);
    }

    if (KChecksumsPlugin::supports(m_items)) {
        KPropertiesDialogPlugin *p = new KChecksumsPlugin(q);
        q->insertPlugin(p);
    }

    if (KDesktopPropsPlugin::supports(m_items)) {
        KPropertiesDialogPlugin *p = new KDesktopPropsPlugin(q);
        q->insertPlugin(p);
    }

    if (KUrlPropsPlugin::supports(m_items)) {
        KPropertiesDialogPlugin *p = new KUrlPropsPlugin(q);
        q->insertPlugin(p);
    }

    if (KDevicePropsPlugin::supports(m_items)) {
        KPropertiesDialogPlugin *p = new KDevicePropsPlugin(q);
        q->insertPlugin(p);
    }

//     if ( KPreviewPropsPlugin::supports( m_items ) ) {
//         KPropertiesDialogPlugin *p = new KPreviewPropsPlugin(q);
//         q->insertPlugin(p);
//     }

    //plugins

    if (m_items.count() != 1) {
        return;
    }

    const KFileItem item = m_items.first();
    const QString mimetype = item.mimetype();

    if (mimetype.isEmpty()) {
        return;
    }

    QString query = QStringLiteral(
        "(((not exist [X-KDE-Protocol]) and (not exist [X-KDE-Protocols])) or ([X-KDE-Protocol] == '%1') or ('%1' in [X-KDE-Protocols]))"
    ).arg(item.url().scheme());

    // qDebug() << "trader query: " << query;
    const KService::List offers = KMimeTypeTrader::self()->query(mimetype, QStringLiteral("KPropertiesDialog/Plugin"), query);
    for (const KService::Ptr &ptr : offers) {
        KPropertiesDialogPlugin *plugin = ptr->createInstance<KPropertiesDialogPlugin>(q);
        if (!plugin) {
            continue;
        }
        plugin->setObjectName(ptr->name());

        q->insertPlugin(plugin);
    }
}

void KPropertiesDialog::updateUrl(const QUrl &_newUrl)
{
    Q_ASSERT(d->m_items.count() == 1);
    // qDebug() << "KPropertiesDialog::updateUrl (pre)" << _newUrl;
    QUrl newUrl = _newUrl;
    emit saveAs(d->m_singleUrl, newUrl);
    // qDebug() << "KPropertiesDialog::updateUrl (post)" << newUrl;

    d->m_singleUrl = newUrl;
    d->m_items.first().setUrl(newUrl);
    Q_ASSERT(!d->m_singleUrl.isEmpty());
    // If we have an Desktop page, set it dirty, so that a full file is saved locally
    // Same for a URL page (because of the Name= hack)
    for (KPropertiesDialogPlugin *it : qAsConst(d->m_pageList)) {
        if (qobject_cast<KUrlPropsPlugin *>(it) ||
                qobject_cast<KDesktopPropsPlugin *>(it)) {
            //qDebug() << "Setting page dirty";
            it->setDirty();
            break;
        }
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
        newUrl.setPath(concatPaths(newUrl.path(), _name));
    } else {
        // It's a directory, so strip the trailing slash first
        newUrl = d->m_singleUrl.adjusted(QUrl::StripTrailingSlash);
        // Now change the filename
        newUrl = newUrl.adjusted(QUrl::RemoveFilename); // keep trailing slash
        newUrl.setPath(concatPaths(newUrl.path(), _name));
    }
    updateUrl(newUrl);
}

void KPropertiesDialog::abortApplying()
{
    d->m_aborted = true;
}

class Q_DECL_HIDDEN KPropertiesDialogPlugin::KPropertiesDialogPluginPrivate
{
public:
    KPropertiesDialogPluginPrivate()
    {
    }
    ~KPropertiesDialogPluginPrivate()
    {
    }

    bool m_bDirty;
    int fontHeight;
};

KPropertiesDialogPlugin::KPropertiesDialogPlugin(KPropertiesDialog *_props)
    : QObject(_props), d(new KPropertiesDialogPluginPrivate)
{
    properties = _props;
    d->fontHeight = 2 * properties->fontMetrics().height();
    d->m_bDirty = false;
}

KPropertiesDialogPlugin::~KPropertiesDialogPlugin()
{
    delete d;
}

#if KIOWIDGETS_BUILD_DEPRECATED_SINCE(4, 1)
bool KPropertiesDialogPlugin::isDesktopFile(const KFileItem &_item)
{
    return _item.isDesktopFile();
}
#endif

void KPropertiesDialogPlugin::setDirty(bool b)
{
    d->m_bDirty = b;
}

void KPropertiesDialogPlugin::setDirty()
{
    d->m_bDirty = true;
}

bool KPropertiesDialogPlugin::isDirty() const
{
    return d->m_bDirty;
}

void KPropertiesDialogPlugin::applyChanges()
{
    qCWarning(KIO_WIDGETS) << "applyChanges() not implemented in page !";
}

int KPropertiesDialogPlugin::fontHeight() const
{
    return d->fontHeight;
}

///////////////////////////////////////////////////////////////////////////////

class KFilePropsPlugin::KFilePropsPluginPrivate
{
public:
    KFilePropsPluginPrivate()
    {
        dirSizeJob = nullptr;
        dirSizeUpdateTimer = nullptr;
        m_lined = nullptr;
        m_capacityBar = nullptr;
        m_linkTargetLineEdit = nullptr;
    }
    ~KFilePropsPluginPrivate()
    {
        if (dirSizeJob) {
            dirSizeJob->kill();
        }
    }

    KIO::DirectorySizeJob *dirSizeJob;
    QTimer *dirSizeUpdateTimer;
    QFrame *m_frame;
    bool bMultiple;
    bool bIconChanged;
    bool bKDesktopMode;
    bool bDesktopFile;
    KCapacityBar *m_capacityBar;
    QString mimeType;
    QString oldFileName;
    KLineEdit *m_lined;
    QLabel *m_fileNameLabel = nullptr;
    QGridLayout *m_grid = nullptr;

    QWidget *iconArea;

    QLabel *m_sizeLabel;

    QPushButton *m_sizeDetermineButton;
    QPushButton *m_sizeStopButton;
    QPushButton *m_sizeDetailsButton;

    KLineEdit *m_linkTargetLineEdit;

    QString m_sRelativePath;
    bool m_bFromTemplate;

    /**
    * The initial filename
    */
    QString oldName;
};

KFilePropsPlugin::KFilePropsPlugin(KPropertiesDialog *_props)
    : KPropertiesDialogPlugin(_props), d(new KFilePropsPluginPrivate)
{
    d->bMultiple = (properties->items().count() > 1);
    d->bIconChanged = false;
    d->bDesktopFile = KDesktopPropsPlugin::supports(properties->items());
    // qDebug() << "KFilePropsPlugin::KFilePropsPlugin bMultiple=" << d->bMultiple;

    // We set this data from the first item, and we'll
    // check that the other items match against it, resetting when not.
    bool isLocal;
    const KFileItem item = properties->item();
    QUrl url = item.mostLocalUrl(&isLocal);
    bool isReallyLocal = item.url().isLocalFile();
    bool bDesktopFile = item.isDesktopFile();
    mode_t mode = item.mode();
    bool hasDirs = item.isDir() && !item.isLink();
    bool hasRoot = url.path() == QLatin1String("/");
    QString iconStr = item.iconName();
    QString directory = properties->url().adjusted(QUrl::RemoveFilename | QUrl::StripTrailingSlash).path();
    QString protocol = properties->url().scheme();
    d->bKDesktopMode = protocol == QLatin1String("desktop") ||
                       properties->currentDir().scheme() == QLatin1String("desktop");
    QString mimeComment = item.mimeComment();
    d->mimeType = item.mimetype();
    KIO::filesize_t totalSize = item.size();
    QString magicMimeComment;
    QMimeDatabase db;
    if (isLocal) {
        QMimeType magicMimeType = db.mimeTypeForFile(url.toLocalFile(), QMimeDatabase::MatchContent);
        if (magicMimeType.isValid() && !magicMimeType.isDefault()) {
            magicMimeComment = magicMimeType.comment();
        }
    }
#ifdef Q_OS_WIN
    if (isReallyLocal) {
        directory = QDir::toNativeSeparators(directory.mid(1));
    }
#endif

    // Those things only apply to 'single file' mode
    QString filename;
    bool isTrash = false;
    d->m_bFromTemplate = false;

    // And those only to 'multiple' mode
    uint iDirCount = hasDirs ? 1 : 0;
    uint iFileCount = 1 - iDirCount;

    d->m_frame = new QFrame();
    properties->addPage(d->m_frame, i18nc("@title:tab File properties", "&General"));

    QVBoxLayout *vbl = new QVBoxLayout(d->m_frame);
    vbl->setContentsMargins(0, 0, 0, 0);
    vbl->setObjectName(QStringLiteral("vbl"));
    QGridLayout *grid = new QGridLayout(); // unknown rows
    d->m_grid = grid;
    grid->setColumnStretch(0, 0);
    grid->setColumnStretch(1, 0);
    grid->setColumnStretch(2, 1);
    const int horizontalSpacing = d->m_frame->style()->pixelMetric(QStyle::PM_LayoutHorizontalSpacing);
    grid->addItem(new QSpacerItem(horizontalSpacing, 0), 0, 1);
    vbl->addLayout(grid);
    int curRow = 0;

    if (!d->bMultiple) {
        QString path;
        if (!d->m_bFromTemplate) {
            isTrash = (properties->url().scheme() == QLatin1String("trash"));
            // Extract the full name, but without file: for local files
            path = properties->url().toDisplayString(QUrl::PreferLocalFile);
        } else {
            path = concatPaths(properties->currentDir().path(), properties->defaultName());
            directory = properties->currentDir().toDisplayString(QUrl::PreferLocalFile);
        }

        if (d->bDesktopFile) {
            determineRelativePath(path);
        }

        // Extract the file name only
        filename = properties->defaultName();
        if (filename.isEmpty()) {   // no template
            const QFileInfo finfo(item.name());  // this gives support for UDS_NAME, e.g. for kio_trash or kio_system
            filename = finfo.fileName(); // Make sure only the file's name is displayed (#160964).
        } else {
            d->m_bFromTemplate = true;
            setDirty(); // to enforce that the copy happens
        }
        d->oldFileName = filename;

        // Make it human-readable
        filename = nameFromFileName(filename);

        if (d->bKDesktopMode && d->bDesktopFile) {
            KDesktopFile config(url.toLocalFile());
            if (config.desktopGroup().hasKey("Name")) {
                filename = config.readName();
            }
        }

        d->oldName = filename;
    } else {
        // Multiple items: see what they have in common
        const KFileItemList items = properties->items();
        KFileItemList::const_iterator kit = items.begin();
        const KFileItemList::const_iterator kend = items.end();
        for (++kit /*no need to check the first one again*/; kit != kend; ++kit) {
            const QUrl url = (*kit).url();
            // qDebug() << "KFilePropsPlugin::KFilePropsPlugin " << url.toDisplayString();
            // The list of things we check here should match the variables defined
            // at the beginning of this method.
            if (url.isLocalFile() != isLocal) {
                isLocal = false;    // not all local
            }
            if (bDesktopFile && (*kit).isDesktopFile() != bDesktopFile) {
                bDesktopFile = false;    // not all desktop files
            }
            if ((*kit).mode() != mode) {
                mode = (mode_t)0;
            }
            if (KIO::iconNameForUrl(url) != iconStr) {
                iconStr = QStringLiteral("document-multiple");
            }
            if (url.adjusted(QUrl::RemoveFilename | QUrl::StripTrailingSlash).path() != directory) {
                directory.clear();
            }
            if (url.scheme() != protocol) {
                protocol.clear();
            }
            if (!mimeComment.isNull() && (*kit).mimeComment() != mimeComment) {
                mimeComment.clear();
            }
            if (isLocal && !magicMimeComment.isNull()) {
                QMimeType magicMimeType = db.mimeTypeForFile(url.toLocalFile(), QMimeDatabase::MatchContent);
                if (magicMimeType.isValid() && magicMimeType.comment() != magicMimeComment) {
                    magicMimeComment.clear();
                }
            }

            if (isLocal && url.path() == QLatin1String("/")) {
                hasRoot = true;
            }
            if ((*kit).isDir() && !(*kit).isLink()) {
                iDirCount++;
                hasDirs = true;
            } else {
                iFileCount++;
                totalSize += (*kit).size();
            }
        }
    }

    if (!isReallyLocal && !protocol.isEmpty()) {
        directory += QLatin1String(" (") + protocol + QLatin1Char(')');
    }

    if (!isTrash && (bDesktopFile || ((mode & QT_STAT_MASK) == QT_STAT_DIR))
            && !d->bMultiple // not implemented for multiple
            && enableIconButton()) { // #56857
        KIconButton *iconButton = new KIconButton(d->m_frame);
        int bsize = 66 + 2 * iconButton->style()->pixelMetric(QStyle::PM_ButtonMargin);
        iconButton->setFixedSize(bsize, bsize);
        iconButton->setIconSize(48);
        iconButton->setStrictIconSize(false);
        if (bDesktopFile && isLocal) {
            const KDesktopFile config(url.toLocalFile());
            if (config.hasDeviceType()) {
                iconButton->setIconType(KIconLoader::Desktop, KIconLoader::Device);
            } else {
                iconButton->setIconType(KIconLoader::Desktop, KIconLoader::Application);
            }
        } else {
            iconButton->setIconType(KIconLoader::Desktop, KIconLoader::Place);
        }
        iconButton->setIcon(iconStr);
        d->iconArea = iconButton;
        connect(iconButton, &KIconButton::iconChanged,
                this, &KFilePropsPlugin::slotIconChanged);
    } else {
        QLabel *iconLabel = new QLabel(d->m_frame);
        iconLabel->setAlignment(Qt::AlignCenter);
        int bsize = 66 + 2 * iconLabel->style()->pixelMetric(QStyle::PM_ButtonMargin);
        iconLabel->setFixedSize(bsize, bsize);
        iconLabel->setPixmap(QIcon::fromTheme(iconStr).pixmap(48));
        d->iconArea = iconLabel;
    }
    grid->addWidget(d->iconArea, curRow, 0, Qt::AlignCenter);

    KFileItemListProperties itemList(KFileItemList() << item);
    if (d->bMultiple || isTrash || hasRoot || !(d->m_bFromTemplate || itemList.supportsMoving())) {
        setFileNameReadOnly(true);
        if (d->bMultiple) {
            d->m_fileNameLabel->setText(KIO::itemsSummaryString(iFileCount + iDirCount, iFileCount, iDirCount, 0, false));
        }

    } else {
        d->m_lined = new KLineEdit(d->m_frame);
        d->m_lined->setObjectName(QStringLiteral("KFilePropsPlugin::nameLineEdit"));
        d->m_lined->setText(filename);
        d->m_lined->setFocus();

        // Enhanced rename: Don't highlight the file extension.
        QString extension = db.suffixForFileName(filename);
        if (!extension.isEmpty()) {
            d->m_lined->setSelection(0, filename.length() - extension.length() - 1);
        } else {
            int lastDot = filename.lastIndexOf(QLatin1Char('.'));
            if (lastDot > 0) {
                d->m_lined->setSelection(0, lastDot);
            }
        }

        connect(d->m_lined, &QLineEdit::textChanged,
                this, &KFilePropsPlugin::nameFileChanged);
        grid->addWidget(d->m_lined, curRow, 2);
    }
    ++curRow;

    KSeparator *sep = new KSeparator(Qt::Horizontal, d->m_frame);
    grid->addWidget(sep, curRow, 0, 1, 3);
    ++curRow;

    QLabel *l;
    if (!mimeComment.isEmpty() && !isTrash) {
        l = new QLabel(i18n("Type:"), d->m_frame);
        grid->addWidget(l, curRow, 0, Qt::AlignRight | Qt::AlignTop);

        QFrame *box = new QFrame(d->m_frame);
        QVBoxLayout *boxLayout = new QVBoxLayout(box);
        boxLayout->setSpacing(2); // without that spacing the button literally “sticks” to the label ;)
        boxLayout->setContentsMargins(0, 0, 0, 0);

        l = new QLabel(mimeComment, box);
        l->setTextInteractionFlags(Qt::TextSelectableByMouse | Qt::TextSelectableByKeyboard);
        grid->addWidget(box, curRow++, 2);

        QPushButton *button = new QPushButton(box);
        button->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);  // Minimum still makes the button grow to the entire layout width
        button->setIcon(QIcon::fromTheme(QStringLiteral("configure")));

        boxLayout->addWidget(l);
        boxLayout->addWidget(button);

        if (d->mimeType == QLatin1String("application/octet-stream")) {
            button->setText(i18n("Create New File Type"));
        } else {
            button->setText(i18n("File Type Options"));
        }

        connect(button, &QAbstractButton::clicked, this, &KFilePropsPlugin::slotEditFileType);

        if (!KAuthorized::authorizeAction(QStringLiteral("editfiletype"))) {
            button->hide();
        }
    }

    if (!magicMimeComment.isEmpty() && magicMimeComment != mimeComment) {
        l = new QLabel(i18n("Contents:"), d->m_frame);
        grid->addWidget(l, curRow, 0, Qt::AlignRight);

        l = new QLabel(magicMimeComment, d->m_frame);
        grid->addWidget(l, curRow++, 2);
    }

    if (!directory.isEmpty()) {
        l = new QLabel(i18n("Location:"), d->m_frame);
        grid->addWidget(l, curRow, 0, Qt::AlignRight);

        l = new KSqueezedTextLabel(directory, d->m_frame);
        // force the layout direction to be always LTR
        l->setLayoutDirection(Qt::LeftToRight);
        // but if we are in RTL mode, align the text to the right
        // otherwise the text is on the wrong side of the dialog
        if (properties->layoutDirection() == Qt::RightToLeft) {
            l->setAlignment(Qt::AlignRight);
        }
        l->setTextInteractionFlags(Qt::TextSelectableByMouse | Qt::TextSelectableByKeyboard);
        grid->addWidget(l, curRow++, 2);
    }

    l = new QLabel(i18n("Size:"), d->m_frame);
    grid->addWidget(l, curRow, 0, Qt::AlignRight | Qt::AlignTop);

    d->m_sizeLabel = new QLabel(d->m_frame);
    d->m_sizeLabel->setTextInteractionFlags(Qt::TextSelectableByMouse | Qt::TextSelectableByKeyboard);
    grid->addWidget(d->m_sizeLabel, curRow++, 2);

    if (!hasDirs) { // Only files [and symlinks]
        d->m_sizeLabel->setText(QStringLiteral("%1 (%2)").arg(KIO::convertSize(totalSize),
           QLocale().toString(totalSize)));
        d->m_sizeDetermineButton = nullptr;
        d->m_sizeStopButton = nullptr;
        d->m_sizeDetailsButton = nullptr;
    } else { // Directory
        QHBoxLayout *sizelay = new QHBoxLayout();
        grid->addLayout(sizelay, curRow++, 2);

        // buttons
        d->m_sizeDetermineButton = new QPushButton(i18n("Calculate"), d->m_frame);
        d->m_sizeStopButton = new QPushButton(i18n("Stop"), d->m_frame);

        d->m_sizeDetermineButton->setIcon(QIcon::fromTheme(QStringLiteral("view-refresh")));
        d->m_sizeStopButton->setIcon(QIcon::fromTheme(QStringLiteral("dialog-cancel")));

        connect(d->m_sizeDetermineButton, &QAbstractButton::clicked, this, &KFilePropsPlugin::slotSizeDetermine);
        connect(d->m_sizeStopButton, &QAbstractButton::clicked, this, &KFilePropsPlugin::slotSizeStop);

        sizelay->addWidget(d->m_sizeDetermineButton, 0);
        sizelay->addWidget(d->m_sizeStopButton, 0);

        if (KService::serviceByDesktopName(QStringLiteral("org.kde.filelight"))) {
            d->m_sizeDetailsButton = new QPushButton(i18n("Explore in Filelight"), d->m_frame);
            d->m_sizeDetailsButton->setIcon(QIcon::fromTheme(QStringLiteral("filelight")));
            connect(d->m_sizeDetailsButton, &QPushButton::clicked, this, &KFilePropsPlugin::slotSizeDetails);
            sizelay->addWidget(d->m_sizeDetailsButton, 0);
        }

        sizelay->addStretch(10); // so that the buttons don't grow horizontally

        // auto-launch for local dirs only, and not for '/'
        if (isLocal && !hasRoot) {
            d->m_sizeDetermineButton->setText(i18n("Refresh"));
            slotSizeDetermine();
        } else {
            d->m_sizeStopButton->setEnabled(false);
        }
    }

    if (!d->bMultiple && item.isLink()) {
        l = new QLabel(i18n("Points to:"), d->m_frame);
        grid->addWidget(l, curRow, 0, Qt::AlignRight);

        d->m_linkTargetLineEdit = new KLineEdit(item.linkDest(), d->m_frame);
        connect(d->m_linkTargetLineEdit, &QLineEdit::textChanged,
                this, QOverload<>::of(&KFilePropsPlugin::setDirty));

        QPushButton *goThereButton = new QPushButton(d->m_frame);
        goThereButton->setIcon(QIcon::fromTheme(QStringLiteral("go-jump")));

        QHBoxLayout *row = new QHBoxLayout();
        row->setContentsMargins(0, 0, 0, 0);
        row->addWidget(d->m_linkTargetLineEdit);
        row->addWidget(goThereButton);
        grid->addLayout(row, curRow++, 2);

        KMessageWidget* messageWidget = new KMessageWidget(d->m_frame);
        messageWidget->setWordWrap(true);
        messageWidget->setMessageType(KMessageWidget::Error);
        messageWidget->setText(i18n("Invalid link target"));
        messageWidget->hide();
        grid->addWidget(messageWidget, curRow++, 0, 1, -1);

        connect(goThereButton, &QPushButton::clicked,
                this, [=]() {
                    const QUrl targetLocation = QUrl::fromLocalFile(d->m_linkTargetLineEdit->text());
                    KIO::StatJob *statJob = KIO::stat(targetLocation, KIO::HideProgressInfo);
                    bool ok = statJob->exec();
                    if (ok) {
                        KIO::highlightInFileManager({targetLocation});
                        _props->close();
                        return;
                    }
                    // Show error message if the link destination doesn't exist
                    messageWidget->animatedShow();
                });
    }

    if (!d->bMultiple) { // Dates and extra fields for multiple don't make much sense...
        QLocale locale;
        const auto extraFields = KProtocolInfo::extraFields(url);
        for (int i = 0; i < extraFields.count(); ++i) {
            const auto &field = extraFields.at(i);

            QString label = item.entry().stringValue(KIO::UDSEntry::UDS_EXTRA + i);
            if (field.type == KProtocolInfo::ExtraField::Invalid || label.isEmpty()) {
                continue;
            }

            if (field.type == KProtocolInfo::ExtraField::DateTime) {
                const QDateTime date = QDateTime::fromString(label, Qt::ISODate);
                if (!date.isValid()) {
                    continue;
                }

                label = locale.toString(date, QLocale::LongFormat);
            }

            l = new QLabel(i18n("%1:", field.name), d->m_frame);
            grid->addWidget(l, curRow, 0, Qt::AlignRight);

            l = new KSqueezedTextLabel(label, d->m_frame);
            if (properties->layoutDirection() == Qt::RightToLeft) {
                l->setAlignment(Qt::AlignRight);
            } else {
                l->setLayoutDirection(Qt::LeftToRight);
            }
            grid->addWidget(l, curRow++, 2);
            l->setTextInteractionFlags(Qt::TextSelectableByMouse | Qt::TextSelectableByKeyboard);
        }

        QDateTime dt = item.time(KFileItem::CreationTime);
        if (!dt.isNull()) {
            l = new QLabel(i18n("Created:"), d->m_frame);
            grid->addWidget(l, curRow, 0, Qt::AlignRight);

            l = new QLabel(locale.toString(dt, QLocale::LongFormat), d->m_frame);
            grid->addWidget(l, curRow++, 2);
            l->setTextInteractionFlags(Qt::TextSelectableByMouse | Qt::TextSelectableByKeyboard);
        }

        dt = item.time(KFileItem::ModificationTime);
        if (!dt.isNull()) {
            l = new QLabel(i18n("Modified:"), d->m_frame);
            grid->addWidget(l, curRow, 0, Qt::AlignRight);

            l = new QLabel(locale.toString(dt, QLocale::LongFormat), d->m_frame);
            l->setTextInteractionFlags(Qt::TextSelectableByMouse | Qt::TextSelectableByKeyboard);
            grid->addWidget(l, curRow++, 2);
        }

        dt = item.time(KFileItem::AccessTime);
        if (!dt.isNull()) {
            l = new QLabel(i18n("Accessed:"), d->m_frame);
            grid->addWidget(l, curRow, 0, Qt::AlignRight);

            l = new QLabel(locale.toString(dt, QLocale::LongFormat), d->m_frame);
            l->setTextInteractionFlags(Qt::TextSelectableByMouse | Qt::TextSelectableByKeyboard);
            grid->addWidget(l, curRow++, 2);
        }
    }

    if (hasDirs) {  // only for directories
        sep = new KSeparator(Qt::Horizontal, d->m_frame);
        grid->addWidget(sep, curRow, 0, 1, 3);
        ++curRow;

        if (isLocal) {
            KMountPoint::Ptr mp = KMountPoint::currentMountPoints().findByPath(url.toLocalFile());

            if (mp) {
                l = new QLabel(i18n("File System:"), d->m_frame);
                grid->addWidget(l, curRow, 0, Qt::AlignRight);

                l = new QLabel(d->m_frame);
                grid->addWidget(l, curRow++, 2);
                l->setTextInteractionFlags(Qt::TextSelectableByMouse | Qt::TextSelectableByKeyboard);
                l->setText(mp->mountType());

                l = new QLabel(i18n("Mounted on:"), d->m_frame);
                grid->addWidget(l, curRow, 0, Qt::AlignRight);

                l = new KSqueezedTextLabel(mp->mountPoint(), d->m_frame);
                l->setTextInteractionFlags(Qt::TextSelectableByMouse | Qt::TextSelectableByKeyboard);
                grid->addWidget(l, curRow++, 2);

                l = new QLabel(i18n("Mounted from:"), d->m_frame);
                grid->addWidget(l, curRow, 0, Qt::AlignRight);

                l = new QLabel(mp->mountedFrom(), d->m_frame);
                grid->addWidget(l, curRow++, 2);
                l->setTextInteractionFlags(Qt::TextSelectableByMouse | Qt::TextSelectableByKeyboard);
            }
        }

        l = new QLabel(i18nc("Amount of used and available space on this device or partition", "Free space:"), d->m_frame);
        grid->addWidget(l, curRow, 0, Qt::AlignRight);

        d->m_capacityBar = new KCapacityBar(KCapacityBar::DrawTextOutline, d->m_frame);
        d->m_capacityBar->setText(i18nc("@info:status", "Unknown size"));
        grid->addWidget(d->m_capacityBar, curRow++, 2);

        KIO::FileSystemFreeSpaceJob *job = KIO::fileSystemFreeSpace(url);
        connect(job, &KIO::FileSystemFreeSpaceJob::result, this, &KFilePropsPlugin::slotFreeSpaceResult);
    }

    vbl->addStretch(1);
}

bool KFilePropsPlugin::enableIconButton() const
{
    const KFileItem item = properties->item();
    // If the current item is a directory, check if it's writable,
    // so we can create/update a .directory
    // Current item is a file, same thing: check if it is writable
    if (item.isWritable()) {
        // exclude remote dirs as changing the icon has no effect (bug 205954)
        if (item.isLocalFile() || item.url().scheme() == QLatin1String("desktop")) {
            return true;
        }
    }

    return false;
}

// QString KFilePropsPlugin::tabName () const
// {
//   return i18n ("&General");
// }

void KFilePropsPlugin::setFileNameReadOnly(bool ro)
{
    Q_ASSERT(ro); // false isn't supported
    if (ro && !d->m_fileNameLabel) {
        Q_ASSERT(!d->m_bFromTemplate);
        delete d->m_lined;
        d->m_lined = nullptr;
        d->m_fileNameLabel = new QLabel(d->m_frame);
        d->m_fileNameLabel->setTextInteractionFlags(Qt::TextSelectableByMouse | Qt::TextSelectableByKeyboard);
        d->m_fileNameLabel->setText(d->oldName); // will get overwritten if d->bMultiple
        d->m_grid->addWidget(d->m_fileNameLabel, 0, 2);
    }
}

void KFilePropsPlugin::slotEditFileType()
{
    QString mime;
    if (d->mimeType == QLatin1String("application/octet-stream")) {
        const int pos = d->oldFileName.lastIndexOf(QLatin1Char('.'));
        if (pos != -1) {
            mime = QLatin1Char('*') + d->oldFileName.midRef(pos);
        } else {
            mime = QStringLiteral("*");
        }
    }  else {
        mime = d->mimeType;
    }
    KMimeTypeEditor::editMimeType(mime, properties->window());
}

void KFilePropsPlugin::slotIconChanged()
{
    d->bIconChanged = true;
    emit changed();
}

void KFilePropsPlugin::nameFileChanged(const QString &text)
{
    properties->buttonBox()->button(QDialogButtonBox::Ok)->setEnabled(!text.isEmpty());
    emit changed();
}

static QString relativeAppsLocation(const QString &file)
{
    const QString canonical = QFileInfo(file).canonicalFilePath();
    const QStringList dirs = QStandardPaths::standardLocations(QStandardPaths::ApplicationsLocation);
    for (const QString &base : dirs) {
        QDir base_dir = QDir(base);
        if (base_dir.exists() && canonical.startsWith(base_dir.canonicalPath())) {
            return canonical.mid(base.length() + 1);
        }
    }
    return QString(); // return empty if the file is not in apps
}

void KFilePropsPlugin::determineRelativePath(const QString &path)
{
    // now let's make it relative
    d->m_sRelativePath = relativeAppsLocation(path);
}

void KFilePropsPlugin::slotFreeSpaceResult(KIO::Job *job, KIO::filesize_t size, KIO::filesize_t available)
{
    if (!job->error()) {
        const quint64 used = size - available;
        const int percentUsed = qRound(100.0 * qreal(used) / qreal(size));

        d->m_capacityBar->setText(
        i18nc("Available space out of total partition size (percent used)", "%1 free of %2 (%3% used)",
              KIO::convertSize(available),
              KIO::convertSize(size),
              percentUsed));

        d->m_capacityBar->setValue(percentUsed);
    } else {
        d->m_capacityBar->setText(i18nc("@info:status", "Unknown size"));
        d->m_capacityBar->setValue(0);
    }
}

void KFilePropsPlugin::slotDirSizeUpdate()
{
    KIO::filesize_t totalSize = d->dirSizeJob->totalSize();
    KIO::filesize_t totalFiles = d->dirSizeJob->totalFiles();
    KIO::filesize_t totalSubdirs = d->dirSizeJob->totalSubdirs();
    d->m_sizeLabel->setText(
        i18n("Calculating... %1 (%2)\n%3, %4",
             KIO::convertSize(totalSize),
             QLocale().toString(totalSize),
             i18np("1 file", "%1 files", totalFiles),
             i18np("1 sub-folder", "%1 sub-folders", totalSubdirs)));
}

void KFilePropsPlugin::slotDirSizeFinished(KJob *job)
{
    if (job->error()) {
        d->m_sizeLabel->setText(job->errorString());
    } else {
        KIO::filesize_t totalSize = d->dirSizeJob->totalSize();
        KIO::filesize_t totalFiles = d->dirSizeJob->totalFiles();
        KIO::filesize_t totalSubdirs = d->dirSizeJob->totalSubdirs();
        d->m_sizeLabel->setText(QStringLiteral("%1 (%2)\n%3, %4")
                                .arg(KIO::convertSize(totalSize),
                                QLocale().toString(totalSize),
                                i18np("1 file", "%1 files", totalFiles),
                                i18np("1 sub-folder", "%1 sub-folders", totalSubdirs)));
    }
    d->m_sizeStopButton->setEnabled(false);
    // just in case you change something and try again :)
    d->m_sizeDetermineButton->setText(i18n("Refresh"));
    d->m_sizeDetermineButton->setEnabled(true);
    d->dirSizeJob = nullptr;
    delete d->dirSizeUpdateTimer;
    d->dirSizeUpdateTimer = nullptr;
}

void KFilePropsPlugin::slotSizeDetermine()
{
    d->m_sizeLabel->setText(i18n("Calculating..."));
    // qDebug() << "properties->item()=" << properties->item() << "URL=" << properties->item().url();

    d->dirSizeJob = KIO::directorySize(properties->items());
    d->dirSizeUpdateTimer = new QTimer(this);
    connect(d->dirSizeUpdateTimer, &QTimer::timeout,
            this, &KFilePropsPlugin::slotDirSizeUpdate);
    d->dirSizeUpdateTimer->start(500);
    connect(d->dirSizeJob, &KJob::result,
            this, &KFilePropsPlugin::slotDirSizeFinished);
    d->m_sizeStopButton->setEnabled(true);
    d->m_sizeDetermineButton->setEnabled(false);

    // also update the "Free disk space" display
    if (d->m_capacityBar) {
        const KFileItem item = properties->item();
        KIO::FileSystemFreeSpaceJob *job = KIO::fileSystemFreeSpace(item.url());
        connect(job, &KIO::FileSystemFreeSpaceJob::result, this, &KFilePropsPlugin::slotFreeSpaceResult);
    }
}

void KFilePropsPlugin::slotSizeStop()
{
    if (d->dirSizeJob) {
        KIO::filesize_t totalSize = d->dirSizeJob->totalSize();
        d->m_sizeLabel->setText(i18n("At least %1",
                                     KIO::convertSize(totalSize)));
        d->dirSizeJob->kill();
        d->dirSizeJob = nullptr;
    }
    if (d->dirSizeUpdateTimer) {
        d->dirSizeUpdateTimer->stop();
    }

    d->m_sizeStopButton->setEnabled(false);
    d->m_sizeDetermineButton->setEnabled(true);
}

void KFilePropsPlugin::slotSizeDetails()
{
    // Open the current folder in filelight
    KService::Ptr service = KService::serviceByDesktopName(QStringLiteral("org.kde.filelight"));
    if (service) {
        auto *job = new KIO::ApplicationLauncherJob(service);
        job->setUrls({ properties->url() });
        job->setUiDelegate(new KDialogJobUiDelegate(KJobUiDelegate::AutoHandlingEnabled, properties));
        job->start();
    }
}

KFilePropsPlugin::~KFilePropsPlugin()
{
    delete d;
}

bool KFilePropsPlugin::supports(const KFileItemList & /*_items*/)
{
    return true;
}

void KFilePropsPlugin::applyChanges()
{
    if (d->dirSizeJob) {
        slotSizeStop();
    }

    // qDebug() << "KFilePropsPlugin::applyChanges";

    if (d->m_lined) {
        QString n = d->m_lined->text();
        // Remove trailing spaces (#4345)
        while (! n.isEmpty() && n[n.length() - 1].isSpace()) {
            n.chop(1);
        }
        if (n.isEmpty()) {
            KMessageBox::sorry(properties, i18n("The new file name is empty."));
            properties->abortApplying();
            return;
        }

        // Do we need to rename the file ?
        // qDebug() << "oldname = " << d->oldName;
        // qDebug() << "newname = " << n;
        if (d->oldName != n || d->m_bFromTemplate) {   // true for any from-template file
            KIO::CopyJob *job = nullptr;
            QUrl oldurl = properties->url();

            QString newFileName = KIO::encodeFileName(n);
            if (d->bDesktopFile && !newFileName.endsWith(QLatin1String(".desktop")) &&
                    !newFileName.endsWith(QLatin1String(".kdelnk"))) {
                newFileName += QLatin1String(".desktop");
            }

            // Tell properties. Warning, this changes the result of properties->url() !
            properties->rename(newFileName);

            // Update also relative path (for apps)
            if (!d->m_sRelativePath.isEmpty()) {
                determineRelativePath(properties->url().toLocalFile());
            }

            // qDebug() << "New URL = " << properties->url();
            // qDebug() << "old = " << oldurl.url();

            // Don't remove the template !!
            if (!d->m_bFromTemplate) { // (normal renaming)
                job = KIO::moveAs(oldurl, properties->url());
            } else { // Copying a template
                job = KIO::copyAs(oldurl, properties->url());
            }

            connect(job, &KJob::result,
                    this, &KFilePropsPlugin::slotCopyFinished);
            connect(job, &KIO::CopyJob::renamed,
                    this, &KFilePropsPlugin::slotFileRenamed);
            // wait for job
            QEventLoop eventLoop;
            connect(this, &KFilePropsPlugin::leaveModality,
                    &eventLoop, &QEventLoop::quit);
            eventLoop.exec(QEventLoop::ExcludeUserInputEvents);
            return;
        }
        properties->updateUrl(properties->url());
        // Update also relative path (for apps)
        if (!d->m_sRelativePath.isEmpty()) {
            determineRelativePath(properties->url().toLocalFile());
        }
    }

    // No job, keep going
    slotCopyFinished(nullptr);
}

void KFilePropsPlugin::slotCopyFinished(KJob *job)
{
    // qDebug() << "KFilePropsPlugin::slotCopyFinished";
    if (job) {
        // allow apply() to return
        emit leaveModality();
        if (job->error()) {
            job->uiDelegate()->showErrorMessage();
            // Didn't work. Revert the URL to the old one
            properties->updateUrl(static_cast<KIO::CopyJob *>(job)->srcUrls().constFirst());
            properties->abortApplying(); // Don't apply the changes to the wrong file !
            return;
        }
    }

    Q_ASSERT(!properties->item().isNull());
    Q_ASSERT(!properties->item().url().isEmpty());

    // Save the file locally
    if (d->bDesktopFile && !d->m_sRelativePath.isEmpty()) {
        // qDebug() << "KFilePropsPlugin::slotCopyFinished " << d->m_sRelativePath;
        const QString newPath = QStandardPaths::writableLocation(QStandardPaths::ApplicationsLocation) + QLatin1Char('/') + d->m_sRelativePath;
        const QUrl newURL = QUrl::fromLocalFile(newPath);
        // qDebug() << "KFilePropsPlugin::slotCopyFinished path=" << newURL;
        properties->updateUrl(newURL);
    }

    if (d->bKDesktopMode && d->bDesktopFile) {
        // Renamed? Update Name field
        // Note: The desktop ioslave does this as well, but not when
        //       the file is copied from a template.
        if (d->m_bFromTemplate) {
            KIO::StatJob *job = KIO::stat(properties->url());
            job->exec();
            KIO::UDSEntry entry = job->statResult();

            KFileItem item(entry, properties->url());
            KDesktopFile config(item.localPath());
            KConfigGroup cg = config.desktopGroup();
            QString nameStr = nameFromFileName(properties->url().fileName());
            cg.writeEntry("Name", nameStr);
            cg.writeEntry("Name", nameStr, KConfigGroup::Persistent | KConfigGroup::Localized);
        }
    }

    if (d->m_linkTargetLineEdit && !d->bMultiple) {
        const KFileItem item = properties->item();
        const QString newTarget = d->m_linkTargetLineEdit->text();
        if (newTarget != item.linkDest()) {
            // qDebug() << "Updating target of symlink to" << newTarget;
            KIO::Job *job = KIO::symlink(newTarget, item.url(), KIO::Overwrite);
            job->uiDelegate()->setAutoErrorHandlingEnabled(true);
            job->exec();
        }
    }

    // "Link to Application" templates need to be made executable
    // Instead of matching against a filename we check if the destination
    // is an Application now.
    if (d->m_bFromTemplate) {
        // destination is not necessarily local, use the src template
        KDesktopFile templateResult(static_cast<KIO::CopyJob *>(job)->srcUrls().constFirst().toLocalFile());
        if (templateResult.hasApplicationType()) {
            // We can either stat the file and add the +x bit or use the larger chmod() job
            // with a umask designed to only touch u+x.  This is only one KIO job, so let's
            // do that.

            KFileItem appLink(properties->item());
            KFileItemList fileItemList;
            fileItemList << appLink;

            // first 0100 adds u+x, second 0100 only allows chmod to change u+x
            KIO::Job *chmodJob = KIO::chmod(fileItemList, 0100, 0100, QString(), QString(), KIO::HideProgressInfo);
            chmodJob->exec();
        }
    }
}

void KFilePropsPlugin::applyIconChanges()
{
    KIconButton *iconButton = qobject_cast<KIconButton *>(d->iconArea);
    if (!iconButton || !d->bIconChanged) {
        return;
    }
    // handle icon changes - only local files (or pseudo-local) for now
    // TODO: Use KTempFile and KIO::file_copy with overwrite = true
    QUrl url = properties->url();
    KIO::StatJob *job = KIO::mostLocalUrl(url);
    KJobWidgets::setWindow(job, properties);
    job->exec();
    url = job->mostLocalUrl();

    if (url.isLocalFile()) {
        QString path;

        if ((properties->item().mode() & QT_STAT_MASK) == QT_STAT_DIR) {
            path = url.toLocalFile() + QLatin1String("/.directory");
            // don't call updateUrl because the other tabs (i.e. permissions)
            // apply to the directory, not the .directory file.
        } else {
            path = url.toLocalFile();
        }

        // Get the default image
        QMimeDatabase db;
        QString str = db.mimeTypeForFile(url.toLocalFile(), QMimeDatabase::MatchExtension).iconName();
        // Is it another one than the default ?
        QString sIcon;
        if (str != iconButton->icon()) {
            sIcon = iconButton->icon();
        }
        // (otherwise write empty value)

        // qDebug() << "**" << path << "**";

        // If default icon and no .directory file -> don't create one
        if (!sIcon.isEmpty() || QFile::exists(path)) {
            KDesktopFile cfg(path);
            // qDebug() << "sIcon = " << (sIcon);
            // qDebug() << "str = " << (str);
            cfg.desktopGroup().writeEntry("Icon", sIcon);
            cfg.sync();

            cfg.reparseConfiguration();
            if (cfg.desktopGroup().readEntry("Icon") != sIcon) {
                KMessageBox::sorry(nullptr, i18n("<qt>Could not save properties. You do not "
                                           "have sufficient access to write to <b>%1</b>.</qt>", path));
            }
        }
    }
}

void KFilePropsPlugin::slotFileRenamed(KIO::Job *, const QUrl &, const QUrl &newUrl)
{
    // This is called in case of an existing local file during the copy/move operation,
    // if the user chooses Rename.
    properties->updateUrl(newUrl);
}

void KFilePropsPlugin::postApplyChanges()
{
    // Save the icon only after applying the permissions changes (#46192)
    applyIconChanges();

    const KFileItemList items = properties->items();
    const QList<QUrl> lst = items.urlList();
    org::kde::KDirNotify::emitFilesChanged(QList<QUrl>(lst));
}

class KFilePermissionsPropsPlugin::KFilePermissionsPropsPluginPrivate
{
public:
    KFilePermissionsPropsPluginPrivate()
    {
    }
    ~KFilePermissionsPropsPluginPrivate()
    {
    }

    QFrame *m_frame;
    QCheckBox *cbRecursive;
    QLabel *explanationLabel;
    KComboBox *ownerPermCombo, *groupPermCombo, *othersPermCombo;
    QCheckBox *extraCheckbox;
    mode_t partialPermissions;
    KFilePermissionsPropsPlugin::PermissionsMode pmode;
    bool canChangePermissions;
    bool isIrregular;
    bool hasExtendedACL;
    KACL extendedACL;
    KACL defaultACL;
    bool fileSystemSupportsACLs;

    KComboBox *grpCombo;

    KLineEdit *usrEdit;
    KLineEdit *grpEdit;

    // Old permissions
    mode_t permissions;
    // Old group
    QString strGroup;
    // Old owner
    QString strOwner;
};

#define UniOwner    (S_IRUSR|S_IWUSR|S_IXUSR)
#define UniGroup    (S_IRGRP|S_IWGRP|S_IXGRP)
#define UniOthers   (S_IROTH|S_IWOTH|S_IXOTH)
#define UniRead     (S_IRUSR|S_IRGRP|S_IROTH)
#define UniWrite    (S_IWUSR|S_IWGRP|S_IWOTH)
#define UniExec     (S_IXUSR|S_IXGRP|S_IXOTH)
#define UniSpecial  (S_ISUID|S_ISGID|S_ISVTX)

// synced with PermissionsTarget
const mode_t KFilePermissionsPropsPlugin::permissionsMasks[3] = {UniOwner, UniGroup, UniOthers};
const mode_t KFilePermissionsPropsPlugin::standardPermissions[4] = { 0, UniRead, UniRead | UniWrite, (mode_t) - 1 };

// synced with PermissionsMode and standardPermissions
const char *const KFilePermissionsPropsPlugin::permissionsTexts[4][4] = {
    {
        I18N_NOOP("No Access"),
        I18N_NOOP("Can Only View"),
        I18N_NOOP("Can View & Modify"),
        nullptr
    },
    {
        I18N_NOOP("No Access"),
        I18N_NOOP("Can Only View Content"),
        I18N_NOOP("Can View & Modify Content"),
        nullptr
    },
    { nullptr, nullptr, nullptr, nullptr}, // no texts for links
    {
        I18N_NOOP("No Access"),
        I18N_NOOP("Can Only View/Read Content"),
        I18N_NOOP("Can View/Read & Modify/Write"),
        nullptr
    }
};

KFilePermissionsPropsPlugin::KFilePermissionsPropsPlugin(KPropertiesDialog *_props)
    : KPropertiesDialogPlugin(_props), d(new KFilePermissionsPropsPluginPrivate)
{
    d->cbRecursive = nullptr;
    d->grpCombo = nullptr; d->grpEdit = nullptr;
    d->usrEdit = nullptr;
    bool isLocal = properties->url().isLocalFile();
    bool isTrash = (properties->url().scheme() == QLatin1String("trash"));
    KUser myself(KUser::UseEffectiveUID);
    const bool IamRoot = myself.isSuperUser();

    const KFileItem item = properties->item();
    bool isLink = item.isLink();
    bool isDir = item.isDir(); // all dirs
    bool hasDir = item.isDir(); // at least one dir
    d->permissions = item.permissions(); // common permissions to all files
    d->partialPermissions = d->permissions; // permissions that only some files have (at first we take everything)
    d->isIrregular = isIrregular(d->permissions, isDir, isLink);
    d->strOwner = item.user();
    d->strGroup = item.group();
    d->hasExtendedACL = item.ACL().isExtended() || item.defaultACL().isValid();
    d->extendedACL = item.ACL();
    d->defaultACL = item.defaultACL();
    d->fileSystemSupportsACLs = false;

    if (properties->items().count() > 1) {
        // Multiple items: see what they have in common
        const KFileItemList items = properties->items();
        KFileItemList::const_iterator it = items.begin();
        const KFileItemList::const_iterator kend = items.end();
        for (++it /*no need to check the first one again*/; it != kend; ++it) {
            if (!d->isIrregular)
                d->isIrregular |= isIrregular((*it).permissions(),
                                              (*it).isDir() == isDir,
                                              (*it).isLink() == isLink);
            d->hasExtendedACL = d->hasExtendedACL || (*it).hasExtendedACL();
            if ((*it).isLink() != isLink) {
                isLink = false;
            }
            if ((*it).isDir() != isDir) {
                isDir = false;
            }
            hasDir |= (*it).isDir();
            if ((*it).permissions() != d->permissions) {
                d->permissions &= (*it).permissions();
                d->partialPermissions |= (*it).permissions();
            }
            if ((*it).user() != d->strOwner) {
                d->strOwner.clear();
            }
            if ((*it).group() != d->strGroup) {
                d->strGroup.clear();
            }
        }
    }

    if (isLink) {
        d->pmode = PermissionsOnlyLinks;
    } else if (isDir) {
        d->pmode = PermissionsOnlyDirs;
    } else if (hasDir) {
        d->pmode = PermissionsMixed;
    } else {
        d->pmode = PermissionsOnlyFiles;
    }

    // keep only what's not in the common permissions
    d->partialPermissions = d->partialPermissions & ~d->permissions;

    bool isMyFile = false;

    if (isLocal && !d->strOwner.isEmpty()) { // local files, and all owned by the same person
        if (myself.isValid()) {
            isMyFile = (d->strOwner == myself.loginName());
        } else {
            qCWarning(KIO_WIDGETS) << "I don't exist ?! geteuid=" << KUserId::currentEffectiveUserId().toString();
        }
    } else {
        //We don't know, for remote files, if they are ours or not.
        //So we let the user change permissions, and
        //KIO::chmod will tell, if he had no right to do it.
        isMyFile = true;
    }

    d->canChangePermissions = (isMyFile || IamRoot) && (!isLink);

    // create GUI

    d->m_frame = new QFrame();
    properties->addPage(d->m_frame, i18n("&Permissions"));

    QBoxLayout *box = new QVBoxLayout(d->m_frame);
    box->setContentsMargins(0, 0, 0, 0);

    QWidget *l;
    QLabel *lbl;
    QGroupBox *gb;
    QGridLayout *gl;
    QPushButton *pbAdvancedPerm = nullptr;

    /* Group: Access Permissions */
    gb = new QGroupBox(i18n("Access Permissions"), d->m_frame);
    box->addWidget(gb);

    gl = new QGridLayout(gb);
    gl->setColumnStretch(1, 1);

    l = d->explanationLabel = new QLabel(gb);
    if (isLink)
        d->explanationLabel->setText(i18np("This file is a link and does not have permissions.",
                                           "All files are links and do not have permissions.",
                                           properties->items().count()));
    else if (!d->canChangePermissions) {
        d->explanationLabel->setText(i18n("Only the owner can change permissions."));
    }
    gl->addWidget(l, 0, 0, 1, 2);

    lbl = new QLabel(i18n("O&wner:"), gb);
    gl->addWidget(lbl, 1, 0, Qt::AlignRight);
    l = d->ownerPermCombo = new KComboBox(gb);
    lbl->setBuddy(l);
    gl->addWidget(l, 1, 1);
    connect(d->ownerPermCombo, QOverload<int>::of(&QComboBox::activated),
            this, &KPropertiesDialogPlugin::changed);
    l->setWhatsThis(i18n("Specifies the actions that the owner is allowed to do."));

    lbl = new QLabel(i18n("Gro&up:"), gb);
    gl->addWidget(lbl, 2, 0, Qt::AlignRight);
    l = d->groupPermCombo = new KComboBox(gb);
    lbl->setBuddy(l);
    gl->addWidget(l, 2, 1);
    connect(d->groupPermCombo, QOverload<int>::of(&QComboBox::activated),
            this, &KPropertiesDialogPlugin::changed);
    l->setWhatsThis(i18n("Specifies the actions that the members of the group are allowed to do."));

    lbl = new QLabel(i18n("O&thers:"), gb);
    gl->addWidget(lbl, 3, 0, Qt::AlignRight);
    l = d->othersPermCombo = new KComboBox(gb);
    lbl->setBuddy(l);
    gl->addWidget(l, 3, 1);
    connect(d->othersPermCombo, QOverload<int>::of(&QComboBox::activated),
            this, &KPropertiesDialogPlugin::changed);
    l->setWhatsThis(i18n("Specifies the actions that all users, who are neither "
                         "owner nor in the group, are allowed to do."));

    if (!isLink) {
        l = d->extraCheckbox = new QCheckBox(hasDir ?
                                             i18n("Only own&er can rename and delete folder content") :
                                             i18n("Is &executable"),
                                             gb);
        connect(d->extraCheckbox, &QAbstractButton::clicked, this, &KPropertiesDialogPlugin::changed);
        gl->addWidget(l, 4, 1);
        l->setWhatsThis(hasDir ? i18n("Enable this option to allow only the folder's owner to "
                                      "delete or rename the contained files and folders. Other "
                                      "users can only add new files, which requires the 'Modify "
                                      "Content' permission.")
                        : i18n("Enable this option to mark the file as executable. This only makes "
                               "sense for programs and scripts. It is required when you want to "
                               "execute them."));

        QLayoutItem *spacer = new QSpacerItem(0, 20, QSizePolicy::Minimum, QSizePolicy::Expanding);
        gl->addItem(spacer, 5, 0, 1, 3);

        pbAdvancedPerm = new QPushButton(i18n("A&dvanced Permissions"), gb);
        gl->addWidget(pbAdvancedPerm, 6, 0, 1, 2, Qt::AlignRight);
        connect(pbAdvancedPerm, &QAbstractButton::clicked,
                this, &KFilePermissionsPropsPlugin::slotShowAdvancedPermissions);
    } else {
        d->extraCheckbox = nullptr;
    }

    /**** Group: Ownership ****/
    gb = new QGroupBox(i18n("Ownership"), d->m_frame);
    box->addWidget(gb);

    gl = new QGridLayout(gb);
    gl->addItem(new QSpacerItem(0, 10), 0, 0);

    /*** Set Owner ***/
    l = new QLabel(i18n("User:"), gb);
    gl->addWidget(l, 1, 0, Qt::AlignRight);

    /* GJ: Don't autocomplete more than 1000 users. This is a kind of random
    * value. Huge sites having 10.000+ user have a fair chance of using NIS,
    * (possibly) making this unacceptably slow.
    * OTOH, it is nice to offer this functionality for the standard user.
    */
    int maxEntries = 1000;

    /* File owner: For root, offer a KLineEdit with autocompletion.
    * For a user, who can never chown() a file, offer a QLabel.
    */
    if (IamRoot && isLocal) {
        d->usrEdit = new KLineEdit(gb);
        KCompletion *kcom = d->usrEdit->completionObject();
        kcom->setOrder(KCompletion::Sorted);
        QStringList userNames = KUser::allUserNames(maxEntries);
        kcom->setItems(userNames);
        d->usrEdit->setCompletionMode((userNames.size() < maxEntries) ? KCompletion::CompletionAuto :
                                      KCompletion::CompletionNone);
        d->usrEdit->setText(d->strOwner);
        gl->addWidget(d->usrEdit, 1, 1);
        connect(d->usrEdit, &QLineEdit::textChanged,
                this, &KPropertiesDialogPlugin::changed);
    } else {
        l = new QLabel(d->strOwner, gb);
        gl->addWidget(l, 1, 1);
    }

    /*** Set Group ***/

    KUser user(KUser::UseEffectiveUID);
    QStringList groupList = user.groupNames();
    const bool isMyGroup = groupList.contains(d->strGroup);

    /* add the group the file currently belongs to ..
    * .. if it is not there already
    */
    if (!isMyGroup) {
        groupList += d->strGroup;
    }

    l = new QLabel(i18n("Group:"), gb);
    gl->addWidget(l, 2, 0, Qt::AlignRight);

    /* Set group: if possible to change:
    * - Offer a KLineEdit for root, since he can change to any group.
    * - Offer a KComboBox for a normal user, since he can change to a fixed
    *   (small) set of groups only.
    * If not changeable: offer a QLabel.
    */
    if (IamRoot && isLocal) {
        d->grpEdit = new KLineEdit(gb);
        KCompletion *kcom = new KCompletion;
        kcom->setItems(groupList);
        d->grpEdit->setCompletionObject(kcom, true);
        d->grpEdit->setAutoDeleteCompletionObject(true);
        d->grpEdit->setCompletionMode(KCompletion::CompletionAuto);
        d->grpEdit->setText(d->strGroup);
        gl->addWidget(d->grpEdit, 2, 1);
        connect(d->grpEdit, &QLineEdit::textChanged,
                this, &KPropertiesDialogPlugin::changed);
    } else if ((groupList.count() > 1) && isMyFile && isLocal) {
        d->grpCombo = new KComboBox(gb);
        d->grpCombo->setObjectName(QStringLiteral("combogrouplist"));
        d->grpCombo->addItems(groupList);
        d->grpCombo->setCurrentIndex(groupList.indexOf(d->strGroup));
        gl->addWidget(d->grpCombo, 2, 1);
        connect(d->grpCombo, QOverload<int>::of(&QComboBox::activated),
                this, &KPropertiesDialogPlugin::changed);
    } else {
        l = new QLabel(d->strGroup, gb);
        gl->addWidget(l, 2, 1);
    }

    gl->setColumnStretch(2, 10);

    // "Apply recursive" checkbox
    if (hasDir && !isLink && !isTrash) {
        d->cbRecursive = new QCheckBox(i18n("Apply changes to all subfolders and their contents"), d->m_frame);
        connect(d->cbRecursive, &QAbstractButton::clicked, this, &KPropertiesDialogPlugin::changed);
        box->addWidget(d->cbRecursive);
    }

    updateAccessControls();

    if (isTrash) {
        //don't allow to change properties for file into trash
        enableAccessControls(false);
        if (pbAdvancedPerm) {
            pbAdvancedPerm->setEnabled(false);
        }
    }

    box->addStretch(10);
}

#if HAVE_POSIX_ACL
static bool fileSystemSupportsACL(const QByteArray &path)
{
    bool fileSystemSupportsACLs = false;
#ifdef Q_OS_FREEBSD
    struct statfs buf;
    fileSystemSupportsACLs = (statfs(path.data(), &buf) == 0) && (buf.f_flags & MNT_ACLS);
#elif defined Q_OS_MACOS
    fileSystemSupportsACLs =
        getxattr(path.data(), "system.posix_acl_access", nullptr, 0, 0, XATTR_NOFOLLOW) >= 0 || errno == ENODATA;
#else
    fileSystemSupportsACLs =
        getxattr(path.data(), "system.posix_acl_access", nullptr, 0) >= 0 || errno == ENODATA;
#endif
    return fileSystemSupportsACLs;
}
#endif

void KFilePermissionsPropsPlugin::slotShowAdvancedPermissions()
{

    bool isDir = (d->pmode == PermissionsOnlyDirs) || (d->pmode == PermissionsMixed);
    QDialog dlg(properties);
    dlg.setModal(true);
    dlg.setWindowTitle(i18n("Advanced Permissions"));

    QLabel *l, *cl[3];
    QGroupBox *gb;
    QGridLayout *gl;

    QVBoxLayout *vbox = new QVBoxLayout;
    dlg.setLayout(vbox);
    // Group: Access Permissions
    gb = new QGroupBox(i18n("Access Permissions"), &dlg);
    vbox->addWidget(gb);

    gl = new QGridLayout(gb);
    gl->addItem(new QSpacerItem(0, 10), 0, 0);

    QVector<QWidget *> theNotSpecials;

    l = new QLabel(i18n("Class"), gb);
    gl->addWidget(l, 1, 0);
    theNotSpecials.append(l);

    QString readWhatsThis;
    QString readLabel;
    if (isDir) {
        readLabel = i18n("Show\nEntries");
        readWhatsThis = i18n("This flag allows viewing the content of the folder.");
    } else {
        readLabel = i18n("Read");
        readWhatsThis = i18n("The Read flag allows viewing the content of the file.");
    }

    QString writeWhatsThis;
    QString writeLabel;
    if (isDir) {
        writeLabel = i18n("Write\nEntries");
        writeWhatsThis = i18n("This flag allows adding, renaming and deleting of files. "
                              "Note that deleting and renaming can be limited using the Sticky flag.");
    } else {
        writeLabel = i18n("Write");
        writeWhatsThis = i18n("The Write flag allows modifying the content of the file.");
    }

    QString execLabel;
    QString execWhatsThis;
    if (isDir) {
        execLabel = i18nc("Enter folder", "Enter");
        execWhatsThis = i18n("Enable this flag to allow entering the folder.");
    } else {
        execLabel = i18n("Exec");
        execWhatsThis = i18n("Enable this flag to allow executing the file as a program.");
    }
    // GJ: Add space between normal and special modes
    QSize size = l->sizeHint();
    size.setWidth(size.width() + 15);
    l->setFixedSize(size);
    gl->addWidget(l, 1, 3);

    l = new QLabel(i18n("Special"), gb);
    gl->addWidget(l, 1, 4, 1, 1);
    QString specialWhatsThis;
    if (isDir)
        specialWhatsThis = i18n("Special flag. Valid for the whole folder, the exact "
                                "meaning of the flag can be seen in the right hand column.");
    else
        specialWhatsThis = i18n("Special flag. The exact meaning of the flag can be seen "
                                "in the right hand column.");
    l->setWhatsThis(specialWhatsThis);

    cl[0] = new QLabel(i18n("User"), gb);
    gl->addWidget(cl[0], 2, 0);
    theNotSpecials.append(cl[0]);

    cl[1] = new QLabel(i18n("Group"), gb);
    gl->addWidget(cl[1], 3, 0);
    theNotSpecials.append(cl[1]);

    cl[2] = new QLabel(i18n("Others"), gb);
    gl->addWidget(cl[2], 4, 0);
    theNotSpecials.append(cl[2]);

    QString setUidWhatsThis;
    if (isDir)
        setUidWhatsThis = i18n("If this flag is set, the owner of this folder will be "
                               "the owner of all new files.");
    else
        setUidWhatsThis = i18n("If this file is an executable and the flag is set, it will "
                               "be executed with the permissions of the owner.");

    QString setGidWhatsThis;
    if (isDir)
        setGidWhatsThis = i18n("If this flag is set, the group of this folder will be "
                               "set for all new files.");
    else
        setGidWhatsThis = i18n("If this file is an executable and the flag is set, it will "
                               "be executed with the permissions of the group.");

    QString stickyWhatsThis;
    if (isDir)
        stickyWhatsThis = i18n("If the Sticky flag is set on a folder, only the owner "
                               "and root can delete or rename files. Otherwise everybody "
                               "with write permissions can do this.");
    else
        stickyWhatsThis = i18n("The Sticky flag on a file is ignored on Linux, but may "
                               "be used on some systems");
    mode_t aPermissions, aPartialPermissions;
    mode_t dummy1, dummy2;

    if (!d->isIrregular) {
        switch (d->pmode) {
        case PermissionsOnlyFiles:
            getPermissionMasks(aPartialPermissions,
                               dummy1,
                               aPermissions,
                               dummy2);
            break;
        case PermissionsOnlyDirs:
        case PermissionsMixed:
            getPermissionMasks(dummy1,
                               aPartialPermissions,
                               dummy2,
                               aPermissions);
            break;
        case PermissionsOnlyLinks:
            aPermissions = UniRead | UniWrite | UniExec | UniSpecial;
            aPartialPermissions = 0;
            break;
        }
    } else {
        aPermissions = d->permissions;
        aPartialPermissions = d->partialPermissions;
    }

    // Draw Checkboxes
    QCheckBox *cba[3][4];
    for (int row = 0; row < 3; ++row) {
        for (int col = 0; col < 4; ++col) {
            QCheckBox *cb = new QCheckBox(gb);
            if (col != 3) {
                theNotSpecials.append(cb);
            }
            cba[row][col] = cb;
            cb->setChecked(aPermissions & fperm[row][col]);
            if (aPartialPermissions & fperm[row][col]) {
                cb->setTristate();
                cb->setCheckState(Qt::PartiallyChecked);
            } else if (d->cbRecursive && d->cbRecursive->isChecked()) {
                cb->setTristate();
            }

            cb->setEnabled(d->canChangePermissions);
            gl->addWidget(cb, row + 2, col + 1);
            switch (col) {
            case 0:
                cb->setText(readLabel);
                cb->setWhatsThis(readWhatsThis);
                break;
            case 1:
                cb->setText(writeLabel);
                cb->setWhatsThis(writeWhatsThis);
                break;
            case 2:
                cb->setText(execLabel);
                cb->setWhatsThis(execWhatsThis);
                break;
            case 3:
                switch (row) {
                case 0:
                    cb->setText(i18n("Set UID"));
                    cb->setWhatsThis(setUidWhatsThis);
                    break;
                case 1:
                    cb->setText(i18n("Set GID"));
                    cb->setWhatsThis(setGidWhatsThis);
                    break;
                case 2:
                    cb->setText(i18nc("File permission", "Sticky"));
                    cb->setWhatsThis(stickyWhatsThis);
                    break;
                }
                break;
            }
        }
    }
    gl->setColumnStretch(6, 10);

#if HAVE_POSIX_ACL
    KACLEditWidget *extendedACLs = nullptr;

    // FIXME make it work with partial entries
    if (properties->items().count() == 1) {
        QByteArray path = QFile::encodeName(properties->item().url().toLocalFile());
        d->fileSystemSupportsACLs = fileSystemSupportsACL(path);
    }
    if (d->fileSystemSupportsACLs) {
        std::for_each(theNotSpecials.begin(), theNotSpecials.end(), std::mem_fun(&QWidget::hide));
        extendedACLs = new KACLEditWidget(&dlg);
        extendedACLs->setEnabled(d->canChangePermissions);
        vbox->addWidget(extendedACLs);
        if (d->extendedACL.isValid() && d->extendedACL.isExtended()) {
            extendedACLs->setACL(d->extendedACL);
        } else {
            extendedACLs->setACL(KACL(aPermissions));
        }

        if (d->defaultACL.isValid()) {
            extendedACLs->setDefaultACL(d->defaultACL);
        }

        if (properties->items().constFirst().isDir()) {
            extendedACLs->setAllowDefaults(true);
        }
    }
#endif

    QDialogButtonBox *buttonBox = new QDialogButtonBox(&dlg);
    buttonBox->setStandardButtons(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    connect(buttonBox, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    connect(buttonBox, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
    vbox->addWidget(buttonBox);

    if (dlg.exec() != QDialog::Accepted) {
        return;
    }

    mode_t andPermissions = mode_t(~0);
    mode_t orPermissions = 0;
    for (int row = 0; row < 3; ++row)
        for (int col = 0; col < 4; ++col) {
            switch (cba[row][col]->checkState()) {
            case Qt::Checked:
                orPermissions |= fperm[row][col];
            //fall through
            case Qt::Unchecked:
                andPermissions &= ~fperm[row][col];
                break;
            case Qt::PartiallyChecked:
                break;
            }
        }

    d->isIrregular = false;
    const KFileItemList items = properties->items();
    KFileItemList::const_iterator it = items.begin();
    const KFileItemList::const_iterator kend = items.end();
    for (; it != kend; ++it) {
        if (isIrregular(((*it).permissions() & andPermissions) | orPermissions,
                        (*it).isDir(), (*it).isLink())) {
            d->isIrregular = true;
            break;
        }
    }

    d->permissions = orPermissions;
    d->partialPermissions = andPermissions;

#if HAVE_POSIX_ACL
    // override with the acls, if present
    if (extendedACLs) {
        d->extendedACL = extendedACLs->getACL();
        d->defaultACL = extendedACLs->getDefaultACL();
        d->hasExtendedACL = d->extendedACL.isExtended() || d->defaultACL.isValid();
        d->permissions = d->extendedACL.basePermissions();
        d->permissions |= (andPermissions | orPermissions) & (S_ISUID | S_ISGID | S_ISVTX);
    }
#endif

    updateAccessControls();
    emit changed();
}

// QString KFilePermissionsPropsPlugin::tabName () const
// {
//   return i18n ("&Permissions");
// }

KFilePermissionsPropsPlugin::~KFilePermissionsPropsPlugin()
{
    delete d;
}

bool KFilePermissionsPropsPlugin::supports(const KFileItemList & /*_items*/)
{
    return true;
}

// sets a combo box in the Access Control frame
void KFilePermissionsPropsPlugin::setComboContent(QComboBox *combo, PermissionsTarget target,
        mode_t permissions, mode_t partial)
{
    combo->clear();
    if (d->isIrregular) { //#176876
        return;
    }

    if (d->pmode == PermissionsOnlyLinks) {
        combo->addItem(i18n("Link"));
        combo->setCurrentIndex(0);
        return;
    }

    mode_t tMask = permissionsMasks[target];
    int textIndex;
    for (textIndex = 0; standardPermissions[textIndex] != (mode_t) - 1; textIndex++) {
        if ((standardPermissions[textIndex]&tMask) == (permissions & tMask & (UniRead | UniWrite))) {
            break;
        }
    }
    Q_ASSERT(standardPermissions[textIndex] != (mode_t) - 1); // must not happen, would be irreglar

    for (int i = 0; permissionsTexts[(int)d->pmode][i]; i++) {
        combo->addItem(i18n(permissionsTexts[(int)d->pmode][i]));
    }

    if (partial & tMask & ~UniExec) {
        combo->addItem(i18n("Varying (No Change)"));
        combo->setCurrentIndex(3);
    } else {
        combo->setCurrentIndex(textIndex);
    }
}

// permissions are irregular if they cant be displayed in a combo box.
bool KFilePermissionsPropsPlugin::isIrregular(mode_t permissions, bool isDir, bool isLink)
{
    if (isLink) {                           // links are always ok
        return false;
    }

    mode_t p = permissions;
    if (p & (S_ISUID | S_ISGID)) { // setuid/setgid -> irregular
        return true;
    }
    if (isDir) {
        p &= ~S_ISVTX;          // ignore sticky on dirs

        // check supported flag combinations
        mode_t p0 = p & UniOwner;
        if ((p0 != 0) && (p0 != (S_IRUSR | S_IXUSR)) && (p0 != UniOwner)) {
            return true;
        }
        p0 = p & UniGroup;
        if ((p0 != 0) && (p0 != (S_IRGRP | S_IXGRP)) && (p0 != UniGroup)) {
            return true;
        }
        p0 = p & UniOthers;
        if ((p0 != 0) && (p0 != (S_IROTH | S_IXOTH)) && (p0 != UniOthers)) {
            return true;
        }
        return false;
    }
    if (p & S_ISVTX) { // sticky on file -> irregular
        return true;
    }

    // check supported flag combinations
    mode_t p0 = p & UniOwner;
    bool usrXPossible = !p0; // true if this file could be an executable
    if (p0 & S_IXUSR) {
        if ((p0 == S_IXUSR) || (p0 == (S_IWUSR | S_IXUSR))) {
            return true;
        }
        usrXPossible = true;
    } else if (p0 == S_IWUSR) {
        return true;
    }

    p0 = p & UniGroup;
    bool grpXPossible = !p0; // true if this file could be an executable
    if (p0 & S_IXGRP) {
        if ((p0 == S_IXGRP) || (p0 == (S_IWGRP | S_IXGRP))) {
            return true;
        }
        grpXPossible = true;
    } else if (p0 == S_IWGRP) {
        return true;
    }
    if (p0 == 0) {
        grpXPossible = true;
    }

    p0 = p & UniOthers;
    bool othXPossible = !p0; // true if this file could be an executable
    if (p0 & S_IXOTH) {
        if ((p0 == S_IXOTH) || (p0 == (S_IWOTH | S_IXOTH))) {
            return true;
        }
        othXPossible = true;
    } else if (p0 == S_IWOTH) {
        return true;
    }

    // check that there either all targets are executable-compatible, or none
    return (p & UniExec) && !(usrXPossible && grpXPossible && othXPossible);
}

// enables/disabled the widgets in the Access Control frame
void KFilePermissionsPropsPlugin::enableAccessControls(bool enable)
{
    d->ownerPermCombo->setEnabled(enable);
    d->groupPermCombo->setEnabled(enable);
    d->othersPermCombo->setEnabled(enable);
    if (d->extraCheckbox) {
        d->extraCheckbox->setEnabled(enable);
    }
    if (d->cbRecursive) {
        d->cbRecursive->setEnabled(enable);
    }
}

// updates all widgets in the Access Control frame
void KFilePermissionsPropsPlugin::updateAccessControls()
{
    setComboContent(d->ownerPermCombo, PermissionsOwner,
                    d->permissions, d->partialPermissions);
    setComboContent(d->groupPermCombo, PermissionsGroup,
                    d->permissions, d->partialPermissions);
    setComboContent(d->othersPermCombo, PermissionsOthers,
                    d->permissions, d->partialPermissions);

    switch (d->pmode) {
    case PermissionsOnlyLinks:
        enableAccessControls(false);
        break;
    case PermissionsOnlyFiles:
        enableAccessControls(d->canChangePermissions && !d->isIrregular && !d->hasExtendedACL);
        if (d->canChangePermissions)
            d->explanationLabel->setText(d->isIrregular || d->hasExtendedACL ?
                                         i18np("This file uses advanced permissions",
                                               "These files use advanced permissions.",
                                               properties->items().count()) : QString());
        if (d->partialPermissions & UniExec) {
            d->extraCheckbox->setTristate();
            d->extraCheckbox->setCheckState(Qt::PartiallyChecked);
        } else {
            d->extraCheckbox->setTristate(false);
            d->extraCheckbox->setChecked(d->permissions & UniExec);
        }
        break;
    case PermissionsOnlyDirs:
        enableAccessControls(d->canChangePermissions && !d->isIrregular && !d->hasExtendedACL);
        // if this is a dir, and we can change permissions, don't dis-allow
        // recursive, we can do that for ACL setting.
        if (d->cbRecursive) {
            d->cbRecursive->setEnabled(d->canChangePermissions && !d->isIrregular);
        }

        if (d->canChangePermissions)
            d->explanationLabel->setText(d->isIrregular || d->hasExtendedACL ?
                                         i18np("This folder uses advanced permissions.",
                                               "These folders use advanced permissions.",
                                               properties->items().count()) : QString());
        if (d->partialPermissions & S_ISVTX) {
            d->extraCheckbox->setTristate();
            d->extraCheckbox->setCheckState(Qt::PartiallyChecked);
        } else {
            d->extraCheckbox->setTristate(false);
            d->extraCheckbox->setChecked(d->permissions & S_ISVTX);
        }
        break;
    case PermissionsMixed:
        enableAccessControls(d->canChangePermissions && !d->isIrregular && !d->hasExtendedACL);
        if (d->canChangePermissions)
            d->explanationLabel->setText(d->isIrregular || d->hasExtendedACL ?
                                         i18n("These files use advanced permissions.") : QString());
        if (d->partialPermissions & S_ISVTX) {
            d->extraCheckbox->setTristate();
            d->extraCheckbox->setCheckState(Qt::PartiallyChecked);
        } else {
            d->extraCheckbox->setTristate(false);
            d->extraCheckbox->setChecked(d->permissions & S_ISVTX);
        }
        break;
    }
}

// gets masks for files and dirs from the Access Control frame widgets
void KFilePermissionsPropsPlugin::getPermissionMasks(mode_t &andFilePermissions,
        mode_t &andDirPermissions,
        mode_t &orFilePermissions,
        mode_t &orDirPermissions)
{
    andFilePermissions = mode_t(~UniSpecial);
    andDirPermissions = mode_t(~(S_ISUID | S_ISGID));
    orFilePermissions = 0;
    orDirPermissions = 0;
    if (d->isIrregular) {
        return;
    }

    mode_t m = standardPermissions[d->ownerPermCombo->currentIndex()];
    if (m != (mode_t) - 1) {
        orFilePermissions |= m & UniOwner;
        if ((m & UniOwner) &&
                ((d->pmode == PermissionsMixed) ||
                 ((d->pmode == PermissionsOnlyFiles) && (d->extraCheckbox->checkState() == Qt::PartiallyChecked)))) {
            andFilePermissions &= ~(S_IRUSR | S_IWUSR);
        } else {
            andFilePermissions &= ~(S_IRUSR | S_IWUSR | S_IXUSR);
            if ((m & S_IRUSR) && (d->extraCheckbox->checkState() == Qt::Checked)) {
                orFilePermissions |= S_IXUSR;
            }
        }

        orDirPermissions |= m & UniOwner;
        if (m & S_IRUSR) {
            orDirPermissions |= S_IXUSR;
        }
        andDirPermissions &= ~(S_IRUSR | S_IWUSR | S_IXUSR);
    }

    m = standardPermissions[d->groupPermCombo->currentIndex()];
    if (m != (mode_t) - 1) {
        orFilePermissions |= m & UniGroup;
        if ((m & UniGroup) &&
                ((d->pmode == PermissionsMixed) ||
                 ((d->pmode == PermissionsOnlyFiles) && (d->extraCheckbox->checkState() == Qt::PartiallyChecked)))) {
            andFilePermissions &= ~(S_IRGRP | S_IWGRP);
        } else {
            andFilePermissions &= ~(S_IRGRP | S_IWGRP | S_IXGRP);
            if ((m & S_IRGRP) && (d->extraCheckbox->checkState() == Qt::Checked)) {
                orFilePermissions |= S_IXGRP;
            }
        }

        orDirPermissions |= m & UniGroup;
        if (m & S_IRGRP) {
            orDirPermissions |= S_IXGRP;
        }
        andDirPermissions &= ~(S_IRGRP | S_IWGRP | S_IXGRP);
    }

    m = d->othersPermCombo->currentIndex() >= 0 ? standardPermissions[d->othersPermCombo->currentIndex()] : (mode_t) - 1;
    if (m != (mode_t) - 1) {
        orFilePermissions |= m & UniOthers;
        if ((m & UniOthers) &&
                ((d->pmode == PermissionsMixed) ||
                 ((d->pmode == PermissionsOnlyFiles) && (d->extraCheckbox->checkState() == Qt::PartiallyChecked)))) {
            andFilePermissions &= ~(S_IROTH | S_IWOTH);
        } else {
            andFilePermissions &= ~(S_IROTH | S_IWOTH | S_IXOTH);
            if ((m & S_IROTH) && (d->extraCheckbox->checkState() == Qt::Checked)) {
                orFilePermissions |= S_IXOTH;
            }
        }

        orDirPermissions |= m & UniOthers;
        if (m & S_IROTH) {
            orDirPermissions |= S_IXOTH;
        }
        andDirPermissions &= ~(S_IROTH | S_IWOTH | S_IXOTH);
    }

    if (((d->pmode == PermissionsMixed) || (d->pmode == PermissionsOnlyDirs)) &&
            (d->extraCheckbox->checkState() != Qt::PartiallyChecked)) {
        andDirPermissions &= ~S_ISVTX;
        if (d->extraCheckbox->checkState() == Qt::Checked) {
            orDirPermissions |= S_ISVTX;
        }
    }
}

void KFilePermissionsPropsPlugin::applyChanges()
{
    mode_t orFilePermissions;
    mode_t orDirPermissions;
    mode_t andFilePermissions;
    mode_t andDirPermissions;

    if (!d->canChangePermissions) {
        return;
    }

    if (!d->isIrregular)
        getPermissionMasks(andFilePermissions,
                           andDirPermissions,
                           orFilePermissions,
                           orDirPermissions);
    else {
        orFilePermissions = d->permissions;
        andFilePermissions = d->partialPermissions;
        orDirPermissions = d->permissions;
        andDirPermissions = d->partialPermissions;
    }

    QString owner, group;
    if (d->usrEdit) {
        owner = d->usrEdit->text();
    }
    if (d->grpEdit) {
        group = d->grpEdit->text();
    } else if (d->grpCombo) {
        group = d->grpCombo->currentText();
    }

    if (owner == d->strOwner) {
        owner.clear();    // no change
    }

    if (group == d->strGroup) {
        group.clear();
    }

    bool recursive = d->cbRecursive && d->cbRecursive->isChecked();
    bool permissionChange = false;

    KFileItemList files, dirs;
    const KFileItemList items = properties->items();
    KFileItemList::const_iterator it = items.begin();
    const KFileItemList::const_iterator kend = items.end();
    for (; it != kend; ++it) {
        if ((*it).isDir()) {
            dirs.append(*it);
            if ((*it).permissions() != (((*it).permissions() & andDirPermissions) | orDirPermissions)) {
                permissionChange = true;
            }
        } else if ((*it).isFile()) {
            files.append(*it);
            if ((*it).permissions() != (((*it).permissions() & andFilePermissions) | orFilePermissions)) {
                permissionChange = true;
            }
        }
    }

    const bool ACLChange = (d->extendedACL !=  properties->item().ACL());
    const bool defaultACLChange = (d->defaultACL != properties->item().defaultACL());

    if (owner.isEmpty() && group.isEmpty() && !recursive
            && !permissionChange && !ACLChange && !defaultACLChange) {
        return;
    }

    KIO::Job *job;
    if (!files.isEmpty()) {
        job = KIO::chmod(files, orFilePermissions, ~andFilePermissions,
                         owner, group, false);
        if (ACLChange && d->fileSystemSupportsACLs) {
            job->addMetaData(QStringLiteral("ACL_STRING"), d->extendedACL.isValid() ? d->extendedACL.asString() : QStringLiteral("ACL_DELETE"));
        }
        if (defaultACLChange && d->fileSystemSupportsACLs) {
            job->addMetaData(QStringLiteral("DEFAULT_ACL_STRING"), d->defaultACL.isValid() ? d->defaultACL.asString() : QStringLiteral("ACL_DELETE"));
        }

        connect(job, &KJob::result,
                this, &KFilePermissionsPropsPlugin::slotChmodResult);
        QEventLoop eventLoop;
        connect(this, &KFilePermissionsPropsPlugin::leaveModality,
                &eventLoop, &QEventLoop::quit);
        eventLoop.exec(QEventLoop::ExcludeUserInputEvents);
    }
    if (!dirs.isEmpty()) {
        job = KIO::chmod(dirs, orDirPermissions, ~andDirPermissions,
                         owner, group, recursive);
        if (ACLChange && d->fileSystemSupportsACLs) {
            job->addMetaData(QStringLiteral("ACL_STRING"), d->extendedACL.isValid() ? d->extendedACL.asString() : QStringLiteral("ACL_DELETE"));
        }
        if (defaultACLChange && d->fileSystemSupportsACLs) {
            job->addMetaData(QStringLiteral("DEFAULT_ACL_STRING"), d->defaultACL.isValid() ? d->defaultACL.asString() : QStringLiteral("ACL_DELETE"));
        }

        connect(job, &KJob::result,
                this, &KFilePermissionsPropsPlugin::slotChmodResult);
        QEventLoop eventLoop;
        connect(this, &KFilePermissionsPropsPlugin::leaveModality,
                &eventLoop, &QEventLoop::quit);
        eventLoop.exec(QEventLoop::ExcludeUserInputEvents);
    }
}

void KFilePermissionsPropsPlugin::slotChmodResult(KJob *job)
{
    // qDebug() << "KFilePermissionsPropsPlugin::slotChmodResult";
    if (job->error()) {
        job->uiDelegate()->showErrorMessage();
    }
    // allow apply() to return
    emit leaveModality();
}

class KChecksumsPlugin::KChecksumsPluginPrivate
{
public:
    KChecksumsPluginPrivate()
    {
    }

    ~KChecksumsPluginPrivate()
    {
    }

    QWidget m_widget;
    Ui::ChecksumsWidget m_ui;

    QFileSystemWatcher fileWatcher;
    QString m_md5;
    QString m_sha1;
    QString m_sha256;
    QString m_sha512;
};

KChecksumsPlugin::KChecksumsPlugin(KPropertiesDialog *dialog)
    : KPropertiesDialogPlugin(dialog), d(new KChecksumsPluginPrivate)
{
    d->m_ui.setupUi(&d->m_widget);
    properties->addPage(&d->m_widget, i18nc("@title:tab", "C&hecksums"));

    d->m_ui.md5CopyButton->hide();
    d->m_ui.sha1CopyButton->hide();
    d->m_ui.sha256CopyButton->hide();
    d->m_ui.sha512CopyButton->hide();

    connect(d->m_ui.lineEdit, &QLineEdit::textChanged, this, [=](const QString &text) {
        slotVerifyChecksum(text.toLower());
    });

    connect(d->m_ui.md5Button, &QPushButton::clicked, this, &KChecksumsPlugin::slotShowMd5);
    connect(d->m_ui.sha1Button, &QPushButton::clicked, this, &KChecksumsPlugin::slotShowSha1);
    connect(d->m_ui.sha256Button, &QPushButton::clicked, this, &KChecksumsPlugin::slotShowSha256);
    connect(d->m_ui.sha512Button, &QPushButton::clicked, this, &KChecksumsPlugin::slotShowSha512);

    d->fileWatcher.addPath(properties->item().localPath());
    connect(&d->fileWatcher, &QFileSystemWatcher::fileChanged, this, &KChecksumsPlugin::slotInvalidateCache);

    auto clipboard = QApplication::clipboard();
    connect(d->m_ui.md5CopyButton, &QPushButton::clicked, this, [=]() {
        clipboard->setText(d->m_md5);
    });

    connect(d->m_ui.sha1CopyButton, &QPushButton::clicked, this, [=]() {
        clipboard->setText(d->m_sha1);
    });

    connect(d->m_ui.sha256CopyButton, &QPushButton::clicked, this, [=]() {
        clipboard->setText(d->m_sha256);
    });

    connect(d->m_ui.sha512CopyButton, &QPushButton::clicked, this, [=]() {
        clipboard->setText(d->m_sha512);
    });

    connect(d->m_ui.pasteButton, &QPushButton::clicked, this, [=]() {
        d->m_ui.lineEdit->setText(clipboard->text());
    });

    setDefaultState();
}

KChecksumsPlugin::~KChecksumsPlugin()
{
    delete d;
}

bool KChecksumsPlugin::supports(const KFileItemList &items)
{
    if (items.count() != 1) {
        return false;
    }

    const KFileItem &item = items.first();
    return item.isFile() && !item.localPath().isEmpty() && item.isReadable() && !item.isDesktopFile() && !item.isLink();
}

void KChecksumsPlugin::slotInvalidateCache()
{
    d->m_md5 = QString();
    d->m_sha1 = QString();
    d->m_sha256 = QString();
    d->m_sha512 = QString();
}

void KChecksumsPlugin::slotShowMd5()
{
    auto label = new QLabel(i18nc("@action:button", "Calculating..."), &d->m_widget);
    label->setTextInteractionFlags(Qt::TextSelectableByMouse | Qt::TextSelectableByKeyboard);

    d->m_ui.calculateWidget->layout()->replaceWidget(d->m_ui.md5Button, label);
    d->m_ui.md5Button->hide();

    showChecksum(QCryptographicHash::Md5, label, d->m_ui.md5CopyButton);
}

void KChecksumsPlugin::slotShowSha1()
{
    auto label = new QLabel(i18nc("@action:button", "Calculating..."), &d->m_widget);
    label->setTextInteractionFlags(Qt::TextSelectableByMouse | Qt::TextSelectableByKeyboard);

    d->m_ui.calculateWidget->layout()->replaceWidget(d->m_ui.sha1Button, label);
    d->m_ui.sha1Button->hide();

    showChecksum(QCryptographicHash::Sha1, label, d->m_ui.sha1CopyButton);
}

void KChecksumsPlugin::slotShowSha256()
{
    auto label = new QLabel(i18nc("@action:button", "Calculating..."), &d->m_widget);
    label->setTextInteractionFlags(Qt::TextSelectableByMouse | Qt::TextSelectableByKeyboard);

    d->m_ui.calculateWidget->layout()->replaceWidget(d->m_ui.sha256Button, label);
    d->m_ui.sha256Button->hide();

    showChecksum(QCryptographicHash::Sha256, label, d->m_ui.sha256CopyButton);
}

void KChecksumsPlugin::slotShowSha512()
{
    auto label = new QLabel(i18nc("@action:button", "Calculating..."), &d->m_widget);
    label->setTextInteractionFlags(Qt::TextSelectableByMouse | Qt::TextSelectableByKeyboard);

    d->m_ui.calculateWidget->layout()->replaceWidget(d->m_ui.sha512Button, label);
    d->m_ui.sha512Button->hide();

    showChecksum(QCryptographicHash::Sha512, label, d->m_ui.sha512CopyButton);
}

void KChecksumsPlugin::slotVerifyChecksum(const QString &input)
{
    auto algorithm = detectAlgorithm(input);

    // Input is not a supported hash algorithm.
    if (algorithm == QCryptographicHash::Md4) {
        if (input.isEmpty()) {
            setDefaultState();
        } else {
            setInvalidChecksumState();
        }
        return;
    }

    const QString checksum = cachedChecksum(algorithm);

    // Checksum already in cache.
    if (!checksum.isEmpty()) {
        const bool isMatch = (checksum == input);
        if (isMatch) {
            setMatchState();
        } else {
            setMismatchState();
        }

        return;
    }

    // Calculate checksum in another thread.
    auto futureWatcher = new QFutureWatcher<QString>(this);
    connect(futureWatcher, &QFutureWatcher<QString>::finished, this, [=]() {

        const QString checksum = futureWatcher->result();
        futureWatcher->deleteLater();

        cacheChecksum(checksum, algorithm);

        switch (algorithm) {
        case QCryptographicHash::Md5:
            slotShowMd5();
            break;
        case QCryptographicHash::Sha1:
            slotShowSha1();
            break;
        case QCryptographicHash::Sha256:
            slotShowSha256();
            break;
        case QCryptographicHash::Sha512:
            slotShowSha512();
            break;
        default:
            break;
        }

        const bool isMatch = (checksum == input);
        if (isMatch) {
            setMatchState();
        } else {
            setMismatchState();
        }
    });

    // Notify the user about the background computation.
    setVerifyState();

    auto future = QtConcurrent::run(&KChecksumsPlugin::computeChecksum, algorithm, properties->item().localPath());
    futureWatcher->setFuture(future);
}

bool KChecksumsPlugin::isMd5(const QString &input)
{
    QRegularExpression regex(QStringLiteral("^[a-f0-9]{32}$"));
    return regex.match(input).hasMatch();
}

bool KChecksumsPlugin::isSha1(const QString &input)
{
    QRegularExpression regex(QStringLiteral("^[a-f0-9]{40}$"));
    return regex.match(input).hasMatch();
}

bool KChecksumsPlugin::isSha256(const QString &input)
{
    QRegularExpression regex(QStringLiteral("^[a-f0-9]{64}$"));
    return regex.match(input).hasMatch();
}

bool KChecksumsPlugin::isSha512(const QString &input)
{
    QRegularExpression regex(QStringLiteral("^[a-f0-9]{128}$"));
    return regex.match(input).hasMatch();
}

QString KChecksumsPlugin::computeChecksum(QCryptographicHash::Algorithm algorithm, const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return QString();
    }

    QCryptographicHash hash(algorithm);
    hash.addData(&file);

    return QString::fromLatin1(hash.result().toHex());
}

QCryptographicHash::Algorithm KChecksumsPlugin::detectAlgorithm(const QString &input)
{
    if (isMd5(input)) {
        return QCryptographicHash::Md5;
    }

    if (isSha1(input)) {
        return QCryptographicHash::Sha1;
    }

    if (isSha256(input)) {
        return QCryptographicHash::Sha256;
    }

    if (isSha512(input)) {
        return QCryptographicHash::Sha512;
    }

    // Md4 used as negative error code.
    return QCryptographicHash::Md4;
}

void KChecksumsPlugin::setDefaultState()
{
    QColor defaultColor = d->m_widget.palette().color(QPalette::Base);

    QPalette palette = d->m_widget.palette();
    palette.setColor(QPalette::Base, defaultColor);

    d->m_ui.feedbackLabel->hide();
    d->m_ui.lineEdit->setPalette(palette);
    d->m_ui.lineEdit->setToolTip(QString());
}

void KChecksumsPlugin::setInvalidChecksumState()
{
    KColorScheme colorScheme(QPalette::Active, KColorScheme::View);
    QColor warningColor = colorScheme.background(KColorScheme::NegativeBackground).color();

    QPalette palette = d->m_widget.palette();
    palette.setColor(QPalette::Base, warningColor);

    d->m_ui.feedbackLabel->setText(i18n("Invalid checksum."));
    d->m_ui.feedbackLabel->show();
    d->m_ui.lineEdit->setPalette(palette);
    d->m_ui.lineEdit->setToolTip(i18nc("@info:tooltip", "The given input is not a valid MD5, SHA1 or SHA256 checksum."));
}

void KChecksumsPlugin::setMatchState()
{
    KColorScheme colorScheme(QPalette::Active, KColorScheme::View);
    QColor positiveColor = colorScheme.background(KColorScheme::PositiveBackground).color();

    QPalette palette = d->m_widget.palette();
    palette.setColor(QPalette::Base, positiveColor);

    d->m_ui.feedbackLabel->setText(i18n("Checksums match."));
    d->m_ui.feedbackLabel->show();
    d->m_ui.lineEdit->setPalette(palette);
    d->m_ui.lineEdit->setToolTip(i18nc("@info:tooltip", "The computed checksum and the expected checksum match."));
}

void KChecksumsPlugin::setMismatchState()
{
    KColorScheme colorScheme(QPalette::Active, KColorScheme::View);
    QColor warningColor = colorScheme.background(KColorScheme::NegativeBackground).color();

    QPalette palette = d->m_widget.palette();
    palette.setColor(QPalette::Base, warningColor);

    d->m_ui.feedbackLabel->setText(i18n("<p>Checksums do not match.</p>"
                                        "This may be due to a faulty download. Try re-downloading the file.<br/>"
                                        "If the verification still fails, contact the source of the file."));
    d->m_ui.feedbackLabel->show();
    d->m_ui.lineEdit->setPalette(palette);
    d->m_ui.lineEdit->setToolTip(i18nc("@info:tooltip", "The computed checksum and the expected checksum differ."));
}

void KChecksumsPlugin::setVerifyState()
{
    // Users can paste a checksum at any time, so reset to default.
    setDefaultState();

    d->m_ui.feedbackLabel->setText(i18nc("notify the user about a computation in the background",  "Verifying checksum..."));
    d->m_ui.feedbackLabel->show();
}

void KChecksumsPlugin::showChecksum(QCryptographicHash::Algorithm algorithm, QLabel *label, QPushButton *copyButton)
{
    const QString checksum = cachedChecksum(algorithm);

    // Checksum in cache, nothing else to do.
    if (!checksum.isEmpty()) {
        label->setText(checksum);
        return;
    }

    // Calculate checksum in another thread.
    auto futureWatcher = new QFutureWatcher<QString>(this);
    connect(futureWatcher, &QFutureWatcher<QString>::finished, this, [=]() {
        const QString checksum = futureWatcher->result();
        futureWatcher->deleteLater();

        label->setText(checksum);
        cacheChecksum(checksum, algorithm);

        copyButton->show();
    });

    auto future = QtConcurrent::run(&KChecksumsPlugin::computeChecksum, algorithm, properties->item().localPath());
    futureWatcher->setFuture(future);
}

QString KChecksumsPlugin::cachedChecksum(QCryptographicHash::Algorithm algorithm) const
{
    switch (algorithm) {
    case QCryptographicHash::Md5:
        return d->m_md5;
    case QCryptographicHash::Sha1:
        return d->m_sha1;
    case QCryptographicHash::Sha256:
        return d->m_sha256;
    case QCryptographicHash::Sha512:
        return d->m_sha512;
    default:
        break;
    }

    return QString();
}

void KChecksumsPlugin::cacheChecksum(const QString &checksum, QCryptographicHash::Algorithm algorithm)
{
    switch (algorithm) {
    case QCryptographicHash::Md5:
        d->m_md5 = checksum;
        break;
    case QCryptographicHash::Sha1:
        d->m_sha1 = checksum;
        break;
    case QCryptographicHash::Sha256:
        d->m_sha256 = checksum;
        break;
    case QCryptographicHash::Sha512:
        d->m_sha512 = checksum;
        break;
    default:
        return;
    }
}

class KUrlPropsPlugin::KUrlPropsPluginPrivate
{
public:
    KUrlPropsPluginPrivate()
    {
    }
    ~KUrlPropsPluginPrivate()
    {
    }

    QFrame *m_frame;
    KUrlRequester *URLEdit;
    QString URLStr;
    bool fileNameReadOnly = false;
};

KUrlPropsPlugin::KUrlPropsPlugin(KPropertiesDialog *_props)
    : KPropertiesDialogPlugin(_props), d(new KUrlPropsPluginPrivate)
{
    d->m_frame = new QFrame();
    properties->addPage(d->m_frame, i18n("U&RL"));
    QVBoxLayout *layout = new QVBoxLayout(d->m_frame);
    layout->setContentsMargins(0, 0, 0, 0);

    QLabel *l;
    l = new QLabel(d->m_frame);
    l->setObjectName(QStringLiteral("Label_1"));
    l->setText(i18n("URL:"));
    layout->addWidget(l, Qt::AlignRight);

    d->URLEdit = new KUrlRequester(d->m_frame);
    layout->addWidget(d->URLEdit);

    KIO::StatJob *job = KIO::mostLocalUrl(properties->url());
    KJobWidgets::setWindow(job, properties);
    job->exec();
    QUrl url = job->mostLocalUrl();

    if (url.isLocalFile()) {
        QString path = url.toLocalFile();

        QFile f(path);
        if (!f.open(QIODevice::ReadOnly)) {
            return;
        }
        f.close();

        KDesktopFile config(path);
        const KConfigGroup dg = config.desktopGroup();
        d->URLStr = dg.readPathEntry("URL", QString());

        if (!d->URLStr.isEmpty()) {
            d->URLEdit->setUrl(QUrl(d->URLStr));
        }
    }

    connect(d->URLEdit, &KUrlRequester::textChanged,
            this, &KPropertiesDialogPlugin::changed);

    layout->addStretch(1);
}

KUrlPropsPlugin::~KUrlPropsPlugin()
{
    delete d;
}

void KUrlPropsPlugin::setFileNameReadOnly(bool ro)
{
    d->fileNameReadOnly = ro;
}

// QString KUrlPropsPlugin::tabName () const
// {
//   return i18n ("U&RL");
// }

bool KUrlPropsPlugin::supports(const KFileItemList &_items)
{
    if (_items.count() != 1) {
        return false;
    }
    const KFileItem &item = _items.first();
    // check if desktop file
    if (!item.isDesktopFile()) {
        return false;
    }

    // open file and check type
    bool isLocal;
    QUrl url = item.mostLocalUrl(&isLocal);
    if (!isLocal) {
        return false;
    }

    KDesktopFile config(url.toLocalFile());
    return config.hasLinkType();
}

void KUrlPropsPlugin::applyChanges()
{
    KIO::StatJob *job = KIO::mostLocalUrl(properties->url());
    KJobWidgets::setWindow(job, properties);
    job->exec();
    const QUrl url = job->mostLocalUrl();

    if (!url.isLocalFile()) {
        KMessageBox::sorry(nullptr, i18n("Could not save properties. Only entries on local file systems are supported."));
        return;
    }

    QString path = url.toLocalFile();
    QFile f(path);
    if (!f.open(QIODevice::ReadWrite)) {
        KMessageBox::sorry(nullptr, i18n("<qt>Could not save properties. You do not have "
                                   "sufficient access to write to <b>%1</b>.</qt>", path));
        return;
    }
    f.close();

    KDesktopFile config(path);
    KConfigGroup dg = config.desktopGroup();
    dg.writeEntry("Type", QStringLiteral("Link"));
    dg.writePathEntry("URL", d->URLEdit->url().toString());
    // Users can't create a Link .desktop file with a Name field,
    // but distributions can. Update the Name field in that case,
    // if the file name could have been changed.
    if (!d->fileNameReadOnly && dg.hasKey("Name")) {
        const QString nameStr = nameFromFileName(properties->url().fileName());
        dg.writeEntry("Name", nameStr);
        dg.writeEntry("Name", nameStr, KConfigBase::Persistent | KConfigBase::Localized);

    }
}

/* ----------------------------------------------------
 *
 * KDevicePropsPlugin
 *
 * -------------------------------------------------- */

class KDevicePropsPlugin::KDevicePropsPluginPrivate
{
public:
    KDevicePropsPluginPrivate()
    {
    }
    ~KDevicePropsPluginPrivate()
    {
    }

    bool isMounted() const
    {
        const QString dev = device->currentText();
        return !dev.isEmpty() && KMountPoint::currentMountPoints().findByDevice(dev);
    }

    QFrame *m_frame;
    QStringList mountpointlist;
    QLabel *m_freeSpaceText;
    QLabel *m_freeSpaceLabel;
    QProgressBar *m_freeSpaceBar;

    KComboBox *device;
    QLabel *mountpoint;
    QCheckBox *readonly;

    QStringList m_devicelist;
};

KDevicePropsPlugin::KDevicePropsPlugin(KPropertiesDialog *_props) : KPropertiesDialogPlugin(_props), d(new KDevicePropsPluginPrivate)
{
    d->m_frame = new QFrame();
    properties->addPage(d->m_frame, i18n("De&vice"));

    QStringList devices;
    const KMountPoint::List mountPoints = KMountPoint::possibleMountPoints();

    for (const KMountPoint::Ptr &mp : mountPoints) {
        QString mountPoint = mp->mountPoint();
        QString device = mp->mountedFrom();
        // qDebug()<<"mountPoint :"<<mountPoint<<" device :"<<device<<" mp->mountType() :"<<mp->mountType();

        if ((mountPoint != QLatin1String("-")) && (mountPoint != QLatin1String("none")) && !mountPoint.isEmpty()
                && device != QLatin1String("none")) {
            devices.append(device + QLatin1String(" (")
                           + mountPoint + QLatin1Char(')'));
            d->m_devicelist.append(device);
            d->mountpointlist.append(mountPoint);
        }
    }

    QGridLayout *layout = new QGridLayout(d->m_frame);

    layout->setContentsMargins(0, 0, 0, 0);
    layout->setColumnStretch(1, 1);

    QLabel *label;
    label = new QLabel(d->m_frame);
    label->setText(devices.isEmpty() ?
                   i18n("Device (/dev/fd0):") : // old style
                   i18n("Device:"));  // new style (combobox)
    layout->addWidget(label, 0, 0, Qt::AlignRight);

    d->device = new KComboBox(d->m_frame);
    d->device->setObjectName(QStringLiteral("ComboBox_device"));
    d->device->setEditable(true);
    d->device->addItems(devices);
    layout->addWidget(d->device, 0, 1);
    connect(d->device, QOverload<int>::of(&QComboBox::activated),
            this, &KDevicePropsPlugin::slotActivated);

    d->readonly = new QCheckBox(d->m_frame);
    d->readonly->setObjectName(QStringLiteral("CheckBox_readonly"));
    d->readonly->setText(i18n("Read only"));
    layout->addWidget(d->readonly, 1, 1);

    label = new QLabel(d->m_frame);
    label->setText(i18n("File system:"));
    layout->addWidget(label, 2, 0, Qt::AlignRight);

    QLabel *fileSystem = new QLabel(d->m_frame);
    layout->addWidget(fileSystem, 2, 1);

    label = new QLabel(d->m_frame);
    label->setText(devices.isEmpty() ?
                   i18n("Mount point (/mnt/floppy):") : // old style
                   i18n("Mount point:")); // new style (combobox)
    layout->addWidget(label, 3, 0, Qt::AlignRight);

    d->mountpoint = new QLabel(d->m_frame);
    d->mountpoint->setObjectName(QStringLiteral("LineEdit_mountpoint"));

    layout->addWidget(d->mountpoint, 3, 1);

    // show disk free
    d->m_freeSpaceText = new QLabel(i18nc("Amount of used and available space on this device or partition", "Free space:"), d->m_frame);
    layout->addWidget(d->m_freeSpaceText, 4, 0, Qt::AlignRight);

    d->m_freeSpaceLabel = new QLabel(d->m_frame);
    layout->addWidget(d->m_freeSpaceLabel, 4, 1);

    d->m_freeSpaceBar = new QProgressBar(d->m_frame);
    d->m_freeSpaceBar->setObjectName(QStringLiteral("freeSpaceBar"));
    layout->addWidget(d->m_freeSpaceBar, 5, 0, 1, 2);

    // we show it in the slot when we know the values
    d->m_freeSpaceText->hide();
    d->m_freeSpaceLabel->hide();
    d->m_freeSpaceBar->hide();

    KSeparator *sep = new KSeparator(Qt::Horizontal, d->m_frame);
    layout->addWidget(sep, 6, 0, 1, 2);

    layout->setRowStretch(7, 1);

    KIO::StatJob *job = KIO::mostLocalUrl(_props->url());
    KJobWidgets::setWindow(job, _props);
    job->exec();
    QUrl url = job->mostLocalUrl();

    if (!url.isLocalFile()) {
        return;
    }
    QString path = url.toLocalFile();

    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) {
        return;
    }
    f.close();

    const KDesktopFile _config(path);
    const KConfigGroup config = _config.desktopGroup();
    QString deviceStr = config.readEntry("Dev");
    QString mountPointStr = config.readEntry("MountPoint");
    bool ro = config.readEntry("ReadOnly", false);

    fileSystem->setText(config.readEntry("FSType"));

    d->device->setEditText(deviceStr);
    if (!deviceStr.isEmpty()) {
        // Set default options for this device (first matching entry)
        int index = d->m_devicelist.indexOf(deviceStr);
        if (index != -1) {
            //qDebug() << "found it" << index;
            slotActivated(index);
        }
    }

    if (!mountPointStr.isEmpty()) {
        d->mountpoint->setText(mountPointStr);
        updateInfo();
    }

    d->readonly->setChecked(ro);

    connect(d->device, QOverload<int>::of(&QComboBox::activated),
            this, &KPropertiesDialogPlugin::changed);
    connect(d->device, &QComboBox::currentTextChanged,
            this, &KPropertiesDialogPlugin::changed);
    connect(d->readonly, &QAbstractButton::toggled,
            this, &KPropertiesDialogPlugin::changed);

    connect(d->device, &QComboBox::currentTextChanged,
            this, &KDevicePropsPlugin::slotDeviceChanged);
}

KDevicePropsPlugin::~KDevicePropsPlugin()
{
    delete d;
}

// QString KDevicePropsPlugin::tabName () const
// {
//   return i18n ("De&vice");
// }

void KDevicePropsPlugin::updateInfo()
{
    // we show it in the slot when we know the values
    d->m_freeSpaceText->hide();
    d->m_freeSpaceLabel->hide();
    d->m_freeSpaceBar->hide();

    if (!d->mountpoint->text().isEmpty() && d->isMounted()) {
        KDiskFreeSpaceInfo info = KDiskFreeSpaceInfo::freeSpaceInfo(d->mountpoint->text());
        slotFoundMountPoint(info.mountPoint(), info.size() / 1024, info.used() / 1024, info.available() / 1024);
    }
}

void KDevicePropsPlugin::slotActivated(int index)
{
    // index can be more than the number of known devices, when the user types
    // a "custom" device.
    if (index < d->m_devicelist.count()) {
        // Update mountpoint so that it matches the device that was selected in the combo
        d->device->setEditText(d->m_devicelist[index]);
        d->mountpoint->setText(d->mountpointlist[index]);
    }

    updateInfo();
}

void KDevicePropsPlugin::slotDeviceChanged()
{
    // Update mountpoint so that it matches the typed device
    int index = d->m_devicelist.indexOf(d->device->currentText());
    if (index != -1) {
        d->mountpoint->setText(d->mountpointlist[index]);
    } else {
        d->mountpoint->setText(QString());
    }

    updateInfo();
}

void KDevicePropsPlugin::slotFoundMountPoint(const QString &,
        quint64 kibSize,
        quint64 /*kibUsed*/,
        quint64 kibAvail)
{
    d->m_freeSpaceText->show();
    d->m_freeSpaceLabel->show();

    const int percUsed = kibSize != 0 ? (100 - (int)(100.0 * kibAvail / kibSize)) : 100;

    d->m_freeSpaceLabel->setText(
        i18nc("Available space out of total partition size (percent used)", "%1 free of %2 (%3% used)",
              KIO::convertSizeFromKiB(kibAvail),
              KIO::convertSizeFromKiB(kibSize),
              percUsed));

    d->m_freeSpaceBar->setRange(0, 100);
    d->m_freeSpaceBar->setValue(percUsed);
    d->m_freeSpaceBar->show();
}

bool KDevicePropsPlugin::supports(const KFileItemList &_items)
{
    if (_items.count() != 1) {
        return false;
    }
    const KFileItem &item = _items.first();
    // check if desktop file
    if (!item.isDesktopFile()) {
        return false;
    }

    // open file and check type
    bool isLocal;
    QUrl url = item.mostLocalUrl(&isLocal);
    if (!isLocal) {
        return false;
    }

    KDesktopFile config(url.toLocalFile());
    return config.hasDeviceType();
}

void KDevicePropsPlugin::applyChanges()
{
    KIO::StatJob *job = KIO::mostLocalUrl(properties->url());
    KJobWidgets::setWindow(job, properties);
    job->exec();
    const QUrl url = job->mostLocalUrl();

    if (!url.isLocalFile()) {
        return;
    }
    const QString path = url.toLocalFile();
    QFile f(path);
    if (!f.open(QIODevice::ReadWrite)) {
        KMessageBox::sorry(nullptr, i18n("<qt>Could not save properties. You do not have sufficient "
                                   "access to write to <b>%1</b>.</qt>", path));
        return;
    }
    f.close();

    KDesktopFile _config(path);
    KConfigGroup config = _config.desktopGroup();
    config.writeEntry("Type", QStringLiteral("FSDevice"));

    config.writeEntry("Dev", d->device->currentText());
    config.writeEntry("MountPoint", d->mountpoint->text());

    config.writeEntry("ReadOnly", d->readonly->isChecked());

    config.sync();
}

/* ----------------------------------------------------
 *
 * KDesktopPropsPlugin
 *
 * -------------------------------------------------- */

class KDesktopPropsPlugin::KDesktopPropsPluginPrivate
{
public:
    KDesktopPropsPluginPrivate()
        : w(new Ui_KPropertiesDesktopBase)
        , m_frame(new QFrame())
    {
    }
    ~KDesktopPropsPluginPrivate()
    {
        delete w;
    }
    Ui_KPropertiesDesktopBase *w;
    QWidget *m_frame;

    QString m_origCommandStr;
    QString m_terminalOptionStr;
    QString m_suidUserStr;
    QString m_dbusStartupType;
    QString m_dbusServiceName;
    QString m_origDesktopFile;
    bool m_terminalBool;
    bool m_suidBool;
    bool m_hasDiscreteGpuBool;
    bool m_runOnDiscreteGpuBool;
    bool m_startupBool;
};

KDesktopPropsPlugin::KDesktopPropsPlugin(KPropertiesDialog *_props)
    : KPropertiesDialogPlugin(_props), d(new KDesktopPropsPluginPrivate)
{
    QMimeDatabase db;

    d->w->setupUi(d->m_frame);

    properties->addPage(d->m_frame, i18n("&Application"));

    bool bKDesktopMode = properties->url().scheme() == QLatin1String("desktop") ||
                         properties->currentDir().scheme() == QLatin1String("desktop");

    if (bKDesktopMode) {
        // Hide Name entry
        d->w->nameEdit->hide();
        d->w->nameLabel->hide();
    }

    d->w->pathEdit->setMode(KFile::Directory | KFile::LocalOnly);
    d->w->pathEdit->lineEdit()->setAcceptDrops(false);

    connect(d->w->nameEdit, &QLineEdit::textChanged, this, &KPropertiesDialogPlugin::changed);
    connect(d->w->genNameEdit, &QLineEdit::textChanged, this, &KPropertiesDialogPlugin::changed);
    connect(d->w->commentEdit, &QLineEdit::textChanged, this, &KPropertiesDialogPlugin::changed);
    connect(d->w->commandEdit, &QLineEdit::textChanged, this, &KPropertiesDialogPlugin::changed);
    connect(d->w->pathEdit, &KUrlRequester::textChanged, this, &KPropertiesDialogPlugin::changed);

    connect(d->w->browseButton, &QAbstractButton::clicked, this, &KDesktopPropsPlugin::slotBrowseExec);
    connect(d->w->addFiletypeButton, &QAbstractButton::clicked, this, &KDesktopPropsPlugin::slotAddFiletype);
    connect(d->w->delFiletypeButton, &QAbstractButton::clicked, this, &KDesktopPropsPlugin::slotDelFiletype);
    connect(d->w->advancedButton, &QAbstractButton::clicked, this, &KDesktopPropsPlugin::slotAdvanced);

    enum DiscreteGpuCheck { NotChecked, Present, Absent };
    static DiscreteGpuCheck s_gpuCheck = NotChecked;

    if (s_gpuCheck == NotChecked) {
        // Check whether we have a discrete gpu
        bool hasDiscreteGpu = false;
        QDBusInterface iface(QStringLiteral("org.kde.Solid.PowerManagement"),
                             QStringLiteral("/org/kde/Solid/PowerManagement"),
                             QStringLiteral("org.kde.Solid.PowerManagement"),
                             QDBusConnection::sessionBus());
        if (iface.isValid()) {
            QDBusReply<bool> reply = iface.call(QStringLiteral("hasDualGpu"));
            if (reply.isValid()) {
                hasDiscreteGpu = reply.value();
            }
        }

        s_gpuCheck = hasDiscreteGpu ? Present : Absent;
    }

    d->m_hasDiscreteGpuBool = s_gpuCheck == Present;

    // now populate the page

    KIO::StatJob *job = KIO::mostLocalUrl(_props->url());
    KJobWidgets::setWindow(job, _props);
    job->exec();
    QUrl url = job->mostLocalUrl();

    if (!url.isLocalFile()) {
        return;
    }

    d->m_origDesktopFile = url.toLocalFile();

    QFile f(d->m_origDesktopFile);
    if (!f.open(QIODevice::ReadOnly)) {
        return;
    }
    f.close();

    KDesktopFile  _config(d->m_origDesktopFile);
    KConfigGroup config = _config.desktopGroup();
    QString nameStr = _config.readName();
    QString genNameStr = _config.readGenericName();
    QString commentStr = _config.readComment();
    QString commandStr = config.readEntry("Exec", QString());

    d->m_origCommandStr = commandStr;
    QString pathStr = config.readEntry("Path", QString());   // not readPathEntry, see kservice.cpp
    d->m_terminalBool = config.readEntry("Terminal", false);
    d->m_terminalOptionStr = config.readEntry("TerminalOptions");
    d->m_suidBool = config.readEntry("X-KDE-SubstituteUID", false);
    d->m_suidUserStr = config.readEntry("X-KDE-Username");
    if (d->m_hasDiscreteGpuBool) {
        d->m_runOnDiscreteGpuBool = config.readEntry("X-KDE-RunOnDiscreteGpu", false);
    }
    if (config.hasKey("StartupNotify")) {
        d->m_startupBool = config.readEntry("StartupNotify", true);
    } else {
        d->m_startupBool = config.readEntry("X-KDE-StartupNotify", true);
    }
    d->m_dbusStartupType = config.readEntry("X-DBUS-StartupType").toLower();
    // ### should there be a GUI for this setting?
    // At least we're copying it over to the local file, to avoid side effects (#157853)
    d->m_dbusServiceName = config.readEntry("X-DBUS-ServiceName");

    const QStringList mimeTypes = config.readXdgListEntry("MimeType");

    if (nameStr.isEmpty() || bKDesktopMode) {
        // We'll use the file name if no name is specified
        // because we _need_ a Name for a valid file.
        // But let's do it in apply, not here, so that we pick up the right name.
        setDirty();
    }
    if (!bKDesktopMode) {
        d->w->nameEdit->setText(nameStr);
    }

    d->w->genNameEdit->setText(genNameStr);
    d->w->commentEdit->setText(commentStr);
    d->w->commandEdit->setText(commandStr);
    d->w->pathEdit->lineEdit()->setText(pathStr);

    // was: d->w->filetypeList->setFullWidth(true);
    //  d->w->filetypeList->header()->setStretchEnabled(true, d->w->filetypeList->columns()-1);

    for (QStringList::ConstIterator it = mimeTypes.begin();
            it != mimeTypes.end();) {
        QMimeType p = db.mimeTypeForName(*it);
        ++it;
        QString preference;
        if (it != mimeTypes.end()) {
            bool numeric;
            (*it).toInt(&numeric);
            if (numeric) {
                preference = *it;
                ++it;
            }
        }
        if (p.isValid()) {
            QTreeWidgetItem *item = new QTreeWidgetItem();
            item->setText(0, p.name());
            item->setText(1, p.comment());
            item->setText(2, preference);
            d->w->filetypeList->addTopLevelItem(item);
        }
    }
    d->w->filetypeList->resizeColumnToContents(0);

}

KDesktopPropsPlugin::~KDesktopPropsPlugin()
{
    delete d;
}

void KDesktopPropsPlugin::slotAddFiletype()
{
    QMimeDatabase db;
    KMimeTypeChooserDialog dlg(i18n("Add File Type for %1", properties->url().fileName()),
                               i18n("Select one or more file types to add:"),
                               QStringList(), // no preselected mimetypes
                               QString(),
                               QStringList(),
                               KMimeTypeChooser::Comments | KMimeTypeChooser::Patterns,
                               d->m_frame);

    if (dlg.exec() == QDialog::Accepted) {
        const QStringList list = dlg.chooser()->mimeTypes();
        for (const QString &mimetype : list) {
            QMimeType p = db.mimeTypeForName(mimetype);
            if (!p.isValid()) {
                continue;
            }

            bool found = false;
            int count = d->w->filetypeList->topLevelItemCount();
            for (int i = 0; !found && i < count; ++i) {
                if (d->w->filetypeList->topLevelItem(i)->text(0) == mimetype) {
                    found = true;
                }
            }
            if (!found) {
                QTreeWidgetItem *item = new QTreeWidgetItem();
                item->setText(0, p.name());
                item->setText(1, p.comment());
                d->w->filetypeList->addTopLevelItem(item);
            }
            d->w->filetypeList->resizeColumnToContents(0);
        }
    }
    emit changed();
}

void KDesktopPropsPlugin::slotDelFiletype()
{
    QTreeWidgetItem *cur = d->w->filetypeList->currentItem();
    if (cur) {
        delete cur;
        emit changed();
    }
}

void KDesktopPropsPlugin::checkCommandChanged()
{
    if (KIO::DesktopExecParser::executableName(d->w->commandEdit->text()) !=
            KIO::DesktopExecParser::executableName(d->m_origCommandStr)) {
        d->m_origCommandStr = d->w->commandEdit->text();
        d->m_dbusStartupType.clear(); // Reset
        d->m_dbusServiceName.clear();
    }
}

void KDesktopPropsPlugin::applyChanges()
{
    // qDebug();
    KIO::StatJob *job = KIO::mostLocalUrl(properties->url());
    KJobWidgets::setWindow(job, properties);
    job->exec();
    const QUrl url = job->mostLocalUrl();

    if (!url.isLocalFile()) {
        KMessageBox::sorry(nullptr, i18n("Could not save properties. Only entries on local file systems are supported."));
        return;
    }

    const QString path(url.toLocalFile());

    // make sure the directory exists
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile f(path);
    if (!f.open(QIODevice::ReadWrite)) {
        KMessageBox::sorry(nullptr, i18n("<qt>Could not save properties. You do not have "
                                   "sufficient access to write to <b>%1</b>.</qt>", path));
        return;
    }
    f.close();

    // If the command is changed we reset certain settings that are strongly
    // coupled to the command.
    checkCommandChanged();

    KDesktopFile origConfig(d->m_origDesktopFile);
    QScopedPointer<KDesktopFile> _config(origConfig.copyTo(path));
    KConfigGroup config = _config->desktopGroup();
    config.writeEntry("Type", QStringLiteral("Application"));
    config.writeEntry("Comment", d->w->commentEdit->text());
    config.writeEntry("Comment", d->w->commentEdit->text(), KConfigGroup::Persistent | KConfigGroup::Localized); // for compat
    config.writeEntry("GenericName", d->w->genNameEdit->text());
    config.writeEntry("GenericName", d->w->genNameEdit->text(), KConfigGroup::Persistent | KConfigGroup::Localized); // for compat
    config.writeEntry("Exec", d->w->commandEdit->text());
    config.writeEntry("Path", d->w->pathEdit->lineEdit()->text());   // not writePathEntry, see kservice.cpp

    // Write mimeTypes
    QStringList mimeTypes;
    int count = d->w->filetypeList->topLevelItemCount();
    for (int i = 0; i < count; ++i) {
        QTreeWidgetItem *item = d->w->filetypeList->topLevelItem(i);
        QString preference = item->text(2);
        mimeTypes.append(item->text(0));
        if (!preference.isEmpty()) {
            mimeTypes.append(preference);
        }
    }

    // qDebug() << mimeTypes;
    config.writeXdgListEntry("MimeType", mimeTypes);

    if (!d->w->nameEdit->isHidden()) {
        QString nameStr = d->w->nameEdit->text();
        config.writeEntry("Name", nameStr);
        config.writeEntry("Name", nameStr, KConfigGroup::Persistent | KConfigGroup::Localized);
    }

    config.writeEntry("Terminal", d->m_terminalBool);
    config.writeEntry("TerminalOptions", d->m_terminalOptionStr);
    config.writeEntry("X-KDE-SubstituteUID", d->m_suidBool);
    config.writeEntry("X-KDE-Username", d->m_suidUserStr);
    if (d->m_hasDiscreteGpuBool) {
        config.writeEntry("X-KDE-RunOnDiscreteGpu", d->m_runOnDiscreteGpuBool);
    }
    config.writeEntry("StartupNotify", d->m_startupBool);
    config.writeEntry("X-DBUS-StartupType", d->m_dbusStartupType);
    config.writeEntry("X-DBUS-ServiceName", d->m_dbusServiceName);
    config.sync();

    // KSycoca update needed?
    bool updateNeeded = !relativeAppsLocation(path).isEmpty();
    if (updateNeeded) {
        KBuildSycocaProgressDialog::rebuildKSycoca(d->m_frame);
    }
}

void KDesktopPropsPlugin::slotBrowseExec()
{
    QUrl f = QFileDialog::getOpenFileUrl(d->m_frame);
    if (f.isEmpty()) {
        return;
    }

    if (!f.isLocalFile()) {
        KMessageBox::sorry(d->m_frame, i18n("Only executables on local file systems are supported."));
        return;
    }

    QString path = f.toLocalFile();
    path = KShell::quoteArg(path);
    d->w->commandEdit->setText(path);
}

void KDesktopPropsPlugin::slotAdvanced()
{
    QDialog dlg(d->m_frame);
    dlg.setObjectName(QStringLiteral("KPropertiesDesktopAdv"));
    dlg.setModal(true);
    dlg.setWindowTitle(i18n("Advanced Options for %1", properties->url().fileName()));

    Ui_KPropertiesDesktopAdvBase w;
    QWidget *mainWidget = new QWidget(&dlg);
    w.setupUi(mainWidget);

    QDialogButtonBox *buttonBox = new QDialogButtonBox(&dlg);
    buttonBox->setStandardButtons(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    connect(buttonBox, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    connect(buttonBox, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);

    QVBoxLayout *layout = new QVBoxLayout;
    layout->addWidget(mainWidget);
    layout->addWidget(buttonBox);
    dlg.setLayout(layout);

    // If the command is changed we reset certain settings that are strongly
    // coupled to the command.
    checkCommandChanged();

    // check to see if we use konsole if not do not add the nocloseonexit
    // because we don't know how to do this on other terminal applications
    KConfigGroup confGroup(KSharedConfig::openConfig(), QStringLiteral("General"));
    QString preferredTerminal = confGroup.readPathEntry("TerminalApplication",
                                QStringLiteral("konsole"));

    bool terminalCloseBool = false;

    if (preferredTerminal == QLatin1String("konsole")) {
        terminalCloseBool = d->m_terminalOptionStr.contains(QLatin1String("--noclose"));
        w.terminalCloseCheck->setChecked(terminalCloseBool);
        d->m_terminalOptionStr.remove(QStringLiteral("--noclose"));
    } else {
        w.terminalCloseCheck->hide();
    }

    w.terminalCheck->setChecked(d->m_terminalBool);
    w.terminalEdit->setText(d->m_terminalOptionStr);
    w.terminalCloseCheck->setEnabled(d->m_terminalBool);
    w.terminalEdit->setEnabled(d->m_terminalBool);
    w.terminalEditLabel->setEnabled(d->m_terminalBool);

    w.suidCheck->setChecked(d->m_suidBool);
    w.suidEdit->setText(d->m_suidUserStr);
    w.suidEdit->setEnabled(d->m_suidBool);
    w.suidEditLabel->setEnabled(d->m_suidBool);

    if (d->m_hasDiscreteGpuBool) {
        w.discreteGpuCheck->setChecked(d->m_runOnDiscreteGpuBool);
    } else {
        w.discreteGpuGroupBox->hide();
    }

    w.startupInfoCheck->setChecked(d->m_startupBool);

    if (d->m_dbusStartupType == QLatin1String("unique")) {
        w.dbusCombo->setCurrentIndex(2);
    } else if (d->m_dbusStartupType == QLatin1String("multi")) {
        w.dbusCombo->setCurrentIndex(1);
    } else if (d->m_dbusStartupType == QLatin1String("wait")) {
        w.dbusCombo->setCurrentIndex(3);
    } else {
        w.dbusCombo->setCurrentIndex(0);
    }

    // Provide username completion up to 1000 users.
    const int maxEntries = 1000;
    QStringList userNames = KUser::allUserNames(maxEntries);
    if (userNames.size() < maxEntries) {
        KCompletion *kcom = new KCompletion;
        kcom->setOrder(KCompletion::Sorted);
        w.suidEdit->setCompletionObject(kcom, true);
        w.suidEdit->setAutoDeleteCompletionObject(true);
        w.suidEdit->setCompletionMode(KCompletion::CompletionAuto);
        kcom->setItems(userNames);
    }

    connect(w.terminalEdit, &QLineEdit::textChanged,
            this, &KPropertiesDialogPlugin::changed);
    connect(w.terminalCloseCheck, &QAbstractButton::toggled,
            this, &KPropertiesDialogPlugin::changed);
    connect(w.terminalCheck, &QAbstractButton::toggled,
            this, &KPropertiesDialogPlugin::changed);
    connect(w.suidCheck, &QAbstractButton::toggled,
            this, &KPropertiesDialogPlugin::changed);
    connect(w.suidEdit, &QLineEdit::textChanged,
            this, &KPropertiesDialogPlugin::changed);
    connect(w.discreteGpuCheck, &QAbstractButton::toggled,
            this, &KPropertiesDialogPlugin::changed);
    connect(w.startupInfoCheck, &QAbstractButton::toggled,
            this, &KPropertiesDialogPlugin::changed);
    connect(w.dbusCombo, QOverload<int>::of(&QComboBox::activated),
            this, &KPropertiesDialogPlugin::changed);

    if (dlg.exec() == QDialog::Accepted) {
        d->m_terminalOptionStr = w.terminalEdit->text().trimmed();
        d->m_terminalBool = w.terminalCheck->isChecked();
        d->m_suidBool = w.suidCheck->isChecked();
        d->m_suidUserStr = w.suidEdit->text().trimmed();
        if (d->m_hasDiscreteGpuBool) {
            d->m_runOnDiscreteGpuBool = w.discreteGpuCheck->isChecked();
        }
        d->m_startupBool = w.startupInfoCheck->isChecked();

        if (w.terminalCloseCheck->isChecked()) {
            d->m_terminalOptionStr.append(QLatin1String(" --noclose"));
        }

        switch (w.dbusCombo->currentIndex()) {
        case 1:  d->m_dbusStartupType = QStringLiteral("multi"); break;
        case 2:  d->m_dbusStartupType = QStringLiteral("unique"); break;
        case 3:  d->m_dbusStartupType = QStringLiteral("wait"); break;
        default: d->m_dbusStartupType = QStringLiteral("none"); break;
        }
    }
}

bool KDesktopPropsPlugin::supports(const KFileItemList &_items)
{
    if (_items.count() != 1) {
        return false;
    }

    const KFileItem &item = _items.first();

    // check if desktop file
    if (!item.isDesktopFile()) {
        return false;
    }

    // open file and check type
    bool isLocal;
    QUrl url = item.mostLocalUrl(&isLocal);
    if (!isLocal) {
        return false;
    }

    KDesktopFile config(url.toLocalFile());
    return config.hasApplicationType() &&
           KAuthorized::authorize(QStringLiteral("run_desktop_files")) &&
           KAuthorized::authorize(QStringLiteral("shell_access"));
}

#include "moc_kpropertiesdialog.cpp"
#include "moc_kpropertiesdialog_p.cpp"
