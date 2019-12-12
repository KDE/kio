/* This file is part of the KDE libraries
    Copyright (C) 1999,2000 Stephan Kulow <coolo@kde.org>
                  1999,2000,2001,2002,2003 Carsten Pfeiffer <pfeiffer@kde.org>

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.
*/

#include "kdiroperator.h"
#include <kprotocolmanager.h>
#include <kiconloader.h>
#include "kdirmodel.h"
#include "kdiroperatordetailview_p.h"
#include "kdiroperatoriconview_p.h"
#include "kdirsortfilterproxymodel.h"
#include "kfileitem.h"
#include "kfilemetapreview_p.h"
#include "kpreviewwidgetbase.h"
#include "knewfilemenu.h"
#include <kurlmimedata.h>
#include "../pathhelpers_p.h"

#include <config-kiofilewidgets.h>
#include <defaults-kfile.h> // ConfigGroup, DefaultShowHidden, DefaultDirsFirst, DefaultSortReversed

#include <QApplication>
#include <QHeaderView>
#include <QListView>
#include <QMenu>
#include <QProgressBar>
#include <QSplitter>
#include <QWheelEvent>
#include <QTimer>
#include <QDebug>
#include <QMimeDatabase>
#include <QStack>

#include <kdirlister.h>
#include <kfileitemdelegate.h>
#include <klocalizedstring.h>
#include <kmessagebox.h>
#include <kjobwidgets.h>
#include <kio/deletejob.h>
#include <kio/copyjob.h>
#include <kio/jobuidelegate.h>
#include <kio/previewjob.h>
#include <KIO/OpenFileManagerWindowJob>
#include <kfilepreviewgenerator.h>
#include <krun.h>
#include <kpropertiesdialog.h>
#include <kactioncollection.h>
#include <kconfiggroup.h>
#include <ksharedconfig.h>

template class QHash<QString, KFileItem>;

// QDir::SortByMask is not only undocumented, it also omits QDir::Type which  is another
// sorting mode.
static const int QDirSortMask = QDir::SortByMask | QDir::Type;

class Q_DECL_HIDDEN KDirOperator::Private
{
public:
    explicit Private(KDirOperator *parent);
    ~Private();

    enum InlinePreviewState {
        ForcedToFalse = 0,
        ForcedToTrue,
        NotForced
    };

    // private methods
    bool checkPreviewInternal() const;
    void checkPath(const QString &txt, bool takeFiles = false);
    bool openUrl(const QUrl &url, KDirLister::OpenUrlFlags flags = KDirLister::NoFlags);
    int sortColumn() const;
    Qt::SortOrder sortOrder() const;
    void updateSorting(QDir::SortFlags sort);

    static bool isReadable(const QUrl &url);
    bool isSchemeSupported(const QString &scheme) const;

    KFile::FileView allViews();

    QMetaObject::Connection m_connection;

    // A pair to store zoom settings for view kinds
    struct ZoomSettingsForView {
        QString name;
        int defaultValue;
    };

    // private slots
    void _k_slotDetailedView();
    void _k_slotSimpleView();
    void _k_slotTreeView();
    void _k_slotDetailedTreeView();
    void _k_slotIconsView();
    void _k_slotCompactView();
    void _k_slotDetailsView();
    void _k_slotToggleHidden(bool);
    void _k_slotToggleAllowExpansion(bool);
    void _k_togglePreview(bool);
    void _k_toggleInlinePreviews(bool);
    void _k_slotOpenFileManager();
    void _k_slotSortByName();
    void _k_slotSortBySize();
    void _k_slotSortByDate();
    void _k_slotSortByType();
    void _k_slotSortReversed(bool doReverse);
    void _k_slotToggleDirsFirst();
    void _k_slotToggleIconsView();
    void _k_slotToggleCompactView();
    void _k_slotToggleDetailsView();
    void _k_slotToggleIgnoreCase();
    void _k_slotStarted();
    void _k_slotProgress(int);
    void _k_slotShowProgress();
    void _k_slotIOFinished();
    void _k_slotCanceled();
    void _k_slotRedirected(const QUrl &);
    void _k_slotProperties();
    void _k_slotActivated(const QModelIndex &);
    void _k_slotSelectionChanged();
    void _k_openContextMenu(const QPoint &);
    void _k_triggerPreview(const QModelIndex &);
    void _k_showPreview();
    void _k_slotSplitterMoved(int, int);
    void _k_assureVisibleSelection();
    void _k_synchronizeSortingState(int, Qt::SortOrder);
    void _k_slotChangeDecorationPosition();
    void _k_slotExpandToUrl(const QModelIndex &);
    void _k_slotItemsChanged();
    void _k_slotDirectoryCreated(const QUrl &);

    int iconSizeForViewType(QAbstractItemView *itemView) const;
    void writeIconZoomSettingsIfNeeded();
    ZoomSettingsForView zoomSettingsForViewForView() const;

    // private members
    KDirOperator * const parent;
    QStack<QUrl *> backStack;   ///< Contains all URLs you can reach with the back button.
    QStack<QUrl *> forwardStack; ///< Contains all URLs you can reach with the forward button.

    QModelIndex lastHoveredIndex;

    KDirLister *dirLister;
    QUrl currUrl;

    KCompletion completion;
    KCompletion dirCompletion;
    bool completeListDirty;
    QDir::SortFlags sorting;
    QStyleOptionViewItem::Position decorationPosition;

    QSplitter *splitter;

    QAbstractItemView *itemView;
    KDirModel *dirModel;
    KDirSortFilterProxyModel *proxyModel;

    KFileItemList pendingMimeTypes;

    // the enum KFile::FileView as an int
    int viewKind;
    int defaultView;

    KFile::Modes mode;
    QProgressBar *progressBar;

    KPreviewWidgetBase *preview;
    QUrl previewUrl;
    int previewWidth;

    bool dirHighlighting;
    bool onlyDoubleClickSelectsFiles;
    bool followNewDirectories;
    bool followSelectedDirectories;
    QString lastURL; // used for highlighting a directory on cdUp
    QTimer *progressDelayTimer;
    int dropOptions;

    KActionMenu *actionMenu;
    KActionCollection *actionCollection;

    KNewFileMenu *newFileMenu;

    KConfigGroup *configGroup;

    KFilePreviewGenerator *previewGenerator;

    bool showPreviews;
    int iconsZoom;

    bool isSaving;

    KActionMenu *decorationMenu;
    KToggleAction *leftAction;
    QList<QUrl> itemsToBeSetAsCurrent;
    bool shouldFetchForItems;
    InlinePreviewState inlinePreviewState;
    QStringList supportedSchemes;
};

KDirOperator::Private::Private(KDirOperator *_parent) :
    parent(_parent),
    dirLister(nullptr),
    decorationPosition(QStyleOptionViewItem::Left),
    splitter(nullptr),
    itemView(nullptr),
    dirModel(nullptr),
    proxyModel(nullptr),
    progressBar(nullptr),
    preview(nullptr),
    previewUrl(),
    previewWidth(0),
    dirHighlighting(false),
    onlyDoubleClickSelectsFiles(!qApp->style()->styleHint(QStyle::SH_ItemView_ActivateItemOnSingleClick)),
    followNewDirectories(true),
    followSelectedDirectories(true),
    progressDelayTimer(nullptr),
    dropOptions(0),
    actionMenu(nullptr),
    actionCollection(nullptr),
    newFileMenu(nullptr),
    configGroup(nullptr),
    previewGenerator(nullptr),
    showPreviews(false),
    iconsZoom(0),
    isSaving(false),
    decorationMenu(nullptr),
    leftAction(nullptr),
    shouldFetchForItems(false),
    inlinePreviewState(NotForced)
{
}

KDirOperator::Private::~Private()
{
    delete itemView;
    itemView = nullptr;

    // TODO:
    // if (configGroup) {
    //     itemView->writeConfig(configGroup);
    // }

    qDeleteAll(backStack);
    qDeleteAll(forwardStack);
    delete preview;
    preview = nullptr;

    delete proxyModel;
    proxyModel = nullptr;
    delete dirModel;
    dirModel = nullptr;
    dirLister = nullptr; // deleted by KDirModel
    delete configGroup;
    configGroup = nullptr;

    delete progressDelayTimer;
    progressDelayTimer = nullptr;
}

KDirOperator::KDirOperator(const QUrl &_url, QWidget *parent) :
    QWidget(parent),
    d(new Private(this))
{
    d->splitter = new QSplitter(this);
    d->splitter->setChildrenCollapsible(false);
    connect(d->splitter, SIGNAL(splitterMoved(int,int)),
            this, SLOT(_k_slotSplitterMoved(int,int)));

    d->preview = nullptr;

    d->mode = KFile::File;
    d->viewKind = KFile::Simple;

    if (_url.isEmpty()) { // no dir specified -> current dir
        QString strPath = QDir::currentPath();
        strPath.append(QLatin1Char('/'));
        d->currUrl = QUrl::fromLocalFile(strPath);
    } else {
        d->currUrl = _url;
        if (d->currUrl.scheme().isEmpty()) {
            d->currUrl.setScheme(QStringLiteral("file"));
        }

        QString path = d->currUrl.path();
        if (!path.endsWith(QLatin1Char('/'))) {
            path.append(QLatin1Char('/')); // make sure we have a trailing slash!
        }
        d->currUrl.setPath(path);
    }

    // We set the direction of this widget to LTR, since even on RTL desktops
    // viewing directory listings in RTL mode makes people's head explode.
    // Is this the correct place? Maybe it should be in some lower level widgets...?
    setLayoutDirection(Qt::LeftToRight);
    setDirLister(new KDirLister());

    connect(&d->completion, &KCompletion::match,
            this, &KDirOperator::slotCompletionMatch);

    d->progressBar = new QProgressBar(this);
    d->progressBar->setObjectName(QStringLiteral("d->progressBar"));
    d->progressBar->adjustSize();
    d->progressBar->move(2, height() - d->progressBar->height() - 2);

    d->progressDelayTimer = new QTimer(this);
    d->progressDelayTimer->setObjectName(QStringLiteral("d->progressBar delay timer"));
    connect(d->progressDelayTimer, SIGNAL(timeout()),
            SLOT(_k_slotShowProgress()));

    d->completeListDirty = false;

    // action stuff
    setupActions();
    setupMenu();

    d->sorting = QDir::NoSort;  //so updateSorting() doesn't think nothing has changed
    d->updateSorting(QDir::Name | QDir::DirsFirst);

    setFocusPolicy(Qt::WheelFocus);
    setAcceptDrops(true);
}

KDirOperator::~KDirOperator()
{
    resetCursor();
    disconnect(d->dirLister, nullptr, this, nullptr);
    delete d;
}

