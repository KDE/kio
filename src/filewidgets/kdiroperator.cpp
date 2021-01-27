/*
    This file is part of the KDE libraries
    SPDX-FileCopyrightText: 1999, 2000 Stephan Kulow <coolo@kde.org>
    SPDX-FileCopyrightText: 1999, 2000, 2001, 2002, 2003 Carsten Pfeiffer <pfeiffer@kde.org>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#include <config-kiofilewidgets.h>
#include <defaults-kfile.h> // ConfigGroup, DefaultShowHidden, DefaultDirsFirst, DefaultSortReversed

#include "kdirmodel.h"
#include "kdiroperator.h"
#include "kdiroperatordetailview_p.h"
#include "kdiroperatoriconview_p.h"
#include "kdirsortfilterproxymodel.h"
#include "kfileitem.h"
#include "kfilemetapreview_p.h"
#include "knewfilemenu.h"
#include "kpreviewwidgetbase.h"
#include <KActionCollection>
#include <KCompletion>
#include <KConfigGroup>
#include <KDirLister>
#include <KIO/OpenFileManagerWindowJob>
#include <KIO/RenameFileDialog>
#include <KIconLoader>
#include <KJobWidgets>
#include <KLocalizedString>
#include <KMessageBox>
#include <KProtocolManager>
#include <KSharedConfig>
#include <KUrlMimeData>
#include <kfileitemdelegate.h>
#include <kfilepreviewgenerator.h>
#include <kio/copyjob.h>
#include <kio/deletejob.h>
#include <kio/jobuidelegate.h>
#include <kio/previewjob.h>
#include <kpropertiesdialog.h>
#include <widgetsaskuseractionhandler.h>
#include "../pathhelpers_p.h"

#include <QApplication>
#include <QDebug>
#include <QHeaderView>
#include <QListView>
#include <QMenu>
#include <QMimeDatabase>
#include <QProgressBar>
#include <QRegularExpression>
#include <QStack>
#include <QSplitter>
#include <QTimer>
#include <QWheelEvent>

#include <memory>

template class QHash<QString, KFileItem>;

// QDir::SortByMask is not only undocumented, it also omits QDir::Type which  is another
// sorting mode.
static const int QDirSortMask = QDir::SortByMask | QDir::Type;

class KDirOperatorPrivate
{
public:
    explicit KDirOperatorPrivate(KDirOperator *qq)
        : q(qq)
    {
    }

    ~KDirOperatorPrivate();

    enum InlinePreviewState {
        ForcedToFalse = 0,
        ForcedToTrue,
        NotForced,
    };

    // private methods
    bool checkPreviewInternal() const;
    bool openUrl(const QUrl &url, KDirLister::OpenUrlFlags flags = KDirLister::NoFlags);
    int sortColumn() const;
    Qt::SortOrder sortOrder() const;
    void updateSorting(QDir::SortFlags sort);
    KIO::WidgetsAskUserActionHandler* askUserHandler();

    bool isReadable(const QUrl &url);
    bool isSchemeSupported(const QString &scheme) const;

    KFile::FileView allViews();

    QMetaObject::Connection m_connection;

    // A pair to store zoom settings for view kinds
    struct ZoomSettingsForView {
        QString name;
        int defaultValue;
    };

    // private slots
    void slotDetailedView();
    void slotSimpleView();
    void slotTreeView();
    void slotDetailedTreeView();
    void slotIconsView();
    void slotCompactView();
    void slotDetailsView();
    void slotToggleHidden(bool);
    void slotToggleAllowExpansion(bool);
    void togglePreview(bool);
    void toggleInlinePreviews(bool);
    void slotOpenFileManager();
    void slotSortByName();
    void slotSortBySize();
    void slotSortByDate();
    void slotSortByType();
    void slotSortReversed(bool doReverse);
    void slotToggleDirsFirst();
    void slotToggleIconsView();
    void slotToggleCompactView();
    void slotToggleDetailsView();
    void slotToggleIgnoreCase();
    void slotStarted();
    void slotProgress(int);
    void slotShowProgress();
    void slotIOFinished();
    void slotCanceled();
    void slotRedirected(const QUrl &);
    void slotProperties();
    void slotActivated(const QModelIndex &);
    void slotSelectionChanged();
    void openContextMenu(const QPoint &);
    void triggerPreview(const QModelIndex &);
    void showPreview();
    void slotSplitterMoved(int, int);
    void assureVisibleSelection();
    void synchronizeSortingState(int, Qt::SortOrder);
    void slotChangeDecorationPosition();
    void slotExpandToUrl(const QModelIndex &);
    void slotItemsChanged();
    void slotDirectoryCreated(const QUrl &);
    void slotAskUserDeleteResult(bool allowDelete, const QList<QUrl> &urls,
                                 KIO::AskUserActionInterface::DeletionType deletionType, QWidget *parent);

    int iconSizeForViewType(QAbstractItemView *itemView) const;
    void writeIconZoomSettingsIfNeeded();
    ZoomSettingsForView zoomSettingsForViewForView() const;

    // private members
    KDirOperator *const q;
    QStack<QUrl *> m_backStack;   ///< Contains all URLs you can reach with the back button.
    QStack<QUrl *> m_forwardStack; ///< Contains all URLs you can reach with the forward button.

    QModelIndex m_lastHoveredIndex;

    KDirLister *m_dirLister = nullptr;
    QUrl m_currUrl;

    KCompletion m_completion;
    KCompletion m_dirCompletion;
    QDir::SortFlags m_sorting;
    QStyleOptionViewItem::Position m_decorationPosition = QStyleOptionViewItem::Left;

    QSplitter *m_splitter = nullptr;

    QAbstractItemView *m_itemView = nullptr;
    KDirModel *m_dirModel = nullptr;
    KDirSortFilterProxyModel *m_proxyModel = nullptr;

    KFileItemList m_pendingMimeTypes;

    // the enum KFile::FileView as an int
    int m_viewKind;
    int m_defaultView;

    KFile::Modes m_mode;
    QProgressBar *m_progressBar = nullptr;

    KPreviewWidgetBase *m_preview = nullptr;
    QUrl m_previewUrl;
    int m_previewWidth = 0;

    bool m_completeListDirty = false;
    bool m_followNewDirectories = true;
    bool m_followSelectedDirectories = true;
    bool m_onlyDoubleClickSelectsFiles = !qApp->style()->styleHint(QStyle::SH_ItemView_ActivateItemOnSingleClick);

    QUrl m_lastUrl; // used for highlighting a directory on back/cdUp

    QTimer *m_progressDelayTimer = nullptr;
    KActionMenu *m_actionMenu = nullptr;
    KActionCollection *m_actionCollection = nullptr;
    KNewFileMenu *m_newFileMenu = nullptr;
    KConfigGroup *m_configGroup = nullptr;
    KFilePreviewGenerator *m_previewGenerator = nullptr;
    KActionMenu *m_decorationMenu = nullptr;
    KToggleAction *m_leftAction = nullptr;

    int m_dropOptions = 0;
    int m_iconSize = KIconLoader::SizeSmall;
    InlinePreviewState m_inlinePreviewState = NotForced;
    bool m_dirHighlighting = true;
    bool m_showPreviews = false;
    bool m_shouldFetchForItems = false;
    bool m_isSaving = false;

    QList<QUrl> m_itemsToBeSetAsCurrent;
    QStringList m_supportedSchemes;

    std::unique_ptr<KIO::WidgetsAskUserActionHandler> m_askUserHandler;
};

KDirOperatorPrivate::~KDirOperatorPrivate()
{
    delete m_itemView;
    m_itemView = nullptr;

    // TODO:
    // if (configGroup) {
    //     itemView->writeConfig(configGroup);
    // }

    qDeleteAll(m_backStack);
    qDeleteAll(m_forwardStack);

    // The parent KDirOperator will delete these
    m_preview = nullptr;
    m_proxyModel = nullptr;
    m_dirModel = nullptr;
    m_progressDelayTimer = nullptr;

    m_dirLister = nullptr; // deleted by KDirModel

    delete m_configGroup;
    m_configGroup = nullptr;
}

KDirOperator::KDirOperator(const QUrl &_url, QWidget *parent)
    : QWidget(parent), d(new KDirOperatorPrivate(this))
{
    d->m_splitter = new QSplitter(this);
    d->m_splitter->setChildrenCollapsible(false);
    connect(d->m_splitter, &QSplitter::splitterMoved, this, [this](int pos, int index) {
        d->slotSplitterMoved(pos, index);
    });

    d->m_preview = nullptr;

    d->m_mode = KFile::File;
    d->m_viewKind = KFile::Simple;

    if (_url.isEmpty()) { // no dir specified -> current dir
        QString strPath = QDir::currentPath();
        strPath.append(QLatin1Char('/'));
        d->m_currUrl = QUrl::fromLocalFile(strPath);
    } else {
        d->m_currUrl = _url;
        if (d->m_currUrl.scheme().isEmpty()) {
            d->m_currUrl.setScheme(QStringLiteral("file"));
        }

        QString path = d->m_currUrl.path();
        if (!path.endsWith(QLatin1Char('/'))) {
            path.append(QLatin1Char('/')); // make sure we have a trailing slash!
        }
        d->m_currUrl.setPath(path);
    }

    // We set the direction of this widget to LTR, since even on RTL desktops
    // viewing directory listings in RTL mode makes people's head explode.
    // Is this the correct place? Maybe it should be in some lower level widgets...?
    setLayoutDirection(Qt::LeftToRight);
    setDirLister(new KDirLister());

    connect(&d->m_completion, &KCompletion::match,
            this, &KDirOperator::slotCompletionMatch);

    d->m_progressBar = new QProgressBar(this);
    d->m_progressBar->setObjectName(QStringLiteral("d->m_progressBar"));
    d->m_progressBar->adjustSize();
    d->m_progressBar->move(2, height() - d->m_progressBar->height() - 2);

    d->m_progressDelayTimer = new QTimer(this);
    d->m_progressDelayTimer->setObjectName(QStringLiteral("d->m_progressBar delay timer"));
    connect(d->m_progressDelayTimer, &QTimer::timeout, this, [this]() { d->slotShowProgress(); });

    d->m_completeListDirty = false;

    // action stuff
    setupActions();
    setupMenu();

    d->m_sorting = QDir::NoSort;  //so updateSorting() doesn't think nothing has changed
    d->updateSorting(QDir::Name | QDir::DirsFirst);

    setFocusPolicy(Qt::WheelFocus);
    setAcceptDrops(true);
}

KDirOperator::~KDirOperator()
{
    resetCursor();
    disconnect(d->m_dirLister, nullptr, this, nullptr);
}

void KDirOperator::setSorting(QDir::SortFlags spec)
{
    d->updateSorting(spec);
}

QDir::SortFlags KDirOperator::sorting() const
{
    return d->m_sorting;
}

bool KDirOperator::isRoot() const
{
#ifdef Q_OS_WIN
    if (url().isLocalFile()) {
        const QString path = url().toLocalFile();
        if (path.length() == 3) {
            return (path[0].isLetter() && path[1] == QLatin1Char(':') && path[2] == QLatin1Char('/'));
        }
        return false;
    } else
#endif
        return url().path() == QLatin1String("/");
}

KDirLister *KDirOperator::dirLister() const
{
    return d->m_dirLister;
}

void KDirOperator::resetCursor()
{
    if (qApp) {
        QApplication::restoreOverrideCursor();
    }
    d->m_progressBar->hide();
}

void KDirOperator::sortByName()
{
    d->updateSorting((d->m_sorting & ~QDirSortMask) | QDir::Name);
}

void KDirOperator::sortBySize()
{
    d->updateSorting((d->m_sorting & ~QDirSortMask) | QDir::Size);
}

void KDirOperator::sortByDate()
{
    d->updateSorting((d->m_sorting & ~QDirSortMask) | QDir::Time);
}

void KDirOperator::sortByType()
{
    d->updateSorting((d->m_sorting & ~QDirSortMask) | QDir::Type);
}

void KDirOperator::sortReversed()
{
    // toggle it, hence the inversion of current state
    d->slotSortReversed(!(d->m_sorting & QDir::Reversed));
}

void KDirOperator::toggleDirsFirst()
{
    d->slotToggleDirsFirst();
}

void KDirOperator::toggleIgnoreCase()
{
    if (d->m_proxyModel != nullptr) {
        Qt::CaseSensitivity cs = d->m_proxyModel->sortCaseSensitivity();
        cs = (cs == Qt::CaseSensitive) ? Qt::CaseInsensitive : Qt::CaseSensitive;
        d->m_proxyModel->setSortCaseSensitivity(cs);
    }
}

void KDirOperator::updateSelectionDependentActions()
{
    const bool hasSelection = (d->m_itemView != nullptr) &&
                              d->m_itemView->selectionModel()->hasSelection();
    d->m_actionCollection->action(QStringLiteral("rename"))->setEnabled(hasSelection);
    d->m_actionCollection->action(QStringLiteral("trash"))->setEnabled(hasSelection);
    d->m_actionCollection->action(QStringLiteral("delete"))->setEnabled(hasSelection);
    d->m_actionCollection->action(QStringLiteral("properties"))->setEnabled(hasSelection);
}

void KDirOperator::setPreviewWidget(KPreviewWidgetBase *w)
{
    const bool showPreview = (w != nullptr);
    if (showPreview) {
        d->m_viewKind = (d->m_viewKind | KFile::PreviewContents);
    } else {
        d->m_viewKind = (d->m_viewKind & ~KFile::PreviewContents);
    }

    delete d->m_preview;
    d->m_preview = w;

    if (w) {
        d->m_splitter->addWidget(w);
    }

    KToggleAction *previewAction = static_cast<KToggleAction *>(d->m_actionCollection->action(QStringLiteral("preview")));
    previewAction->setEnabled(showPreview);
    previewAction->setChecked(showPreview);
    setView(static_cast<KFile::FileView>(d->m_viewKind));
}

KFileItemList KDirOperator::selectedItems() const
{
    KFileItemList itemList;
    if (d->m_itemView == nullptr) {
        return itemList;
    }

    const QItemSelection selection = d->m_proxyModel->mapSelectionToSource(d->m_itemView->selectionModel()->selection());

    const QModelIndexList indexList = selection.indexes();
    for (const QModelIndex &index : indexList) {
        KFileItem item = d->m_dirModel->itemForIndex(index);
        if (!item.isNull()) {
            itemList.append(item);
        }
    }

    return itemList;
}

bool KDirOperator::isSelected(const KFileItem &item) const
{
    if ((item.isNull()) || (d->m_itemView == nullptr)) {
        return false;
    }

    const QModelIndex dirIndex = d->m_dirModel->indexForItem(item);
    const QModelIndex proxyIndex = d->m_proxyModel->mapFromSource(dirIndex);
    return d->m_itemView->selectionModel()->isSelected(proxyIndex);
}

int KDirOperator::numDirs() const
{
    return (d->m_dirLister == nullptr) ? 0 : d->m_dirLister->directories().count();
}

int KDirOperator::numFiles() const
{
    return (d->m_dirLister == nullptr) ? 0 : d->m_dirLister->items().count() - numDirs();
}

KCompletion *KDirOperator::completionObject() const
{
    return const_cast<KCompletion *>(&d->m_completion);
}

KCompletion *KDirOperator::dirCompletionObject() const
{
    return const_cast<KCompletion *>(&d->m_dirCompletion);
}

KActionCollection *KDirOperator::actionCollection() const
{
    return d->m_actionCollection;
}

KFile::FileView KDirOperatorPrivate::allViews()
{
    return static_cast<KFile::FileView>(KFile::Simple | KFile::Detail | KFile::Tree | KFile::DetailTree);
}

void KDirOperatorPrivate::slotDetailedView()
{
    // save old zoom settings
    writeIconZoomSettingsIfNeeded();

    KFile::FileView view = static_cast<KFile::FileView>((m_viewKind & ~allViews()) | KFile::Detail);
    q->setView(view);
}

void KDirOperatorPrivate::slotSimpleView()
{
    // save old zoom settings
    writeIconZoomSettingsIfNeeded();

    KFile::FileView view = static_cast<KFile::FileView>((m_viewKind & ~allViews()) | KFile::Simple);
    q->setView(view);
}

void KDirOperatorPrivate::slotTreeView()
{
    // save old zoom settings
    writeIconZoomSettingsIfNeeded();

    KFile::FileView view = static_cast<KFile::FileView>((m_viewKind & ~allViews()) | KFile::Tree);
    q->setView(view);
}

void KDirOperatorPrivate::slotDetailedTreeView()
{
    // save old zoom settings
    writeIconZoomSettingsIfNeeded();

    KFile::FileView view = static_cast<KFile::FileView>((m_viewKind & ~allViews()) | KFile::DetailTree);
    q->setView(view);
}

void KDirOperatorPrivate::slotToggleAllowExpansion(bool allow) {
    KFile::FileView view = KFile::Detail;
    if (allow) {
        view = KFile::DetailTree;
    }
    q->setView(view);
}

void KDirOperatorPrivate::slotToggleHidden(bool show)
{
    m_dirLister->setShowingDotFiles(show);
    q->updateDir();
    assureVisibleSelection();
}

void KDirOperatorPrivate::togglePreview(bool on)
{
    if (on) {
        m_viewKind |= KFile::PreviewContents;
        if (m_preview == nullptr) {
            m_preview = new KFileMetaPreview(q);
            m_actionCollection->action(QStringLiteral("preview"))->setChecked(true);
            m_splitter->addWidget(m_preview);
        }

        m_preview->show();

        QMetaObject::invokeMethod(q, [this]() { assureVisibleSelection(); }, Qt::QueuedConnection);
        if (m_itemView != nullptr) {
            const QModelIndex index = m_itemView->selectionModel()->currentIndex();
            if (index.isValid()) {
                triggerPreview(index);
            }
        }
    } else if (m_preview != nullptr) {
        m_viewKind = m_viewKind & ~KFile::PreviewContents;
        m_preview->hide();
    }
}

void KDirOperatorPrivate::toggleInlinePreviews(bool show)
{
    if (m_showPreviews == show) {
        return;
    }

    m_showPreviews = show;

    if (!m_previewGenerator) {
        return;
    }

    m_previewGenerator->setPreviewShown(show);
}

void KDirOperatorPrivate::slotOpenFileManager()
{
    const KFileItemList list = q->selectedItems();
    if (list.isEmpty()) {
        KIO::highlightInFileManager({m_currUrl.adjusted(QUrl::StripTrailingSlash)});
    } else {
        KIO::highlightInFileManager(list.urlList());
    }
}

void KDirOperatorPrivate::slotSortByName()
{
    q->sortByName();
}

void KDirOperatorPrivate::slotSortBySize()
{
    q->sortBySize();
}

void KDirOperatorPrivate::slotSortByDate()
{
    q->sortByDate();
}

void KDirOperatorPrivate::slotSortByType()
{
    q->sortByType();
}

void KDirOperatorPrivate::slotSortReversed(bool doReverse)
{
    QDir::SortFlags s = m_sorting & ~QDir::Reversed;
    if (doReverse) {
        s |= QDir::Reversed;
    }
    updateSorting(s);
}

void KDirOperatorPrivate::slotToggleDirsFirst()
{
    QDir::SortFlags s = (m_sorting ^ QDir::DirsFirst);
    updateSorting(s);
}

void KDirOperatorPrivate::slotIconsView()
{
    // save old zoom settings
    writeIconZoomSettingsIfNeeded();

    // Put the icons on top
    m_actionCollection->action(QStringLiteral("decorationAtTop"))->setChecked(true);
    m_decorationPosition = QStyleOptionViewItem::Top;

    // Switch to simple view
    KFile::FileView fileView = static_cast<KFile::FileView>((m_viewKind & ~allViews()) | KFile::Simple);
    q->setView(fileView);
}

void KDirOperatorPrivate::slotCompactView()
{
    // save old zoom settings
    writeIconZoomSettingsIfNeeded();

    // Put the icons on the side
    m_actionCollection->action(QStringLiteral("decorationAtLeft"))->setChecked(true);
    m_decorationPosition = QStyleOptionViewItem::Left;

    // Switch to simple view
    KFile::FileView fileView = static_cast<KFile::FileView>((m_viewKind & ~allViews()) | KFile::Simple);
    q->setView(fileView);
}

void KDirOperatorPrivate::slotDetailsView()
{
    // save old zoom settings
    writeIconZoomSettingsIfNeeded();

    KFile::FileView view;
    if (m_actionCollection->action(QStringLiteral("allow expansion"))->isChecked()) {
        view = static_cast<KFile::FileView>((m_viewKind & ~allViews()) | KFile::DetailTree);
    } else {
        view = static_cast<KFile::FileView>((m_viewKind & ~allViews()) | KFile::Detail);
    }
    q->setView(view);
}

void KDirOperatorPrivate::slotToggleIgnoreCase()
{
    // TODO: port to Qt4's QAbstractItemView
    /*if ( !d->fileView )
      return;

    QDir::SortFlags sorting = d->fileView->sorting();
    if ( !KFile::isSortCaseInsensitive( sorting ) )
        d->fileView->setSorting( sorting | QDir::IgnoreCase );
    else
        d->fileView->setSorting( sorting & ~QDir::IgnoreCase );
    d->sorting = d->fileView->sorting();*/
}

