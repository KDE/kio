// -*- c++ -*-
/*
    This file is part of the KDE libraries
    SPDX-FileCopyrightText: 1997, 1998 Richard Moore <rich@kde.org>
    SPDX-FileCopyrightText: 1998 Stephan Kulow <coolo@kde.org>
    SPDX-FileCopyrightText: 1998 Daniel Grana <grana@ie.iwi.unibe.ch>
    SPDX-FileCopyrightText: 1999, 2000, 2001, 2002, 2003 Carsten Pfeiffer <pfeiffer@kde.org>
    SPDX-FileCopyrightText: 2003 Clarence Dang <dang@kde.org>
    SPDX-FileCopyrightText: 2007 David Faure <faure@kde.org>
    SPDX-FileCopyrightText: 2008 Rafael Fernández López <ereslibre@kde.org>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#include "kfilewidget.h"

#include "../utils_p.h"
#include "kfilebookmarkhandler_p.h"
#include "kfileplacesmodel.h"
#include "kfileplacesview.h"
#include "kfilepreviewgenerator.h"
#include "kfilewidgetdocktitlebar_p.h"
#include "kurlcombobox.h"
#include "kurlnavigator.h"
#include "kurlnavigatorbuttonbase_p.h"

#include <config-kiofilewidgets.h>

#include <defaults-kfile.h>
#include <kdiroperator.h>
#include <kfilefiltercombo.h>
#include <kfileitemdelegate.h>
#include <kio/job.h>
#include <kio/jobuidelegate.h>
#include <kio/statjob.h>
#include <kprotocolmanager.h>
#include <krecentdirs.h>
#include <krecentdocument.h>
#include <kurlauthorized.h>
#include <kurlcompletion.h>

#include <KActionMenu>
#include <KConfigGroup>
#include <KDirLister>
#include <KFileItem>
#include <KFilePlacesModel>
#include <KIconLoader>
#include <KJobWidgets>
#include <KLocalizedString>
#include <KMessageBox>
#include <KMessageWidget>
#include <KSharedConfig>
#include <KShell>
#include <KStandardActions>
#include <KToggleAction>

#include <QAbstractProxyModel>
#include <QApplication>
#include <QCheckBox>
#include <QDebug>
#include <QDockWidget>
#include <QFormLayout>
#include <QHelpEvent>
#include <QIcon>
#include <QLabel>
#include <QLayout>
#include <QLineEdit>
#include <QLoggingCategory>
#include <QMenu>
#include <QMimeDatabase>
#include <QPushButton>
#include <QScreen>
#include <QSplitter>
#include <QStandardPaths>
#include <QTimer>
#include <QToolBar>

#include <algorithm>
#include <array>
#include <qnamespace.h>

Q_DECLARE_LOGGING_CATEGORY(KIO_KFILEWIDGETS_FW)
Q_LOGGING_CATEGORY(KIO_KFILEWIDGETS_FW, "kf.kio.kfilewidgets.kfilewidget", QtInfoMsg)

class KFileWidgetPrivate
{
public:
    explicit KFileWidgetPrivate(KFileWidget *qq)
        : q(qq)
    {
    }

    ~KFileWidgetPrivate()
    {
        delete m_bookmarkHandler; // Should be deleted before m_ops!
        // Must be deleted before m_ops, otherwise the unit test crashes due to the
        // connection to the QDockWidget::visibilityChanged signal, which may get
        // emitted after this object is destroyed
        delete m_placesDock;
        delete m_ops;
    }

    QSize screenSize() const
    {
        return q->parentWidget() ? q->parentWidget()->screen()->availableGeometry().size() //
                                 : QGuiApplication::primaryScreen()->availableGeometry().size();
    }

    void initDirOpWidgets();
    void initToolbar();
    void initZoomWidget();
    void initLocationWidget();
    void initFilterWidget();
    void initQuickFilterWidget();
    void updateLocationWhatsThis();
    void updateAutoSelectExtension();
    void initPlacesPanel();
    void setPlacesViewSplitterSizes();
    void initGUI();
    void readViewConfig();
    void writeViewConfig();
    void setNonExtSelection();
    void setLocationText(const QUrl &);
    void setLocationText(const QList<QUrl> &);
    void appendExtension(QUrl &url);
    void updateLocationEditExtension(const QString &);
    QString findMatchingFilter(const QString &filter, const QString &filename) const;
    void updateFilter();
    void updateFilterText();
    /*
     * Parses the string "line" for files. If line doesn't contain any ", the
     * whole line will be interpreted as one file. If the number of " is odd,
     * an empty list will be returned. Otherwise, all items enclosed in " "
     * will be returned as correct urls.
     */
    QList<QUrl> tokenize(const QString &line) const;
    /*
     * Reads the recent used files and inserts them into the location combobox
     */
    void readRecentFiles();
    /*
     * Saves the entries from the location combobox.
     */
    void saveRecentFiles();
    /*
     * called when an item is highlighted/selected in multiselection mode.
     * handles setting the m_locationEdit.
     */
    void multiSelectionChanged();

    /*
     * Returns the absolute version of the URL specified in m_locationEdit.
     */
    QUrl getCompleteUrl(const QString &) const;

    /*
     * Asks for overwrite confirmation using a KMessageBox and returns
     * true if the user accepts.
     *
     */
    bool toOverwrite(const QUrl &);

    // private slots
    void slotLocationChanged(const QString &);
    void urlEntered(const QUrl &);
    void enterUrl(const QUrl &);
    void enterUrl(const QString &);
    void locationAccepted(const QString &);
    void slotMimeFilterChanged();
    void slotQuickFilterChanged();
    void fileHighlighted(const KFileItem &, bool);
    void fileSelected(const KFileItem &);
    void slotLoadingFinished();
    void togglePlacesPanel(bool show, QObject *sender = nullptr);
    void toggleBookmarks(bool);
    void setQuickFilterVisible(bool);
    void slotAutoSelectExtClicked();
    void placesViewSplitterMoved(int, int);
    void activateUrlNavigator();

    enum ZoomState {
        ZoomOut,
        ZoomIn,
    };
    void changeIconsSize(ZoomState zoom);
    void slotDirOpIconSizeChanged(int size);
    void slotIconSizeSliderMoved(int);
    void slotIconSizeChanged(int);
    void slotViewDoubleClicked(const QModelIndex &);
    void slotViewKeyEnterReturnPressed();

    void addToRecentDocuments();

    QString locationEditCurrentText() const;
    void updateNameFilter(const KFileFilter &);

    /*
     * KIO::NetAccess::mostLocalUrl local replacement.
     * This method won't show any progress dialogs for stating, since
     * they are very annoying when stating.
     */
    QUrl mostLocalUrl(const QUrl &url);

    void setInlinePreviewShown(bool show);

    KFileWidget *const q;

    // the last selected url
    QUrl m_url;

    // now following all kind of widgets, that I need to rebuild
    // the geometry management
    QBoxLayout *m_boxLayout = nullptr;
    QFormLayout *m_lafBox = nullptr;

    QLabel *m_locationLabel = nullptr;
    QWidget *m_opsWidget = nullptr;
    QVBoxLayout *m_opsWidgetLayout = nullptr;

    QLabel *m_filterLabel = nullptr;
    KUrlNavigator *m_urlNavigator = nullptr;
    KMessageWidget *m_messageWidget = nullptr;
    QPushButton *m_okButton = nullptr;
    QPushButton *m_cancelButton = nullptr;
    QDockWidget *m_placesDock = nullptr;
    KFilePlacesView *m_placesView = nullptr;
    QSplitter *m_placesViewSplitter = nullptr;
    // caches the places view width. This value will be updated when the splitter
    // is moved. This allows us to properly set a value when the dialog itself
    // is resized
    int m_placesViewWidth = -1;

    QWidget *m_labeledCustomWidget = nullptr;
    QWidget *m_bottomCustomWidget = nullptr;

    // Automatically Select Extension stuff
    QCheckBox *m_autoSelectExtCheckBox = nullptr;
    QString m_extension; // current extension for this filter

    QList<QUrl> m_urlList; // the list of selected urls

    KFileWidget::OperationMode m_operationMode = KFileWidget::Opening;

    // The file class used for KRecentDirs
    QString m_fileClass;

    KFileBookmarkHandler *m_bookmarkHandler = nullptr;

    KActionMenu *m_bookmarkButton = nullptr;

    QToolBar *m_toolbar = nullptr;
    KUrlComboBox *m_locationEdit = nullptr;
    KDirOperator *m_ops = nullptr;
    KFileFilterCombo *m_filterWidget = nullptr;
    QTimer m_filterDelayTimer;

    QWidget *m_quickFilter = nullptr;
    QLineEdit *m_quickFilterEdit = nullptr;
    QToolButton *m_quickFilterLock = nullptr;
    QToolButton *m_quickFilterClose = nullptr;

    KFilePlacesModel *m_model = nullptr;

    // whether or not the _user_ has checked the above box
    bool m_autoSelectExtChecked = false;

    // indicates if the location edit should be kept or cleared when changing
    // directories
    bool m_keepLocation = false;

    // the KDirOperators view is set in KFileWidget::show(), so to avoid
    // setting it again and again, we have this nice little boolean :)
    bool m_hasView = false;

    bool m_hasDefaultFilter = false; // necessary for the m_operationMode
    bool m_inAccept = false; // true between beginning and end of accept()
    bool m_confirmOverwrite = false;
    bool m_differentHierarchyLevelItemsEntered = false;

    const std::array<short, 8> m_stdIconSizes = {
        KIconLoader::SizeSmall,
        KIconLoader::SizeSmallMedium,
        KIconLoader::SizeMedium,
        KIconLoader::SizeLarge,
        KIconLoader::SizeHuge,
        KIconLoader::SizeEnormous,
        256,
        512,
    };

    QSlider *m_iconSizeSlider = nullptr;
    QAction *m_zoomOutAction = nullptr;
    QAction *m_zoomInAction = nullptr;

    // The group which stores app-specific settings. These settings are recent
    // files and urls. Visual settings (view mode, sorting criteria...) are not
    // app-specific and are stored in kdeglobals
    KConfigGroup m_configGroup;
    KConfigGroup m_stateConfigGroup;

    KToggleAction *m_toggleBookmarksAction = nullptr;
    KToggleAction *m_togglePlacesPanelAction = nullptr;
    KToggleAction *m_toggleQuickFilterAction = nullptr;
};

Q_GLOBAL_STATIC(QUrl, lastDirectory) // to set the start path

// returns true if the string contains "<a>:/" sequence, where <a> is at least 2 alpha chars
static bool containsProtocolSection(const QString &string)
{
    int len = string.length();
    static const char prot[] = ":/";
    for (int i = 0; i < len;) {
        i = string.indexOf(QLatin1String(prot), i);
        if (i == -1) {
            return false;
        }
        int j = i - 1;
        for (; j >= 0; j--) {
            const QChar &ch(string[j]);
            if (ch.toLatin1() == 0 || !ch.isLetter()) {
                break;
            }
            if (ch.isSpace() && (i - j - 1) >= 2) {
                return true;
            }
        }
        if (j < 0 && i >= 2) {
            return true; // at least two letters before ":/"
        }
        i += 3; // skip : and / and one char
    }
    return false;
}

// this string-to-url conversion function handles relative paths, full paths and URLs
// without the http-prepending that QUrl::fromUserInput does.
static QUrl urlFromString(const QString &str)
{
    if (Utils::isAbsoluteLocalPath(str)) {
        return QUrl::fromLocalFile(str);
    }
    QUrl url(str);
    if (url.isRelative()) {
        url.clear();
        url.setPath(str);
    }
    return url;
}