void KDirOperator::setSorting(QDir::SortFlags spec)
{
    d->updateSorting(spec);
}

QDir::SortFlags KDirOperator::sorting() const
{
    return d->sorting;
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
    return d->dirLister;
}

void KDirOperator::resetCursor()
{
    if (qApp) {
        QApplication::restoreOverrideCursor();
    }
    d->progressBar->hide();
}

void KDirOperator::sortByName()
{
    d->updateSorting((d->sorting & ~QDirSortMask) | QDir::Name);
}

void KDirOperator::sortBySize()
{
    d->updateSorting((d->sorting & ~QDirSortMask) | QDir::Size);
}

void KDirOperator::sortByDate()
{
    d->updateSorting((d->sorting & ~QDirSortMask) | QDir::Time);
}

void KDirOperator::sortByType()
{
    d->updateSorting((d->sorting & ~QDirSortMask) | QDir::Type);
}

void KDirOperator::sortReversed()
{
    // toggle it, hence the inversion of current state
    d->_k_slotSortReversed(!(d->sorting & QDir::Reversed));
}

void KDirOperator::toggleDirsFirst()
{
    d->_k_slotToggleDirsFirst();
}

void KDirOperator::toggleIgnoreCase()
{
    if (d->proxyModel != nullptr) {
        Qt::CaseSensitivity cs = d->proxyModel->sortCaseSensitivity();
        cs = (cs == Qt::CaseSensitive) ? Qt::CaseInsensitive : Qt::CaseSensitive;
        d->proxyModel->setSortCaseSensitivity(cs);
    }
}

void KDirOperator::updateSelectionDependentActions()
{
    const bool hasSelection = (d->itemView != nullptr) &&
                              d->itemView->selectionModel()->hasSelection();
    d->actionCollection->action(QStringLiteral("trash"))->setEnabled(hasSelection);
    d->actionCollection->action(QStringLiteral("delete"))->setEnabled(hasSelection);
    d->actionCollection->action(QStringLiteral("properties"))->setEnabled(hasSelection);
}

void KDirOperator::setPreviewWidget(KPreviewWidgetBase *w)
{
    const bool showPreview = (w != nullptr);
    if (showPreview) {
        d->viewKind = (d->viewKind | KFile::PreviewContents);
    } else {
        d->viewKind = (d->viewKind & ~KFile::PreviewContents);
    }

    delete d->preview;
    d->preview = w;

    if (w) {
        d->splitter->addWidget(w);
    }

    KToggleAction *previewAction = static_cast<KToggleAction *>(d->actionCollection->action(QStringLiteral("preview")));
    previewAction->setEnabled(showPreview);
    previewAction->setChecked(showPreview);
    setView(static_cast<KFile::FileView>(d->viewKind));
}

KFileItemList KDirOperator::selectedItems() const
{
    KFileItemList itemList;
    if (d->itemView == nullptr) {
        return itemList;
    }

    const QItemSelection selection = d->proxyModel->mapSelectionToSource(d->itemView->selectionModel()->selection());

    const QModelIndexList indexList = selection.indexes();
    for (const QModelIndex &index : indexList) {
        KFileItem item = d->dirModel->itemForIndex(index);
        if (!item.isNull()) {
            itemList.append(item);
        }
    }

    return itemList;
}

bool KDirOperator::isSelected(const KFileItem &item) const
{
    if ((item.isNull()) || (d->itemView == nullptr)) {
        return false;
    }

    const QModelIndex dirIndex = d->dirModel->indexForItem(item);
    const QModelIndex proxyIndex = d->proxyModel->mapFromSource(dirIndex);
    return d->itemView->selectionModel()->isSelected(proxyIndex);
}

int KDirOperator::numDirs() const
{
    return (d->dirLister == nullptr) ? 0 : d->dirLister->directories().count();
}

int KDirOperator::numFiles() const
{
    return (d->dirLister == nullptr) ? 0 : d->dirLister->items().count() - numDirs();
}

KCompletion *KDirOperator::completionObject() const
{
    return const_cast<KCompletion *>(&d->completion);
}

KCompletion *KDirOperator::dirCompletionObject() const
{
    return const_cast<KCompletion *>(&d->dirCompletion);
}

KActionCollection *KDirOperator::actionCollection() const
{
    return d->actionCollection;
}

KFile::FileView KDirOperator::Private::allViews()
{
    return static_cast<KFile::FileView>(KFile::Simple | KFile::Detail | KFile::Tree | KFile::DetailTree);
}

void KDirOperator::Private::_k_slotDetailedView()
{
    // save old zoom settings
    writeIconZoomSettingsIfNeeded();

    KFile::FileView view = static_cast<KFile::FileView>((viewKind & ~allViews()) | KFile::Detail);
    parent->setView(view);
}

void KDirOperator::Private::_k_slotSimpleView()
{
    // save old zoom settings
    writeIconZoomSettingsIfNeeded();

    KFile::FileView view = static_cast<KFile::FileView>((viewKind & ~allViews()) | KFile::Simple);
    parent->setView(view);
}

void KDirOperator::Private::_k_slotTreeView()
{
    // save old zoom settings
    writeIconZoomSettingsIfNeeded();

    KFile::FileView view = static_cast<KFile::FileView>((viewKind & ~allViews()) | KFile::Tree);
    parent->setView(view);
}

void KDirOperator::Private::_k_slotDetailedTreeView()
{
    // save old zoom settings
    writeIconZoomSettingsIfNeeded();

    KFile::FileView view = static_cast<KFile::FileView>((viewKind & ~allViews()) | KFile::DetailTree);
    parent->setView(view);
}

void KDirOperator::Private::_k_slotToggleAllowExpansion(bool allow) {
    KFile::FileView view = KFile::Detail;
    if (allow) {
        view = KFile::DetailTree;
    }
    parent->setView(view);
}

void KDirOperator::Private::_k_slotToggleHidden(bool show)
{
    dirLister->setShowingDotFiles(show);
    parent->updateDir();
    _k_assureVisibleSelection();
}

void KDirOperator::Private::_k_togglePreview(bool on)
{
    if (on) {
        viewKind = viewKind | KFile::PreviewContents;
        if (preview == nullptr) {
            preview = new KFileMetaPreview(parent);
            actionCollection->action(QStringLiteral("preview"))->setChecked(true);
            splitter->addWidget(preview);
        }

        preview->show();

        QMetaObject::invokeMethod(parent, "_k_assureVisibleSelection", Qt::QueuedConnection);
        if (itemView != nullptr) {
            const QModelIndex index = itemView->selectionModel()->currentIndex();
            if (index.isValid()) {
                _k_triggerPreview(index);
            }
        }
    } else if (preview != nullptr) {
        viewKind = viewKind & ~KFile::PreviewContents;
        preview->hide();
    }
}

void KDirOperator::Private::_k_toggleInlinePreviews(bool show)
{
    if (showPreviews == show) {
        return;
    }

    showPreviews = show;

    if (!previewGenerator) {
        return;
    }

    previewGenerator->setPreviewShown(show);
}

void KDirOperator::Private::_k_slotOpenFileManager()
{
    const KFileItemList list = parent->selectedItems();
    if (list.isEmpty()) {
        KIO::highlightInFileManager({currUrl.adjusted(QUrl::StripTrailingSlash)});
    } else {
        KIO::highlightInFileManager(list.urlList());
    }
}

void KDirOperator::Private::_k_slotSortByName()
{
    parent->sortByName();
}

void KDirOperator::Private::_k_slotSortBySize()
{
    parent->sortBySize();
}

void KDirOperator::Private::_k_slotSortByDate()
{
    parent->sortByDate();
}

void KDirOperator::Private::_k_slotSortByType()
{
    parent->sortByType();
}

void KDirOperator::Private::_k_slotSortReversed(bool doReverse)
{
    QDir::SortFlags s = sorting & ~QDir::Reversed;
    if (doReverse) {
        s |= QDir::Reversed;
    }
    updateSorting(s);
}

void KDirOperator::Private::_k_slotToggleDirsFirst()
{
    QDir::SortFlags s = (sorting ^ QDir::DirsFirst);
    updateSorting(s);
}

void KDirOperator::Private::_k_slotIconsView()
{
    // save old zoom settings
    writeIconZoomSettingsIfNeeded();

    // Put the icons on top
    actionCollection->action(QStringLiteral("decorationAtTop"))->setChecked(true);
    decorationPosition = QStyleOptionViewItem::Top;

    // Switch to simple view
    KFile::FileView fileView = static_cast<KFile::FileView>((viewKind & ~allViews()) | KFile::Simple);
    parent->setView(fileView);
}

void KDirOperator::Private::_k_slotCompactView()
{
    // save old zoom settings
    writeIconZoomSettingsIfNeeded();

    // Put the icons on the side
    actionCollection->action(QStringLiteral("decorationAtLeft"))->setChecked(true);
    decorationPosition = QStyleOptionViewItem::Left;

    // Switch to simple view
    KFile::FileView fileView = static_cast<KFile::FileView>((viewKind & ~allViews()) | KFile::Simple);
    parent->setView(fileView);
}

void KDirOperator::Private::_k_slotDetailsView()
{
    // save old zoom settings
    writeIconZoomSettingsIfNeeded();

    KFile::FileView view;
    if (actionCollection->action(QStringLiteral("allow expansion"))->isChecked()) {
        view = static_cast<KFile::FileView>((viewKind & ~allViews()) | KFile::DetailTree);
    } else {
        view = static_cast<KFile::FileView>((viewKind & ~allViews()) | KFile::Detail);
    }
    parent->setView(view);
}

void KDirOperator::Private::_k_slotToggleIgnoreCase()
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
    d->newFileMenu->setPopupFiles(QList<QUrl>() << url());
    d->newFileMenu->setViewShowsHiddenFiles(showHiddenFiles());
    d->newFileMenu->createDirectory();
}