void KDirOperator::mkdir()
{
    d->m_newFileMenu->setPopupFiles(QList<QUrl>{url()});
    d->m_newFileMenu->setViewShowsHiddenFiles(showHiddenFiles());
    d->m_newFileMenu->createDirectory();
}

#if KIOFILEWIDGETS_BUILD_DEPRECATED_SINCE(5, 78)
bool KDirOperator::mkdir(const QString &directory, bool enterDirectory)
{
    // Creates "directory", relative to the current directory (d->currUrl).
    // The given path may contain any number directories, existent or not.
    // They will all be created, if possible.

    // TODO: very similar to KDirSelectDialog::Private::slotMkdir

    bool writeOk = false;
    bool exists = false;
    QUrl folderurl(d->m_currUrl);

    const QStringList dirs = directory.split(QLatin1Char('/'), Qt::SkipEmptyParts);
    QStringList::ConstIterator it = dirs.begin();

    for (; it != dirs.end(); ++it) {
        folderurl.setPath(concatPaths(folderurl.path(), *it));
        if (folderurl.isLocalFile()) {
            exists = QFile::exists(folderurl.toLocalFile());
        } else {
            KIO::StatJob *job = KIO::stat(folderurl);
            KJobWidgets::setWindow(job, this);
            job->setDetails(KIO::StatNoDetails); //We only want to know if it exists
            job->setSide(KIO::StatJob::DestinationSide);
            exists = job->exec();
        }

        if (!exists) {
            KIO::Job *job = KIO::mkdir(folderurl);
            KJobWidgets::setWindow(job, this);
            writeOk = job->exec();
        }
    }

    if (exists) { // url was already existent
        KMessageBox::sorry(d->m_itemView, i18n("A file or folder named %1 already exists.",
                                             folderurl.toDisplayString(QUrl::PreferLocalFile)));
    } else if (!writeOk) {
        KMessageBox::sorry(d->m_itemView, i18n("You do not have permission to "
                                             "create that folder."));
    } else if (enterDirectory) {
        setUrl(folderurl, true);
    }

    return writeOk;
}
#endif

KIO::DeleteJob *KDirOperator::del(const KFileItemList &items,
                                  QWidget *parent,
                                  bool ask, bool showProgress)
{
    if (items.isEmpty()) {
        KMessageBox::information(parent,
                                 i18n("You did not select a file to delete."),
                                 i18n("Nothing to Delete"));
        return nullptr;
    }

    if (parent == nullptr) {
        parent = this;
    }

    const QList<QUrl> urls = items.urlList();

    bool doIt = !ask;
    if (ask) {
        KIO::JobUiDelegate uiDelegate;
        uiDelegate.setWindow(parent);
        doIt = uiDelegate.askDeleteConfirmation(urls, KIO::JobUiDelegate::Delete, KIO::JobUiDelegate::DefaultConfirmation);
    }

    if (doIt) {
        KIO::JobFlags flags = showProgress ? KIO::DefaultFlags : KIO::HideProgressInfo;
        KIO::DeleteJob *job = KIO::del(urls, flags);
        KJobWidgets::setWindow(job, this);
        job->uiDelegate()->setAutoErrorHandlingEnabled(true);
        return job;
    }

    return nullptr;
}

KIO::WidgetsAskUserActionHandler* KDirOperatorPrivate::askUserHandler()
{
    if (m_askUserHandler) {
        return m_askUserHandler.get();
    }

    m_askUserHandler.reset(new KIO::WidgetsAskUserActionHandler{});
    QObject::connect(m_askUserHandler.get(), &KIO::WidgetsAskUserActionHandler::askUserDeleteResult,
                     q, [this](bool allowDelete, const QList<QUrl> &urls,
                               KIO::AskUserActionInterface::DeletionType deletionType, QWidget *parentWidget) {
                        slotAskUserDeleteResult(allowDelete, urls, deletionType, parentWidget);
                     });

    return m_askUserHandler.get();
}

void KDirOperator::deleteSelected()
{
    const QList<QUrl> urls = selectedItems().urlList();
    if (urls.isEmpty()) {
        KMessageBox::information(this,
                                 i18n("You did not select a file to delete."),
                                 i18n("Nothing to Delete"));
        return;
    }

    d->askUserHandler()->askUserDelete(urls, KIO::AskUserActionInterface::Delete,
                                       KIO::AskUserActionInterface::DefaultConfirmation, this);
}