KFileWidget::KFileWidget(const QUrl &_startDir, QWidget *parent)
    : QWidget(parent)
    , d(new KFileWidgetPrivate(this))
{
    QUrl startDir(_startDir);
    // qDebug() << "startDir" << startDir;
    QString filename;

    d->m_okButton = new QPushButton(this);
    KGuiItem::assign(d->m_okButton, KStandardGuiItem::ok());
    d->m_okButton->setDefault(true);
    d->m_cancelButton = new QPushButton(this);
    KGuiItem::assign(d->m_cancelButton, KStandardGuiItem::cancel());
    // The dialog shows them
    d->m_okButton->hide();
    d->m_cancelButton->hide();

    d->initDirOpWidgets();

    // Resolve this now so that a 'kfiledialog:' URL, if specified,
    // does not get inserted into the urlNavigator history.
    d->m_url = getStartUrl(startDir, d->m_fileClass, filename);
    startDir = d->m_url;

    const auto operatorActions = d->m_ops->allActions();
    for (QAction *action : operatorActions) {
        addAction(action);
    }

    QAction *goToNavigatorAction = new QAction(this);

    connect(goToNavigatorAction, &QAction::triggered, this, [this]() {
        d->activateUrlNavigator();
    });

    goToNavigatorAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_L));

    addAction(goToNavigatorAction);

    KUrlComboBox *pathCombo = d->m_urlNavigator->editor();
    KUrlCompletion *pathCompletionObj = new KUrlCompletion(KUrlCompletion::DirCompletion);
    pathCombo->setCompletionObject(pathCompletionObj);
    pathCombo->setAutoDeleteCompletionObject(true);

    connect(d->m_urlNavigator, &KUrlNavigator::urlChanged, this, [this](const QUrl &url) {
        d->enterUrl(url);
    });
    connect(d->m_urlNavigator, &KUrlNavigator::returnPressed, d->m_ops, qOverload<>(&QWidget::setFocus));

    // Location, "Name:", line-edit and label
    d->initLocationWidget();

    // "Filter:" line-edit and label
    d->initFilterWidget();

    d->initQuickFilterWidget();
    // the Automatically Select Extension checkbox
    // (the text, visibility etc. is set in updateAutoSelectExtension(), which is called by readConfig())
    d->m_autoSelectExtCheckBox = new QCheckBox(this);
    connect(d->m_autoSelectExtCheckBox, &QCheckBox::clicked, this, [this]() {
        d->slotAutoSelectExtClicked();
    });

    d->initGUI(); // activate GM

    // read our configuration
    KSharedConfig::Ptr config = KSharedConfig::openConfig();
    config->reparseConfiguration(); // grab newly added dirs by other processes (#403524)
    d->m_configGroup = KConfigGroup(config, ConfigGroup);

    d->m_stateConfigGroup = KSharedConfig::openStateConfig()->group(ConfigGroup);

    // migrate existing recent files/urls from main config to state config
    if (d->m_configGroup.hasKey(RecentURLs)) {
        d->m_stateConfigGroup.writeEntry(RecentURLs, d->m_configGroup.readEntry(RecentURLs));
        d->m_configGroup.revertToDefault(RecentURLs);
    }

    if (d->m_configGroup.hasKey(RecentFiles)) {
        d->m_stateConfigGroup.writeEntry(RecentFiles, d->m_configGroup.readEntry(RecentFiles));
        d->m_configGroup.revertToDefault(RecentFiles);
    }

    d->readViewConfig();
    d->readRecentFiles();

    d->m_ops->action(KDirOperator::ShowPreview)->setChecked(d->m_ops->isInlinePreviewShown());
    d->slotDirOpIconSizeChanged(d->m_ops->iconSize());

    // getStartUrl() above will have resolved the startDir parameter into
    // a directory and file name in the two cases: (a) where it is a
    // special "kfiledialog:" URL, or (b) where it is a plain file name
    // only without directory or protocol.  For any other startDir
    // specified, it is not possible to resolve whether there is a file name
    // present just by looking at the URL; the only way to be sure is
    // to stat it.
    bool statRes = false;
    if (filename.isEmpty()) {
        KIO::StatJob *statJob = KIO::stat(startDir, KIO::HideProgressInfo);
        KJobWidgets::setWindow(statJob, this);
        statRes = statJob->exec();
        // qDebug() << "stat of" << startDir << "-> statRes" << statRes << "isDir" << statJob->statResult().isDir();
        if (!statRes || !statJob->statResult().isDir()) {
            filename = startDir.fileName();
            startDir = startDir.adjusted(QUrl::RemoveFilename | QUrl::StripTrailingSlash);
            // qDebug() << "statJob -> startDir" << startDir << "filename" << filename;
        }
    }

    d->m_ops->setUrl(startDir, true);
    d->m_urlNavigator->setLocationUrl(startDir);
    if (d->m_placesView) {
        d->m_placesView->setUrl(startDir);
    }

    // We have a file name either explicitly specified, or have checked that
    // we could stat it and it is not a directory.  Set it.
    if (!filename.isEmpty()) {
        QLineEdit *lineEdit = d->m_locationEdit->lineEdit();
        // qDebug() << "selecting filename" << filename;
        if (statRes) {
            d->setLocationText(QUrl(filename));
        } else {
            lineEdit->setText(filename);
            // Preserve this filename when clicking on the view (cf fileHighlighted)
            lineEdit->setModified(true);
        }
        lineEdit->selectAll();
    }

    d->m_locationEdit->setFocus();

    const QAction *showHiddenAction = d->m_ops->action(KDirOperator::ShowHiddenFiles);
    Q_ASSERT(showHiddenAction);
    d->m_urlNavigator->setShowHiddenFolders(showHiddenAction->isChecked());
    connect(showHiddenAction, &QAction::toggled, this, [this](bool checked) {
        d->m_urlNavigator->setShowHiddenFolders(checked);
    });

    const QAction *hiddenFilesLastAction = d->m_ops->action(KDirOperator::SortHiddenFilesLast);
    Q_ASSERT(hiddenFilesLastAction);
    d->m_urlNavigator->setSortHiddenFoldersLast(hiddenFilesLastAction->isChecked());
    connect(hiddenFilesLastAction, &QAction::toggled, this, [this](bool checked) {
        d->m_urlNavigator->setSortHiddenFoldersLast(checked);
    });
}

KFileWidget::~KFileWidget()
{
    KSharedConfig::Ptr config = KSharedConfig::openConfig();
    config->sync();
    d->m_ops->removeEventFilter(this);
    d->m_locationEdit->removeEventFilter(this);
}

void KFileWidget::setLocationLabel(const QString &text)
{
    d->m_locationLabel->setText(text);
}

void KFileWidget::setFilters(const QList<KFileFilter> &filters, const KFileFilter &activeFilter)
{
    d->m_ops->clearFilter();
    d->m_filterWidget->setFilters(filters, activeFilter);
    d->m_ops->updateDir();
    d->m_hasDefaultFilter = false;
    d->m_filterWidget->setEditable(true);
    d->updateFilterText();

    d->updateAutoSelectExtension();
}

KFileFilter KFileWidget::currentFilter() const
{
    return d->m_filterWidget->currentFilter();
}

void KFileWidget::clearFilter()
{
    d->m_filterWidget->setFilters({}, KFileFilter());
    d->m_ops->clearFilter();
    d->m_hasDefaultFilter = false;
    d->m_filterWidget->setEditable(true);

    d->updateAutoSelectExtension();
}

void KFileWidget::setPreviewWidget(KPreviewWidgetBase *w)
{
    d->m_ops->setPreviewWidget(w);
    d->m_ops->clearHistory();
    d->m_hasView = true;
}

QUrl KFileWidgetPrivate::getCompleteUrl(const QString &_url) const
{
    //     qDebug() << "got url " << _url;

    const QString url = KShell::tildeExpand(_url);
    QUrl u;

    if (Utils::isAbsoluteLocalPath(url)) {
        u = QUrl::fromLocalFile(url);
    } else {
        QUrl relativeUrlTest(m_ops->url());
        relativeUrlTest.setPath(Utils::concatPaths(relativeUrlTest.path(), url));
        if (!m_ops->dirLister()->findByUrl(relativeUrlTest).isNull() || !KProtocolInfo::isKnownProtocol(relativeUrlTest)) {
            u = relativeUrlTest;
        } else {
            // Try to preserve URLs if they have a scheme (for example,
            // "https://example.com/foo.txt") and otherwise resolve relative
            // paths to absolute ones (e.g. "foo.txt" -> "file:///tmp/foo.txt").
            u = QUrl(url);
            if (u.isRelative()) {
                u = relativeUrlTest;
            }
        }
    }

    return u;
}

QSize KFileWidget::sizeHint() const
{
    int fontSize = fontMetrics().height();
    const QSize goodSize(48 * fontSize, 30 * fontSize);
    const QSize scrnSize = d->screenSize();
    const QSize minSize(scrnSize / 2);
    const QSize maxSize(scrnSize * qreal(0.9));
    return (goodSize.expandedTo(minSize).boundedTo(maxSize));
}

static QString relativePathOrUrl(const QUrl &baseUrl, const QUrl &url);

/*
 * Escape the given Url so that is fit for use in the selected list of file. This
 * mainly handles double quote (") characters. These are used to separate entries
 * in the list, however, if `"` appears in the filename (or path), this will be
 * escaped as `\"`. Later, the tokenizer is able to understand the difference
 * and do the right thing
 */
static QString escapeDoubleQuotes(QString &&path);

// Called by KFileDialog
void KFileWidget::slotOk()
{
    //     qDebug() << "slotOk\n";

    const QString locationEditCurrentText(KShell::tildeExpand(d->locationEditCurrentText()));

    QList<QUrl> locationEditCurrentTextList(d->tokenize(locationEditCurrentText));
    KFile::Modes mode = d->m_ops->mode();

    // Make sure that one of the modes was provided
    if (!((mode & KFile::File) || (mode & KFile::Directory) || (mode & KFile::Files))) {
        mode |= KFile::File;
        // qDebug() << "No mode() provided";
    }

    const bool directoryMode = (mode & KFile::Directory);
    const bool onlyDirectoryMode = directoryMode && !(mode & KFile::File) && !(mode & KFile::Files);

    // Clear the list as we are going to refill it
    d->m_urlList.clear();

    // In directory mode, treat an empty selection as selecting the current dir.
    // In file mode, there's nothing to do.
    if (locationEditCurrentTextList.isEmpty() && !onlyDirectoryMode) {
        return;
    }

    // if we are on file mode, and the list of provided files/folder is greater than one, inform
    // the user about it
    if (locationEditCurrentTextList.count() > 1) {
        if (mode & KFile::File) {
            KMessageBox::error(this, i18n("You can only select one file"), i18n("More than one file provided"));
            return;
        }

        /*
         * Logic of the next part of code (ends at "end multi relative urls").
         *
         * We allow for instance to be at "/" and insert '"home/foo/bar.txt" "boot/grub/menu.lst"'.
         * Why we need to support this ? Because we provide tree views, which aren't plain.
         *
         * Now, how does this logic work. It will get the first element on the list (with no filename),
         * following the previous example say "/home/foo" and set it as the top most url.
         *
         * After this, it will iterate over the rest of items and check if this URL (topmost url)
         * contains the url being iterated.
         *
         * As you might have guessed it will do "/home/foo" against "/boot/grub" (again stripping
         * filename), and a false will be returned. Then we upUrl the top most url, resulting in
         * "/home" against "/boot/grub", what will again return false, so we upUrl again. Now we
         * have "/" against "/boot/grub", what returns true for us, so we can say that the closest
         * common ancestor of both is "/".
         *
         * This example has been written for 2 urls, but this works for any number of urls.
         */
        if (!d->m_differentHierarchyLevelItemsEntered) { // avoid infinite recursion. running this
            int start = 0;
            QUrl topMostUrl;
            KIO::StatJob *statJob = nullptr;
            bool res = false;

            // we need to check for a valid first url, so in theory we only iterate one time over
            // this loop. However it can happen that the user did
            // "home/foo/nonexistantfile" "boot/grub/menu.lst", so we look for a good first
            // candidate.
            while (!res && start < locationEditCurrentTextList.count()) {
                topMostUrl = locationEditCurrentTextList.at(start);
                statJob = KIO::stat(topMostUrl, KIO::HideProgressInfo);
                KJobWidgets::setWindow(statJob, this);
                res = statJob->exec();
                start++;
            }

            Q_ASSERT(statJob);

            // if this is not a dir, strip the filename. after this we have an existent and valid
            // dir (we stated correctly the file).
            if (!statJob->statResult().isDir()) {
                topMostUrl = topMostUrl.adjusted(QUrl::RemoveFilename | QUrl::StripTrailingSlash);
            }

            // now the funny part. for the rest of filenames, go and look for the closest ancestor
            // of all them.
            for (int i = start; i < locationEditCurrentTextList.count(); ++i) {
                QUrl currUrl = locationEditCurrentTextList.at(i);
                KIO::StatJob *statJob = KIO::stat(currUrl, KIO::HideProgressInfo);
                KJobWidgets::setWindow(statJob, this);
                int res = statJob->exec();
                if (res) {
                    // again, we don't care about filenames
                    if (!statJob->statResult().isDir()) {
                        currUrl = currUrl.adjusted(QUrl::RemoveFilename | QUrl::StripTrailingSlash);
                    }

                    // iterate while this item is contained on the top most url
                    while (!topMostUrl.matches(currUrl, QUrl::StripTrailingSlash) && !topMostUrl.isParentOf(currUrl)) {
                        topMostUrl = KIO::upUrl(topMostUrl);
                    }
                }
            }

            // now recalculate all paths for them being relative in base of the top most url
            QStringList stringList;
            stringList.reserve(locationEditCurrentTextList.count());
            for (int i = 0; i < locationEditCurrentTextList.count(); ++i) {
                Q_ASSERT(topMostUrl.isParentOf(locationEditCurrentTextList[i]));
                QString relativePath = relativePathOrUrl(topMostUrl, locationEditCurrentTextList[i]);
                stringList << escapeDoubleQuotes(std::move(relativePath));
            }

            d->m_ops->setUrl(topMostUrl, true);
            const bool signalsBlocked = d->m_locationEdit->lineEdit()->blockSignals(true);
            d->m_locationEdit->lineEdit()->setText(QStringLiteral("\"%1\"").arg(stringList.join(QStringLiteral("\" \""))));
            d->m_locationEdit->lineEdit()->blockSignals(signalsBlocked);

            d->m_differentHierarchyLevelItemsEntered = true;
            slotOk();
            return;
        }
        /*
         * end multi relative urls
         */
    } else if (!locationEditCurrentTextList.isEmpty()) {
        // if we are on file or files mode, and we have an absolute url written by
        // the user:
        //  * convert it to relative and call slotOk again if the protocol supports listing.
        //  * use the full url if the protocol doesn't support listing
        // This is because when using a protocol that supports listing we want to show the directory
        // the user just opened/saved from the next time they open the dialog, it makes sense usability wise.
        // If the protocol doesn't support listing (i.e. http:// ) the user would end up with the dialog
        // showing an "empty directory" which is bad usability wise.
        if (!locationEditCurrentText.isEmpty() && !onlyDirectoryMode
            && (Utils::isAbsoluteLocalPath(locationEditCurrentText) || containsProtocolSection(locationEditCurrentText))) {
            QUrl url = urlFromString(locationEditCurrentText);
            if (KProtocolManager::supportsListing(url)) {
                QString fileName;
                if (d->m_operationMode == Opening) {
                    KIO::StatJob *statJob = KIO::stat(url, KIO::HideProgressInfo);
                    KJobWidgets::setWindow(statJob, this);
                    int res = statJob->exec();
                    if (res) {
                        if (!statJob->statResult().isDir()) {
                            fileName = url.fileName();
                            url = url.adjusted(QUrl::RemoveFilename); // keeps trailing slash
                        } else {
                            Utils::appendSlashToPath(url);
                        }
                    }
                } else {
                    const QUrl directory = url.adjusted(QUrl::RemoveFilename);
                    // Check if the folder exists
                    KIO::StatJob *statJob = KIO::stat(directory, KIO::HideProgressInfo);
                    KJobWidgets::setWindow(statJob, this);
                    int res = statJob->exec();
                    if (res) {
                        if (statJob->statResult().isDir()) {
                            url = url.adjusted(QUrl::StripTrailingSlash);
                            fileName = url.fileName();
                            url = url.adjusted(QUrl::RemoveFilename);
                        }
                    }
                }
                d->m_ops->setUrl(url, true);
                const bool signalsBlocked = d->m_locationEdit->lineEdit()->blockSignals(true);
                d->m_locationEdit->lineEdit()->setText(fileName);
                d->m_locationEdit->lineEdit()->blockSignals(signalsBlocked);
                slotOk();
                return;
            } else {
                locationEditCurrentTextList = {url};
            }
        }
    }

    // restore it
    d->m_differentHierarchyLevelItemsEntered = false;

    // locationEditCurrentTextList contains absolute paths
    // this is the general loop for the File and Files mode. Obviously we know
    // that the File mode will iterate only one time here
    QList<QUrl>::ConstIterator it = locationEditCurrentTextList.constBegin();
    while (it != locationEditCurrentTextList.constEnd()) {
        QUrl url(*it);

        if (d->m_operationMode == Saving && !directoryMode) {
            d->appendExtension(url);
        }

        d->m_url = url;
        KIO::StatJob *statJob = KIO::stat(url, KIO::HideProgressInfo);
        KJobWidgets::setWindow(statJob, this);
        int res = statJob->exec();

        if (!KUrlAuthorized::authorizeUrlAction(QStringLiteral("open"), QUrl(), url)) {
            QString msg = KIO::buildErrorString(KIO::ERR_ACCESS_DENIED, d->m_url.toDisplayString());
            KMessageBox::error(this, msg);
            return;
        }

        // if we are on local mode, make sure we haven't got a remote base url
        if ((mode & KFile::LocalOnly) && !d->mostLocalUrl(d->m_url).isLocalFile()) {
            KMessageBox::error(this, i18n("You can only select local files"), i18n("Remote files not accepted"));
            return;
        }

        const auto &supportedSchemes = d->m_model->supportedSchemes();
        if (!supportedSchemes.isEmpty() && !supportedSchemes.contains(d->m_url.scheme())) {
            KMessageBox::error(this,
                               i18np("The selected URL uses an unsupported scheme. "
                                     "Please use the following scheme: %2",
                                     "The selected URL uses an unsupported scheme. "
                                     "Please use one of the following schemes: %2",
                                     supportedSchemes.size(),
                                     supportedSchemes.join(QLatin1String(", "))),
                               i18n("Unsupported URL scheme"));
            return;
        }

        // if user has typed folder name manually, open it
        if (res && !directoryMode && statJob->statResult().isDir()) {
            d->m_ops->setUrl(url, true);
            const bool signalsBlocked = d->m_locationEdit->lineEdit()->blockSignals(true);
            d->m_locationEdit->lineEdit()->setText(QString());
            d->m_locationEdit->lineEdit()->blockSignals(signalsBlocked);
            return;
        } else if (res && onlyDirectoryMode && !statJob->statResult().isDir()) {
            // if we are given a file when on directory only mode, reject it
            return;
        } else if (!(mode & KFile::ExistingOnly) || res) {
            // if we don't care about ExistingOnly flag, add the file even if
            // it doesn't exist. If we care about it, don't add it to the list
            if (!onlyDirectoryMode || (res && statJob->statResult().isDir())) {
                d->m_urlList << url;
            }
        } else {
            KMessageBox::error(this, i18n("The file \"%1\" could not be found", url.toDisplayString(QUrl::PreferLocalFile)), i18n("Cannot open file"));
            return; // do not emit accepted() if we had ExistingOnly flag and stat failed
        }

        if ((d->m_operationMode == Saving) && d->m_confirmOverwrite && !d->toOverwrite(url)) {
            return;
        }

        ++it;
    }

    // if we have reached this point and we didn't return before, that is because
    // we want this dialog to be accepted
    Q_EMIT accepted();
}