bool KDirOperator::mkdir(const QString &directory, bool enterDirectory)
{
    // Creates "directory", relative to the current directory (d->currUrl).
    // The given path may contain any number directories, existent or not.
    // They will all be created, if possible.

    // TODO: very similar to KDirSelectDialog::Private::slotMkdir

    bool writeOk = false;
    bool exists = false;
    QUrl folderurl(d->currUrl);

    const QStringList dirs = directory.split(QLatin1Char('/'), QString::SkipEmptyParts);
    QStringList::ConstIterator it = dirs.begin();

    for (; it != dirs.end(); ++it) {
        folderurl.setPath(concatPaths(folderurl.path(), *it));
        if (folderurl.isLocalFile()) {
            exists = QFile::exists(folderurl.toLocalFile());
        } else {
            KIO::StatJob *job = KIO::stat(folderurl);
            KJobWidgets::setWindow(job, this);
            job->setDetails(0); //We only want to know if it exists, 0 == that.
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
        KMessageBox::sorry(d->itemView, i18n("A file or folder named %1 already exists.",
                                             folderurl.toDisplayString(QUrl::PreferLocalFile)));
    } else if (!writeOk) {
        KMessageBox::sorry(d->itemView, i18n("You do not have permission to "
                                             "create that folder."));
    } else if (enterDirectory) {
        setUrl(folderurl, true);
    }

    return writeOk;
}

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

void KDirOperator::deleteSelected()
{
    const KFileItemList list = selectedItems();
    if (!list.isEmpty()) {
        del(list, this);
    }
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
    return d->previewGenerator;
}

void KDirOperator::setInlinePreviewShown(bool show)
{
    d->inlinePreviewState = show ? Private::ForcedToTrue : Private::ForcedToFalse;
}

bool KDirOperator::isInlinePreviewShown() const
{
    return d->showPreviews;
}

int KDirOperator::iconsZoom() const
{
    return d->iconsZoom;
}

void KDirOperator::setIsSaving(bool isSaving)
{
    d->isSaving = isSaving;
}

bool KDirOperator::isSaving() const
{
    return d->isSaving;
}

void KDirOperator::trashSelected()
{
    if (d->itemView == nullptr) {
        return;
    }

    if (QApplication::keyboardModifiers() & Qt::ShiftModifier) {
        deleteSelected();
        return;
    }

    const KFileItemList list = selectedItems();
    if (!list.isEmpty()) {
        trash(list, this);
    }
}

void KDirOperator::setIconsZoom(int _value)
{
    if (d->iconsZoom == _value) {
        return;
    }

    int value = _value;
    value = qMin(100, value);
    value = qMax(0, value);

    d->iconsZoom = value;

    if (!d->previewGenerator) {
        return;
    }

    const int maxSize = KIconLoader::SizeEnormous - KIconLoader::SizeSmall;
    const int val = (maxSize * value / 100) + KIconLoader::SizeSmall;
    d->itemView->setIconSize(QSize(val, val));
    d->previewGenerator->updatePreviews();

    emit currentIconSizeChanged(value);
}

void KDirOperator::close()
{
    resetCursor();
    d->pendingMimeTypes.clear();
    d->completion.clear();
    d->dirCompletion.clear();
    d->completeListDirty = true;
    d->dirLister->stop();
}

void KDirOperator::Private::checkPath(const QString &, bool /*takeFiles*/) // SLOT
{
#if 0
    // copy the argument in a temporary string
    QString text = _txt;
    // it's unlikely to happen, that at the beginning are spaces, but
    // for the end, it happens quite often, I guess.
    text = text.trimmed();
    // if the argument is no URL (the check is quite fragil) and it's
    // no absolute path, we add the current directory to get a correct url
    if (text.find(':') < 0 && text[0] != '/') {
        text.insert(0, d->currUrl);
    }

    // in case we have a selection defined and someone patched the file-
    // name, we check, if the end of the new name is changed.
    if (!selection.isNull()) {
        int position = text.lastIndexOf('/');
        ASSERT(position >= 0); // we already inserted the current d->dirLister in case
        QString filename = text.mid(position + 1, text.length());
        if (filename != selection) {
            selection.clear();
        }
    }

    QUrl u(text); // I have to take care of entered URLs
    bool filenameEntered = false;

    if (u.isLocalFile()) {
        // the empty path is kind of a hack
        KFileItem i("", u.toLocalFile());
        if (i.isDir()) {
            setUrl(text, true);
        } else {
            if (takeFiles)
                if (acceptOnlyExisting && !i.isFile()) {
                    warning("you entered an invalid URL");
                } else {
                    filenameEntered = true;
                }
        }
    } else {
        setUrl(text, true);
    }

    if (filenameEntered) {
        filename_ = u.url();
        emit fileSelected(filename_);

        QApplication::restoreOverrideCursor();

        accept();
    }
#endif
    // qDebug() << "TODO KDirOperator::checkPath()";
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
    if (newurl.matches(d->currUrl, QUrl::StripTrailingSlash)) {
        return;
    }

    if (!d->isSchemeSupported(newurl.scheme()))
        return;

    if (!Private::isReadable(newurl)) {
        // maybe newurl is a file? check its parent directory
        newurl = newurl.adjusted(QUrl::RemoveFilename | QUrl::StripTrailingSlash);
        if (newurl.matches(d->currUrl, QUrl::StripTrailingSlash)) {
            return;    // parent is current dir, nothing to do (fixes #173454, too)
        }
        KIO::StatJob *job = KIO::stat(newurl);
        KJobWidgets::setWindow(job, this);
        bool res = job->exec();

        KIO::UDSEntry entry = job->statResult();
        KFileItem i(entry, newurl);
        if ((!res || !Private::isReadable(newurl)) && i.isDir()) {
            resetCursor();
            KMessageBox::error(d->itemView,
                               i18n("The specified folder does not exist "
                                    "or was not readable."));
            return;
        } else if (!i.isDir()) {
            return;
        }
    }

    if (clearforward) {
        // autodelete should remove this one
        d->backStack.push(new QUrl(d->currUrl));
        qDeleteAll(d->forwardStack);
        d->forwardStack.clear();
    }

    d->lastURL = d->currUrl.toString(QUrl::StripTrailingSlash);
    d->currUrl = newurl;

    pathChanged();
    emit urlEntered(newurl);

    // enable/disable actions
    QAction *forwardAction = d->actionCollection->action(QStringLiteral("forward"));
    forwardAction->setEnabled(!d->forwardStack.isEmpty());

    QAction *backAction = d->actionCollection->action(QStringLiteral("back"));
    backAction->setEnabled(!d->backStack.isEmpty());

    QAction *upAction = d->actionCollection->action(QStringLiteral("up"));
    upAction->setEnabled(!isRoot());

    d->openUrl(newurl);
}

void KDirOperator::updateDir()
{
    QApplication::setOverrideCursor(Qt::WaitCursor);
    d->dirLister->emitChanges();
    QApplication::restoreOverrideCursor();
}

void KDirOperator::rereadDir()
{
    pathChanged();
    d->openUrl(d->currUrl, KDirLister::Reload);
}

bool KDirOperator::Private::isSchemeSupported(const QString &scheme) const
{
    return supportedSchemes.isEmpty() || supportedSchemes.contains(scheme);
}

bool KDirOperator::Private::openUrl(const QUrl &url, KDirLister::OpenUrlFlags flags)
{
    const bool result = KProtocolManager::supportsListing(url)
                        && isSchemeSupported(url.scheme())
                        && dirLister->openUrl(url, flags);
    if (!result) { // in that case, neither completed() nor canceled() will be emitted by KDL
        _k_slotCanceled();
    }

    return result;
}

int KDirOperator::Private::sortColumn() const
{
    int column = KDirModel::Name;
    if (KFile::isSortByDate(sorting)) {
        column = KDirModel::ModifiedTime;
    } else if (KFile::isSortBySize(sorting)) {
        column = KDirModel::Size;
    } else if (KFile::isSortByType(sorting)) {
        column = KDirModel::Type;
    } else {
        Q_ASSERT(KFile::isSortByName(sorting));
    }

    return column;
}

Qt::SortOrder KDirOperator::Private::sortOrder() const
{
    return (sorting & QDir::Reversed) ? Qt::DescendingOrder :
           Qt::AscendingOrder;
}

void KDirOperator::Private::updateSorting(QDir::SortFlags sort)
{
    // qDebug() << "changing sort flags from"  << sorting << "to" << sort;
    if (sort == sorting) {
        return;
    }

    if ((sorting ^ sort) & QDir::DirsFirst) {
        // The "Folders First" setting has been changed.
        // We need to make sure that the files and folders are really re-sorted.
        // Without the following intermediate "fake resorting",
        // QSortFilterProxyModel::sort(int column, Qt::SortOrder order)
        // would do nothing because neither the column nor the sort order have been changed.
        Qt::SortOrder tmpSortOrder = (sortOrder() == Qt::AscendingOrder ? Qt::DescendingOrder : Qt::AscendingOrder);
        proxyModel->sort(sortOrder(), tmpSortOrder);
        proxyModel->setSortFoldersFirst(sort & QDir::DirsFirst);
    }

    sorting = sort;
    parent->updateSortActions();
    proxyModel->sort(sortColumn(), sortOrder());

    // TODO: The headers from QTreeView don't take care about a sorting
    // change of the proxy model hence they must be updated the manually.
    // This is done here by a qobject_cast, but it would be nicer to:
    // - provide a signal 'sortingChanged()'
    // - connect KDirOperatorDetailView() with this signal and update the
    //   header internally
    QTreeView *treeView = qobject_cast<QTreeView *>(itemView);
    if (treeView != nullptr) {
        QHeaderView *headerView = treeView->header();
        headerView->blockSignals(true);
        headerView->setSortIndicator(sortColumn(), sortOrder());
        headerView->blockSignals(false);
    }

    _k_assureVisibleSelection();
}

// Protected
void KDirOperator::pathChanged()
{
    if (d->itemView == nullptr) {
        return;
    }

    d->pendingMimeTypes.clear();
    //d->fileView->clear(); TODO
    d->completion.clear();
    d->dirCompletion.clear();

    // it may be, that we weren't ready at this time
    QApplication::restoreOverrideCursor();

    // when KIO::Job emits finished, the slot will restore the cursor
    QApplication::setOverrideCursor(Qt::WaitCursor);

    if (!Private::isReadable(d->currUrl)) {
        KMessageBox::error(d->itemView,
                           i18n("The specified folder does not exist "
                                "or was not readable."));
        if (d->backStack.isEmpty()) {
            home();
        } else {
            back();
        }
    }
}

void KDirOperator::Private::_k_slotRedirected(const QUrl &newURL)
{
    currUrl = newURL;
    pendingMimeTypes.clear();
    completion.clear();
    dirCompletion.clear();
    completeListDirty = true;
    emit parent->urlEntered(newURL);
}

// Code pinched from kfm then hacked
void KDirOperator::back()
{
    if (d->backStack.isEmpty()) {
        return;
    }

    d->forwardStack.push(new QUrl(d->currUrl));

    QUrl *s = d->backStack.pop();

    setUrl(*s, false);
    delete s;
}

// Code pinched from kfm then hacked
void KDirOperator::forward()
{
    if (d->forwardStack.isEmpty()) {
        return;
    }

    d->backStack.push(new QUrl(d->currUrl));

    QUrl *s = d->forwardStack.pop();
    setUrl(*s, false);
    delete s;
}

QUrl KDirOperator::url() const
{
    return d->currUrl;
}

void KDirOperator::cdUp()
{
    // Allow /d/c// to go up to /d/ instead of /d/c/
    QUrl tmp(d->currUrl.adjusted(QUrl::NormalizePathSegments));
    setUrl(tmp.resolved(QUrl(QStringLiteral(".."))), true);
}

void KDirOperator::home()
{
    setUrl(QUrl::fromLocalFile(QDir::homePath()), true);
}

void KDirOperator::clearFilter()
{
    d->dirLister->setNameFilter(QString());
    d->dirLister->clearMimeFilter();
    checkPreviewSupport();
}

void KDirOperator::setNameFilter(const QString &filter)
{
    d->dirLister->setNameFilter(filter);
    checkPreviewSupport();
}

QString KDirOperator::nameFilter() const
{
    return d->dirLister->nameFilter();
}

void KDirOperator::setMimeFilter(const QStringList &mimetypes)
{
    d->dirLister->setMimeFilter(mimetypes);
    checkPreviewSupport();
}

QStringList KDirOperator::mimeFilter() const
{
    return d->dirLister->mimeFilters();
}

void KDirOperator::setNewFileMenuSupportedMimeTypes(const QStringList &mimeTypes)
{
    d->newFileMenu->setSupportedMimeTypes(mimeTypes);
}

QStringList KDirOperator::newFileMenuSupportedMimeTypes() const
{
    return d->newFileMenu->supportedMimeTypes();
}

bool KDirOperator::checkPreviewSupport()
{
    KToggleAction *previewAction = static_cast<KToggleAction *>(d->actionCollection->action(QStringLiteral("preview")));

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

    d->newFileMenu->setPopupFiles(QList<QUrl>() << item.url());
    d->newFileMenu->setViewShowsHiddenFiles(showHiddenFiles());
    d->newFileMenu->checkUpToDate();

    d->actionCollection->action(QStringLiteral("new"))->setEnabled(item.isDir());

    emit contextMenuAboutToShow(item, d->actionMenu->menu());

    d->actionMenu->menu()->exec(pos);
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
        if (d->preview && !d->preview->isHidden()) {
            const QModelIndex hoveredIndex = d->itemView->indexAt(d->itemView->viewport()->mapFromGlobal(QCursor::pos()));

            if (d->lastHoveredIndex == hoveredIndex) {
                return QWidget::eventFilter(watched, event);
            }

            d->lastHoveredIndex = hoveredIndex;

            const QModelIndex focusedIndex = d->itemView->selectionModel() ? d->itemView->selectionModel()->currentIndex()
                                             : QModelIndex();

            if (!hoveredIndex.isValid() && focusedIndex.isValid() &&
                    d->itemView->selectionModel()->isSelected(focusedIndex) &&
                    (d->lastHoveredIndex != focusedIndex)) {
                const QModelIndex sourceFocusedIndex = d->proxyModel->mapToSource(focusedIndex);
                const KFileItem item = d->dirModel->itemForIndex(sourceFocusedIndex);
                if (!item.isNull()) {
                    d->preview->showPreview(item.url());
                }
            }
        }
    }
    break;
    case QEvent::MouseButtonRelease: {
        if (d->preview != nullptr && !d->preview->isHidden()) {
            const QModelIndex hoveredIndex = d->itemView->indexAt(d->itemView->viewport()->mapFromGlobal(QCursor::pos()));
            const QModelIndex focusedIndex = d->itemView->selectionModel() ? d->itemView->selectionModel()->currentIndex()
                                             : QModelIndex();

            if (((!focusedIndex.isValid()) ||
                    !d->itemView->selectionModel()->isSelected(focusedIndex)) &&
                    (!hoveredIndex.isValid())) {
                d->preview->clearPreview();
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
                setIconsZoom(d->iconsZoom + 10);
            } else {
                setIconsZoom(d->iconsZoom - 10);
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
            // mimetype filtering
            bool mimeFilterPass = true;
            const QStringList mimeFilters = d->dirLister->mimeFilters();

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

        KIO::UDSEntry entry = job->statResult();

        if (entry.isDir()) {
            // if this was a directory
            setUrl(url, false);
        } else {
            // if the current url is not known
            if (d->dirLister->findByUrl(url).isNull()) {
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
            if (!d->itemView->currentIndex().isValid()) {
                emit keyEnterReturnPressed();
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

bool KDirOperator::Private::checkPreviewInternal() const
{
    const QStringList supported = KIO::PreviewJob::supportedMimeTypes();
    // no preview support for directories?
    if (parent->dirOnlyMode() && supported.indexOf(QLatin1String("inode/directory")) == -1) {
        return false;
    }

    QStringList mimeTypes = dirLister->mimeFilters();
    const QStringList nameFilter = dirLister->nameFilter().split(QLatin1Char(' '), QString::SkipEmptyParts);
    QMimeDatabase db;

    if (mimeTypes.isEmpty() && nameFilter.isEmpty() && !supported.isEmpty()) {
        return true;
    } else {
        QRegExp r;
        r.setPatternSyntax(QRegExp::Wildcard);   // the "mimetype" can be "image/*"

        if (!mimeTypes.isEmpty()) {
            QStringList::ConstIterator it = supported.begin();

            for (; it != supported.end(); ++it) {
                r.setPattern(*it);

                QStringList result = mimeTypes.filter(r);
                if (!result.isEmpty()) {   // matches! -> we want previews
                    return true;
                }
            }
        }

        if (!nameFilter.isEmpty()) {
            // find the mimetypes of all the filter-patterns
            QStringList::const_iterator it1 = nameFilter.begin();
            for (; it1 != nameFilter.end(); ++it1) {
                if ((*it1) == QLatin1String("*")) {
                    return true;
                }

                QMimeType mt = db.mimeTypeForFile(*it1, QMimeDatabase::MatchExtension /*fast mode, no file contents exist*/);
                if (!mt.isValid()) {
                    continue;
                }
                QString mime = mt.name();

                // the "mimetypes" we get from the PreviewJob can be "image/*"
                // so we need to check in wildcard mode
                QStringList::ConstIterator it2 = supported.begin();
                for (; it2 != supported.end(); ++it2) {
                    r.setPattern(*it2);
                    if (r.indexIn(mime) != -1) {
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
    d->dropOptions = options;
    // TODO:
    //if (d->fileView)
    //   d->fileView->setDropOptions(options);
}

void KDirOperator::setView(KFile::FileView viewKind)
{
    bool preview = (KFile::isPreviewInfo(viewKind) || KFile::isPreviewContents(viewKind));

    if (viewKind == KFile::Default) {
        if (KFile::isDetailView((KFile::FileView)d->defaultView)) {
            viewKind = KFile::Detail;
        } else if (KFile::isTreeView((KFile::FileView)d->defaultView)) {
            viewKind = KFile::Tree;
        } else if (KFile::isDetailTreeView((KFile::FileView)d->defaultView)) {
            viewKind = KFile::DetailTree;
        } else {
            viewKind = KFile::Simple;
        }

        const KFile::FileView defaultViewKind = static_cast<KFile::FileView>(d->defaultView);
        preview = (KFile::isPreviewInfo(defaultViewKind) || KFile::isPreviewContents(defaultViewKind))
                  && d->actionCollection->action(QStringLiteral("preview"))->isEnabled();
    }

    d->viewKind = static_cast<int>(viewKind);
    viewKind = static_cast<KFile::FileView>(d->viewKind);

    QAbstractItemView *newView = createView(this, viewKind);
    setView(newView);

    if (acceptDrops()) {
        newView->setAcceptDrops(true);
        newView->installEventFilter(this);
    }

    d->_k_togglePreview(preview);
}

KFile::FileView KDirOperator::viewMode() const
{
    return static_cast<KFile::FileView>(d->viewKind);
}

QAbstractItemView *KDirOperator::view() const
{
    return d->itemView;
}

KFile::Modes KDirOperator::mode() const
{
    return d->mode;
}

void KDirOperator::setMode(KFile::Modes mode)
{
    if (d->mode == mode) {
        return;
    }

    d->mode = mode;

    d->dirLister->setDirOnlyMode(dirOnlyMode());

    // reset the view with the different mode
    if (d->itemView != nullptr) {
        setView(static_cast<KFile::FileView>(d->viewKind));
    }
}

void KDirOperator::setView(QAbstractItemView *view)
{
    if (view == d->itemView) {
        return;
    }

    // TODO: do a real timer and restart it after that
    d->pendingMimeTypes.clear();
    const bool listDir = (d->itemView == nullptr);

    if (d->mode & KFile::Files) {
        view->setSelectionMode(QAbstractItemView::ExtendedSelection);
    } else {
        view->setSelectionMode(QAbstractItemView::SingleSelection);
    }

    QItemSelectionModel *selectionModel = nullptr;
    if ((d->itemView != nullptr) && d->itemView->selectionModel()->hasSelection()) {
        // remember the selection of the current item view and apply this selection
        // to the new view later
        const QItemSelection selection = d->itemView->selectionModel()->selection();
        selectionModel = new QItemSelectionModel(d->proxyModel, this);
        selectionModel->select(selection, QItemSelectionModel::Select);
    }

    setFocusProxy(nullptr);
    delete d->itemView;
    d->itemView = view;
    d->itemView->setModel(d->proxyModel);
    setFocusProxy(d->itemView);

    view->viewport()->installEventFilter(this);

    KFileItemDelegate *delegate = new KFileItemDelegate(d->itemView);
    d->itemView->setItemDelegate(delegate);
    d->itemView->viewport()->setAttribute(Qt::WA_Hover);
    d->itemView->setContextMenuPolicy(Qt::CustomContextMenu);
    d->itemView->setMouseTracking(true);
    //d->itemView->setDropOptions(d->dropOptions);

    // first push our settings to the view, then listen for changes from the view
    QTreeView *treeView = qobject_cast<QTreeView *>(d->itemView);
    if (treeView) {
        QHeaderView *headerView = treeView->header();
        headerView->setSortIndicator(d->sortColumn(), d->sortOrder());
        connect(headerView, SIGNAL(sortIndicatorChanged(int,Qt::SortOrder)),
                this, SLOT(_k_synchronizeSortingState(int,Qt::SortOrder)));
    }

    connect(d->itemView, SIGNAL(activated(QModelIndex)),
            this, SLOT(_k_slotActivated(QModelIndex)));
    connect(d->itemView, SIGNAL(customContextMenuRequested(QPoint)),
            this, SLOT(_k_openContextMenu(QPoint)));
    connect(d->itemView, SIGNAL(entered(QModelIndex)),
            this, SLOT(_k_triggerPreview(QModelIndex)));

    d->splitter->insertWidget(0, d->itemView);

    d->splitter->resize(size());
    d->itemView->show();

    if (listDir) {
        QApplication::setOverrideCursor(Qt::WaitCursor);
        d->openUrl(d->currUrl);
    }

    if (selectionModel != nullptr) {
        d->itemView->setSelectionModel(selectionModel);
        QMetaObject::invokeMethod(this, "_k_assureVisibleSelection", Qt::QueuedConnection);
    }

    connect(d->itemView->selectionModel(),
            SIGNAL(currentChanged(QModelIndex,QModelIndex)),
            this, SLOT(_k_triggerPreview(QModelIndex)));
    connect(d->itemView->selectionModel(),
            SIGNAL(selectionChanged(QItemSelection,QItemSelection)),
            this, SLOT(_k_slotSelectionChanged()));

    // if we cannot cast it to a QListView, disable the "Icon Position" menu. Note that this check
    // needs to be done here, and not in createView, since we can be set an external view
    d->decorationMenu->setEnabled(qobject_cast<QListView *>(d->itemView));

    d->shouldFetchForItems = qobject_cast<QTreeView *>(view);
    if (d->shouldFetchForItems) {
        connect(d->dirModel, SIGNAL(expand(QModelIndex)), this, SLOT(_k_slotExpandToUrl(QModelIndex)));
    } else {
        d->itemsToBeSetAsCurrent.clear();
    }

    const bool previewForcedToTrue = d->inlinePreviewState == Private::ForcedToTrue;
    const bool previewShown = d->inlinePreviewState == Private::NotForced ? d->showPreviews : previewForcedToTrue;
    d->previewGenerator = new KFilePreviewGenerator(d->itemView);
    const int maxSize = KIconLoader::SizeEnormous - KIconLoader::SizeSmall;
    const int val = (maxSize * d->iconsZoom / 100) + KIconLoader::SizeSmall;
    d->itemView->setIconSize(previewForcedToTrue ? QSize(KIconLoader::SizeHuge, KIconLoader::SizeHuge) : QSize(val, val));
    d->previewGenerator->setPreviewShown(previewShown);
    d->actionCollection->action(QStringLiteral("inline preview"))->setChecked(previewShown);

    // ensure we change everything needed
    d->_k_slotChangeDecorationPosition();
    updateViewActions();

    emit viewChanged(view);

    const int zoom = previewForcedToTrue ? (KIconLoader::SizeHuge - KIconLoader::SizeSmall + 1) * 100 / maxSize : d->iconSizeForViewType(view);

    // this will make d->iconsZoom be updated, since setIconsZoom slot will be called
    emit currentIconSizeChanged(zoom);
}

void KDirOperator::setDirLister(KDirLister *lister)
{
    if (lister == d->dirLister) { // sanity check
        return;
    }

    delete d->dirModel;
    d->dirModel = nullptr;

    delete d->proxyModel;
    d->proxyModel = nullptr;

    //delete d->dirLister; // deleted by KDirModel already, which took ownership
    d->dirLister = lister;

    d->dirModel = new KDirModel();
    d->dirModel->setDirLister(d->dirLister);
    d->dirModel->setDropsAllowed(KDirModel::DropOnDirectory);

    d->shouldFetchForItems = qobject_cast<QTreeView *>(d->itemView);
    if (d->shouldFetchForItems) {
        connect(d->dirModel, SIGNAL(expand(QModelIndex)), this, SLOT(_k_slotExpandToUrl(QModelIndex)));
    } else {
        d->itemsToBeSetAsCurrent.clear();
    }

    d->proxyModel = new KDirSortFilterProxyModel(this);
    d->proxyModel->setSourceModel(d->dirModel);

    d->dirLister->setDelayedMimeTypes(true);

    QWidget *mainWidget = topLevelWidget();
    d->dirLister->setMainWindow(mainWidget);
    // qDebug() << "mainWidget=" << mainWidget;

    connect(d->dirLister, SIGNAL(percent(int)),
            SLOT(_k_slotProgress(int)));
    connect(d->dirLister, SIGNAL(started(QUrl)), SLOT(_k_slotStarted()));
    connect(d->dirLister, SIGNAL(completed()), SLOT(_k_slotIOFinished()));
    connect(d->dirLister, SIGNAL(canceled()), SLOT(_k_slotCanceled()));
    connect(d->dirLister, SIGNAL(redirection(QUrl)),
            SLOT(_k_slotRedirected(QUrl)));
    connect(d->dirLister, SIGNAL(newItems(KFileItemList)), SLOT(_k_slotItemsChanged()));
    connect(d->dirLister, SIGNAL(itemsDeleted(KFileItemList)), SLOT(_k_slotItemsChanged()));
    connect(d->dirLister, SIGNAL(itemsFilteredByMime(KFileItemList)), SLOT(_k_slotItemsChanged()));
    connect(d->dirLister, SIGNAL(clear()), SLOT(_k_slotItemsChanged()));
}

void KDirOperator::selectDir(const KFileItem &item)
{
    setUrl(item.targetUrl(), true);
}

void KDirOperator::selectFile(const KFileItem &item)
{
    QApplication::restoreOverrideCursor();

    emit fileSelected(item);
}

void KDirOperator::highlightFile(const KFileItem &item)
{
    if ((d->preview != nullptr && !d->preview->isHidden()) && !item.isNull()) {
        d->preview->showPreview(item.url());
    }

    emit fileHighlighted(item);
}

void KDirOperator::setCurrentItem(const QUrl &url)
{
    // qDebug();

    KFileItem item = d->dirLister->findByUrl(url);
    if (d->shouldFetchForItems && item.isNull()) {
        d->itemsToBeSetAsCurrent << url;
        d->dirModel->expandToUrl(url);
        return;
    }

    setCurrentItem(item);
}

void KDirOperator::setCurrentItem(const KFileItem &item)
{
    // qDebug();

    if (!d->itemView) {
        return;
    }

    QItemSelectionModel *selModel = d->itemView->selectionModel();
    if (selModel) {
        selModel->clear();
        if (!item.isNull()) {
            const QModelIndex dirIndex = d->dirModel->indexForItem(item);
            const QModelIndex proxyIndex = d->proxyModel->mapFromSource(dirIndex);
            selModel->setCurrentIndex(proxyIndex, QItemSelectionModel::Select);
        }
    }
}

void KDirOperator::setCurrentItems(const QList<QUrl> &urls)
{
    // qDebug();

    if (!d->itemView) {
        return;
    }

    KFileItemList itemList;
    for (const QUrl &url : urls) {
        KFileItem item = d->dirLister->findByUrl(url);
        if (d->shouldFetchForItems && item.isNull()) {
            d->itemsToBeSetAsCurrent << url;
            d->dirModel->expandToUrl(url);
            continue;
        }
        itemList << item;
    }

    setCurrentItems(itemList);
}

void KDirOperator::setCurrentItems(const KFileItemList &items)
{
    // qDebug();

    if (d->itemView == nullptr) {
        return;
    }

    QItemSelectionModel *selModel = d->itemView->selectionModel();
    if (selModel) {
        selModel->clear();
        QModelIndex proxyIndex;
        for (const KFileItem &item : items) {
            if (!item.isNull()) {
                const QModelIndex dirIndex = d->dirModel->indexForItem(item);
                proxyIndex = d->proxyModel->mapFromSource(dirIndex);
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
        d->itemView->selectionModel()->clear();
        return QString();
    }

    prepareCompletionObjects();
    return d->completion.makeCompletion(string);
}

QString KDirOperator::makeDirCompletion(const QString &string)
{
    if (string.isEmpty()) {
        d->itemView->selectionModel()->clear();
        return QString();
    }

    prepareCompletionObjects();
    return d->dirCompletion.makeCompletion(string);
}

void KDirOperator::prepareCompletionObjects()
{
    if (d->itemView == nullptr) {
        return;
    }

    if (d->completeListDirty) {   // create the list of all possible completions
        const KFileItemList itemList = d->dirLister->items();
        for (const KFileItem &item : itemList) {
            d->completion.addItem(item.name());
            if (item.isDir()) {
                d->dirCompletion.addItem(item.name());
            }
        }
        d->completeListDirty = false;
    }
}

void KDirOperator::slotCompletionMatch(const QString &match)
{
    QUrl url(match);
    if (url.isRelative()) {
        url = d->currUrl.resolved(url);
    }
    setCurrentItem(url);
    emit completion(match);
}

void KDirOperator::setupActions()
{
    d->actionCollection = new KActionCollection(this);
    d->actionCollection->setObjectName(QStringLiteral("KDirOperator::actionCollection"));

    d->actionMenu = new KActionMenu(i18n("Menu"), this);
    d->actionCollection->addAction(QStringLiteral("popupMenu"), d->actionMenu);

    QAction *upAction = d->actionCollection->addAction(KStandardAction::Up, QStringLiteral("up"), this, SLOT(cdUp()));
    upAction->setText(i18n("Parent Folder"));

    QAction *backAction = d->actionCollection->addAction(KStandardAction::Back, QStringLiteral("back"), this, SLOT(back()));
    backAction->setShortcut(Qt::Key_Backspace);

    d->actionCollection->addAction(KStandardAction::Forward, QStringLiteral("forward"), this, SLOT(forward()));

    QAction *homeAction = d->actionCollection->addAction(KStandardAction::Home, QStringLiteral("home"), this, SLOT(home()));
    homeAction->setText(i18n("Home Folder"));

    QAction *reloadAction = d->actionCollection->addAction(KStandardAction::Redisplay, QStringLiteral("reload"), this, SLOT(rereadDir()));
    reloadAction->setText(i18n("Reload"));
    reloadAction->setShortcuts(KStandardShortcut::shortcut(KStandardShortcut::Reload));

    QAction *mkdirAction = new QAction(i18n("New Folder..."), this);
    d->actionCollection->addAction(QStringLiteral("mkdir"), mkdirAction);
    mkdirAction->setIcon(QIcon::fromTheme(QStringLiteral("folder-new")));
    connect(mkdirAction, SIGNAL(triggered(bool)), this, SLOT(mkdir()));

    QAction *trash = new QAction(i18n("Move to Trash"), this);
    d->actionCollection->addAction(QStringLiteral("trash"), trash);
    trash->setIcon(QIcon::fromTheme(QStringLiteral("user-trash")));
    trash->setShortcut(Qt::Key_Delete);
    connect(trash, &QAction::triggered, this, &KDirOperator::trashSelected);

    QAction *action = new QAction(i18n("Delete"), this);
    d->actionCollection->addAction(QStringLiteral("delete"), action);
    action->setIcon(QIcon::fromTheme(QStringLiteral("edit-delete")));
    action->setShortcut(Qt::SHIFT + Qt::Key_Delete);
    connect(action, &QAction::triggered, this, &KDirOperator::deleteSelected);

    // the sort menu actions
    KActionMenu *sortMenu = new KActionMenu(i18n("Sorting"), this);
    sortMenu->setIcon(QIcon::fromTheme(QStringLiteral("view-sort")));
    sortMenu->setDelayed(false);
    d->actionCollection->addAction(QStringLiteral("sorting menu"),  sortMenu);

    KToggleAction *byNameAction = new KToggleAction(i18n("Sort by Name"), this);
    d->actionCollection->addAction(QStringLiteral("by name"), byNameAction);
    connect(byNameAction, SIGNAL(triggered(bool)), this, SLOT(_k_slotSortByName()));

    KToggleAction *bySizeAction = new KToggleAction(i18n("Sort by Size"), this);
    d->actionCollection->addAction(QStringLiteral("by size"), bySizeAction);
    connect(bySizeAction, SIGNAL(triggered(bool)), this, SLOT(_k_slotSortBySize()));

    KToggleAction *byDateAction = new KToggleAction(i18n("Sort by Date"), this);
    d->actionCollection->addAction(QStringLiteral("by date"), byDateAction);
    connect(byDateAction, SIGNAL(triggered(bool)), this, SLOT(_k_slotSortByDate()));

    KToggleAction *byTypeAction = new KToggleAction(i18n("Sort by Type"), this);
    d->actionCollection->addAction(QStringLiteral("by type"), byTypeAction);
    connect(byTypeAction, SIGNAL(triggered(bool)), this, SLOT(_k_slotSortByType()));

    QActionGroup *sortOrderGroup = new QActionGroup(this);
    sortOrderGroup->setExclusive(true);

    KToggleAction *ascendingAction = new KToggleAction(i18n("Ascending"), this);
    d->actionCollection->addAction(QStringLiteral("ascending"), ascendingAction);
    ascendingAction->setActionGroup(sortOrderGroup);
    connect(ascendingAction, &QAction::triggered, this, [this]() {
        this->d->_k_slotSortReversed(false);
    });

    KToggleAction *descendingAction = new KToggleAction(i18n("Descending"), this);
    d->actionCollection->addAction(QStringLiteral("descending"), descendingAction);
    descendingAction->setActionGroup(sortOrderGroup);
    connect(descendingAction, &QAction::triggered, this, [this]() {
        this->d->_k_slotSortReversed(true);
    });

    KToggleAction *dirsFirstAction = new KToggleAction(i18n("Folders First"), this);
    d->actionCollection->addAction(QStringLiteral("dirs first"), dirsFirstAction);
    connect(dirsFirstAction, SIGNAL(triggered(bool)), this, SLOT(_k_slotToggleDirsFirst()));

    // View modes that match those of Dolphin
    KToggleAction *iconsViewAction = new KToggleAction(i18n("Icons View"), this);
    iconsViewAction->setIcon(QIcon::fromTheme(QStringLiteral("view-list-icons")));
    d->actionCollection->addAction(QStringLiteral("icons view"), iconsViewAction);
    connect(iconsViewAction, SIGNAL(triggered(bool)), this, SLOT(_k_slotIconsView()));

    KToggleAction *compactViewAction = new KToggleAction(i18n("Compact View"), this);
    compactViewAction->setIcon(QIcon::fromTheme(QStringLiteral("view-list-details")));
    d->actionCollection->addAction(QStringLiteral("compact view"), compactViewAction);
    connect(compactViewAction, SIGNAL(triggered(bool)), this, SLOT(_k_slotCompactView()));

    KToggleAction *detailsViewAction = new KToggleAction(i18n("Details View"), this);
    detailsViewAction->setIcon(QIcon::fromTheme(QStringLiteral("view-list-tree")));
    d->actionCollection->addAction(QStringLiteral("details view"), detailsViewAction);
    connect(detailsViewAction, SIGNAL(triggered(bool)), this, SLOT(_k_slotDetailsView()));

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

    d->decorationMenu = new KActionMenu(i18n("Icon Position"), this);
    d->actionCollection->addAction(QStringLiteral("decoration menu"), d->decorationMenu);

    d->leftAction = new KToggleAction(i18n("Next to File Name"), this);
    d->actionCollection->addAction(QStringLiteral("decorationAtLeft"), d->leftAction);
    connect(d->leftAction, SIGNAL(triggered(bool)), this, SLOT(_k_slotChangeDecorationPosition()));

    KToggleAction *topAction = new KToggleAction(i18n("Above File Name"), this);
    d->actionCollection->addAction(QStringLiteral("decorationAtTop"), topAction);
    connect(topAction, SIGNAL(triggered(bool)), this, SLOT(_k_slotChangeDecorationPosition()));

    d->decorationMenu->addAction(d->leftAction);
    d->decorationMenu->addAction(topAction);

    QActionGroup *decorationGroup = new QActionGroup(this);
    decorationGroup->setExclusive(true);
    d->leftAction->setActionGroup(decorationGroup);
    topAction->setActionGroup(decorationGroup);

    KToggleAction *shortAction = new KToggleAction(i18n("Short View"), this);
    d->actionCollection->addAction(QStringLiteral("short view"),  shortAction);
    shortAction->setIcon(QIcon::fromTheme(QStringLiteral("view-list-icons")));
    connect(shortAction, SIGNAL(triggered()), SLOT(_k_slotSimpleView()));

    KToggleAction *detailedAction = new KToggleAction(i18n("Detailed View"), this);
    d->actionCollection->addAction(QStringLiteral("detailed view"), detailedAction);
    detailedAction->setIcon(QIcon::fromTheme(QStringLiteral("view-list-details")));
    connect(detailedAction, SIGNAL(triggered()), SLOT(_k_slotDetailedView()));

    KToggleAction *treeAction = new KToggleAction(i18n("Tree View"), this);
    d->actionCollection->addAction(QStringLiteral("tree view"), treeAction);
    treeAction->setIcon(QIcon::fromTheme(QStringLiteral("view-list-tree")));
    connect(treeAction, SIGNAL(triggered()), SLOT(_k_slotTreeView()));

    KToggleAction *detailedTreeAction = new KToggleAction(i18n("Detailed Tree View"), this);
    d->actionCollection->addAction(QStringLiteral("detailed tree view"), detailedTreeAction);
    detailedTreeAction->setIcon(QIcon::fromTheme(QStringLiteral("view-list-tree")));
    connect(detailedTreeAction, SIGNAL(triggered()), SLOT(_k_slotDetailedTreeView()));

    QActionGroup *viewGroup = new QActionGroup(this);
    shortAction->setActionGroup(viewGroup);
    detailedAction->setActionGroup(viewGroup);
    treeAction->setActionGroup(viewGroup);
    detailedTreeAction->setActionGroup(viewGroup);

    KToggleAction *allowExpansionAction = new KToggleAction(i18n("Allow Expansion in Details View"), this);
    d->actionCollection->addAction(QStringLiteral("allow expansion"), allowExpansionAction);
    connect(allowExpansionAction, SIGNAL(toggled(bool)), SLOT(_k_slotToggleAllowExpansion(bool)));

    KToggleAction *showHiddenAction = new KToggleAction(i18n("Show Hidden Files"), this);
    d->actionCollection->addAction(QStringLiteral("show hidden"), showHiddenAction);
    showHiddenAction->setShortcuts({Qt::ALT + Qt::Key_Period, Qt::CTRL + Qt::Key_H, Qt::Key_F8});
    connect(showHiddenAction, SIGNAL(toggled(bool)), SLOT(_k_slotToggleHidden(bool)));

    KToggleAction *previewAction = new KToggleAction(i18n("Show Preview Panel"), this);
    d->actionCollection->addAction(QStringLiteral("preview"), previewAction);
    previewAction->setShortcut(Qt::Key_F11);
    connect(previewAction, SIGNAL(toggled(bool)),
            SLOT(_k_togglePreview(bool)));

    KToggleAction *inlinePreview = new KToggleAction(QIcon::fromTheme(QStringLiteral("view-preview")),
            i18n("Show Preview"), this);
    d->actionCollection->addAction(QStringLiteral("inline preview"), inlinePreview);
    inlinePreview->setShortcut(Qt::Key_F12);
    connect(inlinePreview, SIGNAL(toggled(bool)), SLOT(_k_toggleInlinePreviews(bool)));

    QAction *fileManager = new QAction(i18n("Open Containing Folder"), this);
    d->actionCollection->addAction(QStringLiteral("file manager"), fileManager);
    fileManager->setIcon(QIcon::fromTheme(QStringLiteral("system-file-manager")));
    connect(fileManager, SIGNAL(triggered()), SLOT(_k_slotOpenFileManager()));

    action = new QAction(i18n("Properties"), this);
    d->actionCollection->addAction(QStringLiteral("properties"), action);
    action->setIcon(QIcon::fromTheme(QStringLiteral("document-properties")));
    action->setShortcut(Qt::ALT + Qt::Key_Return);
    connect(action, SIGNAL(triggered(bool)), this, SLOT(_k_slotProperties()));

    // the view menu actions
    KActionMenu *viewMenu = new KActionMenu(i18n("&View"), this);
    d->actionCollection->addAction(QStringLiteral("view menu"), viewMenu);
    viewMenu->addAction(shortAction);
    viewMenu->addAction(detailedAction);
    // Comment following lines to hide the extra two modes
    viewMenu->addAction(treeAction);
    viewMenu->addAction(detailedTreeAction);
    // TODO: QAbstractItemView does not offer an action collection. Provide
    // an interface to add a custom action collection.

    d->newFileMenu = new KNewFileMenu(d->actionCollection, QStringLiteral("new"), this);
    connect(d->newFileMenu, SIGNAL(directoryCreated(QUrl)), this, SLOT(_k_slotDirectoryCreated(QUrl)));

    d->actionCollection->addAssociatedWidget(this);
    const QList<QAction *> list = d->actionCollection->actions();
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
    KActionMenu *sortMenu = static_cast<KActionMenu *>(d->actionCollection->action(QStringLiteral("sorting menu")));
    sortMenu->menu()->clear();
    sortMenu->addAction(d->actionCollection->action(QStringLiteral("by name")));
    sortMenu->addAction(d->actionCollection->action(QStringLiteral("by size")));
    sortMenu->addAction(d->actionCollection->action(QStringLiteral("by date")));
    sortMenu->addAction(d->actionCollection->action(QStringLiteral("by type")));
    sortMenu->addSeparator();
    sortMenu->addAction(d->actionCollection->action(QStringLiteral("ascending")));
    sortMenu->addAction(d->actionCollection->action(QStringLiteral("descending")));
    sortMenu->addSeparator();
    sortMenu->addAction(d->actionCollection->action(QStringLiteral("dirs first")));

    // now plug everything into the popupmenu
    d->actionMenu->menu()->clear();
    if (whichActions & NavActions) {
        d->actionMenu->addAction(d->actionCollection->action(QStringLiteral("up")));
        d->actionMenu->addAction(d->actionCollection->action(QStringLiteral("back")));
        d->actionMenu->addAction(d->actionCollection->action(QStringLiteral("forward")));
        d->actionMenu->addAction(d->actionCollection->action(QStringLiteral("home")));
        d->actionMenu->addSeparator();
    }

    if (whichActions & FileActions) {
        d->actionMenu->addAction(d->actionCollection->action(QStringLiteral("new")));
        if (d->currUrl.isLocalFile() && !(QApplication::keyboardModifiers() & Qt::ShiftModifier)) {
            d->actionMenu->addAction(d->actionCollection->action(QStringLiteral("trash")));
        }
        KConfigGroup cg(KSharedConfig::openConfig(), QStringLiteral("KDE"));
        const bool del = !d->currUrl.isLocalFile() ||
                         (QApplication::keyboardModifiers() & Qt::ShiftModifier) ||
                         cg.readEntry("ShowDeleteCommand", false);
        if (del) {
            d->actionMenu->addAction(d->actionCollection->action(QStringLiteral("delete")));
        }
        d->actionMenu->addSeparator();
    }

    if (whichActions & SortActions) {
        d->actionMenu->addAction(sortMenu);
        if (!(whichActions & ViewActions)) {
            d->actionMenu->addSeparator();
        }
    }

    if (whichActions & ViewActions) {
        d->actionMenu->addAction(d->actionCollection->action(QStringLiteral("view menu")));
        d->actionMenu->addAction(d->actionCollection->action(QStringLiteral("reload")));
        d->actionMenu->addSeparator();
    }

    if (whichActions & FileActions) {
        d->actionMenu->addAction(d->actionCollection->action(QStringLiteral("file manager")));
        d->actionMenu->addAction(d->actionCollection->action(QStringLiteral("properties")));
    }
}

void KDirOperator::updateSortActions()
{
    QAction *ascending = d->actionCollection->action(QStringLiteral("ascending"));
    QAction *descending = d->actionCollection->action(QStringLiteral("descending"));

    if (KFile::isSortByName(d->sorting)) {
        d->actionCollection->action(QStringLiteral("by name"))->setChecked(true);
        descending->setText(i18nc("Sort descending", "Z-A"));
        ascending->setText(i18nc("Sort ascending", "A-Z"));
    } else if (KFile::isSortByDate(d->sorting)) {
        d->actionCollection->action(QStringLiteral("by date"))->setChecked(true);
        descending->setText(i18nc("Sort descending", "Newest First"));
        ascending->setText(i18nc("Sort ascending", "Oldest First"));
    } else if (KFile::isSortBySize(d->sorting)) {
        d->actionCollection->action(QStringLiteral("by size"))->setChecked(true);
        descending->setText(i18nc("Sort descending", "Largest First"));
        ascending->setText(i18nc("Sort ascending", "Smallest First"));
    } else if (KFile::isSortByType(d->sorting)) {
        d->actionCollection->action(QStringLiteral("by type"))->setChecked(true);
        descending->setText(i18nc("Sort descending", "Z-A"));
        ascending->setText(i18nc("Sort ascending", "A-Z"));
    }
    ascending->setChecked(!(d->sorting & QDir::Reversed));
    descending->setChecked(d->sorting & QDir::Reversed);
    d->actionCollection->action(QStringLiteral("dirs first"))->setChecked(d->sorting & QDir::DirsFirst);
}

void KDirOperator::updateViewActions()
{
    KFile::FileView fv = static_cast<KFile::FileView>(d->viewKind);

    //QAction *separateDirs = d->actionCollection->action("separate dirs");
    //separateDirs->setChecked(KFile::isSeparateDirs(fv) &&
    //                         separateDirs->isEnabled());

    d->actionCollection->action(QStringLiteral("short view"))->setChecked(KFile::isSimpleView(fv));
    d->actionCollection->action(QStringLiteral("detailed view"))->setChecked(KFile::isDetailView(fv));
    d->actionCollection->action(QStringLiteral("tree view"))->setChecked(KFile::isTreeView(fv));
    d->actionCollection->action(QStringLiteral("detailed tree view"))->setChecked(KFile::isDetailTreeView(fv));

    // dolphin style views
    d->actionCollection->action(QStringLiteral("icons view"))->setChecked(KFile::isSimpleView(fv) && d->decorationPosition == QStyleOptionViewItem::Top);
    d->actionCollection->action(QStringLiteral("compact view"))->setChecked(KFile::isSimpleView(fv) && d->decorationPosition == QStyleOptionViewItem::Left);
    d->actionCollection->action(QStringLiteral("details view"))->setChecked(KFile::isDetailTreeView(fv) || KFile::isDetailView(fv));
}

void KDirOperator::readConfig(const KConfigGroup &configGroup)
{
    d->defaultView = 0;
    QString viewStyle = configGroup.readEntry("View Style", "DetailTree");
    if (viewStyle == QLatin1String("Detail")) {
        d->defaultView |= KFile::Detail;
    } else if (viewStyle == QLatin1String("Tree")) {
        d->defaultView |= KFile::Tree;
    } else if (viewStyle == QLatin1String("DetailTree")) {
        d->defaultView |= KFile::DetailTree;
    } else {
        d->defaultView |= KFile::Simple;
    }
    //if (configGroup.readEntry(QLatin1String("Separate Directories"),
    //                          DefaultMixDirsAndFiles)) {
    //    d->defaultView |= KFile::SeparateDirs;
    //}
    if (configGroup.readEntry(QStringLiteral("Show Preview"), false)) {
        d->defaultView |= KFile::PreviewContents;
    }

    d->previewWidth = configGroup.readEntry(QStringLiteral("Preview Width"), 100);

    if (configGroup.readEntry(QStringLiteral("Show hidden files"),
                              DefaultShowHidden)) {
        d->actionCollection->action(QStringLiteral("show hidden"))->setChecked(true);
        d->dirLister->setShowingDotFiles(true);
    }

    if (configGroup.readEntry(QStringLiteral("Allow Expansion"),
                              DefaultShowHidden)) {
        d->actionCollection->action(QStringLiteral("allow expansion"))->setChecked(true);
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

    if (d->inlinePreviewState == Private::NotForced) {
        d->showPreviews = configGroup.readEntry(QStringLiteral("Show Inline Previews"), true);
    }
    QStyleOptionViewItem::Position pos = (QStyleOptionViewItem::Position) configGroup.readEntry(QStringLiteral("Decoration position"), (int) QStyleOptionViewItem::Top);
    setDecorationPosition(pos);
}

void KDirOperator::writeConfig(KConfigGroup &configGroup)
{
    QString sortBy = QStringLiteral("Name");
    if (KFile::isSortBySize(d->sorting)) {
        sortBy = QStringLiteral("Size");
    } else if (KFile::isSortByDate(d->sorting)) {
        sortBy = QStringLiteral("Date");
    } else if (KFile::isSortByType(d->sorting)) {
        sortBy = QStringLiteral("Type");
    }

    configGroup.writeEntry(QStringLiteral("Sort by"), sortBy);

    configGroup.writeEntry(QStringLiteral("Sort reversed"),
                           d->actionCollection->action(QStringLiteral("descending"))->isChecked());

    configGroup.writeEntry(QStringLiteral("Sort directories first"),
                           d->actionCollection->action(QStringLiteral("dirs first"))->isChecked());

    // don't save the preview when an application specific preview is in use.
    bool appSpecificPreview = false;
    if (d->preview) {
        KFileMetaPreview *tmp = dynamic_cast<KFileMetaPreview *>(d->preview);
        appSpecificPreview = (tmp == nullptr);
    }

    if (!appSpecificPreview) {
        KToggleAction *previewAction = static_cast<KToggleAction *>(d->actionCollection->action(QStringLiteral("preview")));
        if (previewAction->isEnabled()) {
            bool hasPreview = previewAction->isChecked();
            configGroup.writeEntry(QStringLiteral("Show Preview"), hasPreview);

            if (hasPreview) {
                // remember the width of the preview widget
                QList<int> sizes = d->splitter->sizes();
                Q_ASSERT(sizes.count() == 2);
                configGroup.writeEntry(QStringLiteral("Preview Width"), sizes[1]);
            }
        }
    }

    configGroup.writeEntry(QStringLiteral("Show hidden files"),
                           d->actionCollection->action(QStringLiteral("show hidden"))->isChecked());

    configGroup.writeEntry(QStringLiteral("Allow Expansion"),
                           d->actionCollection->action(QStringLiteral("allow expansion"))->isChecked());

    KFile::FileView fv = static_cast<KFile::FileView>(d->viewKind);
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

    if (d->inlinePreviewState == Private::NotForced) {
        configGroup.writeEntry(QStringLiteral("Show Inline Previews"), d->showPreviews);
        d->writeIconZoomSettingsIfNeeded();
    }

    configGroup.writeEntry(QStringLiteral("Decoration position"), (int) d->decorationPosition);
}

void KDirOperator::Private::writeIconZoomSettingsIfNeeded() {
     // must match behavior of iconSizeForViewType
     if (configGroup && itemView) {
         ZoomSettingsForView zoomSettings = zoomSettingsForViewForView();
         configGroup->writeEntry(zoomSettings.name, iconsZoom);
     }
}

void KDirOperator::resizeEvent(QResizeEvent *)
{
    // resize the splitter and assure that the width of
    // the preview widget is restored
    QList<int> sizes = d->splitter->sizes();
    const bool hasPreview = (sizes.count() == 2);

    d->splitter->resize(size());
    sizes = d->splitter->sizes();

    const bool restorePreviewWidth = hasPreview && (d->previewWidth != sizes[1]);
    if (restorePreviewWidth) {
        const int availableWidth = sizes[0] + sizes[1];
        sizes[0] = availableWidth - d->previewWidth;
        sizes[1] = d->previewWidth;
        d->splitter->setSizes(sizes);
    }
    if (hasPreview) {
        d->previewWidth = sizes[1];
    }

    if (d->progressBar->parent() == this) {
        // might be reparented into a statusbar
        d->progressBar->move(2, height() - d->progressBar->height() - 2);
    }
}

void KDirOperator::keyPressEvent(QKeyEvent *e) // TODO KF6 remove
{
    QWidget::keyPressEvent(e);
}

void KDirOperator::setOnlyDoubleClickSelectsFiles(bool enable)
{
    d->onlyDoubleClickSelectsFiles = enable;
    // TODO: port to QAbstractItemModel
    //if (d->itemView != 0) {
    //    d->itemView->setOnlyDoubleClickSelectsFiles(enable);
    //}
}

bool KDirOperator::onlyDoubleClickSelectsFiles() const
{
    return d->onlyDoubleClickSelectsFiles;
}

void KDirOperator::setFollowNewDirectories(bool enable)
{
    d->followNewDirectories = enable;
}

bool KDirOperator::followNewDirectories() const
{
    return d->followNewDirectories;
}

void KDirOperator::setFollowSelectedDirectories(bool enable)
{
    d->followSelectedDirectories = enable;
}

bool KDirOperator::followSelectedDirectories() const
{
    return d->followSelectedDirectories;
}

void KDirOperator::Private::_k_slotStarted()
{
    progressBar->setValue(0);
    // delay showing the progressbar for one second
    progressDelayTimer->setSingleShot(true);
    progressDelayTimer->start(1000);
}

void KDirOperator::Private::_k_slotShowProgress()
{
    progressBar->raise();
    progressBar->show();
}

void KDirOperator::Private::_k_slotProgress(int percent)
{
    progressBar->setValue(percent);
}

void KDirOperator::Private::_k_slotIOFinished()
{
    progressDelayTimer->stop();
    _k_slotProgress(100);
    progressBar->hide();
    emit parent->finishedLoading();
    parent->resetCursor();

    if (preview) {
        preview->clearPreview();
    }
}

void KDirOperator::Private::_k_slotCanceled()
{
    emit parent->finishedLoading();
    parent->resetCursor();
}

QProgressBar *KDirOperator::progressBar() const
{
    return d->progressBar;
}

void KDirOperator::clearHistory()
{
    qDeleteAll(d->backStack);
    d->backStack.clear();
    d->actionCollection->action(QStringLiteral("back"))->setEnabled(false);

    qDeleteAll(d->forwardStack);
    d->forwardStack.clear();
    d->actionCollection->action(QStringLiteral("forward"))->setEnabled(false);
}

void KDirOperator::setEnableDirHighlighting(bool enable)
{
    d->dirHighlighting = enable;
}

bool KDirOperator::dirHighlighting() const
{
    return d->dirHighlighting;
}

bool KDirOperator::dirOnlyMode() const
{
    return dirOnlyMode(d->mode);
}

bool KDirOperator::dirOnlyMode(uint mode)
{
    return ((mode & KFile::Directory) &&
            (mode & (KFile::File | KFile::Files)) == 0);
}

void KDirOperator::Private::_k_slotProperties()
{
    if (itemView == nullptr) {
        return;
    }

    const KFileItemList list = parent->selectedItems();
    if (!list.isEmpty()) {
        KPropertiesDialog dialog(list, parent);
        dialog.exec();
    }
}

void KDirOperator::Private::_k_slotActivated(const QModelIndex &index)
{
    const QModelIndex dirIndex = proxyModel->mapToSource(index);
    KFileItem item = dirModel->itemForIndex(dirIndex);

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
        if (followSelectedDirectories ||
            (viewKind != KFile::Tree && viewKind != KFile::DetailTree)) {
            parent->selectDir(item);
        }
    } else {
        parent->selectFile(item);
    }
}

void KDirOperator::Private::_k_slotSelectionChanged()
{
    if (itemView == nullptr) {
        return;
    }

    // In the multiselection mode each selection change is indicated by
    // emitting a null item. Also when the selection has been cleared, a
    // null item must be emitted.
    const bool multiSelectionMode = (itemView->selectionMode() == QAbstractItemView::ExtendedSelection);
    const bool hasSelection = itemView->selectionModel()->hasSelection();
    if (multiSelectionMode || !hasSelection) {
        KFileItem nullItem;
        parent->highlightFile(nullItem);
    } else {
        const KFileItem selectedItem = parent->selectedItems().constFirst();
        parent->highlightFile(selectedItem);
    }
}

void KDirOperator::Private::_k_openContextMenu(const QPoint &pos)
{
    const QModelIndex proxyIndex = itemView->indexAt(pos);
    const QModelIndex dirIndex = proxyModel->mapToSource(proxyIndex);
    KFileItem item = dirModel->itemForIndex(dirIndex);

    if (item.isNull()) {
        return;
    }

    parent->activatedMenu(item, QCursor::pos());
}

void KDirOperator::Private::_k_triggerPreview(const QModelIndex &index)
{
    if ((preview != nullptr && !preview->isHidden()) && index.isValid() && (index.column() == KDirModel::Name)) {
        const QModelIndex dirIndex = proxyModel->mapToSource(index);
        const KFileItem item = dirModel->itemForIndex(dirIndex);

        if (item.isNull()) {
            return;
        }

        if (!item.isDir()) {
            previewUrl = item.url();
            _k_showPreview();
        } else {
            preview->clearPreview();
        }
    }
}

void KDirOperator::Private::_k_showPreview()
{
    if (preview != nullptr) {
        preview->showPreview(previewUrl);
    }
}

void KDirOperator::Private::_k_slotSplitterMoved(int, int)
{
    const QList<int> sizes = splitter->sizes();
    if (sizes.count() == 2) {
        // remember the width of the preview widget (see KDirOperator::resizeEvent())
        previewWidth = sizes[1];
    }
}

void KDirOperator::Private::_k_assureVisibleSelection()
{
    if (itemView == nullptr) {
        return;
    }

    QItemSelectionModel *selModel = itemView->selectionModel();
    if (selModel->hasSelection()) {
        const QModelIndex index = selModel->currentIndex();
        itemView->scrollTo(index, QAbstractItemView::EnsureVisible);
        _k_triggerPreview(index);
    }
}

void KDirOperator::Private::_k_synchronizeSortingState(int logicalIndex, Qt::SortOrder order)
{
    QDir::SortFlags newSort = sorting & ~(QDirSortMask | QDir::Reversed);

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

    QMetaObject::invokeMethod(parent, "_k_assureVisibleSelection", Qt::QueuedConnection);
}

void KDirOperator::Private::_k_slotChangeDecorationPosition()
{
    if (!itemView) {
        return;
    }

    KDirOperatorIconView *view = qobject_cast<KDirOperatorIconView *>(itemView);

    if (!view) {
        return;
    }

    const bool leftChecked = actionCollection->action(QStringLiteral("decorationAtLeft"))->isChecked();

    if (leftChecked) {
        view->setDecorationPosition(QStyleOptionViewItem::Left);
    } else {
        view->setDecorationPosition(QStyleOptionViewItem::Top);
    }

    itemView->update();
}

void KDirOperator::Private::_k_slotExpandToUrl(const QModelIndex &index)
{
    QTreeView *treeView = qobject_cast<QTreeView *>(itemView);

    if (!treeView) {
        return;
    }

    const KFileItem item = dirModel->itemForIndex(index);

    if (item.isNull()) {
        return;
    }

    if (!item.isDir()) {
        const QModelIndex proxyIndex = proxyModel->mapFromSource(index);

        QList<QUrl>::Iterator it = itemsToBeSetAsCurrent.begin();
        while (it != itemsToBeSetAsCurrent.end()) {
            const QUrl url = *it;
            if (url.matches(item.url(), QUrl::StripTrailingSlash) || url.isParentOf(item.url())) {
                const KFileItem _item = dirLister->findByUrl(url);
                if (!_item.isNull() && _item.isDir()) {
                    const QModelIndex _index = dirModel->indexForItem(_item);
                    const QModelIndex _proxyIndex = proxyModel->mapFromSource(_index);
                    treeView->expand(_proxyIndex);

                    // if we have expanded the last parent of this item, select it
                    if (item.url().adjusted(QUrl::RemoveFilename | QUrl::StripTrailingSlash) == url.adjusted(QUrl::StripTrailingSlash)) {
                        treeView->selectionModel()->select(proxyIndex, QItemSelectionModel::Select);
                    }
                }
                it = itemsToBeSetAsCurrent.erase(it);
            } else {
                ++it;
            }
        }
    } else if (!itemsToBeSetAsCurrent.contains(item.url())) {
        itemsToBeSetAsCurrent << item.url();
    }
}

void KDirOperator::Private::_k_slotItemsChanged()
{
    completeListDirty = true;
}

int KDirOperator::Private::iconSizeForViewType(QAbstractItemView *itemView) const
{
    // must match behavior of writeIconZoomSettingsIfNeeded
    if (!itemView || !configGroup) {
        return 0;
    }

    ZoomSettingsForView ZoomSettingsForView = zoomSettingsForViewForView();
    return configGroup->readEntry(ZoomSettingsForView.name, ZoomSettingsForView.defaultValue);
}

KDirOperator::Private::ZoomSettingsForView KDirOperator::Private::zoomSettingsForViewForView() const {
     KFile::FileView fv = static_cast<KFile::FileView>(viewKind);
     if (KFile::isSimpleView(fv)) {
         if (decorationPosition == QStyleOptionViewItem::Top){
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
    delete d->configGroup;
    d->configGroup = new KConfigGroup(configGroup);
}

KConfigGroup *KDirOperator::viewConfigGroup() const
{
    return d->configGroup;
}

void KDirOperator::setShowHiddenFiles(bool s)
{
    d->actionCollection->action(QStringLiteral("show hidden"))->setChecked(s);
}

bool KDirOperator::showHiddenFiles() const
{
    return d->actionCollection->action(QStringLiteral("show hidden"))->isChecked();
}

QStyleOptionViewItem::Position KDirOperator::decorationPosition() const
{
    return d->decorationPosition;
}

void KDirOperator::setDecorationPosition(QStyleOptionViewItem::Position position)
{
    d->decorationPosition = position;
    const bool decorationAtLeft = d->decorationPosition == QStyleOptionViewItem::Left;
    d->actionCollection->action(QStringLiteral("decorationAtLeft"))->setChecked(decorationAtLeft);
    d->actionCollection->action(QStringLiteral("decorationAtTop"))->setChecked(!decorationAtLeft);
}

bool KDirOperator::Private::isReadable(const QUrl &url)
{
    if (!url.isLocalFile()) {
        return true; // what else can we say?
    }
    return QDir(url.toLocalFile()).isReadable();
}

void KDirOperator::Private::_k_slotDirectoryCreated(const QUrl &url)
{
    if (followNewDirectories) {
        parent->setUrl(url, true);
    }
}

void KDirOperator::setSupportedSchemes(const QStringList &schemes)
{
    d->supportedSchemes = schemes;
    rereadDir();
}

QStringList KDirOperator::supportedSchemes() const
{
    return d->supportedSchemes;
}

#include "moc_kdiroperator.cpp"