KIO::CopyJob *KDirOperator::trash(const KFileItemList &items,
                                  QWidget *parent,
                                  bool ask, bool showProgress)
{
    if (items.isEmpty()) {
        KMessageBox::information(parent,
                                 i18n("You did not select a file to trash."),
                                 i18n("Nothing to Trash"));
        return nullptr;
    }

    const QList<QUrl> urls = items.urlList();

    bool doIt = !ask;
    if (ask) {
        KIO::JobUiDelegate uiDelegate;
        uiDelegate.setWindow(parent);
        doIt = uiDelegate.askDeleteConfirmation(urls, KIO::JobUiDelegate::Trash, KIO::JobUiDelegate::DefaultConfirmation);
    }

    if (doIt) {
        KIO::JobFlags flags = showProgress ? KIO::DefaultFlags : KIO::HideProgressInfo;
        KIO::CopyJob *job = KIO::trash(urls, flags);
        KJobWidgets::setWindow(job, this);
        job->uiDelegate()->setAutoErrorHandlingEnabled(true);
        return job;
    }

    return nullptr;
}

KFilePreviewGenerator *KDirOperator::previewGenerator() const
{
    return d->m_previewGenerator;
}

void KDirOperator::setInlinePreviewShown(bool show)
{
    d->m_inlinePreviewState = show ? KDirOperatorPrivate::ForcedToTrue : KDirOperatorPrivate::ForcedToFalse;
}

bool KDirOperator::isInlinePreviewShown() const
{
    return d->m_showPreviews;
}

#if KIOFILEWIDGETS_BUILD_DEPRECATED_SINCE(5, 76)
int KDirOperator::iconsZoom() const
{
    const int stepSize = (KIconLoader::SizeEnormous - KIconLoader::SizeSmall) / 100;
    const int zoom = (d->m_iconSize / stepSize) - KIconLoader::SizeSmall;
    return zoom;
}
#endif

int KDirOperator::iconSize() const
{
    return d->m_iconSize;
}

void KDirOperator::setIsSaving(bool isSaving)
{
    d->m_isSaving = isSaving;
}

bool KDirOperator::isSaving() const
{
    return d->m_isSaving;
}

void KDirOperator::renameSelected()
{
    if (d->m_itemView == nullptr) {
        return;
    }

    const KFileItemList items = selectedItems();
    if (items.isEmpty()) {
        return;
    }

    KIO::RenameFileDialog* dialog = new KIO::RenameFileDialog(items, this);
    connect(dialog, &KIO::RenameFileDialog::renamingFinished, this, [this]() { d->assureVisibleSelection(); });

    dialog->open();
}

void KDirOperator::trashSelected()
{
    if (d->m_itemView == nullptr) {
        return;
    }

    if (QApplication::keyboardModifiers() & Qt::ShiftModifier) {
        deleteSelected();
        return;
    }

    const QList<QUrl> urls = selectedItems().urlList();
    if (urls.isEmpty()) {
        KMessageBox::information(this,
                                 i18n("You did not select a file to trash."),
                                 i18n("Nothing to Trash"));
        return;
    }

    d->askUserHandler()->askUserDelete(urls, KIO::AskUserActionInterface::Trash,
                                       KIO::AskUserActionInterface::DefaultConfirmation, this);
}

void KDirOperatorPrivate::slotAskUserDeleteResult(bool allowDelete, const QList<QUrl> &urls,
                                                       KIO::AskUserActionInterface::DeletionType deletionType,
                                                       QWidget *parentWidget)
{
    if (parentWidget != q || !allowDelete) {
        return;
    }

    KIO::Job *job = nullptr;
    if (deletionType == KIO::AskUserActionInterface::Trash) {
        job = KIO::trash(urls);
    } else if (deletionType == KIO::AskUserActionInterface::Delete) {
        job = KIO::del(urls, KIO::DefaultFlags);
    }

    if (!job) {
        return;
    }
    KJobWidgets::setWindow(job, q);
    job->uiDelegate()->setAutoErrorHandlingEnabled(true);
}

#if KIOFILEWIDGETS_BUILD_DEPRECATED_SINCE(5, 76)
void KDirOperator::setIconsZoom(int _value)
{
    int value = _value;
    value = qMin(100, value);
    value = qMax(0, value);
    const int stepSize = (KIconLoader::SizeEnormous - KIconLoader::SizeSmall) / 100;
    const int val = (value * stepSize) + KIconLoader::SizeSmall;
    setIconSize(val);
}
#endif

void KDirOperator::setIconSize(int value)
{
    if (d->m_iconSize == value) {
        return;
    }

    int size = value;
    size = qMin(static_cast<int>(KIconLoader::SizeEnormous), size);
    size = qMax(static_cast<int>(KIconLoader::SizeSmall), size);

    d->m_iconSize = size;

    if (!d->m_previewGenerator) {
        return;
    }

    d->m_itemView->setIconSize(QSize(size, size));
    d->m_previewGenerator->updateIcons();

    Q_EMIT currentIconSizeChanged(size);
}

void KDirOperator::close()
{
    resetCursor();
    d->m_pendingMimeTypes.clear();
    d->m_completion.clear();
    d->m_dirCompletion.clear();
    d->m_completeListDirty = true;
    d->m_dirLister->stop();
}

void KDirOperator::setUrl(const QUrl &_newurl, bool clearforward)
{
    QUrl newurl;

    if (!_newurl.isValid()) {
        newurl = QUrl::fromLocalFile(QDir::homePath());
    } else {
        newurl = _newurl.adjusted(QUrl::NormalizePathSegments);
    }

    if (!newurl.path().isEmpty() && !newurl.path().endsWith(QLatin1Char('/'))) {
        newurl.setPath(newurl.path() + QLatin1Char('/'));
    }

    // already set
    if (newurl.matches(d->m_currUrl, QUrl::StripTrailingSlash)) {
        return;
    }

    if (!d->isSchemeSupported(newurl.scheme()))
        return;

    if (!d->isReadable(newurl)) {
        // maybe newurl is a file? check its parent directory
        newurl = newurl.adjusted(QUrl::RemoveFilename | QUrl::StripTrailingSlash);
        if (newurl.matches(d->m_currUrl, QUrl::StripTrailingSlash)) {
            return;    // parent is current dir, nothing to do (fixes #173454, too)
        }
        KIO::StatJob *job = KIO::stat(newurl);
        KJobWidgets::setWindow(job, this);
        bool res = job->exec();

        KIO::UDSEntry entry = job->statResult();
        KFileItem i(entry, newurl);
        if ((!res || !d->isReadable(newurl)) && i.isDir()) {
            resetCursor();
            KMessageBox::error(d->m_itemView,
                               i18n("The specified folder does not exist "
                                    "or was not readable."));
            return;
        } else if (!i.isDir()) {
            return;
        }
    }

    if (clearforward) {
        // autodelete should remove this one
        d->m_backStack.push(new QUrl(d->m_currUrl));
        qDeleteAll(d->m_forwardStack);
        d->m_forwardStack.clear();
    }

    d->m_currUrl = newurl;

    pathChanged();
    Q_EMIT urlEntered(newurl);

    // enable/disable actions
    QAction *forwardAction = d->m_actionCollection->action(QStringLiteral("forward"));
    forwardAction->setEnabled(!d->m_forwardStack.isEmpty());

    QAction *backAction = d->m_actionCollection->action(QStringLiteral("back"));
    backAction->setEnabled(!d->m_backStack.isEmpty());

    QAction *upAction = d->m_actionCollection->action(QStringLiteral("up"));
    upAction->setEnabled(!isRoot());

    d->openUrl(newurl);
}

void KDirOperator::updateDir()
{
    QApplication::setOverrideCursor(Qt::WaitCursor);
    d->m_dirLister->emitChanges();
    QApplication::restoreOverrideCursor();
}

void KDirOperator::rereadDir()
{
    pathChanged();
    d->openUrl(d->m_currUrl, KDirLister::Reload);
}

bool KDirOperatorPrivate::isSchemeSupported(const QString &scheme) const
{
    return m_supportedSchemes.isEmpty() || m_supportedSchemes.contains(scheme);
}

bool KDirOperatorPrivate::openUrl(const QUrl &url, KDirLister::OpenUrlFlags flags)
{
    const bool result = KProtocolManager::supportsListing(url)
                        && isSchemeSupported(url.scheme())
                        && m_dirLister->openUrl(url, flags);
    if (!result) { // in that case, neither completed() nor canceled() will be emitted by KDL
        slotCanceled();
    }

    return result;
}

int KDirOperatorPrivate::sortColumn() const
{
    int column = KDirModel::Name;
    if (KFile::isSortByDate(m_sorting)) {
        column = KDirModel::ModifiedTime;
    } else if (KFile::isSortBySize(m_sorting)) {
        column = KDirModel::Size;
    } else if (KFile::isSortByType(m_sorting)) {
        column = KDirModel::Type;
    } else {
        Q_ASSERT(KFile::isSortByName(m_sorting));
    }

    return column;
}

Qt::SortOrder KDirOperatorPrivate::sortOrder() const
{
    return (m_sorting & QDir::Reversed) ? Qt::DescendingOrder :
           Qt::AscendingOrder;
}

void KDirOperatorPrivate::updateSorting(QDir::SortFlags sort)
{
    // qDebug() << "changing sort flags from"  << m_sorting << "to" << sort;
    if (sort == m_sorting) {
        return;
    }

    if ((m_sorting ^ sort) & QDir::DirsFirst) {
        // The "Folders First" setting has been changed.
        // We need to make sure that the files and folders are really re-sorted.
        // Without the following intermediate "fake resorting",
        // QSortFilterProxyModel::sort(int column, Qt::SortOrder order)
        // would do nothing because neither the column nor the sort order have been changed.
        Qt::SortOrder tmpSortOrder = (sortOrder() == Qt::AscendingOrder ? Qt::DescendingOrder : Qt::AscendingOrder);
        m_proxyModel->sort(sortOrder(), tmpSortOrder);
        m_proxyModel->setSortFoldersFirst(sort & QDir::DirsFirst);
    }

    m_sorting = sort;
    q->updateSortActions();
    m_proxyModel->sort(sortColumn(), sortOrder());

    // TODO: The headers from QTreeView don't take care about a sorting
    // change of the proxy model hence they must be updated the manually.
    // This is done here by a qobject_cast, but it would be nicer to:
    // - provide a signal 'sortingChanged()'
    // - connect KDirOperatorDetailView() with this signal and update the
    //   header internally
    QTreeView *treeView = qobject_cast<QTreeView *>(m_itemView);
    if (treeView != nullptr) {
        QHeaderView *headerView = treeView->header();
        headerView->blockSignals(true);
        headerView->setSortIndicator(sortColumn(), sortOrder());
        headerView->blockSignals(false);
    }

    assureVisibleSelection();
}

// Protected
void KDirOperator::pathChanged()
{
    if (d->m_itemView == nullptr) {
        return;
    }

    d->m_pendingMimeTypes.clear();
    //d->fileView->clear(); TODO
    d->m_completion.clear();
    d->m_dirCompletion.clear();

    // it may be, that we weren't ready at this time
    QApplication::restoreOverrideCursor();

    // when KIO::Job emits finished, the slot will restore the cursor
    QApplication::setOverrideCursor(Qt::WaitCursor);

    if (!d->isReadable(d->m_currUrl)) {
        KMessageBox::error(d->m_itemView,
                           i18n("The specified folder does not exist "
                                "or was not readable."));
        if (d->m_backStack.isEmpty()) {
            home();
        } else {
            back();
        }
    }
}

void KDirOperatorPrivate::slotRedirected(const QUrl &newURL)
{
    m_currUrl = newURL;
    m_pendingMimeTypes.clear();
    m_completion.clear();
    m_dirCompletion.clear();
    m_completeListDirty = true;
    Q_EMIT q->urlEntered(newURL);
}

// Code pinched from kfm then hacked
void KDirOperator::back()
{
    if (d->m_backStack.isEmpty()) {
        return;
    }

    d->m_forwardStack.push(new QUrl(d->m_currUrl));

    QUrl *s = d->m_backStack.pop();
    const QUrl newUrl = *s;
    delete s;

    if (d->m_dirHighlighting) {
        const QUrl _url =
            newUrl.adjusted(QUrl::StripTrailingSlash).adjusted(QUrl::RemoveFilename | QUrl::StripTrailingSlash);

        if (_url == d->m_currUrl.adjusted(QUrl::StripTrailingSlash) && !d->m_backStack.isEmpty()) {
            // e.g. started in a/b/c, cdUp() twice to "a", then back(), we highlight "c"
            d->m_lastUrl = *(d->m_backStack.top());
        } else {
            d->m_lastUrl = d->m_currUrl;
        }
    }

    setUrl(newUrl, false);
}

// Code pinched from kfm then hacked
void KDirOperator::forward()
{
    if (d->m_forwardStack.isEmpty()) {
        return;
    }

    d->m_backStack.push(new QUrl(d->m_currUrl));

    QUrl *s = d->m_forwardStack.pop();
    setUrl(*s, false);
    delete s;
}

QUrl KDirOperator::url() const
{
    return d->m_currUrl;
}