void KFileWidget::accept()
{
    d->m_inAccept = true;

    *lastDirectory() = d->m_ops->url();
    if (!d->m_fileClass.isEmpty()) {
        KRecentDirs::add(d->m_fileClass, d->m_ops->url().toString());
    }

    // clear the topmost item, we insert it as full path later on as item 1
    d->m_locationEdit->setItemText(0, QString());

    const QList<QUrl> list = selectedUrls();
    int atmost = d->m_locationEdit->maxItems(); // don't add more items than necessary
    for (const auto &url : list) {
        if (atmost-- == 0) {
            break;
        }

        // we strip the last slash (-1) because KUrlComboBox does that as well
        // when operating in file-mode. If we wouldn't , dupe-finding wouldn't
        // work.
        const QString file = url.toDisplayString(QUrl::StripTrailingSlash | QUrl::PreferLocalFile);

        // remove dupes
        for (int i = 1; i < d->m_locationEdit->count(); ++i) {
            if (d->m_locationEdit->itemText(i) == file) {
                d->m_locationEdit->removeItem(i--);
                break;
            }
        }
        // FIXME I don't think this works correctly when the KUrlComboBox has some default urls.
        // KUrlComboBox should provide a function to add an url and rotate the existing ones, keeping
        // track of maxItems, and we shouldn't be able to insert items as we please.
        d->m_locationEdit->insertItem(1, file);
    }

    d->writeViewConfig();
    d->saveRecentFiles();

    d->addToRecentDocuments();

    if (!(mode() & KFile::Files)) { // single selection
        Q_EMIT fileSelected(d->m_url);
    }

    d->m_ops->close();
}

void KFileWidgetPrivate::fileHighlighted(const KFileItem &i, bool isKeyNavigation)
{
    if ((m_locationEdit->hasFocus() && !m_locationEdit->currentText().isEmpty())) { // don't disturb
        return;
    }

    if (!i.isNull() && i.isDir() && !(m_ops->mode() & KFile::Directory)) {
        return;
    }

    const bool modified = m_locationEdit->lineEdit()->isModified();

    if (!(m_ops->mode() & KFile::Files)) {
        if (i.isNull()) {
            if (!modified) {
                setLocationText(QUrl());
            }
            return;
        }

        m_url = i.url();

        if (!m_locationEdit->hasFocus()) { // don't disturb while editing
            setLocationText(m_url);
        }

        Q_EMIT q->fileHighlighted(m_url);
    } else {
        multiSelectionChanged();
        Q_EMIT q->selectionChanged();
    }

    m_locationEdit->lineEdit()->setModified(false);

    // When saving, and when double-click mode is being used, highlight the
    // filename after a file is single-clicked so the user has a chance to quickly
    // rename it if desired
    // Note that double-clicking will override this and overwrite regardless of
    // single/double click mouse setting (see slotViewDoubleClicked() )
    if (!isKeyNavigation && m_operationMode == KFileWidget::Saving) {
        m_locationEdit->setFocus();
    }
}

void KFileWidgetPrivate::fileSelected(const KFileItem &i)
{
    if (!i.isNull() && i.isDir()) {
        return;
    }

    if (!(m_ops->mode() & KFile::Files)) {
        if (i.isNull()) {
            setLocationText(QUrl());
            return;
        }
        setLocationText(i.targetUrl());
    } else {
        multiSelectionChanged();
        Q_EMIT q->selectionChanged();
    }

    // Same as above in fileHighlighted(), but for single-click mode
    if (m_operationMode == KFileWidget::Saving) {
        m_locationEdit->setFocus();
    } else {
        q->slotOk();
    }
}

// I know it's slow to always iterate thru the whole filelist
// (d->m_ops->selectedItems()), but what can we do?
void KFileWidgetPrivate::multiSelectionChanged()
{
    if (m_locationEdit->hasFocus() && !m_locationEdit->currentText().isEmpty()) { // don't disturb
        return;
    }

    const KFileItemList list = m_ops->selectedItems();

    if (list.isEmpty()) {
        setLocationText(QUrl());
        return;
    }

    // Allow single folder selection, so user can click "Open" to open it
    if (list.length() == 1 && list.first().isDir()) {
        setLocationText(list.first().targetUrl());
        return;
    }
    // Remove any selected folders from the locations
    QList<QUrl> urlList;
    for (const auto &item : list) {
        if (!item.isDir()) {
            urlList.append(item.targetUrl());
        }
    }
    setLocationText(urlList);
}

void KFileWidgetPrivate::setLocationText(const QUrl &url)
{
    // fileHighlighed and fileSelected will be called one after the other:
    // avoid to set two times in a row the location text with the same name
    // as this would insert spurious entries in the undo stack
    if ((url.isEmpty() && m_locationEdit->lineEdit()->text().isEmpty()) || m_locationEdit->lineEdit()->text() == escapeDoubleQuotes(url.fileName())) {
        return;
    }
    // Block m_locationEdit signals as setCurrentItem() will cause textChanged() to get
    // emitted, so slotLocationChanged() will be called. Make sure we don't clear the
    // KDirOperator's view-selection in there
    const QSignalBlocker blocker(m_locationEdit);

    if (!url.isEmpty()) {
        if (!url.isRelative()) {
            const QUrl directory = url.adjusted(QUrl::RemoveFilename);
            if (!directory.path().isEmpty()) {
                q->setUrl(directory, false);
            } else {
                q->setUrl(url, false);
            }
        }
        m_locationEdit->lineEdit()->selectAll();
        m_locationEdit->lineEdit()->insert(escapeDoubleQuotes(url.fileName()));
    } else if (!m_locationEdit->lineEdit()->text().isEmpty()) {
        m_locationEdit->clearEditText();
    }

    if (m_operationMode == KFileWidget::Saving) {
        setNonExtSelection();
    }
}

static QString relativePathOrUrl(const QUrl &baseUrl, const QUrl &url)
{
    if (baseUrl.isParentOf(url)) {
        const QString basePath(QDir::cleanPath(baseUrl.path()));
        QString relPath(QDir::cleanPath(url.path()));
        relPath.remove(0, basePath.length());
        if (relPath.startsWith(QLatin1Char('/'))) {
            relPath.remove(0, 1);
        }
        return relPath;
    } else {
        return url.toDisplayString();
    }
}

static QString escapeDoubleQuotes(QString &&path)
{
    // First escape the escape character that we are using
    path.replace(QStringLiteral("\\"), QStringLiteral("\\\\"));
    // Second, escape the quotes
    path.replace(QStringLiteral("\""), QStringLiteral("\\\""));
    return path;
}

void KFileWidgetPrivate::initDirOpWidgets()
{
    m_opsWidget = new QWidget(q);
    m_opsWidgetLayout = new QVBoxLayout(m_opsWidget);
    m_opsWidgetLayout->setContentsMargins(0, 0, 0, 0);
    m_opsWidgetLayout->setSpacing(0);

    m_model = new KFilePlacesModel(q);

    // Don't pass "startDir" (KFileWidget constructor 1st arg) to the
    // KUrlNavigator at this stage: it may also contain a file name which
    // should not get inserted in that form into the old-style navigation
    // bar history. Wait until the KIO::stat has been done later.
    //
    // The stat cannot be done before this point, bug 172678.
    m_urlNavigator = new KUrlNavigator(m_model, QUrl(), m_opsWidget); // d->m_toolbar);
    m_urlNavigator->setPlacesSelectorVisible(false);

    // Add the urlNavigator inside a widget to give it proper padding
    const auto navWidget = new QWidget(m_opsWidget);
    const auto navLayout = new QHBoxLayout(navWidget);
    navLayout->addWidget(m_urlNavigator);
    navLayout->setContentsMargins(q->style()->pixelMetric(QStyle::PM_LayoutLeftMargin),
                                  0,
                                  q->style()->pixelMetric(QStyle::PM_LayoutRightMargin),
                                  q->style()->pixelMetric(QStyle::PM_LayoutBottomMargin));

    m_messageWidget = new KMessageWidget(q);
    m_messageWidget->setMessageType(KMessageWidget::Error);
    m_messageWidget->setWordWrap(true);
    m_messageWidget->hide();

    auto topSeparator = new QFrame(q);
    topSeparator->setFrameStyle(QFrame::HLine);

    m_ops = new KDirOperator(QUrl(), m_opsWidget);
    m_ops->installEventFilter(q);
    m_ops->setObjectName(QStringLiteral("KFileWidget::ops"));
    m_ops->setIsSaving(m_operationMode == KFileWidget::Saving);
    m_ops->setNewFileMenuSelectDirWhenAlreadyExist(true);
    m_ops->showOpenWithActions(true);
    m_ops->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    auto bottomSparator = new QFrame(q);
    bottomSparator->setFrameStyle(QFrame::HLine);

    q->connect(m_ops, &KDirOperator::urlEntered, q, [this](const QUrl &url) {
        urlEntered(url);
    });
    q->connect(m_ops, &KDirOperator::fileHighlighted, q, [this](const KFileItem &item) {
        fileHighlighted(item, m_ops->usingKeyNavigation());
    });
    q->connect(m_ops, &KDirOperator::fileSelected, q, [this](const KFileItem &item) {
        fileSelected(item);
    });
    q->connect(m_ops, &KDirOperator::finishedLoading, q, [this]() {
        slotLoadingFinished();
    });
    q->connect(m_ops, &KDirOperator::keyEnterReturnPressed, q, [this]() {
        slotViewKeyEnterReturnPressed();
    });
    q->connect(m_ops, &KDirOperator::renamingFinished, q, [this](const QList<QUrl> &urls) {
        // Update file names in location text field after renaming selected files
        q->setSelectedUrls(urls);
    });

    q->connect(m_ops, &KDirOperator::viewChanged, q, [](QAbstractItemView *newView) {
        newView->setProperty("_breeze_borders_sides", QVariant::fromValue(QFlags{Qt::TopEdge | Qt::BottomEdge}));
    });

    m_ops->dirLister()->setAutoErrorHandlingEnabled(false);
    q->connect(m_ops->dirLister(), &KDirLister::jobError, q, [this](KIO::Job *job) {
        m_messageWidget->setText(job->errorString());
        m_messageWidget->animatedShow();
    });

    m_ops->setupMenu(KDirOperator::SortActions | KDirOperator::FileActions | KDirOperator::ViewActions);

    initToolbar();

    m_opsWidgetLayout->addWidget(m_toolbar);
    m_opsWidgetLayout->addWidget(navWidget);
    m_opsWidgetLayout->addWidget(m_messageWidget);
    m_opsWidgetLayout->addWidget(topSeparator);
    m_opsWidgetLayout->addWidget(m_ops);
    m_opsWidgetLayout->addWidget(bottomSparator);
}

void KFileWidgetPrivate::initZoomWidget()
{
    m_iconSizeSlider = new QSlider(q);
    m_iconSizeSlider->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Fixed);
    m_iconSizeSlider->setMinimumWidth(40);
    m_iconSizeSlider->setOrientation(Qt::Horizontal);
    m_iconSizeSlider->setMinimum(0);
    m_iconSizeSlider->setMaximum(m_stdIconSizes.size() - 1);
    m_iconSizeSlider->setSingleStep(1);
    m_iconSizeSlider->setPageStep(1);
    m_iconSizeSlider->setTickPosition(QSlider::TicksBelow);

    q->connect(m_iconSizeSlider, &QAbstractSlider::valueChanged, q, [this](int step) {
        slotIconSizeChanged(m_stdIconSizes[step]);
    });

    q->connect(m_iconSizeSlider, &QAbstractSlider::sliderMoved, q, [this](int step) {
        slotIconSizeSliderMoved(m_stdIconSizes[step]);
    });

    q->connect(m_ops, &KDirOperator::currentIconSizeChanged, q, [this](int iconSize) {
        slotDirOpIconSizeChanged(iconSize);
    });

    m_zoomOutAction = KStandardActions::create(
        KStandardActions::ZoomOut,
        q,
        [this]() {
            changeIconsSize(ZoomOut);
        },
        q);

    q->addAction(m_zoomOutAction);

    m_zoomInAction = KStandardActions::create(
        KStandardActions::ZoomIn,
        q,
        [this]() {
            changeIconsSize(ZoomIn);
        },
        q);

    q->addAction(m_zoomInAction);
}

void KFileWidgetPrivate::initToolbar()
{
    m_toolbar = new QToolBar(m_opsWidget);
    m_toolbar->setObjectName(QStringLiteral("KFileWidget::toolbar"));
    m_toolbar->setMovable(false);

    // add nav items to the toolbar
    //
    // NOTE:  The order of the button icons here differs from that
    // found in the file manager and web browser, but has been discussed
    // and agreed upon on the kde-core-devel mailing list:
    //
    // http://lists.kde.org/?l=kde-core-devel&m=116888382514090&w=2

    m_ops->action(KDirOperator::Up)
        ->setWhatsThis(i18n("<qt>Click this button to enter the parent folder.<br /><br />"
                            "For instance, if the current location is file:/home/konqi clicking this "
                            "button will take you to file:/home.</qt>"));

    m_ops->action(KDirOperator::Back)->setWhatsThis(i18n("Click this button to move backwards one step in the browsing history."));
    m_ops->action(KDirOperator::Forward)->setWhatsThis(i18n("Click this button to move forward one step in the browsing history."));

    m_ops->action(KDirOperator::Reload)->setWhatsThis(i18n("Click this button to reload the contents of the current location."));
    m_ops->action(KDirOperator::NewFolder)->setShortcuts(KStandardShortcut::createFolder());
    m_ops->action(KDirOperator::NewFolder)->setWhatsThis(i18n("Click this button to create a new folder."));

    m_togglePlacesPanelAction = new KToggleAction(i18n("Show Places Panel"), q);
    q->addAction(m_togglePlacesPanelAction);
    m_togglePlacesPanelAction->setShortcut(QKeySequence(Qt::Key_F9));
    q->connect(m_togglePlacesPanelAction, &QAction::toggled, q, [this](bool show) {
        togglePlacesPanel(show);
    });

    m_toggleBookmarksAction = new KToggleAction(i18n("Show Bookmarks Button"), q);
    q->addAction(m_toggleBookmarksAction);
    q->connect(m_toggleBookmarksAction, &QAction::toggled, q, [this](bool show) {
        toggleBookmarks(show);
    });

    m_toggleQuickFilterAction = new KToggleAction(i18n("Show Quick Filter"), q);
    q->addAction(m_toggleQuickFilterAction);
    m_toggleQuickFilterAction->setShortcuts(QList{QKeySequence(Qt::CTRL | Qt::Key_I), QKeySequence(Qt::Key_Backslash)});
    q->connect(m_toggleQuickFilterAction, &QAction::toggled, q, [this](bool show) {
        setQuickFilterVisible(show);
    });

    // Build the settings menu
    KActionMenu *menu = new KActionMenu(QIcon::fromTheme(QStringLiteral("configure")), i18n("Options"), q);
    q->addAction(menu);
    menu->setWhatsThis(
        i18n("<qt>This is the preferences menu for the file dialog. "
             "Various options can be accessed from this menu including: <ul>"
             "<li>how files are sorted in the list</li>"
             "<li>types of view, including icon and list</li>"
             "<li>showing of hidden files</li>"
             "<li>the Places panel</li>"
             "<li>file previews</li>"
             "<li>separating folders from files</li></ul></qt>"));

    menu->addAction(m_ops->action(KDirOperator::AllowExpansionInDetailsView));
    menu->addSeparator();
    menu->addAction(m_ops->action(KDirOperator::ShowHiddenFiles));
    menu->addAction(m_togglePlacesPanelAction);
    menu->addAction(m_toggleQuickFilterAction);
    menu->addAction(m_toggleBookmarksAction);
    menu->addAction(m_ops->action(KDirOperator::ShowPreviewPanel));

    menu->setPopupMode(QToolButton::InstantPopup);
    q->connect(menu->menu(), &QMenu::aboutToShow, m_ops, &KDirOperator::updateSelectionDependentActions);

    m_bookmarkButton = new KActionMenu(QIcon::fromTheme(QStringLiteral("bookmarks")), i18n("Bookmarks"), q);
    m_bookmarkButton->setPopupMode(QToolButton::InstantPopup);
    q->addAction(m_bookmarkButton);
    m_bookmarkButton->setWhatsThis(
        i18n("<qt>This button allows you to bookmark specific locations. "
             "Click on this button to open the bookmark menu where you may add, "
             "edit or select a bookmark.<br /><br />"
             "These bookmarks are specific to the file dialog, but otherwise operate "
             "like bookmarks elsewhere in KDE.</qt>"));

    QWidget *midSpacer = new QWidget(q);
    midSpacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    m_toolbar->addAction(m_ops->action(KDirOperator::Back));
    m_toolbar->addAction(m_ops->action(KDirOperator::Forward));
    m_toolbar->addAction(m_ops->action(KDirOperator::Up));
    m_toolbar->addAction(m_ops->action(KDirOperator::Reload));
    m_toolbar->addSeparator();
    m_toolbar->addAction(m_ops->action(KDirOperator::ViewIconsView));
    m_toolbar->addAction(m_ops->action(KDirOperator::ViewCompactView));
    m_toolbar->addAction(m_ops->action(KDirOperator::ViewDetailsView));
    m_toolbar->addSeparator();
    m_toolbar->addAction(m_ops->action(KDirOperator::ShowPreview));
    m_toolbar->addAction(m_ops->action(KDirOperator::SortMenu));
    m_toolbar->addAction(m_bookmarkButton);

    m_toolbar->addWidget(midSpacer);

    initZoomWidget();
    m_toolbar->addAction(m_zoomOutAction);
    m_toolbar->addWidget(m_iconSizeSlider);
    m_toolbar->addAction(m_zoomInAction);
    m_toolbar->addSeparator();

    m_toolbar->addAction(m_ops->action(KDirOperator::NewFolder));
    m_toolbar->addAction(menu);

    m_toolbar->setToolButtonStyle(Qt::ToolButtonIconOnly);
    m_toolbar->setMovable(false);
}

void KFileWidgetPrivate::initLocationWidget()
{
    m_locationLabel = new QLabel(i18n("&Name:"), q);
    m_locationEdit = new KUrlComboBox(KUrlComboBox::Files, true, q);
    m_locationEdit->installEventFilter(q);
    // Properly let the dialog be resized (to smaller). Otherwise we could have
    // huge dialogs that can't be resized to smaller (it would be as big as the longest
    // item in this combo box). (ereslibre)
    m_locationEdit->setSizeAdjustPolicy(QComboBox::AdjustToContentsOnFirstShow);
    q->connect(m_locationEdit, &KUrlComboBox::editTextChanged, q, [this](const QString &text) {
        slotLocationChanged(text);
    });

    // Only way to have the undo button before the clear button
    m_locationEdit->lineEdit()->setClearButtonEnabled(false);

    QAction *clearAction = new QAction(QIcon::fromTheme(QStringLiteral("edit-clear")), {}, m_locationEdit->lineEdit());
    m_locationEdit->lineEdit()->addAction(clearAction, QLineEdit::TrailingPosition);
    clearAction->setVisible(false);
    q->connect(clearAction, &QAction::triggered, m_locationEdit->lineEdit(), &QLineEdit::clear);
    q->connect(m_locationEdit->lineEdit(), &QLineEdit::textEdited, q, [this, clearAction]() {
        clearAction->setVisible(m_locationEdit->lineEdit()->text().length() > 0);
    });
    q->connect(m_locationEdit->lineEdit(), &QLineEdit::textChanged, q, [this](const QString &text) {
        m_okButton->setEnabled(!text.isEmpty());
    });

    QAction *undoAction = new QAction(QIcon::fromTheme(QStringLiteral("edit-undo")), i18nc("@info:tooltip", "Undo filename change"), m_locationEdit->lineEdit());
    m_locationEdit->lineEdit()->addAction(undoAction, QLineEdit::TrailingPosition);
    undoAction->setVisible(false);
    q->connect(undoAction, &QAction::triggered, m_locationEdit->lineEdit(), &QLineEdit::undo);
    q->connect(m_locationEdit->lineEdit(), &QLineEdit::textEdited, q, [this, undoAction]() {
        undoAction->setVisible(m_locationEdit->lineEdit()->isUndoAvailable());
    });

    updateLocationWhatsThis();
    m_locationLabel->setBuddy(m_locationEdit);

    KUrlCompletion *fileCompletionObj = new KUrlCompletion(KUrlCompletion::FileCompletion);
    m_locationEdit->setCompletionObject(fileCompletionObj);
    m_locationEdit->setAutoDeleteCompletionObject(true);

    q->connect(m_locationEdit, &KUrlComboBox::returnPressed, q, [this](const QString &text) {
        locationAccepted(text);
    });
}

void KFileWidgetPrivate::initFilterWidget()
{
    m_filterLabel = new QLabel(q);
    m_filterWidget = new KFileFilterCombo(q);
    m_filterWidget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    updateFilterText();
    // Properly let the dialog be resized (to smaller). Otherwise we could have
    // huge dialogs that can't be resized to smaller (it would be as big as the longest
    // item in this combo box). (ereslibre)
    m_filterWidget->setSizeAdjustPolicy(QComboBox::AdjustToContentsOnFirstShow);
    m_filterLabel->setBuddy(m_filterWidget);
    q->connect(m_filterWidget, &KFileFilterCombo::filterChanged, q, [this]() {
        slotMimeFilterChanged();
    });

    m_filterDelayTimer.setSingleShot(true);
    m_filterDelayTimer.setInterval(300);
    q->connect(m_filterWidget, &QComboBox::editTextChanged, &m_filterDelayTimer, qOverload<>(&QTimer::start));
    q->connect(&m_filterDelayTimer, &QTimer::timeout, q, [this]() {
        slotMimeFilterChanged();
    });
}

void KFileWidgetPrivate::initQuickFilterWidget()
{
    m_quickFilter = new QWidget(q);
    // Lock is used for keeping filter open when changing folders
    m_quickFilterLock = new QToolButton(m_quickFilter);
    m_quickFilterLock->setAutoRaise(true);
    m_quickFilterLock->setCheckable(true);
    m_quickFilterLock->setIcon(QIcon::fromTheme(QStringLiteral("object-unlocked")));
    m_quickFilterLock->setToolTip(i18nc("@info:tooltip", "Keep Filter When Changing Folders"));

    m_quickFilterEdit = new QLineEdit(m_quickFilter);
    m_quickFilterEdit->setClearButtonEnabled(true);
    m_quickFilterEdit->setPlaceholderText(i18n("Filter by name…"));
    QObject::connect(m_quickFilterEdit, &QLineEdit::textChanged, q, [this]() {
        slotQuickFilterChanged();
    });

    m_quickFilterClose = new QToolButton(m_quickFilter);
    m_quickFilterClose->setAutoRaise(true);
    m_quickFilterClose->setIcon(QIcon::fromTheme(QStringLiteral("dialog-close")));
    m_quickFilterClose->setToolTip(i18nc("@info:tooltip", "Hide Filter Bar"));
    QObject::connect(m_quickFilterClose, &QToolButton::clicked, q, [this]() {
        setQuickFilterVisible(false);
    });

    QHBoxLayout *hLayout = new QHBoxLayout(m_quickFilter);
    hLayout->setContentsMargins(0, 0, 0, 0);
    hLayout->addWidget(m_quickFilterLock);
    hLayout->addWidget(m_quickFilterEdit);
    hLayout->addWidget(m_quickFilterClose);

    m_quickFilter->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    m_quickFilter->hide();
}

void KFileWidgetPrivate::setLocationText(const QList<QUrl> &urlList)
{
    // Block m_locationEdit signals as setCurrentItem() will cause textChanged() to get
    // emitted, so slotLocationChanged() will be called. Make sure we don't clear the
    // KDirOperator's view-selection in there
    const QSignalBlocker blocker(m_locationEdit);

    const QUrl baseUrl = m_ops->url();

    if (urlList.count() > 1) {
        QString urls;
        for (const QUrl &url : urlList) {
            urls += QStringLiteral("\"%1\" ").arg(escapeDoubleQuotes(relativePathOrUrl(baseUrl, url)));
        }
        urls.chop(1);
        // Never use setEditText, because it forgets the undo history
        m_locationEdit->lineEdit()->selectAll();
        m_locationEdit->lineEdit()->insert(urls);
    } else if (urlList.count() == 1) {
        const auto url = urlList[0];
        m_locationEdit->lineEdit()->selectAll();
        m_locationEdit->lineEdit()->insert(escapeDoubleQuotes(relativePathOrUrl(baseUrl, url)));
    } else if (!m_locationEdit->lineEdit()->text().isEmpty()) {
        m_locationEdit->clearEditText();
    }

    if (m_operationMode == KFileWidget::Saving) {
        setNonExtSelection();
    }
}