void KDirOperator::cdUp()
{
    // Allow /d/c// to go up to /d/ instead of /d/c/
    QUrl tmp(d->m_currUrl.adjusted(QUrl::NormalizePathSegments));

    if (d->m_dirHighlighting) {
        d->m_lastUrl = d->m_currUrl;
    }

    setUrl(tmp.resolved(QUrl(QStringLiteral(".."))), true);
}

void KDirOperator::home()
{
    setUrl(QUrl::fromLocalFile(QDir::homePath()), true);
}

void KDirOperator::clearFilter()
{
    d->m_dirLister->setNameFilter(QString());
    d->m_dirLister->clearMimeFilter();
    checkPreviewSupport();
}

void KDirOperator::setNameFilter(const QString &filter)
{
    d->m_dirLister->setNameFilter(filter);
    checkPreviewSupport();
}

QString KDirOperator::nameFilter() const
{
    return d->m_dirLister->nameFilter();
}

void KDirOperator::setMimeFilter(const QStringList &mimetypes)
{
    d->m_dirLister->setMimeFilter(mimetypes);
    checkPreviewSupport();
}

QStringList KDirOperator::mimeFilter() const
{
    return d->m_dirLister->mimeFilters();
}

void KDirOperator::setNewFileMenuSupportedMimeTypes(const QStringList &mimeTypes)
{
    d->m_newFileMenu->setSupportedMimeTypes(mimeTypes);
}

QStringList KDirOperator::newFileMenuSupportedMimeTypes() const
{
    return d->m_newFileMenu->supportedMimeTypes();
}

void KDirOperator::setNewFileMenuSelectDirWhenAlreadyExist(bool selectOnDirExists)
{
    d->m_newFileMenu->setSelectDirWhenAlreadyExist(selectOnDirExists);
}

bool KDirOperator::checkPreviewSupport()
{
    KToggleAction *previewAction = static_cast<KToggleAction *>(d->m_actionCollection->action(QStringLiteral("preview")));

    bool hasPreviewSupport = false;
    KConfigGroup cg(KSharedConfig::openConfig(), ConfigGroup);
    if (cg.readEntry("Show Default Preview", true)) {
        hasPreviewSupport = d->checkPreviewInternal();
    }

    previewAction->setEnabled(hasPreviewSupport);
    return hasPreviewSupport;
}

void KDirOperator::activatedMenu(const KFileItem &item, const QPoint &pos)
{
    updateSelectionDependentActions();

    d->m_newFileMenu->setPopupFiles(QList<QUrl>{item.url()});
    d->m_newFileMenu->setViewShowsHiddenFiles(showHiddenFiles());
    d->m_newFileMenu->checkUpToDate();

    d->m_actionCollection->action(QStringLiteral("new"))->setEnabled(item.isDir());

    Q_EMIT contextMenuAboutToShow(item, d->m_actionMenu->menu());

    d->m_actionMenu->menu()->exec(pos);
}

void KDirOperator::changeEvent(QEvent *event)
{
    QWidget::changeEvent(event);
}

bool KDirOperator::eventFilter(QObject *watched, QEvent *event)
{
    Q_UNUSED(watched);

    // If we are not hovering any items, check if there is a current index
    // set. In that case, we show the preview of that item.
    switch (event->type()) {
    case QEvent::MouseMove: {
        if (d->m_preview && !d->m_preview->isHidden()) {
            const QModelIndex hoveredIndex = d->m_itemView->indexAt(d->m_itemView->viewport()->mapFromGlobal(QCursor::pos()));

            if (d->m_lastHoveredIndex == hoveredIndex) {
                return QWidget::eventFilter(watched, event);
            }

            d->m_lastHoveredIndex = hoveredIndex;

            const QModelIndex currentIndex = d->m_itemView->selectionModel() ? d->m_itemView->selectionModel()->currentIndex()
                                             : QModelIndex();

            if (!hoveredIndex.isValid() && currentIndex.isValid() &&
                    (d->m_lastHoveredIndex != currentIndex)) {
                const KFileItem item = d->m_itemView->model()->data(currentIndex, KDirModel::FileItemRole).value<KFileItem>();
                if (!item.isNull()) {
                    d->m_preview->showPreview(item.url());
                }
            }
        }
    }
    break;
    case QEvent::Leave: {
        if (d->m_preview && !d->m_preview->isHidden()) {
            // when mouse leaves the view, show preview of selected file
            const QModelIndex currentIndex = d->m_itemView->selectionModel() ? d->m_itemView->selectionModel()->currentIndex()
                                             : QModelIndex();
            if (currentIndex.isValid()) {
                const KFileItem item = d->m_itemView->model()->data(currentIndex, KDirModel::FileItemRole).value<KFileItem>();
                if (!item.isNull()) {
                    d->m_preview->showPreview(item.url());
                }
            }
        }
    }
    break;
    case QEvent::MouseButtonRelease: {
        if (d->m_preview != nullptr && !d->m_preview->isHidden()) {
            const QModelIndex hoveredIndex = d->m_itemView->indexAt(d->m_itemView->viewport()->mapFromGlobal(QCursor::pos()));
            const QModelIndex focusedIndex = d->m_itemView->selectionModel() ? d->m_itemView->selectionModel()->currentIndex()
                                             : QModelIndex();

            if (((!focusedIndex.isValid()) ||
                    !d->m_itemView->selectionModel()->isSelected(focusedIndex)) &&
                    (!hoveredIndex.isValid())) {
                d->m_preview->clearPreview();
            }
        }
        QMouseEvent *mouseEvent = static_cast<QMouseEvent *>(event);
        if (mouseEvent) {
            switch (mouseEvent->button()) {
            case Qt::BackButton:
                back();
                break;
            case Qt::ForwardButton:
                forward();
                break;
            default:
                break;
            }
        }
    }
    break;
    case QEvent::Wheel: {
        QWheelEvent *evt = static_cast<QWheelEvent *>(event);
        if (evt->modifiers() & Qt::ControlModifier) {
            if (evt->angleDelta().y() > 0) {
                setIconSize(d->m_iconSize + 10);
            } else {
                setIconSize(d->m_iconSize - 10);
            }
            return true;
        }
    }
    break;
    case QEvent::DragEnter: {
        // Accepts drops of one file or folder only
        QDragEnterEvent *evt = static_cast<QDragEnterEvent *>(event);
        const QList<QUrl> urls = KUrlMimeData::urlsFromMimeData(evt->mimeData(), KUrlMimeData::DecodeOptions::PreferLocalUrls);

        // only one file/folder can be dropped at the moment
        if (urls.size() != 1) {
            evt->ignore();

        } else {
            // MIME type filtering
            bool mimeFilterPass = true;
            const QStringList mimeFilters = d->m_dirLister->mimeFilters();

            if (mimeFilters.size() > 1) {
                mimeFilterPass = false;
                const QUrl &url = urls.constFirst();

                QMimeDatabase mimeDataBase;
                QMimeType fileMimeType = mimeDataBase.mimeTypeForUrl(url);

                QRegularExpression regex;
                for (const auto& mimeFilter : mimeFilters) {
                    regex.setPattern(mimeFilter);
                    if (regex.match(fileMimeType.name()).hasMatch()) {   // matches!
                        mimeFilterPass = true;
                        break;
                    }
                }
            }

            event->setAccepted(mimeFilterPass);
        }

        return true;
    }
    case QEvent::Drop: {
        QDropEvent *evt = static_cast<QDropEvent *>(event);
        const QList<QUrl> urls = KUrlMimeData::urlsFromMimeData(evt->mimeData(), KUrlMimeData::DecodeOptions::PreferLocalUrls);

        const QUrl &url = urls.constFirst();

        // stat the url to get details
        KIO::StatJob *job = KIO::stat(url, KIO::HideProgressInfo);
        job->exec();

        setFocus();

        const KIO::UDSEntry entry = job->statResult();

        if (entry.isDir()) {
            // if this was a directory
            setUrl(url, false);
        } else {
            // if the current url is not known
            if (d->m_dirLister->findByUrl(url).isNull()) {
                setUrl(url.adjusted(QUrl::RemoveFilename), false);

                // Will set the current item once loading has finished
                auto urlSetterClosure = [this, url](){
                    setCurrentItem(url);
                    QObject::disconnect(d->m_connection);
                };
                d->m_connection = connect(this, &KDirOperator::finishedLoading, this, urlSetterClosure);
            } else {
                setCurrentItem(url);
            }
        }
        evt->accept();
        return true;
    }
    case QEvent::KeyPress: {
        QKeyEvent *evt = static_cast<QKeyEvent *>(event);
        if (evt->key() == Qt::Key_Return || evt->key() == Qt::Key_Enter) {
            // when no elements are selected and Return/Enter is pressed
            // emit keyEnterReturnPressed
            // let activated event be emitted by subsequent QAbstractItemView::keyPress otherwise
            if (!d->m_itemView->currentIndex().isValid()) {
                Q_EMIT keyEnterReturnPressed();
                evt->accept();
                return true;
            }
        }
    }
    default:
        break;
    }

    return QWidget::eventFilter(watched, event);
}

bool KDirOperatorPrivate::checkPreviewInternal() const
{
    const QStringList supported = KIO::PreviewJob::supportedMimeTypes();
    // no preview support for directories?
    if (q->dirOnlyMode() && supported.indexOf(QLatin1String("inode/directory")) == -1) {
        return false;
    }

    const QStringList mimeTypes = m_dirLister->mimeFilters();
    const QStringList nameFilters = m_dirLister->nameFilter().split(QLatin1Char(' '), Qt::SkipEmptyParts);

    if (mimeTypes.isEmpty() && nameFilters.isEmpty() && !supported.isEmpty()) {
        return true;
    } else {
        QMimeDatabase db;
        QRegularExpression re;

        if (!mimeTypes.isEmpty()) {
            for (const QString &str : supported) {
                // wilcard matching because the "mimetype" can be "image/*"
                re.setPattern(QRegularExpression::wildcardToRegularExpression(str));

                if (mimeTypes.indexOf(re) != -1) {  // matches! -> we want previews
                    return true;
                }
            }
        }

        if (!nameFilters.isEmpty()) {
            // find the MIME types of all the filter-patterns
            for (const QString &filter : nameFilters) {
                if (filter == QLatin1Char('*')) {
                    return true;
                }

                const QMimeType mt = db.mimeTypeForFile(filter, QMimeDatabase::MatchExtension /*fast mode, no file contents exist*/);
                if (!mt.isValid()) {
                    continue;
                }
                const QString mime = mt.name();

                for (const QString &str : supported) {
                // the "mimetypes" we get from the PreviewJob can be "image/*"
                // so we need to check in wildcard mode
                    re.setPattern(QRegularExpression::wildcardToRegularExpression(str));
                    if (re.match(mime).hasMatch()) {
                        return true;
                    }
                }
            }
        }
    }

    return false;
}

QAbstractItemView *KDirOperator::createView(QWidget *parent, KFile::FileView viewKind)
{
    QAbstractItemView *itemView = nullptr;
    if (KFile::isDetailView(viewKind) || KFile::isTreeView(viewKind) || KFile::isDetailTreeView(viewKind)) {
        KDirOperatorDetailView *detailView = new KDirOperatorDetailView(parent);
        detailView->setViewMode(viewKind);
        itemView = detailView;
    } else {
        itemView = new KDirOperatorIconView(parent, decorationPosition());
    }

    return itemView;
}

void KDirOperator::setAcceptDrops(bool acceptsDrops)
{
    QWidget::setAcceptDrops(acceptsDrops);
    if (view()) {
        view()->setAcceptDrops(acceptsDrops);
        if (acceptsDrops) {
            view()->installEventFilter(this);
        } else {
            view()->removeEventFilter(this);
        }
    }
}

void KDirOperator::setDropOptions(int options)
{
    d->m_dropOptions = options;
    // TODO:
    //if (d->fileView)
    //   d->fileView->setDropOptions(options);
}

void KDirOperator::setView(KFile::FileView viewKind)
{
    bool preview = (KFile::isPreviewInfo(viewKind) || KFile::isPreviewContents(viewKind));

    if (viewKind == KFile::Default) {
        if (KFile::isDetailView((KFile::FileView)d->m_defaultView)) {
            viewKind = KFile::Detail;
        } else if (KFile::isTreeView((KFile::FileView)d->m_defaultView)) {
            viewKind = KFile::Tree;
        } else if (KFile::isDetailTreeView((KFile::FileView)d->m_defaultView)) {
            viewKind = KFile::DetailTree;
        } else {
            viewKind = KFile::Simple;
        }

        const KFile::FileView defaultViewKind = static_cast<KFile::FileView>(d->m_defaultView);
        preview = (KFile::isPreviewInfo(defaultViewKind) || KFile::isPreviewContents(defaultViewKind))
                  && d->m_actionCollection->action(QStringLiteral("preview"))->isEnabled();
    }

    d->m_viewKind = static_cast<int>(viewKind);
    viewKind = static_cast<KFile::FileView>(d->m_viewKind);

    QAbstractItemView *newView = createView(this, viewKind);
    setView(newView);

    if (acceptDrops()) {
        newView->setAcceptDrops(true);
        newView->installEventFilter(this);
    }

    d->togglePreview(preview);
}