void KFileWidgetPrivate::updateLocationWhatsThis()
{
    const QString autocompletionWhatsThisText = i18n(
        "<qt>While typing in the text area, you may be presented "
        "with possible matches. "
        "This feature can be controlled by clicking with the right mouse button "
        "and selecting a preferred mode from the <b>Text Completion</b> menu.</qt>");

    QString whatsThisText;
    if (m_operationMode == KFileWidget::Saving) {
        whatsThisText = QLatin1String("<qt>") + i18n("This is the name to save the file as.") + autocompletionWhatsThisText;
    } else if (m_ops->mode() & KFile::Files) {
        whatsThisText = QLatin1String("<qt>")
            + i18n("This is the list of files to open. More than "
                   "one file can be specified by listing several "
                   "files, separated by spaces.")
            + autocompletionWhatsThisText;
    } else {
        whatsThisText = QLatin1String("<qt>") + i18n("This is the name of the file to open.") + autocompletionWhatsThisText;
    }

    m_locationLabel->setWhatsThis(whatsThisText);
    m_locationEdit->setWhatsThis(whatsThisText);
}

void KFileWidgetPrivate::initPlacesPanel()
{
    if (m_placesDock) {
        return;
    }

    m_placesDock = new QDockWidget(i18nc("@title:window", "Places"), q);
    m_placesDock->setFeatures(QDockWidget::NoDockWidgetFeatures);
    m_placesDock->setTitleBarWidget(new KDEPrivate::KFileWidgetDockTitleBar(m_placesDock));

    m_placesView = new KFilePlacesView(m_placesDock);
    m_placesView->setModel(m_model);
    m_placesView->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    m_placesView->setObjectName(QStringLiteral("url bar"));
    QObject::connect(m_placesView, &KFilePlacesView::urlChanged, q, [this](const QUrl &url) {
        enterUrl(url);
    });

    QObject::connect(qobject_cast<KFilePlacesModel *>(m_placesView->model()), &KFilePlacesModel::errorMessage, q, [this](const QString &errorMessage) {
        m_messageWidget->setText(errorMessage);
        m_messageWidget->animatedShow();
    });

    // need to set the current url of the urlbar manually (not via urlEntered()
    // here, because the initial url of KDirOperator might be the same as the
    // one that will be set later (and then urlEntered() won't be emitted).
    // TODO: KDE5 ### REMOVE THIS when KDirOperator's initial URL (in the c'tor) is gone.
    m_placesView->setUrl(m_url);

    m_placesDock->setWidget(m_placesView);
    m_placesViewSplitter->insertWidget(0, m_placesDock);

    // initialize the size of the splitter
    m_placesViewWidth = m_configGroup.readEntry(SpeedbarWidth, m_placesView->sizeHint().width());

    // Needed for when the dialog is shown with the places panel initially hidden
    setPlacesViewSplitterSizes();

    QObject::connect(m_placesDock, &QDockWidget::visibilityChanged, q, [this](bool visible) {
        togglePlacesPanel(visible, m_placesDock);
    });
}

void KFileWidgetPrivate::setPlacesViewSplitterSizes()
{
    if (m_placesViewWidth > 0) {
        QList<int> sizes = m_placesViewSplitter->sizes();
        sizes[0] = m_placesViewWidth;
        sizes[1] = q->width() - m_placesViewWidth - m_placesViewSplitter->handleWidth();
        m_placesViewSplitter->setSizes(sizes);
    }
}

void KFileWidgetPrivate::initGUI()
{
    delete m_boxLayout; // deletes all sub layouts

    m_boxLayout = new QVBoxLayout(q);
    m_boxLayout->setContentsMargins(0, 0, 0, 0); // no additional margin to the already existing

    m_placesViewSplitter = new QSplitter(q);
    m_placesViewSplitter->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    m_placesViewSplitter->setChildrenCollapsible(false);
    m_boxLayout->addWidget(m_placesViewSplitter);

    QObject::connect(m_placesViewSplitter, &QSplitter::splitterMoved, q, [this](int pos, int index) {
        placesViewSplitterMoved(pos, index);
    });
    m_placesViewSplitter->insertWidget(0, m_opsWidget);

    m_lafBox = new QFormLayout();
    m_lafBox->setSpacing(q->style()->pixelMetric(QStyle::PM_LayoutVerticalSpacing));
    m_lafBox->setContentsMargins(q->style()->pixelMetric(QStyle::PM_LayoutLeftMargin),
                                 q->style()->pixelMetric(QStyle::PM_LayoutTopMargin),
                                 q->style()->pixelMetric(QStyle::PM_LayoutRightMargin),
                                 0);

    m_lafBox->addRow(m_quickFilter);
    m_lafBox->addRow(m_locationLabel, m_locationEdit);
    m_lafBox->addRow(m_filterLabel, m_filterWidget);
    // Add the "Automatically Select Extension" checkbox
    m_lafBox->addWidget(m_autoSelectExtCheckBox);

    m_opsWidgetLayout->addLayout(m_lafBox);

    auto hbox = new QHBoxLayout();
    hbox->setSpacing(q->style()->pixelMetric(QStyle::PM_LayoutHorizontalSpacing));
    hbox->setContentsMargins(q->style()->pixelMetric(QStyle::PM_LayoutTopMargin),
                             q->style()->pixelMetric(QStyle::PM_LayoutLeftMargin),
                             q->style()->pixelMetric(QStyle::PM_LayoutRightMargin),
                             q->style()->pixelMetric(QStyle::PM_LayoutBottomMargin));

    hbox->addStretch(2);
    hbox->addWidget(m_okButton);
    hbox->addWidget(m_cancelButton);

    m_opsWidgetLayout->addLayout(hbox);

    auto updateTabOrder = [this]() {
        // First the url navigator and its internal tab order
        q->setTabOrder(m_urlNavigator, m_ops);
        // Add the other elements in the ui that aren't int he toolbar
        q->setTabOrder(m_ops, m_autoSelectExtCheckBox);
        q->setTabOrder(m_autoSelectExtCheckBox, m_quickFilterLock);
        q->setTabOrder(m_quickFilterLock, m_quickFilterEdit);
        q->setTabOrder(m_quickFilterEdit, m_quickFilterClose);
        q->setTabOrder(m_quickFilterClose, m_locationEdit);
        q->setTabOrder(m_locationEdit, m_filterWidget);
        q->setTabOrder(m_filterWidget, m_okButton);
        q->setTabOrder(m_okButton, m_cancelButton);
        q->setTabOrder(m_cancelButton, m_placesView);

        // Now add every widget in the toolbar
        const auto toolbarChildren = m_toolbar->children();
        QList<QWidget *> toolbarButtons;
        for (QObject *obj : std::as_const(toolbarChildren)) {
            if (auto *button = qobject_cast<QToolButton *>(obj)) {
                // Make toolbar buttons focusable only via tab
                button->setFocusPolicy(Qt::TabFocus);
                toolbarButtons << button;
            } else if (auto *slider = qobject_cast<QSlider *>(obj)) {
                toolbarButtons << slider;
            }
        }

        q->setTabOrder(m_placesView, toolbarButtons.first());

        auto it = toolbarButtons.constBegin();
        auto nextIt = ++toolbarButtons.constBegin();
        while (nextIt != toolbarButtons.constEnd()) {
            q->setTabOrder(*it, *nextIt);
            it++;
            nextIt++;
        }
        // Do not manually close the loop: it would break the chain
    };
    q->connect(m_urlNavigator, &KUrlNavigator::layoutChanged, q, updateTabOrder);
    updateTabOrder();
}

void KFileWidgetPrivate::slotMimeFilterChanged()
{
    m_filterDelayTimer.stop();

    KFileFilter filter = m_filterWidget->currentFilter();

    m_ops->clearFilter();

    if (!filter.mimePatterns().isEmpty()) {
        QStringList types = filter.mimePatterns();
        types.prepend(QStringLiteral("inode/directory"));
        m_ops->setMimeFilter(types);
    }

    updateNameFilter(filter);

    updateAutoSelectExtension();

    m_ops->updateDir();

    Q_EMIT q->filterChanged(filter);
}

void KFileWidgetPrivate::slotQuickFilterChanged()
{
    m_filterDelayTimer.stop();

    KFileFilter filter(QStringLiteral("quickFilter"), QStringList{m_quickFilterEdit->text()}, m_filterWidget->currentFilter().mimePatterns());
    m_ops->clearFilter();
    m_ops->setMimeFilter(filter.mimePatterns());

    updateNameFilter(filter);

    m_ops->updateDir();

    Q_EMIT q->filterChanged(filter);
}

void KFileWidgetPrivate::updateNameFilter(const KFileFilter &filter)
{
    const auto filePatterns = filter.filePatterns();
    const bool hasRegExSyntax = std::any_of(filePatterns.constBegin(), filePatterns.constEnd(), [](const QString &filter) {
        // Keep the filter.contains checks in sync with Dolphin: dolphin/src/kitemviews/private/kfileitemmodelfilter.cpp setPattern
        return filter.contains(QLatin1Char('*')) || filter.contains(QLatin1Char('?')) || filter.contains(QLatin1Char('['));
    });

    if (hasRegExSyntax) {
        m_ops->setNameFilter(filter.filePatterns().join(QLatin1Char(' ')));
    } else {
        m_ops->setNameFilter(QLatin1Char('*') + filePatterns.join(QLatin1Char('*')) + QLatin1Char('*'));
    }
}

void KFileWidget::setUrl(const QUrl &url, bool clearforward)
{
    if (url.isLocalFile() && QDir::isRelativePath(url.path())) {
        d->m_ops->setUrl(QUrl::fromLocalFile(QDir::currentPath() + u'/' + url.path()), clearforward);
    } else {
        d->m_ops->setUrl(url, clearforward);
    }
}

// Protected
void KFileWidgetPrivate::urlEntered(const QUrl &url)
{
    //     qDebug();

    KUrlComboBox *pathCombo = m_urlNavigator->editor();
    if (pathCombo->count() != 0) { // little hack
        pathCombo->setUrl(url);
    }

    bool blocked = m_locationEdit->blockSignals(true);
    if (m_keepLocation) {
        const QUrl currentUrl = urlFromString(locationEditCurrentText());
        // iconNameForUrl will get the icon or fallback to a generic one
        m_locationEdit->setItemIcon(0, QIcon::fromTheme(KIO::iconNameForUrl(currentUrl)));
        // Preserve the text when clicking on the view (cf fileHighlighted)
        m_locationEdit->lineEdit()->setModified(true);
    }

    m_locationEdit->blockSignals(blocked);

    m_urlNavigator->setLocationUrl(url);

    // is triggered in ctor before completion object is set
    KUrlCompletion *completion = dynamic_cast<KUrlCompletion *>(m_locationEdit->completionObject());
    if (completion) {
        completion->setDir(url);
    }

    if (m_placesView) {
        m_placesView->setUrl(url);
    }

    m_messageWidget->hide();
}

void KFileWidgetPrivate::locationAccepted(const QString &url)
{
    Q_UNUSED(url);
    //     qDebug();
    q->slotOk();
}

void KFileWidgetPrivate::enterUrl(const QUrl &url)
{
    //     qDebug();

    // append '/' if needed: url combo does not add it
    // tokenize() expects it because it uses QUrl::adjusted(QUrl::RemoveFilename)
    QUrl u(url);
    Utils::appendSlashToPath(u);
    q->setUrl(u);

    // We need to check window()->focusWidget() instead of m_locationEdit->hasFocus
    // because when the window is showing up m_locationEdit
    // may still not have focus but it'll be the one that will have focus when the window
    // gets it and we don't want to steal its focus either
    if (q->window()->focusWidget() != m_locationEdit) {
        m_ops->setFocus();
    }

    // Clear the quick filter if its not locked
    if (!m_quickFilterLock->isChecked()) {
        setQuickFilterVisible(false);
    }
}

void KFileWidgetPrivate::enterUrl(const QString &url)
{
    //     qDebug();

    enterUrl(urlFromString(KUrlCompletion::replacedPath(url, true, true)));
}

bool KFileWidgetPrivate::toOverwrite(const QUrl &url)
{
    //     qDebug();

    KIO::StatJob *statJob = KIO::stat(url, KIO::HideProgressInfo);
    KJobWidgets::setWindow(statJob, q);
    bool res = statJob->exec();

    if (res) {
        int ret = KMessageBox::warningContinueCancel(q,
                                                     i18n("The file \"%1\" already exists. Do you wish to overwrite it?", url.fileName()),
                                                     i18n("Overwrite File?"),
                                                     KStandardGuiItem::overwrite(),
                                                     KStandardGuiItem::cancel(),
                                                     QString(),
                                                     KMessageBox::Notify | KMessageBox::Dangerous);

        if (ret != KMessageBox::Continue) {
            m_locationEdit->setFocus();
            setNonExtSelection();

            return false;
        }
        return true;
    }

    return true;
}

void KFileWidget::setSelectedUrl(const QUrl &url)
{
    // Honor protocols that do not support directory listing
    if (!url.isRelative() && !KProtocolManager::supportsListing(url)) {
        return;
    }
    d->setLocationText(url);
}

void KFileWidget::setSelectedUrls(const QList<QUrl> &urls)
{
    if (urls.isEmpty()) {
        return;
    }

    // Honor protocols that do not support directory listing
    if (!urls[0].isRelative() && !KProtocolManager::supportsListing(urls[0])) {
        return;
    }
    d->setLocationText(urls);
}

void KFileWidgetPrivate::slotLoadingFinished()
{
    const QString currentText = m_locationEdit->currentText();
    if (currentText.isEmpty()) {
        return;
    }

    m_ops->blockSignals(true);
    QUrl u(m_ops->url());
    if (currentText.startsWith(QLatin1Char('/'))) {
        u.setPath(currentText);
    } else {
        u.setPath(Utils::concatPaths(m_ops->url().path(), currentText));
    }
    m_ops->setCurrentItem(u);
    m_ops->blockSignals(false);
}

void KFileWidgetPrivate::slotLocationChanged(const QString &text)
{
    //     qDebug();

    m_locationEdit->lineEdit()->setModified(true);

    if (text.isEmpty() && m_ops->view()) {
        m_ops->view()->clearSelection();
    }

    if (!m_locationEdit->lineEdit()->text().isEmpty()) {
        const QList<QUrl> urlList(tokenize(text));
        m_ops->setCurrentItems(urlList);
    }

    updateFilter();
}

QUrl KFileWidget::selectedUrl() const
{
    //     qDebug();

    if (d->m_inAccept) {
        return d->m_url;
    } else {
        return QUrl();
    }
}

QList<QUrl> KFileWidget::selectedUrls() const
{
    //     qDebug();

    QList<QUrl> list;
    if (d->m_inAccept) {
        if (d->m_ops->mode() & KFile::Files) {
            list = d->m_urlList;
        } else {
            list.append(d->m_url);
        }
    }
    return list;
}

QList<QUrl> KFileWidgetPrivate::tokenize(const QString &line) const
{
    qCDebug(KIO_KFILEWIDGETS_FW) << "Tokenizing:" << line;

    QList<QUrl> urls;
    QUrl baseUrl(m_ops->url().adjusted(QUrl::RemoveFilename));
    Utils::appendSlashToPath(baseUrl);

    // A helper that creates, validates and appends a new url based
    // on the given filename.
    auto addUrl = [baseUrl, &urls](const QString &partial_name) {
        if (partial_name.trimmed().isEmpty()) {
            return;
        }

        // url could be absolute
        QUrl partial_url(partial_name);
        if (!partial_url.isValid()
            || partial_url.isRelative()
            // the text might look like a url scheme but not be a real one
            || (!partial_url.scheme().isEmpty() && (!partial_name.contains(QStringLiteral("://")) || !KProtocolInfo::isKnownProtocol(partial_url.scheme())))) {
            // We have to use setPath here, so that something like "test#file"
            // isn't interpreted to have path "test" and fragment "file".
            partial_url.clear();
            partial_url.setPath(partial_name);
        }

        // This returns QUrl(partial_name) for absolute URLs.
        // Otherwise, returns the concatenated url.
        if (partial_url.isRelative() || baseUrl.isParentOf(partial_url)) {
            partial_url = baseUrl.resolved(partial_url);
        }

        if (partial_url.isValid()) {
            urls.append(partial_url);
        } else {
            // This can happen in the first quote! (ex: ' "something here"')
            qCDebug(KIO_KFILEWIDGETS_FW) << "Discarding Invalid" << partial_url;
        }
    };

    // An iterative approach here where we toggle the "escape" flag
    // if we hit `\`. If we hit `"` and the escape flag is false,
    // we split
    QString partial_name;
    bool escape = false;
    for (int i = 0; i < line.length(); i++) {
        const QChar ch = line[i];

        // Handle any character previously escaped
        if (escape) {
            partial_name += ch;
            escape = false;
            continue;
        }

        // Handle escape start
        if (ch.toLatin1() == '\\') {
            escape = true;
            continue;
        }

        // Handle UNESCAPED quote (") since the above ifs are
        // dealing with the escaped ones
        // Ignore this in single-file mode
        if (ch.toLatin1() == '"' && q->mode() != KFile::Mode::File) {
            addUrl(partial_name);
            partial_name.clear();
            continue;
        }

        // Any other character just append
        partial_name += ch;
    }

    // Handle the last item which is buffered in partial_name. This is
    // required for single-file selection dialogs since the name will not
    // be wrapped in quotes
    if (!partial_name.isEmpty()) {
        addUrl(partial_name);
        partial_name.clear();
    }

    return urls;
}

QString KFileWidget::selectedFile() const
{
    //     qDebug();

    if (d->m_inAccept) {
        const QUrl url = d->mostLocalUrl(d->m_url);
        if (url.isLocalFile()) {
            return url.toLocalFile();
        } else {
            KMessageBox::error(const_cast<KFileWidget *>(this), i18n("You can only select local files."), i18n("Remote Files Not Accepted"));
        }
    }
    return QString();
}

QStringList KFileWidget::selectedFiles() const
{
    //     qDebug();

    QStringList list;

    if (d->m_inAccept) {
        if (d->m_ops->mode() & KFile::Files) {
            const QList<QUrl> urls = d->m_urlList;
            for (const auto &u : urls) {
                const QUrl url = d->mostLocalUrl(u);
                if (url.isLocalFile()) {
                    list.append(url.toLocalFile());
                }
            }
        }

        else { // single-selection mode
            if (d->m_url.isLocalFile()) {
                list.append(d->m_url.toLocalFile());
            }
        }
    }

    return list;
}

QUrl KFileWidget::baseUrl() const
{
    return d->m_ops->url();
}

void KFileWidget::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);

    if (d->m_placesDock) {
        // we don't want our places dock actually changing size when we resize
        // and qt doesn't make it easy to enforce such a thing with QSplitter
        d->setPlacesViewSplitterSizes();
    }
}

void KFileWidget::showEvent(QShowEvent *event)
{
    if (!d->m_hasView) { // delayed view-creation
        Q_ASSERT(d);
        Q_ASSERT(d->m_ops);
        d->m_ops->setViewMode(KFile::Default);
        d->m_hasView = true;

        connect(d->m_ops->view(), &QAbstractItemView::doubleClicked, this, [this](const QModelIndex &index) {
            d->slotViewDoubleClicked(index);
        });
    }
    d->m_ops->clearHistory();

    QWidget::showEvent(event);
}

bool KFileWidget::eventFilter(QObject *watched, QEvent *event)
{
    const bool res = QWidget::eventFilter(watched, event);

    QKeyEvent *keyEvent = dynamic_cast<QKeyEvent *>(event);
    if (!keyEvent) {
        return res;
    }

    const auto type = event->type();
    const auto key = keyEvent->key();

    if (watched == d->m_ops && type == QEvent::KeyPress && (key == Qt::Key_Return || key == Qt::Key_Enter)) {
        // ignore return events from the KDirOperator
        // they are not needed, activated is used to handle this case
        event->accept();
        return true;
    }

    return res;
}

void KFileWidget::setMode(KFile::Modes m)
{
    //     qDebug();

    d->m_ops->setMode(m);
    if (d->m_ops->dirOnlyMode()) {
        d->m_filterWidget->setDefaultFilter(KFileFilter(i18n("All Folders"), {QStringLiteral("*")}, {}));
    } else {
        d->m_filterWidget->setDefaultFilter(KFileFilter(i18n("All Files"), {QStringLiteral("*")}, {}));
    }

    d->updateAutoSelectExtension();
}

KFile::Modes KFileWidget::mode() const
{
    return d->m_ops->mode();
}

void KFileWidgetPrivate::readViewConfig()
{
    m_ops->setViewConfig(m_configGroup);
    m_ops->readConfig(m_configGroup);
    KUrlComboBox *combo = m_urlNavigator->editor();

    KCompletion::CompletionMode cm =
        (KCompletion::CompletionMode)m_configGroup.readEntry(PathComboCompletionMode, static_cast<int>(KCompletion::CompletionPopup));
    if (cm != KCompletion::CompletionPopup) {
        combo->setCompletionMode(cm);
    }

    cm = (KCompletion::CompletionMode)m_configGroup.readEntry(LocationComboCompletionMode, static_cast<int>(KCompletion::CompletionPopup));
    if (cm != KCompletion::CompletionPopup) {
        m_locationEdit->setCompletionMode(cm);
    }

    // Show or don't show the places panel
    togglePlacesPanel(m_configGroup.readEntry(ShowSpeedbar, true));

    // show or don't show the bookmarks
    toggleBookmarks(m_configGroup.readEntry(ShowBookmarks, false));

    // does the user want Automatically Select Extension?
    m_autoSelectExtChecked = m_configGroup.readEntry(AutoSelectExtChecked, DefaultAutoSelectExtChecked);
    updateAutoSelectExtension();

    // should the URL navigator use the breadcrumb navigation?
    m_urlNavigator->setUrlEditable(!m_configGroup.readEntry(BreadcrumbNavigation, true));

    // should the URL navigator show the full path?
    m_urlNavigator->setShowFullPath(m_configGroup.readEntry(ShowFullPath, false));

    int w1 = q->minimumSize().width();
    int w2 = m_toolbar->sizeHint().width();
    if (w1 < w2) {
        q->setMinimumWidth(w2);
    }
}

void KFileWidgetPrivate::writeViewConfig()
{
    // these settings are global settings; ALL instances of the file dialog
    // should reflect them.
    // There is no way to tell KFileOperator::writeConfig() to write to
    // kdeglobals so we write settings to a temporary config group then copy
    // them all to kdeglobals
    KConfig tmp(QString(), KConfig::SimpleConfig);
    KConfigGroup tmpGroup(&tmp, ConfigGroup);

    KUrlComboBox *pathCombo = m_urlNavigator->editor();
    // saveDialogSize( tmpGroup, KConfigGroup::Persistent | KConfigGroup::Global );
    tmpGroup.writeEntry(PathComboCompletionMode, static_cast<int>(pathCombo->completionMode()));
    tmpGroup.writeEntry(LocationComboCompletionMode, static_cast<int>(m_locationEdit->completionMode()));

    const bool showPlacesPanel = m_placesDock && !m_placesDock->isHidden();
    tmpGroup.writeEntry(ShowSpeedbar, showPlacesPanel);
    if (m_placesViewWidth > 0) {
        tmpGroup.writeEntry(SpeedbarWidth, m_placesViewWidth);
    }

    tmpGroup.writeEntry(ShowBookmarks, m_bookmarkHandler != nullptr);
    tmpGroup.writeEntry(AutoSelectExtChecked, m_autoSelectExtChecked);
    tmpGroup.writeEntry(BreadcrumbNavigation, !m_urlNavigator->isUrlEditable());
    tmpGroup.writeEntry(ShowFullPath, m_urlNavigator->showFullPath());

    m_ops->writeConfig(tmpGroup);

    // Copy saved settings to kdeglobals
    tmpGroup.copyTo(&m_configGroup, KConfigGroup::Persistent | KConfigGroup::Global);
}

void KFileWidgetPrivate::readRecentFiles()
{
    //     qDebug();

    const bool oldState = m_locationEdit->blockSignals(true);
    m_locationEdit->setMaxItems(m_configGroup.readEntry(RecentFilesNumber, DefaultRecentURLsNumber));
    m_locationEdit->setUrls(m_stateConfigGroup.readPathEntry(RecentFiles, QStringList()), KUrlComboBox::RemoveBottom);
    m_locationEdit->setCurrentIndex(-1);
    m_locationEdit->blockSignals(oldState);

    KUrlComboBox *combo = m_urlNavigator->editor();
    combo->setUrls(m_stateConfigGroup.readPathEntry(RecentURLs, QStringList()), KUrlComboBox::RemoveTop);
    combo->setMaxItems(m_configGroup.readEntry(RecentURLsNumber, DefaultRecentURLsNumber));
    combo->setUrl(m_ops->url());
    // since we delayed this moment, initialize the directory of the completion object to
    // our current directory (that was very probably set on the constructor)
    KUrlCompletion *completion = dynamic_cast<KUrlCompletion *>(m_locationEdit->completionObject());
    if (completion) {
        completion->setDir(m_ops->url());
    }
}

void KFileWidgetPrivate::saveRecentFiles()
{
    //     qDebug();
    m_stateConfigGroup.writePathEntry(RecentFiles, m_locationEdit->urls());

    KUrlComboBox *pathCombo = m_urlNavigator->editor();
    m_stateConfigGroup.writePathEntry(RecentURLs, pathCombo->urls());
}

QPushButton *KFileWidget::okButton() const
{
    return d->m_okButton;
}

QPushButton *KFileWidget::cancelButton() const
{
    return d->m_cancelButton;
}

// Called by KFileDialog
void KFileWidget::slotCancel()
{
    d->writeViewConfig();
    d->m_ops->close();
}

void KFileWidget::setKeepLocation(bool keep)
{
    d->m_keepLocation = keep;
}

bool KFileWidget::keepsLocation() const
{
    return d->m_keepLocation;
}

void KFileWidget::setOperationMode(OperationMode mode)
{
    //     qDebug();

    d->m_operationMode = mode;
    d->m_keepLocation = (mode == Saving);
    d->m_filterWidget->setEditable(!d->m_hasDefaultFilter || mode != Saving);
    if (mode == Opening) {
        // don't use KStandardGuiItem::open() here which has trailing ellipsis!
        d->m_okButton->setText(i18n("&Open"));
        d->m_okButton->setIcon(QIcon::fromTheme(QStringLiteral("document-open")));
        // hide the new folder actions...usability team says they shouldn't be in open file dialog
        d->m_ops->action(KDirOperator::NewFolder)->setEnabled(false);
        d->m_toolbar->removeAction(d->m_ops->action(KDirOperator::NewFolder));
    } else if (mode == Saving) {
        KGuiItem::assign(d->m_okButton, KStandardGuiItem::save());
        d->setNonExtSelection();
    } else {
        KGuiItem::assign(d->m_okButton, KStandardGuiItem::ok());
    }
    d->updateLocationWhatsThis();
    d->updateAutoSelectExtension();

    if (d->m_ops) {
        d->m_ops->setIsSaving(mode == Saving);
    }
    d->updateFilterText();
}

KFileWidget::OperationMode KFileWidget::operationMode() const
{
    return d->m_operationMode;
}

void KFileWidgetPrivate::slotAutoSelectExtClicked()
{
    //     qDebug() << "slotAutoSelectExtClicked(): "
    //                          << m_autoSelectExtCheckBox->isChecked() << endl;

    // whether the _user_ wants it on/off
    m_autoSelectExtChecked = m_autoSelectExtCheckBox->isChecked();

    // update the current filename's extension
    updateLocationEditExtension(m_extension /* extension hasn't changed */);
}

void KFileWidgetPrivate::placesViewSplitterMoved(int pos, int index)
{
    //     qDebug();

    // we need to record the size of the splitter when the splitter changes size
    // so we can keep the places box the right size!
    if (m_placesDock && index == 1) {
        m_placesViewWidth = pos;
        //         qDebug() << "setting m_lafBox minwidth to" << m_placesViewWidth;
    }
}

void KFileWidgetPrivate::activateUrlNavigator()
{
    //     qDebug();

    QLineEdit *lineEdit = m_urlNavigator->editor()->lineEdit();

    // If the text field currently has focus and everything is selected,
    // pressing the keyboard shortcut returns the whole thing to breadcrumb mode
    if (m_urlNavigator->isUrlEditable() && lineEdit->hasFocus() && lineEdit->selectedText() == lineEdit->text()) {
        m_urlNavigator->setUrlEditable(false);
    } else {
        m_urlNavigator->setUrlEditable(true);
        m_urlNavigator->setFocus();
        lineEdit->selectAll();
    }
}