KFile::FileView KDirOperator::viewMode() const
{
    return static_cast<KFile::FileView>(d->m_viewKind);
}

QAbstractItemView *KDirOperator::view() const
{
    return d->m_itemView;
}

KFile::Modes KDirOperator::mode() const
{
    return d->m_mode;
}

void KDirOperator::setMode(KFile::Modes mode)
{
    if (d->m_mode == mode) {
        return;
    }

    d->m_mode = mode;

    d->m_dirLister->setDirOnlyMode(dirOnlyMode());

    // reset the view with the different mode
    if (d->m_itemView != nullptr) {
        setView(static_cast<KFile::FileView>(d->m_viewKind));
    }
}

void KDirOperator::setView(QAbstractItemView *view)
{
    if (view == d->m_itemView) {
        return;
    }

    // TODO: do a real timer and restart it after that
    d->m_pendingMimeTypes.clear();
    const bool listDir = (d->m_itemView == nullptr);

    if (d->m_mode & KFile::Files) {
        view->setSelectionMode(QAbstractItemView::ExtendedSelection);
    } else {
        view->setSelectionMode(QAbstractItemView::SingleSelection);
    }

    QItemSelectionModel *selectionModel = nullptr;
    if ((d->m_itemView != nullptr) && d->m_itemView->selectionModel()->hasSelection()) {
        // remember the selection of the current item view and apply this selection
        // to the new view later
        const QItemSelection selection = d->m_itemView->selectionModel()->selection();
        selectionModel = new QItemSelectionModel(d->m_proxyModel, this);
        selectionModel->select(selection, QItemSelectionModel::Select);
    }

    setFocusProxy(nullptr);
    delete d->m_itemView;
    d->m_itemView = view;
    d->m_itemView->setModel(d->m_proxyModel);
    setFocusProxy(d->m_itemView);

    view->viewport()->installEventFilter(this);

    KFileItemDelegate *delegate = new KFileItemDelegate(d->m_itemView);
    d->m_itemView->setItemDelegate(delegate);
    d->m_itemView->viewport()->setAttribute(Qt::WA_Hover);
    d->m_itemView->setContextMenuPolicy(Qt::CustomContextMenu);
    d->m_itemView->setMouseTracking(true);
    //d->itemView->setDropOptions(d->dropOptions);

    // first push our settings to the view, then listen for changes from the view
    QTreeView *treeView = qobject_cast<QTreeView *>(d->m_itemView);
    if (treeView) {
        QHeaderView *headerView = treeView->header();
        headerView->setSortIndicator(d->sortColumn(), d->sortOrder());
        connect(headerView, &QHeaderView::sortIndicatorChanged, this, [this](int logicalIndex, Qt::SortOrder order) {
            d->synchronizeSortingState(logicalIndex, order);
        });
    }

    connect(d->m_itemView, &QAbstractItemView::activated,
            this, [this](QModelIndex index) { d->slotActivated(index); });

    connect(d->m_itemView, &QAbstractItemView::customContextMenuRequested,
            this, [this](QPoint pos) { d->openContextMenu(pos); });

    connect(d->m_itemView, &QAbstractItemView::entered,
            this, [this](QModelIndex index) { d->triggerPreview(index); });

    d->m_splitter->insertWidget(0, d->m_itemView);

    d->m_splitter->resize(size());
    d->m_itemView->show();

    if (listDir) {
        QApplication::setOverrideCursor(Qt::WaitCursor);
        d->openUrl(d->m_currUrl);
    }

    if (selectionModel != nullptr) {
        d->m_itemView->setSelectionModel(selectionModel);
        QMetaObject::invokeMethod(this, [this]() { d->assureVisibleSelection(); }, Qt::QueuedConnection);
    }

    connect(d->m_itemView->selectionModel(), &QItemSelectionModel::currentChanged,
            this, [this](QModelIndex index) { d->triggerPreview(index); });
    connect(d->m_itemView->selectionModel(), &QItemSelectionModel::selectionChanged,
            this, [this]() { d->slotSelectionChanged(); });

    // if we cannot cast it to a QListView, disable the "Icon Position" menu. Note that this check
    // needs to be done here, and not in createView, since we can be set an external view
    d->m_decorationMenu->setEnabled(qobject_cast<QListView *>(d->m_itemView));

    d->m_shouldFetchForItems = qobject_cast<QTreeView *>(view);
    if (d->m_shouldFetchForItems) {
        connect(d->m_dirModel, &KDirModel::expand, this, [this](QModelIndex index) { d->slotExpandToUrl(index); });
    } else {
        d->m_itemsToBeSetAsCurrent.clear();
    }

    const bool previewForcedToTrue = d->m_inlinePreviewState == KDirOperatorPrivate::ForcedToTrue;
    const bool previewShown = d->m_inlinePreviewState == KDirOperatorPrivate::NotForced ? d->m_showPreviews : previewForcedToTrue;
    d->m_previewGenerator = new KFilePreviewGenerator(d->m_itemView);
    d->m_itemView->setIconSize(previewForcedToTrue ? QSize(KIconLoader::SizeHuge, KIconLoader::SizeHuge)
                                                   : QSize(d->m_iconSize, d->m_iconSize));
    d->m_previewGenerator->setPreviewShown(previewShown);
    d->m_actionCollection->action(QStringLiteral("inline preview"))->setChecked(previewShown);

    // ensure we change everything needed
    d->slotChangeDecorationPosition();
    updateViewActions();

    Q_EMIT viewChanged(view);

    const int zoom = previewForcedToTrue ? KIconLoader::SizeHuge : d->iconSizeForViewType(view);

    // this will make d->m_iconSize be updated, since setIconSize slot will be called
    Q_EMIT currentIconSizeChanged(zoom);
}

void KDirOperator::setDirLister(KDirLister *lister)
{
    if (lister == d->m_dirLister) { // sanity check
        return;
    }

    delete d->m_dirModel;
    d->m_dirModel = nullptr;

    delete d->m_proxyModel;
    d->m_proxyModel = nullptr;

    //delete d->m_dirLister; // deleted by KDirModel already, which took ownership
    d->m_dirLister = lister;

    d->m_dirModel = new KDirModel(this);
    d->m_dirModel->setDirLister(d->m_dirLister);
    d->m_dirModel->setDropsAllowed(KDirModel::DropOnDirectory);

    d->m_shouldFetchForItems = qobject_cast<QTreeView *>(d->m_itemView);
    if (d->m_shouldFetchForItems) {
        connect(d->m_dirModel, &KDirModel::expand, this, [this](QModelIndex index) { d->slotExpandToUrl(index); });
    } else {
        d->m_itemsToBeSetAsCurrent.clear();
    }

    d->m_proxyModel = new KDirSortFilterProxyModel(this);
    d->m_proxyModel->setSourceModel(d->m_dirModel);

    d->m_dirLister->setDelayedMimeTypes(true);

    QWidget *mainWidget = topLevelWidget();
    d->m_dirLister->setMainWindow(mainWidget);
    // qDebug() << "mainWidget=" << mainWidget;

    connect(d->m_dirLister, &KCoreDirLister::percent, this, [this](int percent) { d->slotProgress(percent); });
    connect(d->m_dirLister, &KCoreDirLister::started, this, [this]() { d->slotStarted(); });
    connect(d->m_dirLister, QOverload<>::of(&KCoreDirLister::completed), this, [this]() { d->slotIOFinished(); });
    connect(d->m_dirLister, QOverload<>::of(&KCoreDirLister::canceled), this, [this]() { d->slotCanceled(); });
    connect(d->m_dirLister, QOverload<const QUrl &>::of(&KCoreDirLister::redirection),
            this, [this](const QUrl &url) { d->slotRedirected(url); });
    connect(d->m_dirLister, &KCoreDirLister::newItems, this, [this]() { d->slotItemsChanged(); });
    connect(d->m_dirLister, &KCoreDirLister::itemsDeleted, this, [this]() { d->slotItemsChanged(); });
    connect(d->m_dirLister, &KCoreDirLister::itemsFilteredByMime, this, [this]() { d->slotItemsChanged(); });
    connect(d->m_dirLister, QOverload<>::of(&KCoreDirLister::clear), this, [this]() { d->slotItemsChanged(); });
}

void KDirOperator::selectDir(const KFileItem &item)
{
    setUrl(item.targetUrl(), true);
}

void KDirOperator::selectFile(const KFileItem &item)
{
    QApplication::restoreOverrideCursor();

    Q_EMIT fileSelected(item);
}

void KDirOperator::highlightFile(const KFileItem &item)
{
    if ((d->m_preview != nullptr && !d->m_preview->isHidden()) && !item.isNull()) {
        d->m_preview->showPreview(item.url());
    }

    Q_EMIT fileHighlighted(item);
}

void KDirOperator::setCurrentItem(const QUrl &url)
{
    // qDebug();

    KFileItem item = d->m_dirLister->findByUrl(url);
    if (d->m_shouldFetchForItems && item.isNull()) {
        d->m_itemsToBeSetAsCurrent << url;
        d->m_dirModel->expandToUrl(url);
        return;
    }

    setCurrentItem(item);
}

void KDirOperator::setCurrentItem(const KFileItem &item)
{
    // qDebug();

    if (!d->m_itemView) {
        return;
    }

    QItemSelectionModel *selModel = d->m_itemView->selectionModel();
    if (selModel) {
        selModel->clear();
        if (!item.isNull()) {
            const QModelIndex dirIndex = d->m_dirModel->indexForItem(item);
            const QModelIndex proxyIndex = d->m_proxyModel->mapFromSource(dirIndex);
            selModel->setCurrentIndex(proxyIndex, QItemSelectionModel::Select);
        }
    }
}

void KDirOperator::setCurrentItems(const QList<QUrl> &urls)
{
    // qDebug();

    if (!d->m_itemView) {
        return;
    }

    KFileItemList itemList;
    for (const QUrl &url : urls) {
        KFileItem item = d->m_dirLister->findByUrl(url);
        if (d->m_shouldFetchForItems && item.isNull()) {
            d->m_itemsToBeSetAsCurrent << url;
            d->m_dirModel->expandToUrl(url);
            continue;
        }
        itemList << item;
    }

    setCurrentItems(itemList);
}

void KDirOperator::setCurrentItems(const KFileItemList &items)
{
    // qDebug();

    if (d->m_itemView == nullptr) {
        return;
    }

    QItemSelectionModel *selModel = d->m_itemView->selectionModel();
    if (selModel) {
        selModel->clear();
        QModelIndex proxyIndex;
        for (const KFileItem &item : items) {
            if (!item.isNull()) {
                const QModelIndex dirIndex = d->m_dirModel->indexForItem(item);
                proxyIndex = d->m_proxyModel->mapFromSource(dirIndex);
                selModel->select(proxyIndex, QItemSelectionModel::Select);
            }
        }
        if (proxyIndex.isValid()) {
            selModel->setCurrentIndex(proxyIndex, QItemSelectionModel::NoUpdate);
        }
    }
}

QString KDirOperator::makeCompletion(const QString &string)
{
    if (string.isEmpty()) {
        d->m_itemView->selectionModel()->clear();
        return QString();
    }

    prepareCompletionObjects();
    return d->m_completion.makeCompletion(string);
}

QString KDirOperator::makeDirCompletion(const QString &string)
{
    if (string.isEmpty()) {
        d->m_itemView->selectionModel()->clear();
        return QString();
    }

    prepareCompletionObjects();
    return d->m_dirCompletion.makeCompletion(string);
}

void KDirOperator::prepareCompletionObjects()
{
    if (d->m_itemView == nullptr) {
        return;
    }

    if (d->m_completeListDirty) {   // create the list of all possible completions
        const KFileItemList itemList = d->m_dirLister->items();
        for (const KFileItem &item : itemList) {
            d->m_completion.addItem(item.name());
            if (item.isDir()) {
                d->m_dirCompletion.addItem(item.name());
            }
        }
        d->m_completeListDirty = false;
    }
}

void KDirOperator::slotCompletionMatch(const QString &match)
{
    QUrl url(match);
    if (url.isRelative()) {
        url = d->m_currUrl.resolved(url);
    }
    setCurrentItem(url);
    Q_EMIT completion(match);
}

void KDirOperator::setupActions()
{
    d->m_actionCollection = new KActionCollection(this);
    d->m_actionCollection->setObjectName(QStringLiteral("KDirOperator::actionCollection"));

    d->m_actionMenu = new KActionMenu(i18n("Menu"), this);
    d->m_actionCollection->addAction(QStringLiteral("popupMenu"), d->m_actionMenu);

    QAction *upAction = d->m_actionCollection->addAction(KStandardAction::Up, QStringLiteral("up"), this, SLOT(cdUp()));
    upAction->setText(i18n("Parent Folder"));

    QAction *backAction = d->m_actionCollection->addAction(KStandardAction::Back, QStringLiteral("back"), this, SLOT(back()));
    backAction->setShortcut(Qt::Key_Backspace);
    backAction->setToolTip(i18nc("@info", "Go back"));

    QAction* forwardAction = d->m_actionCollection->addAction(KStandardAction::Forward, QStringLiteral("forward"), this, SLOT(forward()));
    forwardAction->setToolTip(i18nc("@info", "Go forward"));

    QAction *homeAction = d->m_actionCollection->addAction(KStandardAction::Home, QStringLiteral("home"), this, SLOT(home()));
    homeAction->setText(i18n("Home Folder"));

    QAction *reloadAction = d->m_actionCollection->addAction(KStandardAction::Redisplay, QStringLiteral("reload"), this, SLOT(rereadDir()));
    reloadAction->setText(i18n("Reload"));
    reloadAction->setShortcuts(KStandardShortcut::shortcut(KStandardShortcut::Reload));

    QAction *mkdirAction = new QAction(i18n("New Folder..."), this);
    d->m_actionCollection->addAction(QStringLiteral("mkdir"), mkdirAction);
    mkdirAction->setIcon(QIcon::fromTheme(QStringLiteral("folder-new")));
    connect(mkdirAction, &QAction::triggered, this, [this]() { mkdir(); });

    QAction *rename = KStandardAction::renameFile(this, &KDirOperator::renameSelected, this);
    d->m_actionCollection->addAction(QStringLiteral("rename"), rename);

    QAction *trash = new QAction(i18n("Move to Trash"), this);
    d->m_actionCollection->addAction(QStringLiteral("trash"), trash);
    trash->setIcon(QIcon::fromTheme(QStringLiteral("user-trash")));
    trash->setShortcut(Qt::Key_Delete);
    connect(trash, &QAction::triggered, this, &KDirOperator::trashSelected);

    QAction *action = new QAction(i18n("Delete"), this);
    d->m_actionCollection->addAction(QStringLiteral("delete"), action);
    action->setIcon(QIcon::fromTheme(QStringLiteral("edit-delete")));
    action->setShortcut(Qt::SHIFT | Qt::Key_Delete);
    connect(action, &QAction::triggered, this, &KDirOperator::deleteSelected);

    // the sort menu actions
    KActionMenu *sortMenu = new KActionMenu(i18n("Sorting"), this);
    sortMenu->setIcon(QIcon::fromTheme(QStringLiteral("view-sort")));
    sortMenu->setPopupMode(QToolButton::InstantPopup);
    d->m_actionCollection->addAction(QStringLiteral("sorting menu"),  sortMenu);

    KToggleAction *byNameAction = new KToggleAction(i18n("Sort by Name"), this);
    d->m_actionCollection->addAction(QStringLiteral("by name"), byNameAction);
    connect(byNameAction, &QAction::triggered, this, [this]() { d->slotSortByName(); });

    KToggleAction *bySizeAction = new KToggleAction(i18n("Sort by Size"), this);
    d->m_actionCollection->addAction(QStringLiteral("by size"), bySizeAction);
    connect(bySizeAction, &QAction::triggered, this, [this]() { d->slotSortBySize(); });

    KToggleAction *byDateAction = new KToggleAction(i18n("Sort by Date"), this);
    d->m_actionCollection->addAction(QStringLiteral("by date"), byDateAction);
    connect(byDateAction, &QAction::triggered, this, [this]() { d->slotSortByDate(); });

    KToggleAction *byTypeAction = new KToggleAction(i18n("Sort by Type"), this);
    d->m_actionCollection->addAction(QStringLiteral("by type"), byTypeAction);
    connect(byTypeAction, &QAction::triggered, this, [this]() { d->slotSortByType(); });

    QActionGroup *sortOrderGroup = new QActionGroup(this);
    sortOrderGroup->setExclusive(true);

    KToggleAction *ascendingAction = new KToggleAction(i18n("Ascending"), this);
    d->m_actionCollection->addAction(QStringLiteral("ascending"), ascendingAction);
    ascendingAction->setActionGroup(sortOrderGroup);
    connect(ascendingAction, &QAction::triggered, this, [this]() {
        this->d->slotSortReversed(false);
    });

    KToggleAction *descendingAction = new KToggleAction(i18n("Descending"), this);
    d->m_actionCollection->addAction(QStringLiteral("descending"), descendingAction);
    descendingAction->setActionGroup(sortOrderGroup);
    connect(descendingAction, &QAction::triggered, this, [this]() {
        this->d->slotSortReversed(true);
    });

    KToggleAction *dirsFirstAction = new KToggleAction(i18n("Folders First"), this);
    d->m_actionCollection->addAction(QStringLiteral("dirs first"), dirsFirstAction);
    connect(dirsFirstAction, &QAction::triggered, this, [this]() { d->slotToggleDirsFirst(); });

    // View modes that match those of Dolphin
    KToggleAction *iconsViewAction = new KToggleAction(i18n("Icons View"), this);
    iconsViewAction->setIcon(QIcon::fromTheme(QStringLiteral("view-list-icons")));
    d->m_actionCollection->addAction(QStringLiteral("icons view"), iconsViewAction);
    connect(iconsViewAction, &QAction::triggered, this, [this]() { d->slotIconsView(); });

    KToggleAction *compactViewAction = new KToggleAction(i18n("Compact View"), this);
    compactViewAction->setIcon(QIcon::fromTheme(QStringLiteral("view-list-details")));
    d->m_actionCollection->addAction(QStringLiteral("compact view"), compactViewAction);
    connect(compactViewAction, &QAction::triggered, this, [this]() { d->slotCompactView(); });

    KToggleAction *detailsViewAction = new KToggleAction(i18n("Details View"), this);
    detailsViewAction->setIcon(QIcon::fromTheme(QStringLiteral("view-list-tree")));
    d->m_actionCollection->addAction(QStringLiteral("details view"), detailsViewAction);
    connect(detailsViewAction, &QAction::triggered, this, [this]() { d->slotDetailsView(); });

    QActionGroup *viewModeGroup = new QActionGroup(this);
    viewModeGroup->setExclusive(true);
    iconsViewAction->setActionGroup(viewModeGroup);
    compactViewAction->setActionGroup(viewModeGroup);
    detailsViewAction->setActionGroup(viewModeGroup);

    QActionGroup *sortGroup = new QActionGroup(this);
    byNameAction->setActionGroup(sortGroup);
    bySizeAction->setActionGroup(sortGroup);
    byDateAction->setActionGroup(sortGroup);
    byTypeAction->setActionGroup(sortGroup);

    d->m_decorationMenu = new KActionMenu(i18n("Icon Position"), this);
    d->m_actionCollection->addAction(QStringLiteral("decoration menu"), d->m_decorationMenu);

    d->m_leftAction = new KToggleAction(i18n("Next to File Name"), this);
    d->m_actionCollection->addAction(QStringLiteral("decorationAtLeft"), d->m_leftAction);
    connect(d->m_leftAction, &QAction::triggered, this, [this]() { d->slotChangeDecorationPosition(); });

    KToggleAction *topAction = new KToggleAction(i18n("Above File Name"), this);
    d->m_actionCollection->addAction(QStringLiteral("decorationAtTop"), topAction);
    connect(topAction, &QAction::triggered, this, [this]() { d->slotChangeDecorationPosition(); });

    d->m_decorationMenu->addAction(d->m_leftAction);
    d->m_decorationMenu->addAction(topAction);

    QActionGroup *decorationGroup = new QActionGroup(this);
    decorationGroup->setExclusive(true);
    d->m_leftAction->setActionGroup(decorationGroup);
    topAction->setActionGroup(decorationGroup);

    KToggleAction *shortAction = new KToggleAction(i18n("Short View"), this);
    d->m_actionCollection->addAction(QStringLiteral("short view"),  shortAction);
    shortAction->setIcon(QIcon::fromTheme(QStringLiteral("view-list-icons")));
    connect(shortAction, &QAction::triggered, this, [this]() {
        d->slotSimpleView();
    });

    KToggleAction *detailedAction = new KToggleAction(i18n("Detailed View"), this);
    d->m_actionCollection->addAction(QStringLiteral("detailed view"), detailedAction);
    detailedAction->setIcon(QIcon::fromTheme(QStringLiteral("view-list-details")));
    connect(detailedAction, &QAction::triggered, this, [this]() { d->slotDetailedView(); });

    KToggleAction *treeAction = new KToggleAction(i18n("Tree View"), this);
    d->m_actionCollection->addAction(QStringLiteral("tree view"), treeAction);
    treeAction->setIcon(QIcon::fromTheme(QStringLiteral("view-list-tree")));
    connect(treeAction, &QAction::triggered, this, [this]() { d->slotTreeView(); });

    KToggleAction *detailedTreeAction = new KToggleAction(i18n("Detailed Tree View"), this);
    d->m_actionCollection->addAction(QStringLiteral("detailed tree view"), detailedTreeAction);
    detailedTreeAction->setIcon(QIcon::fromTheme(QStringLiteral("view-list-tree")));
    connect(detailedTreeAction, &QAction::triggered, this, [this]() { d->slotDetailedTreeView(); });

    QActionGroup *viewGroup = new QActionGroup(this);
    shortAction->setActionGroup(viewGroup);
    detailedAction->setActionGroup(viewGroup);
    treeAction->setActionGroup(viewGroup);
    detailedTreeAction->setActionGroup(viewGroup);

    KToggleAction *allowExpansionAction = new KToggleAction(i18n("Allow Expansion in Details View"), this);
    d->m_actionCollection->addAction(QStringLiteral("allow expansion"), allowExpansionAction);
    connect(allowExpansionAction, &QAction::toggled, this, [this](bool allow) { d->slotToggleAllowExpansion(allow); });

    KToggleAction *showHiddenAction = new KToggleAction(i18n("Show Hidden Files"), this);
    d->m_actionCollection->addAction(QStringLiteral("show hidden"), showHiddenAction);
    showHiddenAction->setShortcuts(KStandardShortcut::showHideHiddenFiles());
    connect(showHiddenAction, &QAction::toggled, this, [this](bool show) { d->slotToggleHidden(show); });

    KToggleAction *previewAction = new KToggleAction(i18n("Show Preview Panel"), this);
    d->m_actionCollection->addAction(QStringLiteral("preview"), previewAction);
    previewAction->setShortcut(Qt::Key_F11);
    connect(previewAction, &QAction::toggled, this, [this](bool enable) { d->togglePreview(enable); });

    KToggleAction *inlinePreview = new KToggleAction(QIcon::fromTheme(QStringLiteral("view-preview")),
            i18n("Show Preview"), this);
    d->m_actionCollection->addAction(QStringLiteral("inline preview"), inlinePreview);
    inlinePreview->setShortcut(Qt::Key_F12);
    connect(inlinePreview, &QAction::toggled, this, [this](bool enable) { d->toggleInlinePreviews(enable); });

    QAction *fileManager = new QAction(i18n("Open Containing Folder"), this);
    d->m_actionCollection->addAction(QStringLiteral("file manager"), fileManager);
    fileManager->setIcon(QIcon::fromTheme(QStringLiteral("system-file-manager")));
    connect(fileManager, &QAction::triggered, this, [this]() { d->slotOpenFileManager(); });

    action = new QAction(i18n("Properties"), this);
    d->m_actionCollection->addAction(QStringLiteral("properties"), action);
    action->setIcon(QIcon::fromTheme(QStringLiteral("document-properties")));
    action->setShortcut(Qt::ALT | Qt::Key_Return);
    connect(action, &QAction::triggered, this, [this]() { d->slotProperties(); });

    // the view menu actions
    KActionMenu *viewMenu = new KActionMenu(i18n("&View"), this);
    d->m_actionCollection->addAction(QStringLiteral("view menu"), viewMenu);
    viewMenu->addAction(shortAction);
    viewMenu->addAction(detailedAction);
    // Comment following lines to hide the extra two modes
    viewMenu->addAction(treeAction);
    viewMenu->addAction(detailedTreeAction);
    // TODO: QAbstractItemView does not offer an action collection. Provide
    // an interface to add a custom action collection.

    d->m_newFileMenu = new KNewFileMenu(d->m_actionCollection, QStringLiteral("new"), this);
    connect(d->m_newFileMenu, &KNewFileMenu::directoryCreated, this, [this](const QUrl &url) {
        d->slotDirectoryCreated(url);
    });
    connect(d->m_newFileMenu, &KNewFileMenu::selectExistingDir, this, [this](const QUrl &url) {
        setCurrentItem(url);
    });

    d->m_actionCollection->addAssociatedWidget(this);
    const QList<QAction *> list = d->m_actionCollection->actions();
    for (QAction *action : list) {
        action->setShortcutContext(Qt::WidgetWithChildrenShortcut);
    }
}

void KDirOperator::setupMenu()
{
    setupMenu(SortActions | ViewActions | FileActions);
}