void KFileWidgetPrivate::slotDirOpIconSizeChanged(int size)
{
    auto beginIt = m_stdIconSizes.cbegin();
    auto endIt = m_stdIconSizes.cend();
    auto it = std::lower_bound(beginIt, endIt, size);
    const int sliderStep = it != endIt ? it - beginIt : 0;
    m_iconSizeSlider->setValue(sliderStep);
    m_zoomOutAction->setDisabled(it == beginIt);
    m_zoomInAction->setDisabled(it == (endIt - 1));
}

void KFileWidgetPrivate::changeIconsSize(ZoomState zoom)
{
    int step = m_iconSizeSlider->value();

    if (zoom == ZoomOut) {
        if (step == 0) {
            return;
        }
        --step;
    } else { // ZoomIn
        if (step == static_cast<int>(m_stdIconSizes.size() - 1)) {
            return;
        }
        ++step;
    }

    m_iconSizeSlider->setValue(step);
    slotIconSizeSliderMoved(m_stdIconSizes[step]);
}

void KFileWidgetPrivate::slotIconSizeChanged(int _value)
{
    m_ops->setIconSize(_value);
    m_iconSizeSlider->setToolTip(i18n("Icon size: %1 pixels", _value));
}

void KFileWidgetPrivate::slotIconSizeSliderMoved(int size)
{
    // Force this to be called in case this slot is called first on the
    // slider move.
    slotIconSizeChanged(size);

    QPoint global(m_iconSizeSlider->rect().topLeft());
    global.ry() += m_iconSizeSlider->height() / 2;
    QHelpEvent toolTipEvent(QEvent::ToolTip, QPoint(0, 0), m_iconSizeSlider->mapToGlobal(global));
    QApplication::sendEvent(m_iconSizeSlider, &toolTipEvent);
}

void KFileWidgetPrivate::slotViewDoubleClicked(const QModelIndex &index)
{
    // double clicking to save should only work on files
    if (m_operationMode == KFileWidget::Saving && index.isValid() && m_ops->selectedItems().constFirst().isFile()) {
        q->slotOk();
    }
}

void KFileWidgetPrivate::slotViewKeyEnterReturnPressed()
{
    // an enter/return event occurred in the view
    // when we are saving one file and there is no selection in the view (otherwise we get an activated event)
    if (m_operationMode == KFileWidget::Saving && (m_ops->mode() & KFile::File) && m_ops->selectedItems().isEmpty()) {
        q->slotOk();
    }
}

static QString getExtensionFromPatternList(const QStringList &patternList)
{
    //     qDebug();

    QString ret;
    //     qDebug() << "\tgetExtension " << patternList;

    QStringList::ConstIterator patternListEnd = patternList.end();
    for (QStringList::ConstIterator it = patternList.begin(); it != patternListEnd; ++it) {
        //         qDebug() << "\t\ttry: \'" << (*it) << "\'";

        // is this pattern like "*.BMP" rather than useless things like:
        //
        // README
        // *.
        // *.*
        // *.JP*G
        // *.JP?
        // *.[Jj][Pp][Gg]
        if ((*it).startsWith(QLatin1String("*.")) && (*it).length() > 2 && (*it).indexOf(QLatin1Char('*'), 2) < 0 && (*it).indexOf(QLatin1Char('?'), 2) < 0
            && (*it).indexOf(QLatin1Char('['), 2) < 0 && (*it).indexOf(QLatin1Char(']'), 2) < 0) {
            ret = (*it).mid(1);
            break;
        }
    }

    return ret;
}

static QString stripUndisplayable(const QString &string)
{
    QString ret = string;

    ret.remove(QLatin1Char(':'));
    ret = KLocalizedString::removeAcceleratorMarker(ret);

    return ret;
}

// QString KFileWidget::currentFilterExtension()
//{
//    return d->m_extension;
//}

void KFileWidgetPrivate::updateAutoSelectExtension()
{
    if (!m_autoSelectExtCheckBox) {
        return;
    }

    QMimeDatabase db;
    //
    // Figure out an extension for the Automatically Select Extension thing
    // (some Windows users apparently don't know what to do when confronted
    // with a text file called "COPYING" but do know what to do with
    // COPYING.txt ...)
    //

    //     qDebug() << "Figure out an extension: ";
    QString lastExtension = m_extension;
    m_extension.clear();

    // Automatically Select Extension is only valid if the user is _saving_ a _file_
    if ((m_operationMode == KFileWidget::Saving) && (m_ops->mode() & KFile::File)) {
        //
        // Get an extension from the filter
        //

        KFileFilter fileFilter = m_filterWidget->currentFilter();
        if (!fileFilter.isEmpty()) {
            // if the currently selected filename already has an extension which
            // is also included in the currently allowed extensions, keep it
            // otherwise use the default extension
            QString currentExtension = db.suffixForFileName(locationEditCurrentText());
            if (currentExtension.isEmpty()) {
                currentExtension = locationEditCurrentText().section(QLatin1Char('.'), -1, -1);
            }
            // qDebug() << "filter:" << filter << "locationEdit:" << locationEditCurrentText() << "currentExtension:" << currentExtension;

            QString defaultExtension;
            QStringList extensionList;

            // e.g. "*.cpp"
            if (!fileFilter.filePatterns().isEmpty()) {
                extensionList = fileFilter.filePatterns();
                defaultExtension = getExtensionFromPatternList(extensionList);
            }
            // e.g. "text/html"
            else if (!fileFilter.mimePatterns().isEmpty()) {
                QMimeType mime = db.mimeTypeForName(fileFilter.mimePatterns().first());
                if (mime.isValid()) {
                    extensionList = mime.globPatterns();
                    defaultExtension = mime.preferredSuffix();
                    if (!defaultExtension.isEmpty()) {
                        defaultExtension.prepend(QLatin1Char('.'));
                    }
                }
            }

            if ((!currentExtension.isEmpty() && extensionList.contains(QLatin1String("*.") + currentExtension))) {
                m_extension = QLatin1Char('.') + currentExtension;
            } else {
                m_extension = defaultExtension;
            }

            // qDebug() << "List:" << extensionList << "auto-selected extension:" << m_extension;
        }

        //
        // GUI: checkbox
        //

        QString whatsThisExtension;
        if (!m_extension.isEmpty()) {
            // remember: sync any changes to the string with below
            m_autoSelectExtCheckBox->setText(i18n("Automatically select filename e&xtension (%1)", m_extension));
            whatsThisExtension = i18n("the extension <b>%1</b>", m_extension);

            m_autoSelectExtCheckBox->setEnabled(true);
            m_autoSelectExtCheckBox->setChecked(m_autoSelectExtChecked);
        } else {
            // remember: sync any changes to the string with above
            m_autoSelectExtCheckBox->setText(i18n("Automatically select filename e&xtension"));
            whatsThisExtension = i18n("a suitable extension");

            m_autoSelectExtCheckBox->setChecked(false);
            m_autoSelectExtCheckBox->setEnabled(false);
        }

        const QString locationLabelText = stripUndisplayable(m_locationLabel->text());
        m_autoSelectExtCheckBox->setWhatsThis(QLatin1String("<qt>")
                                              + i18n("This option enables some convenient features for "
                                                     "saving files with extensions:<br />"
                                                     "<ol>"
                                                     "<li>Any extension specified in the <b>%1</b> text "
                                                     "area will be updated if you change the file type "
                                                     "to save in.<br />"
                                                     "<br /></li>"
                                                     "<li>If no extension is specified in the <b>%2</b> "
                                                     "text area when you click "
                                                     "<b>Save</b>, %3 will be added to the end of the "
                                                     "filename (if the filename does not already exist). "
                                                     "This extension is based on the file type that you "
                                                     "have chosen to save in.<br />"
                                                     "<br />"
                                                     "If you do not want KDE to supply an extension for the "
                                                     "filename, you can either turn this option off or you "
                                                     "can suppress it by adding a period (.) to the end of "
                                                     "the filename (the period will be automatically "
                                                     "removed)."
                                                     "</li>"
                                                     "</ol>"
                                                     "If unsure, keep this option enabled as it makes your "
                                                     "files more manageable.",
                                                     locationLabelText,
                                                     locationLabelText,
                                                     whatsThisExtension)
                                              + QLatin1String("</qt>"));

        m_autoSelectExtCheckBox->show();

        // update the current filename's extension
        updateLocationEditExtension(lastExtension);
    }
    // Automatically Select Extension not valid
    else {
        m_autoSelectExtCheckBox->setChecked(false);
        m_autoSelectExtCheckBox->hide();
    }
}

// Updates the extension of the filename specified in d->m_locationEdit if the
// Automatically Select Extension feature is enabled.
// (this prevents you from accidentally saving "file.kwd" as RTF, for example)
void KFileWidgetPrivate::updateLocationEditExtension(const QString &lastExtension)
{
    if (!m_autoSelectExtCheckBox->isChecked() || m_extension.isEmpty()) {
        return;
    }

    const QString urlStr = locationEditCurrentText();
    if (urlStr.isEmpty()) {
        return;
    }

    const int fileNameOffset = urlStr.lastIndexOf(QLatin1Char('/')) + 1;
    QStringView fileName = QStringView(urlStr).mid(fileNameOffset);

    const int dot = fileName.lastIndexOf(QLatin1Char('.'));
    const int len = fileName.length();
    if (dot > 0 && // has an extension already and it's not a hidden file
                   // like ".hidden" (but we do accept ".hidden.ext")
        dot != len - 1 // and not deliberately suppressing extension
    ) {
        const QUrl url = getCompleteUrl(urlStr);
        //     qDebug() << "updateLocationEditExtension (" << url << ")";
        // exists?
        KIO::StatJob *statJob = KIO::stat(url, KIO::HideProgressInfo);
        KJobWidgets::setWindow(statJob, q);
        bool result = statJob->exec();
        if (result) {
            //             qDebug() << "\tfile exists";

            if (statJob->statResult().isDir()) {
                //                 qDebug() << "\tisDir - won't alter extension";
                return;
            }

            // --- fall through ---
        }

        //
        // try to get rid of the current extension
        //

        // catch "double extensions" like ".tar.gz"
        if (!lastExtension.isEmpty() && fileName.endsWith(lastExtension)) {
            fileName.chop(lastExtension.length());
        } else if (!m_extension.isEmpty() && fileName.endsWith(m_extension)) {
            fileName.chop(m_extension.length());
        } else { // can only handle "single extensions"
            fileName.truncate(dot);
        }

        // add extension
        const QString newText = QStringView(urlStr).left(fileNameOffset) + fileName + m_extension;
        if (newText != locationEditCurrentText()) {
            const int idx = m_locationEdit->currentIndex();
            if (idx == -1) {
                m_locationEdit->lineEdit()->selectAll();
                m_locationEdit->lineEdit()->insert(newText);
            } else {
                m_locationEdit->setItemText(idx, newText);
            }
            m_locationEdit->lineEdit()->setModified(true);
        }
    }
}

QString KFileWidgetPrivate::findMatchingFilter(const QString &filter, const QString &filename) const
{
    // e.g.: '*.foo *.bar|Foo type' -> '*.foo', '*.bar'
    const QStringList patterns = filter.left(filter.indexOf(QLatin1Char('|'))).split(QLatin1Char(' '), Qt::SkipEmptyParts);

    QRegularExpression rx;
    for (const QString &p : patterns) {
        rx.setPattern(QRegularExpression::wildcardToRegularExpression(p));
        if (rx.match(filename).hasMatch()) {
            return p;
        }
    }
    return QString();
}

// Updates the filter if the extension of the filename specified in d->m_locationEdit is changed
// (this prevents you from accidentally saving "file.kwd" as RTF, for example)
void KFileWidgetPrivate::updateFilter()
{
    if ((m_operationMode == KFileWidget::Saving) && (m_ops->mode() & KFile::File)) {
        QString urlStr = locationEditCurrentText();
        if (urlStr.isEmpty()) {
            return;
        }

        QMimeDatabase db;
        QMimeType urlMimeType = db.mimeTypeForFile(urlStr, QMimeDatabase::MatchExtension);

        bool matchesCurrentFilter = [this, urlMimeType, urlStr] {
            const KFileFilter filter = m_filterWidget->currentFilter();
            if (filter.mimePatterns().contains(urlMimeType.name())) {
                return true;
            }

            QString filename = urlStr.mid(urlStr.lastIndexOf(QLatin1Char('/')) + 1); // only filename

            const auto filePatterns = filter.filePatterns();
            const bool hasMatch = std::any_of(filePatterns.cbegin(), filePatterns.cend(), [filename](const QString &pattern) {
                QRegularExpression rx(QRegularExpression::wildcardToRegularExpression(pattern));

                return rx.match(filename).hasMatch();
            });
            return hasMatch;
        }();

        if (matchesCurrentFilter) {
            return;
        }

        const auto filters = m_filterWidget->filters();

        auto filterIt = std::find_if(filters.cbegin(), filters.cend(), [urlStr, urlMimeType](const KFileFilter &filter) {
            if (filter.mimePatterns().contains(urlMimeType.name())) {
                return true;
            }

            QString filename = urlStr.mid(urlStr.lastIndexOf(QLatin1Char('/')) + 1); // only filename
            // accept any match to honor the user's selection; see later code handling the "*" match

            const auto filePatterns = filter.filePatterns();
            const bool hasMatch = std::any_of(filePatterns.cbegin(), filePatterns.cend(), [filename](const QString &pattern) {
                // never match the catch-all filter
                if (pattern == QLatin1String("*")) {
                    return false;
                }

                QRegularExpression rx(QRegularExpression::wildcardToRegularExpression(pattern));

                return rx.match(filename).hasMatch();
            });

            return hasMatch;
        });

        if (filterIt != filters.cend()) {
            m_filterWidget->setCurrentFilter(*filterIt);
        }
    }
}