void KDirOperator::setupMenu(int whichActions)
{
    // first fill the submenus (sort and view)
    KActionMenu *sortMenu = static_cast<KActionMenu *>(d->m_actionCollection->action(QStringLiteral("sorting menu")));
    sortMenu->menu()->clear();
    sortMenu->addAction(d->m_actionCollection->action(QStringLiteral("by name")));
    sortMenu->addAction(d->m_actionCollection->action(QStringLiteral("by size")));
    sortMenu->addAction(d->m_actionCollection->action(QStringLiteral("by date")));
    sortMenu->addAction(d->m_actionCollection->action(QStringLiteral("by type")));
    sortMenu->addSeparator();
    sortMenu->addAction(d->m_actionCollection->action(QStringLiteral("ascending")));
    sortMenu->addAction(d->m_actionCollection->action(QStringLiteral("descending")));
    sortMenu->addSeparator();
    sortMenu->addAction(d->m_actionCollection->action(QStringLiteral("dirs first")));

    // now plug everything into the popupmenu
    d->m_actionMenu->menu()->clear();
    if (whichActions & NavActions) {
        d->m_actionMenu->addAction(d->m_actionCollection->action(QStringLiteral("up")));
        d->m_actionMenu->addAction(d->m_actionCollection->action(QStringLiteral("back")));
        d->m_actionMenu->addAction(d->m_actionCollection->action(QStringLiteral("forward")));
        d->m_actionMenu->addAction(d->m_actionCollection->action(QStringLiteral("home")));
        d->m_actionMenu->addSeparator();
    }

    if (whichActions & FileActions) {
        d->m_actionMenu->addAction(d->m_actionCollection->action(QStringLiteral("new")));

        d->m_actionMenu->addAction(d->m_actionCollection->action(QStringLiteral("rename")));
        d->m_actionCollection->action(QStringLiteral("rename"))->setEnabled(KProtocolManager::supportsMoving(d->m_currUrl));

        if (d->m_currUrl.isLocalFile() && !(QApplication::keyboardModifiers() & Qt::ShiftModifier)) {
            d->m_actionMenu->addAction(d->m_actionCollection->action(QStringLiteral("trash")));
        }
        KConfigGroup cg(KSharedConfig::openConfig(), QStringLiteral("KDE"));
        const bool del = !d->m_currUrl.isLocalFile() ||
                         (QApplication::keyboardModifiers() & Qt::ShiftModifier) ||
                         cg.readEntry("ShowDeleteCommand", false);
        if (del) {
            d->m_actionMenu->addAction(d->m_actionCollection->action(QStringLiteral("delete")));
        }
        d->m_actionMenu->addSeparator();
    }

    if (whichActions & SortActions) {
        d->m_actionMenu->addAction(sortMenu);
        if (!(whichActions & ViewActions)) {
            d->m_actionMenu->addSeparator();
        }
    }

    if (whichActions & ViewActions) {
        d->m_actionMenu->addAction(d->m_actionCollection->action(QStringLiteral("view menu")));
        d->m_actionMenu->addAction(d->m_actionCollection->action(QStringLiteral("reload")));
        d->m_actionMenu->addSeparator();
    }

    if (whichActions & FileActions) {
        d->m_actionMenu->addAction(d->m_actionCollection->action(QStringLiteral("file manager")));
        d->m_actionMenu->addAction(d->m_actionCollection->action(QStringLiteral("properties")));
    }
}

void KDirOperator::updateSortActions()
{
    QAction *ascending = d->m_actionCollection->action(QStringLiteral("ascending"));
    QAction *descending = d->m_actionCollection->action(QStringLiteral("descending"));

    if (KFile::isSortByName(d->m_sorting)) {
        d->m_actionCollection->action(QStringLiteral("by name"))->setChecked(true);
        descending->setText(i18nc("Sort descending", "Z-A"));
        ascending->setText(i18nc("Sort ascending", "A-Z"));
    } else if (KFile::isSortByDate(d->m_sorting)) {
        d->m_actionCollection->action(QStringLiteral("by date"))->setChecked(true);
        descending->setText(i18nc("Sort descending", "Newest First"));
        ascending->setText(i18nc("Sort ascending", "Oldest First"));
    } else if (KFile::isSortBySize(d->m_sorting)) {
        d->m_actionCollection->action(QStringLiteral("by size"))->setChecked(true);
        descending->setText(i18nc("Sort descending", "Largest First"));
        ascending->setText(i18nc("Sort ascending", "Smallest First"));
    } else if (KFile::isSortByType(d->m_sorting)) {
        d->m_actionCollection->action(QStringLiteral("by type"))->setChecked(true);
        descending->setText(i18nc("Sort descending", "Z-A"));
        ascending->setText(i18nc("Sort ascending", "A-Z"));
    }
    ascending->setChecked(!(d->m_sorting & QDir::Reversed));
    descending->setChecked(d->m_sorting & QDir::Reversed);
    d->m_actionCollection->action(QStringLiteral("dirs first"))->setChecked(d->m_sorting & QDir::DirsFirst);
}

void KDirOperator::updateViewActions()
{
    KFile::FileView fv = static_cast<KFile::FileView>(d->m_viewKind);

    //QAction *separateDirs = d->actionCollection->action("separate dirs");
    //separateDirs->setChecked(KFile::isSeparateDirs(fv) &&
    //                         separateDirs->isEnabled());

    d->m_actionCollection->action(QStringLiteral("short view"))->setChecked(KFile::isSimpleView(fv));
    d->m_actionCollection->action(QStringLiteral("detailed view"))->setChecked(KFile::isDetailView(fv));
    d->m_actionCollection->action(QStringLiteral("tree view"))->setChecked(KFile::isTreeView(fv));
    d->m_actionCollection->action(QStringLiteral("detailed tree view"))->setChecked(KFile::isDetailTreeView(fv));

    // dolphin style views
    d->m_actionCollection->action(QStringLiteral("icons view"))->setChecked(KFile::isSimpleView(fv) && d->m_decorationPosition == QStyleOptionViewItem::Top);
    d->m_actionCollection->action(QStringLiteral("compact view"))->setChecked(KFile::isSimpleView(fv) && d->m_decorationPosition == QStyleOptionViewItem::Left);
    d->m_actionCollection->action(QStringLiteral("details view"))->setChecked(KFile::isDetailTreeView(fv) || KFile::isDetailView(fv));
}

void KDirOperator::readConfig(const KConfigGroup &configGroup)
{
    d->m_defaultView = 0;
    QString viewStyle = configGroup.readEntry("View Style", "DetailTree");
    if (viewStyle == QLatin1String("Detail")) {
        d->m_defaultView |= KFile::Detail;
    } else if (viewStyle == QLatin1String("Tree")) {
        d->m_defaultView |= KFile::Tree;
    } else if (viewStyle == QLatin1String("DetailTree")) {
        d->m_defaultView |= KFile::DetailTree;
    } else {
        d->m_defaultView |= KFile::Simple;
    }
    //if (configGroup.readEntry(QLatin1String("Separate Directories"),
    //                          DefaultMixDirsAndFiles)) {
    //    d->defaultView |= KFile::SeparateDirs;
    //}
    if (configGroup.readEntry(QStringLiteral("Show Preview"), false)) {
        d->m_defaultView |= KFile::PreviewContents;
    }

    d->m_previewWidth = configGroup.readEntry(QStringLiteral("Preview Width"), 100);

    if (configGroup.readEntry(QStringLiteral("Show hidden files"),
                              DefaultShowHidden)) {
        d->m_actionCollection->action(QStringLiteral("show hidden"))->setChecked(true);
        d->m_dirLister->setShowingDotFiles(true);
    }

    if (configGroup.readEntry(QStringLiteral("Allow Expansion"),
                              DefaultShowHidden)) {
        d->m_actionCollection->action(QStringLiteral("allow expansion"))->setChecked(true);
    }

    QDir::SortFlags sorting = QDir::Name;
    if (configGroup.readEntry(QStringLiteral("Sort directories first"),
                              DefaultDirsFirst)) {
        sorting |= QDir::DirsFirst;
    }
    QString name = QStringLiteral("Name");
    QString sortBy = configGroup.readEntry(QStringLiteral("Sort by"), name);
    if (sortBy == name) {
        sorting |= QDir::Name;
    } else if (sortBy == QLatin1String("Size")) {
        sorting |= QDir::Size;
    } else if (sortBy == QLatin1String("Date")) {
        sorting |= QDir::Time;
    } else if (sortBy == QLatin1String("Type")) {
        sorting |= QDir::Type;
    }
    if (configGroup.readEntry(QStringLiteral("Sort reversed"), DefaultSortReversed)) {
        sorting |= QDir::Reversed;
    }
    d->updateSorting(sorting);

    if (d->m_inlinePreviewState == KDirOperatorPrivate::NotForced) {
        d->m_showPreviews = configGroup.readEntry(QStringLiteral("Show Inline Previews"), true);
    }
    QStyleOptionViewItem::Position pos = (QStyleOptionViewItem::Position) configGroup.readEntry(QStringLiteral("Decoration position"), (int) QStyleOptionViewItem::Top);
    setDecorationPosition(pos);
}

void KDirOperator::writeConfig(KConfigGroup &configGroup)
{
    QString sortBy = QStringLiteral("Name");
    if (KFile::isSortBySize(d->m_sorting)) {
        sortBy = QStringLiteral("Size");
    } else if (KFile::isSortByDate(d->m_sorting)) {
        sortBy = QStringLiteral("Date");
    } else if (KFile::isSortByType(d->m_sorting)) {
        sortBy = QStringLiteral("Type");
    }

    configGroup.writeEntry(QStringLiteral("Sort by"), sortBy);

    configGroup.writeEntry(QStringLiteral("Sort reversed"),
                           d->m_actionCollection->action(QStringLiteral("descending"))->isChecked());

    configGroup.writeEntry(QStringLiteral("Sort directories first"),
                           d->m_actionCollection->action(QStringLiteral("dirs first"))->isChecked());

    // don't save the preview when an application specific preview is in use.
    bool appSpecificPreview = false;
    if (d->m_preview) {
        KFileMetaPreview *tmp = dynamic_cast<KFileMetaPreview *>(d->m_preview);
        appSpecificPreview = (tmp == nullptr);
    }

    if (!appSpecificPreview) {
        KToggleAction *previewAction = static_cast<KToggleAction *>(d->m_actionCollection->action(QStringLiteral("preview")));
        if (previewAction->isEnabled()) {
            bool hasPreview = previewAction->isChecked();
            configGroup.writeEntry(QStringLiteral("Show Preview"), hasPreview);

            if (hasPreview) {
                // remember the width of the preview widget
                QList<int> sizes = d->m_splitter->sizes();
                Q_ASSERT(sizes.count() == 2);
                configGroup.writeEntry(QStringLiteral("Preview Width"), sizes[1]);
            }
        }
    }

    configGroup.writeEntry(QStringLiteral("Show hidden files"),
                           d->m_actionCollection->action(QStringLiteral("show hidden"))->isChecked());

    configGroup.writeEntry(QStringLiteral("Allow Expansion"),
                           d->m_actionCollection->action(QStringLiteral("allow expansion"))->isChecked());

    KFile::FileView fv = static_cast<KFile::FileView>(d->m_viewKind);
    QString style;
    if (KFile::isDetailView(fv)) {
        style = QStringLiteral("Detail");
    } else if (KFile::isSimpleView(fv)) {
        style = QStringLiteral("Simple");
    } else if (KFile::isTreeView(fv)) {
        style = QStringLiteral("Tree");
    } else if (KFile::isDetailTreeView(fv)) {
        style = QStringLiteral("DetailTree");
    }
    configGroup.writeEntry(QStringLiteral("View Style"), style);

    if (d->m_inlinePreviewState == KDirOperatorPrivate::NotForced) {
        configGroup.writeEntry(QStringLiteral("Show Inline Previews"), d->m_showPreviews);
        d->writeIconZoomSettingsIfNeeded();
    }

    configGroup.writeEntry(QStringLiteral("Decoration position"), (int) d->m_decorationPosition);
}

void KDirOperatorPrivate::writeIconZoomSettingsIfNeeded() {
     // must match behavior of iconSizeForViewType
     if (m_configGroup && m_itemView) {
         ZoomSettingsForView zoomSettings = zoomSettingsForViewForView();
         m_configGroup->writeEntry(zoomSettings.name, m_iconSize);
     }
}

void KDirOperator::resizeEvent(QResizeEvent *)
{
    // resize the m_splitter and assure that the width of
    // the preview widget is restored
    QList<int> sizes = d->m_splitter->sizes();
    const bool hasPreview = (sizes.count() == 2);

    d->m_splitter->resize(size());
    sizes = d->m_splitter->sizes();

    const bool restorePreviewWidth = hasPreview && (d->m_previewWidth != sizes[1]);
    if (restorePreviewWidth) {
        const int availableWidth = sizes[0] + sizes[1];
        sizes[0] = availableWidth - d->m_previewWidth;
        sizes[1] = d->m_previewWidth;
        d->m_splitter->setSizes(sizes);
    }
    if (hasPreview) {
        d->m_previewWidth = sizes[1];
    }

    if (d->m_progressBar->parent() == this) {
        // might be reparented into a statusbar
        d->m_progressBar->move(2, height() - d->m_progressBar->height() - 2);
    }
}