// applies only to a file that doesn't already exist
void KFileWidgetPrivate::appendExtension(QUrl &url)
{
    //     qDebug();

    if (!m_autoSelectExtCheckBox->isChecked() || m_extension.isEmpty()) {
        return;
    }

    QString fileName = url.fileName();
    if (fileName.isEmpty()) {
        return;
    }

    //     qDebug() << "appendExtension(" << url << ")";

    const int len = fileName.length();
    const int dot = fileName.lastIndexOf(QLatin1Char('.'));

    const bool suppressExtension = (dot == len - 1);
    const bool unspecifiedExtension = !fileName.endsWith(m_extension);

    // don't KIO::Stat if unnecessary
    if (!(suppressExtension || unspecifiedExtension)) {
        return;
    }

    // exists?
    KIO::StatJob *statJob = KIO::stat(url, KIO::HideProgressInfo);
    KJobWidgets::setWindow(statJob, q);
    bool res = statJob->exec();
    if (res) {
        //         qDebug() << "\tfile exists - won't append extension";
        return;
    }

    // suppress automatically append extension?
    if (suppressExtension) {
        //
        // Strip trailing dot
        // This allows lazy people to have m_autoSelectExtCheckBox->isChecked
        // but don't want a file extension to be appended
        // e.g. "README." will make a file called "README"
        //
        // If you really want a name like "README.", then type "README.."
        // and the trailing dot will be removed (or just stop being lazy and
        // turn off this feature so that you can type "README.")
        //
        //         qDebug() << "\tstrip trailing dot";
        QString path = url.path();
        path.chop(1);
        url.setPath(path);
    }
    // evilmatically append extension :) if the user hasn't specified one
    else if (unspecifiedExtension) {
        //         qDebug() << "\tappending extension \'" << m_extension << "\'...";
        url = url.adjusted(QUrl::RemoveFilename); // keeps trailing slash
        url.setPath(url.path() + fileName + m_extension);
        //         qDebug() << "\tsaving as \'" << url << "\'";
    }
}

// adds the selected files/urls to 'recent documents'
void KFileWidgetPrivate::addToRecentDocuments()
{
    int m = m_ops->mode();
    int atmost = KRecentDocument::maximumItems();
    // don't add more than we need. KRecentDocument::add() is pretty slow

    if (m & KFile::LocalOnly) {
        const QStringList files = q->selectedFiles();
        QStringList::ConstIterator it = files.begin();
        for (; it != files.end() && atmost > 0; ++it) {
            KRecentDocument::add(QUrl::fromLocalFile(*it));
            atmost--;
        }
    }

    else { // urls
        const QList<QUrl> urls = q->selectedUrls();
        QList<QUrl>::ConstIterator it = urls.begin();
        for (; it != urls.end() && atmost > 0; ++it) {
            if ((*it).isValid()) {
                KRecentDocument::add(*it);
                atmost--;
            }
        }
    }
}

KUrlComboBox *KFileWidget::locationEdit() const
{
    return d->m_locationEdit;
}

KFileFilterCombo *KFileWidget::filterWidget() const
{
    return d->m_filterWidget;
}

void KFileWidgetPrivate::togglePlacesPanel(bool show, QObject *sender)
{
    if (show) {
        initPlacesPanel();
        m_placesDock->show();

        // check to see if they have a home item defined, if not show the home button
        QUrl homeURL;
        homeURL.setPath(QDir::homePath());
        KFilePlacesModel *model = static_cast<KFilePlacesModel *>(m_placesView->model());
        for (int rowIndex = 0; rowIndex < model->rowCount(); rowIndex++) {
            QModelIndex index = model->index(rowIndex, 0);
            QUrl url = model->url(index);

            if (homeURL.matches(url, QUrl::StripTrailingSlash)) {
                m_toolbar->removeAction(m_ops->action(KDirOperator::Home));
                break;
            }
        }
    } else {
        if (sender == m_placesDock && m_placesDock && m_placesDock->isVisibleTo(q)) {
            // we didn't *really* go away! the dialog was simply hidden or
            // we changed virtual desktops or ...
            return;
        }

        if (m_placesDock) {
            m_placesDock->hide();
        }

        QAction *homeAction = m_ops->action(KDirOperator::Home);
        QAction *reloadAction = m_ops->action(KDirOperator::Reload);
        if (!m_toolbar->actions().contains(homeAction)) {
            m_toolbar->insertAction(reloadAction, homeAction);
        }
    }

    m_togglePlacesPanelAction->setChecked(show);

    // if we don't show the places panel, at least show the places menu
    m_urlNavigator->setPlacesSelectorVisible(!show);
}

void KFileWidgetPrivate::toggleBookmarks(bool show)
{
    if (show) {
        if (m_bookmarkHandler) {
            return;
        }
        m_bookmarkHandler = new KFileBookmarkHandler(q);
        q->connect(m_bookmarkHandler, &KFileBookmarkHandler::openUrl, q, [this](const QString &path) {
            enterUrl(path);
        });
        m_bookmarkButton->setMenu(m_bookmarkHandler->menu());
    } else if (m_bookmarkHandler) {
        m_bookmarkButton->setMenu(nullptr);
        delete m_bookmarkHandler;
        m_bookmarkHandler = nullptr;
    }

    if (m_bookmarkButton) {
        m_bookmarkButton->setVisible(show);
    }

    m_toggleBookmarksAction->setChecked(show);
}

void KFileWidgetPrivate::setQuickFilterVisible(bool show)
{
    if (m_quickFilter->isVisible() == show) {
        return;
    }
    m_quickFilter->setVisible(show);
    m_filterWidget->setEnabled(!show);
    if (show) {
        m_quickFilterEdit->setFocus();
    } else {
        m_quickFilterEdit->clear();
    }
    m_quickFilterLock->setChecked(false);
    m_ops->dirLister()->setQuickFilterMode(show);
    m_toggleQuickFilterAction->setChecked(show);
}

// static, overloaded
QUrl KFileWidget::getStartUrl(const QUrl &startDir, QString &recentDirClass)
{
    QString fileName; // result discarded
    return getStartUrl(startDir, recentDirClass, fileName);
}

// static, overloaded
QUrl KFileWidget::getStartUrl(const QUrl &startDir, QString &recentDirClass, QString &fileName)
{
    recentDirClass.clear();
    fileName.clear();
    QUrl ret;

    bool useDefaultStartDir = startDir.isEmpty();
    if (!useDefaultStartDir) {
        if (startDir.scheme() == QLatin1String("kfiledialog")) {
            //  The startDir URL with this protocol may be in the format:
            //                                                    directory()   fileName()
            //  1.  kfiledialog:///keyword                           "/"         keyword
            //  2.  kfiledialog:///keyword?global                    "/"         keyword
            //  3.  kfiledialog:///keyword/                          "/"         keyword
            //  4.  kfiledialog:///keyword/?global                   "/"         keyword
            //  5.  kfiledialog:///keyword/filename                /keyword      filename
            //  6.  kfiledialog:///keyword/filename?global         /keyword      filename

            QString keyword;
            QString urlDir = startDir.adjusted(QUrl::RemoveFilename | QUrl::StripTrailingSlash).path();
            QString urlFile = startDir.fileName();
            if (urlDir == QLatin1String("/")) { // '1'..'4' above
                keyword = urlFile;
                fileName.clear();
            } else { // '5' or '6' above
                keyword = urlDir.mid(1);
                fileName = urlFile;
            }

            const QLatin1String query(":%1");
            recentDirClass = query.arg(keyword);

            ret = QUrl::fromLocalFile(KRecentDirs::dir(recentDirClass));
        } else { // not special "kfiledialog" URL
            ret = startDir;
            if (startDir.isLocalFile() && QDir::isRelativePath(startDir.path())) {
                ret = QUrl::fromLocalFile(QDir::currentPath() + u'/' + startDir.path());
            }

            // "foo.png" only gives us a file name, the default start dir will be used.
            // "file:foo.png" (from KHTML/webkit, due to fromPath()) means the same
            //   (and is the reason why we don't just use QUrl::isRelative()).

            // In all other cases (startDir contains a directory path, or has no
            // fileName for us anyway, such as smb://), startDir is indeed a dir url.
            if (!ret.adjusted(QUrl::RemoveFilename | QUrl::StripTrailingSlash).path().isEmpty() || ret.fileName().isEmpty()) {
                // can use start directory
                // If we won't be able to list it (e.g. http), then use default
                if (!KProtocolManager::supportsListing(ret)) {
                    useDefaultStartDir = true;
                    fileName = startDir.fileName();
                }
            } else { // file name only
                fileName = startDir.fileName();
                useDefaultStartDir = true;
            }
        }
    }

    if (useDefaultStartDir) {
        if (lastDirectory()->isEmpty()) {
            *lastDirectory() = QUrl::fromLocalFile(QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation));
            const QUrl home(QUrl::fromLocalFile(QDir::homePath()));
            // if there is no docpath set (== home dir), we prefer the current
            // directory over it. We also prefer the homedir when our CWD is
            // different from our homedirectory or when the document dir
            // does not exist
            if (lastDirectory()->adjusted(QUrl::StripTrailingSlash) == home.adjusted(QUrl::StripTrailingSlash) //
                || QDir::currentPath() != QDir::homePath() //
                || !QDir(lastDirectory()->toLocalFile()).exists()) {
                *lastDirectory() = QUrl::fromLocalFile(QDir::currentPath());
            }
        }
        ret = *lastDirectory();
    }

    // qDebug() << "for" << startDir << "->" << ret << "recentDirClass" << recentDirClass << "fileName" << fileName;
    return ret;
}

void KFileWidget::setStartDir(const QUrl &directory)
{
    if (directory.isValid()) {
        *lastDirectory() = directory;
    }
}

void KFileWidgetPrivate::setNonExtSelection()
{
    // Enhanced rename: Don't highlight the file extension.
    QString filename = locationEditCurrentText();
    QMimeDatabase db;
    QString extension = db.suffixForFileName(filename);

    if (!extension.isEmpty()) {
        m_locationEdit->lineEdit()->setSelection(0, filename.length() - extension.length() - 1);
    } else {
        int lastDot = filename.lastIndexOf(QLatin1Char('.'));
        if (lastDot > 0) {
            m_locationEdit->lineEdit()->setSelection(0, lastDot);
        } else {
            m_locationEdit->lineEdit()->selectAll();
        }
    }
}

// Sets the filter text to "File type" if the dialog is saving and a MIME type
// filter has been set; otherwise, the text is "Filter:"
void KFileWidgetPrivate::updateFilterText()
{
    QString label = i18n("&File type:");
    QString whatsThisText;

    if (m_operationMode == KFileWidget::Saving && !m_filterWidget->currentFilter().mimePatterns().isEmpty()) {
        whatsThisText = i18n("<qt>This is the file type selector. It is used to select the format that the file will be saved as.</qt>");
    } else {
        whatsThisText = i18n("<qt>This is the file type selector. It is used to select the format of the files shown.</qt>");
    }

    if (m_filterLabel) {
        m_filterLabel->setText(label);
        m_filterLabel->setWhatsThis(whatsThisText);
    }
    if (m_filterWidget) {
        m_filterWidget->setWhatsThis(whatsThisText);
    }
}

void KFileWidget::setCustomWidget(QWidget *widget)
{
    delete d->m_bottomCustomWidget;
    d->m_bottomCustomWidget = widget;

    // add it to the dialog, below the filter list box.

    // Change the parent so that this widget is a child of the main widget
    d->m_bottomCustomWidget->setParent(this);

    d->m_opsWidgetLayout->addWidget(d->m_bottomCustomWidget);

    // FIXME: This should adjust the tab orders so that the custom widget
    // comes after the Cancel button. The code appears to do this, but the result
    // somehow screws up the tab order of the file path combo box. Not a major
    // problem, but ideally the tab order with a custom widget should be
    // the same as the order without one.
    setTabOrder(d->m_cancelButton, d->m_bottomCustomWidget);
    setTabOrder(d->m_bottomCustomWidget, d->m_urlNavigator);
}

void KFileWidget::setCustomWidget(const QString &text, QWidget *widget)
{
    delete d->m_labeledCustomWidget;
    d->m_labeledCustomWidget = widget;

    QLabel *label = new QLabel(text, this);
    label->setAlignment(Qt::AlignRight);
    d->m_lafBox->addRow(label, widget);
}

KDirOperator *KFileWidget::dirOperator()
{
    return d->m_ops;
}

#if KIOFILEWIDGETS_BUILD_DEPRECATED_SINCE(6, 3)
void KFileWidget::readConfig(KConfigGroup &group)
{
    d->m_configGroup = group;
    d->readViewConfig();
    d->readRecentFiles();
}
#endif

QString KFileWidgetPrivate::locationEditCurrentText() const
{
    return QDir::fromNativeSeparators(m_locationEdit->currentText());
}

QUrl KFileWidgetPrivate::mostLocalUrl(const QUrl &url)
{
    if (url.isLocalFile()) {
        return url;
    }

    KIO::StatJob *statJob = KIO::stat(url, KIO::HideProgressInfo);
    KJobWidgets::setWindow(statJob, q);
    bool res = statJob->exec();

    if (!res) {
        return url;
    }

    const QString path = statJob->statResult().stringValue(KIO::UDSEntry::UDS_LOCAL_PATH);
    if (!path.isEmpty()) {
        QUrl newUrl;
        newUrl.setPath(path);
        return newUrl;
    }

    return url;
}

void KFileWidgetPrivate::setInlinePreviewShown(bool show)
{
    m_ops->setInlinePreviewShown(show);
}

void KFileWidget::setConfirmOverwrite(bool enable)
{
    d->m_confirmOverwrite = enable;
}

void KFileWidget::setInlinePreviewShown(bool show)
{
    d->setInlinePreviewShown(show);
}

QSize KFileWidget::dialogSizeHint() const
{
    int fontSize = fontMetrics().height();
    QSize goodSize(48 * fontSize, 30 * fontSize);
    const QSize scrnSize = d->screenSize();
    QSize minSize(scrnSize / 2);
    QSize maxSize(scrnSize * qreal(0.9));
    return (goodSize.expandedTo(minSize).boundedTo(maxSize));
}

void KFileWidget::setViewMode(KFile::FileView mode)
{
    d->m_ops->setViewMode(mode);
    d->m_hasView = true;
}

void KFileWidget::setSupportedSchemes(const QStringList &schemes)
{
    d->m_model->setSupportedSchemes(schemes);
    d->m_ops->setSupportedSchemes(schemes);
    d->m_urlNavigator->setSupportedSchemes(schemes);
}

QStringList KFileWidget::supportedSchemes() const
{
    return d->m_model->supportedSchemes();
}

#include "moc_kfilewidget.cpp"