void KDirOperator::keyPressEvent(QKeyEvent *e) // TODO KF6 remove
{
    QWidget::keyPressEvent(e);
}

void KDirOperator::setOnlyDoubleClickSelectsFiles(bool enable)
{
    d->m_onlyDoubleClickSelectsFiles = enable;
    // TODO: port to QAbstractItemModel
    //if (d->itemView != 0) {
    //    d->itemView->setOnlyDoubleClickSelectsFiles(enable);
    //}
}

bool KDirOperator::onlyDoubleClickSelectsFiles() const
{
    return d->m_onlyDoubleClickSelectsFiles;
}

void KDirOperator::setFollowNewDirectories(bool enable)
{
    d->m_followNewDirectories = enable;
}

bool KDirOperator::followNewDirectories() const
{
    return d->m_followNewDirectories;
}

void KDirOperator::setFollowSelectedDirectories(bool enable)
{
    d->m_followSelectedDirectories = enable;
}

bool KDirOperator::followSelectedDirectories() const
{
    return d->m_followSelectedDirectories;
}

void KDirOperatorPrivate::slotStarted()
{
    m_progressBar->setValue(0);
    // delay showing the progressbar for one second
    m_progressDelayTimer->setSingleShot(true);
    m_progressDelayTimer->start(1000);
}

void KDirOperatorPrivate::slotShowProgress()
{
    m_progressBar->raise();
    m_progressBar->show();
}

void KDirOperatorPrivate::slotProgress(int percent)
{
    m_progressBar->setValue(percent);
}

void KDirOperatorPrivate::slotIOFinished()
{
    m_progressDelayTimer->stop();
    slotProgress(100);
    m_progressBar->hide();
    Q_EMIT q->finishedLoading();
    q->resetCursor();

    if (m_preview) {
        m_preview->clearPreview();
    }

    // m_lastUrl can be empty when e.g. kfilewidget is first opened
    if (!m_lastUrl.isEmpty() && m_dirHighlighting) {
        q->setCurrentItem(m_lastUrl);
    }
}

void KDirOperatorPrivate::slotCanceled()
{
    Q_EMIT q->finishedLoading();
    q->resetCursor();
}

QProgressBar *KDirOperator::progressBar() const
{
    return d->m_progressBar;
}

void KDirOperator::clearHistory()
{
    qDeleteAll(d->m_backStack);
    d->m_backStack.clear();
    d->m_actionCollection->action(QStringLiteral("back"))->setEnabled(false);

    qDeleteAll(d->m_forwardStack);
    d->m_forwardStack.clear();
    d->m_actionCollection->action(QStringLiteral("forward"))->setEnabled(false);
}

void KDirOperator::setEnableDirHighlighting(bool enable)
{
    d->m_dirHighlighting = enable;
}

bool KDirOperator::dirHighlighting() const
{
    return d->m_dirHighlighting;
}

bool KDirOperator::dirOnlyMode() const
{
    return dirOnlyMode(d->m_mode);
}

bool KDirOperator::dirOnlyMode(uint mode)
{
    return ((mode & KFile::Directory) &&
            (mode & (KFile::File | KFile::Files)) == 0);
}

void KDirOperatorPrivate::slotProperties()
{
    if (m_itemView == nullptr) {
        return;
    }

    const KFileItemList list = q->selectedItems();
    if (!list.isEmpty()) {
        KPropertiesDialog dialog(list, q);
        dialog.exec();
    }
}

void KDirOperatorPrivate::slotActivated(const QModelIndex &index)
{
    const QModelIndex dirIndex = m_proxyModel->mapToSource(index);
    KFileItem item = m_dirModel->itemForIndex(dirIndex);

    const Qt::KeyboardModifiers modifiers = QApplication::keyboardModifiers();
    if (item.isNull() || (modifiers & Qt::ShiftModifier) || (modifiers & Qt::ControlModifier)) {
        return;
    }

    if (item.isDir()) {
        // Only allow disabling following selected directories on Tree and
        // DetailTree views as selected directories in these views still expand
        // when selected. For other views, disabling following selected
        // directories would make selecting a directory a noop which is
        // unintuitive.
        if (m_followSelectedDirectories ||
            (m_viewKind != KFile::Tree && m_viewKind != KFile::DetailTree)) {
            q->selectDir(item);
        }
    } else {
        q->selectFile(item);
    }
}

void KDirOperatorPrivate::slotSelectionChanged()
{
    if (m_itemView == nullptr) {
        return;
    }

    // In the multiselection mode each selection change is indicated by
    // emitting a null item. Also when the selection has been cleared, a
    // null item must be emitted.
    const bool multiSelectionMode = (m_itemView->selectionMode() == QAbstractItemView::ExtendedSelection);
    const bool hasSelection = m_itemView->selectionModel()->hasSelection();
    if (multiSelectionMode || !hasSelection) {
        KFileItem nullItem;
        q->highlightFile(nullItem);
    } else {
        const KFileItem selectedItem = q->selectedItems().constFirst();
        q->highlightFile(selectedItem);
    }
}

void KDirOperatorPrivate::openContextMenu(const QPoint &pos)
{
    const QModelIndex proxyIndex = m_itemView->indexAt(pos);
    const QModelIndex dirIndex = m_proxyModel->mapToSource(proxyIndex);
    KFileItem item = m_dirModel->itemForIndex(dirIndex);

    if (item.isNull()) {
        return;
    }

    q->activatedMenu(item, QCursor::pos());
}

void KDirOperatorPrivate::triggerPreview(const QModelIndex &index)
{
    if ((m_preview != nullptr && !m_preview->isHidden()) && index.isValid() && (index.column() == KDirModel::Name)) {
        const QModelIndex dirIndex = m_proxyModel->mapToSource(index);
        const KFileItem item = m_dirModel->itemForIndex(dirIndex);

        if (item.isNull()) {
            return;
        }

        if (!item.isDir()) {
            m_previewUrl = item.url();
            showPreview();
        } else {
            m_preview->clearPreview();
        }
    }
}

void KDirOperatorPrivate::showPreview()
{
    if (m_preview != nullptr) {
        m_preview->showPreview(m_previewUrl);
    }
}

void KDirOperatorPrivate::slotSplitterMoved(int, int)
{
    const QList<int> sizes = m_splitter->sizes();
    if (sizes.count() == 2) {
        // remember the width of the preview widget (see KDirOperator::resizeEvent())
        m_previewWidth = sizes[1];
    }
}

void KDirOperatorPrivate::assureVisibleSelection()
{
    if (m_itemView == nullptr) {
        return;
    }

    QItemSelectionModel *selModel = m_itemView->selectionModel();
    if (selModel->hasSelection()) {
        const QModelIndex index = selModel->currentIndex();
        m_itemView->scrollTo(index, QAbstractItemView::EnsureVisible);
        triggerPreview(index);
    }
}

void KDirOperatorPrivate::synchronizeSortingState(int logicalIndex, Qt::SortOrder order)
{
    QDir::SortFlags newSort = m_sorting & ~(QDirSortMask | QDir::Reversed);

    switch (logicalIndex) {
    case KDirModel::Name:
        newSort |= QDir::Name;
        break;
    case KDirModel::Size:
        newSort |= QDir::Size;
        break;
    case KDirModel::ModifiedTime:
        newSort |= QDir::Time;
        break;
    case KDirModel::Type:
        newSort |= QDir::Type;
        break;
    default:
        Q_ASSERT(false);
    }

    if (order == Qt::DescendingOrder) {
        newSort |= QDir::Reversed;
    }

    updateSorting(newSort);

    QMetaObject::invokeMethod(q, [this]() { assureVisibleSelection(); }, Qt::QueuedConnection);
}

void KDirOperatorPrivate::slotChangeDecorationPosition()
{
    if (!m_itemView) {
        return;
    }

    KDirOperatorIconView *view = qobject_cast<KDirOperatorIconView *>(m_itemView);

    if (!view) {
        return;
    }

    const bool leftChecked = m_actionCollection->action(QStringLiteral("decorationAtLeft"))->isChecked();

    if (leftChecked) {
        view->setDecorationPosition(QStyleOptionViewItem::Left);
    } else {
        view->setDecorationPosition(QStyleOptionViewItem::Top);
    }

    m_itemView->update();
}

void KDirOperatorPrivate::slotExpandToUrl(const QModelIndex &index)
{
    QTreeView *treeView = qobject_cast<QTreeView *>(m_itemView);

    if (!treeView) {
        return;
    }

    const KFileItem item = m_dirModel->itemForIndex(index);

    if (item.isNull()) {
        return;
    }

    if (!item.isDir()) {
        const QModelIndex proxyIndex = m_proxyModel->mapFromSource(index);

        QList<QUrl>::Iterator it = m_itemsToBeSetAsCurrent.begin();
        while (it != m_itemsToBeSetAsCurrent.end()) {
            const QUrl url = *it;
            if (url.matches(item.url(), QUrl::StripTrailingSlash) || url.isParentOf(item.url())) {
                const KFileItem _item = m_dirLister->findByUrl(url);
                if (!_item.isNull() && _item.isDir()) {
                    const QModelIndex _index = m_dirModel->indexForItem(_item);
                    const QModelIndex _proxyIndex = m_proxyModel->mapFromSource(_index);
                    treeView->expand(_proxyIndex);

                    // if we have expanded the last parent of this item, select it
                    if (item.url().adjusted(QUrl::RemoveFilename | QUrl::StripTrailingSlash) == url.adjusted(QUrl::StripTrailingSlash)) {
                        treeView->selectionModel()->select(proxyIndex, QItemSelectionModel::Select);
                    }
                }
                it = m_itemsToBeSetAsCurrent.erase(it);
            } else {
                ++it;
            }
        }
    } else if (!m_itemsToBeSetAsCurrent.contains(item.url())) {
        m_itemsToBeSetAsCurrent << item.url();
    }
}

void KDirOperatorPrivate::slotItemsChanged()
{
    m_completeListDirty = true;
}

int KDirOperatorPrivate::iconSizeForViewType(QAbstractItemView *itemView) const
{
    // must match behavior of writeIconZoomSettingsIfNeeded
    if (!itemView || !m_configGroup) {
        return 0;
    }

    ZoomSettingsForView ZoomSettingsForView = zoomSettingsForViewForView();
    return m_configGroup->readEntry(ZoomSettingsForView.name, ZoomSettingsForView.defaultValue);
}

KDirOperatorPrivate::ZoomSettingsForView KDirOperatorPrivate::zoomSettingsForViewForView() const {
     KFile::FileView fv = static_cast<KFile::FileView>(m_viewKind);
     if (KFile::isSimpleView(fv)) {
         if (m_decorationPosition == QStyleOptionViewItem::Top){
             // Simple view decoration above, aka Icons View
             // default to 43% aka 64px
             return {QStringLiteral("iconViewIconSize"), 43};
         } else {
             // Simple view decoration left, aka compact view
             // default to 15% aka 32px
             return {QStringLiteral("listViewIconSize"), 15};
         }
    } else {
         if (KFile::isTreeView(fv)) {
             return {QStringLiteral("treeViewIconSize"), 0};
         } else {
             // DetailView and DetailTreeView
             return {QStringLiteral("detailViewIconSize"), 0};
         }
    }
}

void KDirOperator::setViewConfig(KConfigGroup &configGroup)
{
    delete d->m_configGroup;
    d->m_configGroup = new KConfigGroup(configGroup);
}

KConfigGroup *KDirOperator::viewConfigGroup() const
{
    return d->m_configGroup;
}

void KDirOperator::setShowHiddenFiles(bool s)
{
    d->m_actionCollection->action(QStringLiteral("show hidden"))->setChecked(s);
}

bool KDirOperator::showHiddenFiles() const
{
    return d->m_actionCollection->action(QStringLiteral("show hidden"))->isChecked();
}

QStyleOptionViewItem::Position KDirOperator::decorationPosition() const
{
    return d->m_decorationPosition;
}

void KDirOperator::setDecorationPosition(QStyleOptionViewItem::Position position)
{
    d->m_decorationPosition = position;
    const bool decorationAtLeft = d->m_decorationPosition == QStyleOptionViewItem::Left;
    d->m_actionCollection->action(QStringLiteral("decorationAtLeft"))->setChecked(decorationAtLeft);
    d->m_actionCollection->action(QStringLiteral("decorationAtTop"))->setChecked(!decorationAtLeft);
}

bool KDirOperatorPrivate::isReadable(const QUrl &url)
{
    if (!url.isLocalFile()) {
        return true; // what else can we say?
    }
    return QDir(url.toLocalFile()).isReadable();
}

void KDirOperatorPrivate::slotDirectoryCreated(const QUrl &url)
{
    if (m_followNewDirectories) {
        q->setUrl(url, true);
    }
}

void KDirOperator::setSupportedSchemes(const QStringList &schemes)
{
    d->m_supportedSchemes = schemes;
    rereadDir();
}

QStringList KDirOperator::supportedSchemes() const
{
    return d->m_supportedSchemes;
}

#include "moc_kdiroperator.cpp"
