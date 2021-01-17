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

#include "../pathhelpers_p.h" // concatPaths() and isAbsoluteLocalPath()
#include "kfilebookmarkhandler_p.h"
#include "kfileplacesmodel.h"
#include "kfileplacesview.h"
#include "kfilepreviewgenerator.h"
#include "kfilewidgetdocktitlebar_p.h"
#include "kurlcombobox.h"
#include "kurlnavigator.h"
#include <KActionCollection>
#include <KActionMenu>
#include <KConfigGroup>
#include <KFileItem>
#include <KLocalizedString>
#include <KSharedConfig>
#include <KToolBar>
#include <config-kiofilewidgets.h>
#include <defaults-kfile.h>
#include <KDirLister>
#include <kdiroperator.h>
#include <kfilefiltercombo.h>
#include <kfileitemdelegate.h>
#include <kimagefilepreview.h>
#include <kio/job.h>
#include <kio/jobuidelegate.h>
#include <kio/scheduler.h>
#include <kprotocolmanager.h>
#include <krecentdirs.h>
#include <krecentdocument.h>
#include <kurlcompletion.h>

#include <QCheckBox>
#include <QDebug>
#include <QDesktopWidget>
#include <QDockWidget>
#include <QIcon>
#include <QLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QSplitter>
#include <QAbstractProxyModel>
#include <QLoggingCategory>
#include <QHelpEvent>
#include <QApplication>
#include <QPushButton>
#include <QStandardPaths>
#include <QMimeDatabase>
#include <QTimer>

#include <KIconLoader>
#include <KShell>
#include <KMessageBox>
#include <kurlauthorized.h>
#include <KJobWidgets>

#include <algorithm>
#include <array>

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

    void updateLocationWhatsThis();
    void updateAutoSelectExtension();
    void initPlacesPanel();
    void setPlacesViewSplitterSizes();
    void setLafBoxColumnWidth();
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
    /**
     * Parses the string "line" for files. If line doesn't contain any ", the
     * whole line will be interpreted as one file. If the number of " is odd,
     * an empty list will be returned. Otherwise, all items enclosed in " "
     * will be returned as correct urls.
     */
    QList<QUrl> tokenize(const QString &line) const;
    /**
     * Reads the recent used files and inserts them into the location combobox
     */
    void readRecentFiles();
    /**
     * Saves the entries from the location combobox.
     */
    void saveRecentFiles();
    /**
     * called when an item is highlighted/selected in multiselection mode.
     * handles setting the m_locationEdit.
     */
    void multiSelectionChanged();

    /**
     * Returns the absolute version of the URL specified in m_locationEdit.
     */
    QUrl getCompleteUrl(const QString &) const;

    /**
     * Sets the dummy entry on the history combo box. If the dummy entry
     * already exists, it is overwritten with this information.
     */
    void setDummyHistoryEntry(const QString &text, const QIcon &icon = QIcon(),
                              bool usePreviousPixmapIfNull = true);

    /**
     * Removes the dummy entry of the history combo box.
     */
    void removeDummyHistoryEntry();

    /**
     * Asks for overwrite confirmation using a KMessageBox and returns
     * true if the user accepts.
     *
     * @since 4.2
     */
    bool toOverwrite(const QUrl &);

    // private slots
    void slotLocationChanged(const QString &);
    void urlEntered(const QUrl &);
    void enterUrl(const QUrl &);
    void enterUrl(const QString &);
    void locationAccepted(const QString &);
    void slotFilterChanged();
    void fileHighlighted(const KFileItem &);
    void fileSelected(const KFileItem &);
    void slotLoadingFinished();
    void fileCompletion(const QString &);
    void togglePlacesPanel(bool show, QObject *sender = nullptr);
    void toggleBookmarks(bool);
    void slotAutoSelectExtClicked();
    void placesViewSplitterMoved(int, int);
    void activateUrlNavigator();
    void zoomOutIconsSize();
    void zoomInIconsSize();
    void slotIconSizeSliderMoved(int);
    void slotIconSizeChanged(int);
    void slotViewDoubleClicked(const QModelIndex&);
    void slotViewKeyEnterReturnPressed();

    void addToRecentDocuments();

    QString locationEditCurrentText() const;

    /**
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
    QGridLayout *m_lafBox = nullptr;
    QVBoxLayout *m_vbox = nullptr;

    QLabel *m_locationLabel = nullptr;
    QWidget *m_opsWidget = nullptr;

    QLabel *m_filterLabel = nullptr;
    KUrlNavigator *m_urlNavigator = nullptr;
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

    QList<QUrl> m_urlList; //the list of selected urls

    KFileWidget::OperationMode m_operationMode = KFileWidget::Opening;

    // The file class used for KRecentDirs
    QString m_fileClass;

    KFileBookmarkHandler *m_bookmarkHandler = nullptr;

    KActionMenu *m_bookmarkButton = nullptr;

    KToolBar *m_toolbar = nullptr;
    KUrlComboBox *m_locationEdit = nullptr;
    KDirOperator *m_ops = nullptr;
    KFileFilterCombo *m_filterWidget = nullptr;
    QTimer m_filterDelayTimer;

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
    bool m_dummyAdded = false; // if the dummy item has been added. This prevents the combo from having a
    // blank item added when loaded
    bool m_confirmOverwrite = false;
    bool m_differentHierarchyLevelItemsEntered = false;

    const std::array<KIconLoader::StdSizes, 6> m_stdIconSizes =
            {KIconLoader::SizeSmall, KIconLoader::SizeSmallMedium, KIconLoader::SizeMedium,
             KIconLoader::SizeLarge, KIconLoader::SizeHuge, KIconLoader::SizeEnormous};

    QSlider *m_iconSizeSlider = nullptr;
    QAction *m_zoomOutAction = nullptr;
    QAction *m_zoomInAction = nullptr;

    // The group which stores app-specific settings. These settings are recent
    // files and urls. Visual settings (view mode, sorting criteria...) are not
    // app-specific and are stored in kdeglobals
    KConfigGroup m_configGroup;
};

Q_GLOBAL_STATIC(QUrl, lastDirectory) // to set the start path

static const char autocompletionWhatsThisText[] = I18N_NOOP("<qt>While typing in the text area, you may be presented "
        "with possible matches. "
        "This feature can be controlled by clicking with the right mouse button "
        "and selecting a preferred mode from the <b>Text Completion</b> menu.</qt>");

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
            return true;    // at least two letters before ":/"
        }
        i += 3; // skip : and / and one char
    }
    return false;
}

// this string-to-url conversion function handles relative paths, full paths and URLs
// without the http-prepending that QUrl::fromUserInput does.
static QUrl urlFromString(const QString& str)
{
    if (isAbsoluteLocalPath(str)) {
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
    : QWidget(parent), d(new KFileWidgetPrivate(this))
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

    d->m_opsWidget = new QWidget(this);
    QVBoxLayout *opsWidgetLayout = new QVBoxLayout(d->m_opsWidget);
    opsWidgetLayout->setContentsMargins(0, 0, 0, 0);
    opsWidgetLayout->setSpacing(0);
    //d->m_toolbar = new KToolBar(this, true);
    d->m_toolbar = new KToolBar(d->m_opsWidget, true);
    d->m_toolbar->setObjectName(QStringLiteral("KFileWidget::toolbar"));
    d->m_toolbar->setMovable(false);
    opsWidgetLayout->addWidget(d->m_toolbar);

    d->m_model = new KFilePlacesModel(this);

    // Resolve this now so that a 'kfiledialog:' URL, if specified,
    // does not get inserted into the urlNavigator history.
    d->m_url = getStartUrl(startDir, d->m_fileClass, filename);
    startDir = d->m_url;

    // Don't pass startDir to the KUrlNavigator at this stage: as well as
    // the above, it may also contain a file name which should not get
    // inserted in that form into the old-style navigation bar history.
    // Wait until the KIO::stat has been done later.
    //
    // The stat cannot be done before this point, bug 172678.
    d->m_urlNavigator = new KUrlNavigator(d->m_model, QUrl(), d->m_opsWidget); //d->m_toolbar);
    d->m_urlNavigator->setPlacesSelectorVisible(false);
    opsWidgetLayout->addWidget(d->m_urlNavigator);

    d->m_ops = new KDirOperator(QUrl(), d->m_opsWidget);
    d->m_ops->installEventFilter(this);
    d->m_ops->setObjectName(QStringLiteral("KFileWidget::ops"));
    d->m_ops->setIsSaving(d->m_operationMode == Saving);
    d->m_ops->setNewFileMenuSelectDirWhenAlreadyExist(true);
    opsWidgetLayout->addWidget(d->m_ops);
    connect(d->m_ops, &KDirOperator::urlEntered, this, [this](const QUrl &url) { d->urlEntered(url); });
    connect(d->m_ops, &KDirOperator::fileHighlighted, this, [this](const KFileItem &item) { d->fileHighlighted(item); });
    connect(d->m_ops, &KDirOperator::fileSelected, this, [this](const KFileItem &item) { d->fileSelected(item); });
    connect(d->m_ops, &KDirOperator::finishedLoading, this, [this]() { d->slotLoadingFinished(); });
    connect(d->m_ops, &KDirOperator::keyEnterReturnPressed, this, [this]() { d->slotViewKeyEnterReturnPressed(); });

    d->m_ops->setupMenu(KDirOperator::SortActions |
                      KDirOperator::FileActions |
                      KDirOperator::ViewActions);
    KActionCollection *coll = d->m_ops->actionCollection();
    coll->addAssociatedWidget(this);

    // add nav items to the toolbar
    //
    // NOTE:  The order of the button icons here differs from that
    // found in the file manager and web browser, but has been discussed
    // and agreed upon on the kde-core-devel mailing list:
    //
    // http://lists.kde.org/?l=kde-core-devel&m=116888382514090&w=2

    coll->action(QStringLiteral("up"))->setWhatsThis(i18n("<qt>Click this button to enter the parent folder.<br /><br />"
                                          "For instance, if the current location is file:/home/konqi clicking this "
                                          "button will take you to file:/home.</qt>"));

    coll->action(QStringLiteral("back"))->setWhatsThis(i18n("Click this button to move backwards one step in the browsing history."));
    coll->action(QStringLiteral("forward"))->setWhatsThis(i18n("Click this button to move forward one step in the browsing history."));

    coll->action(QStringLiteral("reload"))->setWhatsThis(i18n("Click this button to reload the contents of the current location."));
    coll->action(QStringLiteral("mkdir"))->setShortcuts(KStandardShortcut::createFolder());
    coll->action(QStringLiteral("mkdir"))->setWhatsThis(i18n("Click this button to create a new folder."));

    QAction *goToNavigatorAction = coll->addAction(QStringLiteral("gotonavigator"), this,
                                                   [this]() { d->activateUrlNavigator(); });
    goToNavigatorAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_L));

    KToggleAction *showSidebarAction = new KToggleAction(i18n("Show Places Panel"), this);
    coll->addAction(QStringLiteral("togglePlacesPanel"), showSidebarAction);
    showSidebarAction->setShortcut(QKeySequence(Qt::Key_F9));
    connect(showSidebarAction, &QAction::toggled, this, [this](bool show) { d->togglePlacesPanel(show); });

    KToggleAction *showBookmarksAction =
        new KToggleAction(i18n("Show Bookmarks Button"), this);
    coll->addAction(QStringLiteral("toggleBookmarks"), showBookmarksAction);
    connect(showBookmarksAction, &QAction::toggled, this, [this](bool show) { d->toggleBookmarks(show); });

    // Build the settings menu
    KActionMenu *menu = new KActionMenu(QIcon::fromTheme(QStringLiteral("configure")), i18n("Options"), this);
    coll->addAction(QStringLiteral("extra menu"), menu);
    menu->setWhatsThis(i18n("<qt>This is the preferences menu for the file dialog. "
                            "Various options can be accessed from this menu including: <ul>"
                            "<li>how files are sorted in the list</li>"
                            "<li>types of view, including icon and list</li>"
                            "<li>showing of hidden files</li>"
                            "<li>the Places panel</li>"
                            "<li>file previews</li>"
                            "<li>separating folders from files</li></ul></qt>"));

    menu->addAction(coll->action(QStringLiteral("allow expansion")));
    menu->addSeparator();
    menu->addAction(coll->action(QStringLiteral("show hidden")));
    menu->addAction(showSidebarAction);
    menu->addAction(showBookmarksAction);
    menu->addAction(coll->action(QStringLiteral("preview")));

    menu->setPopupMode(QToolButton::InstantPopup);
    connect(menu->menu(), &QMenu::aboutToShow,
            d->m_ops, &KDirOperator::updateSelectionDependentActions);

    d->m_iconSizeSlider = new QSlider(this);
    d->m_iconSizeSlider->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Fixed);
    d->m_iconSizeSlider->setMinimumWidth(40);
    d->m_iconSizeSlider->setOrientation(Qt::Horizontal);
    d->m_iconSizeSlider->setMinimum(KIconLoader::SizeSmall);
    d->m_iconSizeSlider->setMaximum(KIconLoader::SizeEnormous);
    d->m_iconSizeSlider->installEventFilter(this);

    connect(d->m_iconSizeSlider, &QAbstractSlider::valueChanged,
            this, [this](int value) { d->slotIconSizeChanged(value); });

    connect(d->m_iconSizeSlider, &QAbstractSlider::sliderMoved,
            this, [this](int value) { d->slotIconSizeSliderMoved(value); });

    connect(d->m_ops, &KDirOperator::currentIconSizeChanged, this, [this](int value) {
        d->m_iconSizeSlider->setValue(value);
        d->m_zoomOutAction->setDisabled(value <= d->m_iconSizeSlider->minimum());
        d->m_zoomInAction->setDisabled(value >= d->m_iconSizeSlider->maximum());
    });

    d->m_zoomOutAction = new QAction(QIcon::fromTheme(QStringLiteral("file-zoom-out")), i18n("Zoom out"), this);
    connect(d->m_zoomOutAction, &QAction::triggered, this, [this]() { d->zoomOutIconsSize(); });

    d->m_zoomInAction = new QAction(QIcon::fromTheme(QStringLiteral("file-zoom-in")), i18n("Zoom in"), this);
    connect(d->m_zoomInAction, &QAction::triggered, this, [this]() { d->zoomInIconsSize(); });

    d->m_bookmarkButton = new KActionMenu(QIcon::fromTheme(QStringLiteral("bookmarks")), i18n("Bookmarks"), this);
    d->m_bookmarkButton->setPopupMode(QToolButton::InstantPopup);
    coll->addAction(QStringLiteral("bookmark"), d->m_bookmarkButton);
    d->m_bookmarkButton->setWhatsThis(i18n("<qt>This button allows you to bookmark specific locations. "
                                        "Click on this button to open the bookmark menu where you may add, "
                                        "edit or select a bookmark.<br /><br />"
                                        "These bookmarks are specific to the file dialog, but otherwise operate "
                                        "like bookmarks elsewhere in KDE.</qt>"));

    QWidget *midSpacer = new QWidget(this);
    midSpacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    d->m_toolbar->addAction(coll->action(QStringLiteral("back")));
    d->m_toolbar->addAction(coll->action(QStringLiteral("forward")));
    d->m_toolbar->addAction(coll->action(QStringLiteral("up")));
    d->m_toolbar->addAction(coll->action(QStringLiteral("reload")));
    d->m_toolbar->addSeparator();
    d->m_toolbar->addAction(coll->action(QStringLiteral("icons view")));
    d->m_toolbar->addAction(coll->action(QStringLiteral("compact view")));
    d->m_toolbar->addAction(coll->action(QStringLiteral("details view")));
    d->m_toolbar->addSeparator();
    d->m_toolbar->addAction(coll->action(QStringLiteral("inline preview")));
    d->m_toolbar->addAction(coll->action(QStringLiteral("sorting menu")));
    d->m_toolbar->addAction(d->m_bookmarkButton);

    d->m_toolbar->addWidget(midSpacer);

    d->m_toolbar->addAction(d->m_zoomOutAction);
    d->m_toolbar->addWidget(d->m_iconSizeSlider);
    d->m_toolbar->addAction(d->m_zoomInAction);
    d->m_toolbar->addSeparator();
    d->m_toolbar->addAction(coll->action(QStringLiteral("mkdir")));
    d->m_toolbar->addAction(menu);

    d->m_toolbar->setToolButtonStyle(Qt::ToolButtonIconOnly);
    d->m_toolbar->setMovable(false);

    KUrlComboBox *pathCombo = d->m_urlNavigator->editor();
    KUrlCompletion *pathCompletionObj = new KUrlCompletion(KUrlCompletion::DirCompletion);
    pathCombo->setCompletionObject(pathCompletionObj);
    pathCombo->setAutoDeleteCompletionObject(true);

    connect(d->m_urlNavigator, &KUrlNavigator::urlChanged, this, [this](const QUrl &url) { d->enterUrl(url); });
    connect(d->m_urlNavigator, &KUrlNavigator::returnPressed, d->m_ops, QOverload<>::of(&QWidget::setFocus));

    // the Location label/edit
    d->m_locationLabel = new QLabel(i18n("&Name:"), this);
    d->m_locationEdit = new KUrlComboBox(KUrlComboBox::Files, true, this);
    d->m_locationEdit->installEventFilter(this);
    // Properly let the dialog be resized (to smaller). Otherwise we could have
    // huge dialogs that can't be resized to smaller (it would be as big as the longest
    // item in this combo box). (ereslibre)
    d->m_locationEdit->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLength);
    connect(d->m_locationEdit, &KUrlComboBox::editTextChanged,
            this, [this](const QString &text) { d->slotLocationChanged(text); });

    d->updateLocationWhatsThis();
    d->m_locationLabel->setBuddy(d->m_locationEdit);

    KUrlCompletion *fileCompletionObj = new KUrlCompletion(KUrlCompletion::FileCompletion);
    d->m_locationEdit->setCompletionObject(fileCompletionObj);
    d->m_locationEdit->setAutoDeleteCompletionObject(true);
    connect(fileCompletionObj, &KUrlCompletion::match, this, [this](const QString &match) { d->fileCompletion(match); });

    connect(d->m_locationEdit, QOverload<const QString &>::of(&KUrlComboBox::returnPressed),
            this, [this](const QString &text) { d->locationAccepted(text); });

    // the Filter label/edit
    d->m_filterLabel = new QLabel(this);
    d->m_filterWidget = new KFileFilterCombo(this);
    d->updateFilterText();
    // Properly let the dialog be resized (to smaller). Otherwise we could have
    // huge dialogs that can't be resized to smaller (it would be as big as the longest
    // item in this combo box). (ereslibre)
    d->m_filterWidget->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLength);
    d->m_filterLabel->setBuddy(d->m_filterWidget);
    connect(d->m_filterWidget, &KFileFilterCombo::filterChanged, this, [this]() { d->slotFilterChanged(); });

    d->m_filterDelayTimer.setSingleShot(true);
    d->m_filterDelayTimer.setInterval(300);
    connect(d->m_filterWidget, &QComboBox::editTextChanged, &d->m_filterDelayTimer, QOverload<>::of(&QTimer::start));
    connect(&d->m_filterDelayTimer, &QTimer::timeout, this, [this]() { d->slotFilterChanged(); });

    // the Automatically Select Extension checkbox
    // (the text, visibility etc. is set in updateAutoSelectExtension(), which is called by readConfig())
    d->m_autoSelectExtCheckBox = new QCheckBox(this);
    const int spacingHint = style()->pixelMetric(QStyle::PM_DefaultLayoutSpacing);
    d->m_autoSelectExtCheckBox->setStyleSheet(QStringLiteral("QCheckBox { padding-top: %1px; }").arg(spacingHint));
    connect(d->m_autoSelectExtCheckBox, &QCheckBox::clicked, this, [this]() { d->slotAutoSelectExtClicked(); });

    d->initGUI(); // activate GM

    // read our configuration
    KSharedConfig::Ptr config = KSharedConfig::openConfig();
    config->reparseConfiguration(); // grab newly added dirs by other processes (#403524)
    KConfigGroup group(config, ConfigGroup);
    readConfig(group);

    coll->action(QStringLiteral("inline preview"))->setChecked(d->m_ops->isInlinePreviewShown());
    d->m_iconSizeSlider->setValue(d->m_ops->iconSize());

    KFilePreviewGenerator *pg = d->m_ops->previewGenerator();
    if (pg) {
        coll->action(QStringLiteral("inline preview"))->setChecked(pg->isPreviewShown());
    }

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
}

KFileWidget::~KFileWidget()
{
    KSharedConfig::Ptr config = KSharedConfig::openConfig();
    config->sync();
}

void KFileWidget::setLocationLabel(const QString &text)
{
    d->m_locationLabel->setText(text);
}

void KFileWidget::setFilter(const QString &filter)
{
    int pos = filter.indexOf(QLatin1Char('/'));

    // Check for an un-escaped '/', if found
    // interpret as a MIME filter.

    if (pos > 0 && filter[pos - 1] != QLatin1Char('\\')) {
        QStringList filters = filter.split(QLatin1Char(' '), Qt::SkipEmptyParts);
        setMimeFilter(filters);
        return;
    }

    // Strip the escape characters from
    // escaped '/' characters.

    QString copy(filter);
    for (pos = 0; (pos = copy.indexOf(QLatin1String("\\/"), pos)) != -1; ++pos) {
        copy.remove(pos, 1);
    }

    d->m_ops->clearFilter();
    d->m_filterWidget->setFilter(copy);
    d->m_ops->setNameFilter(d->m_filterWidget->currentFilter());
    d->m_ops->updateDir();
    d->m_hasDefaultFilter = false;
    d->m_filterWidget->setEditable(true);

    d->updateAutoSelectExtension();
}

QString KFileWidget::currentFilter() const
{
    return d->m_filterWidget->currentFilter();
}

void KFileWidget::setMimeFilter(const QStringList &mimeTypes,
                                const QString &defaultType)
{
    d->m_filterWidget->setMimeFilter(mimeTypes, defaultType);

    QStringList types = d->m_filterWidget->currentFilter().split(QLatin1Char(' '), Qt::SkipEmptyParts); //QStringList::split(" ", d->m_filterWidget->currentFilter());

    types.append(QStringLiteral("inode/directory"));
    d->m_ops->clearFilter();
    d->m_ops->setMimeFilter(types);
    d->m_hasDefaultFilter = !defaultType.isEmpty();
    d->m_filterWidget->setEditable(!d->m_hasDefaultFilter ||
                                 d->m_operationMode != Saving);

    d->updateAutoSelectExtension();
    d->updateFilterText();
}

void KFileWidget::clearFilter()
{
    d->m_filterWidget->setFilter(QString());
    d->m_ops->clearFilter();
    d->m_hasDefaultFilter = false;
    d->m_filterWidget->setEditable(true);

    d->updateAutoSelectExtension();
}

QString KFileWidget::currentMimeFilter() const
{
    int i = d->m_filterWidget->currentIndex();
    if (d->m_filterWidget->showsAllTypes() && i == 0) {
        return QString();    // The "all types" item has no MIME type
    }

    return d->m_filterWidget->filters().at(i);
}

QMimeType KFileWidget::currentFilterMimeType()
{
    QMimeDatabase db;
    return db.mimeTypeForName(currentMimeFilter());
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

    if (isAbsoluteLocalPath(url)) {
        u = QUrl::fromLocalFile(url);
    } else {
        QUrl relativeUrlTest(m_ops->url());
        relativeUrlTest.setPath(concatPaths(relativeUrlTest.path(), url));
        if (!m_ops->dirLister()->findByUrl(relativeUrlTest).isNull() ||
                !KProtocolInfo::isKnownProtocol(relativeUrlTest)) {
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
    const QSize screenSize = QApplication::desktop()->availableGeometry(this).size();
    const QSize minSize(screenSize / 2);
    const QSize maxSize(screenSize * qreal(0.9));
    return (goodSize.expandedTo(minSize).boundedTo(maxSize));
}

static QString relativePathOrUrl(const QUrl &baseUrl, const QUrl &url);

/**
 * Escape the given Url so that is fit for use in the selected list of file. This
 * mainly handles double quote (") characters. These are used to separate entries
 * in the list, however, if `"` appears in the filename (or path), this will be
 * escaped as `\"`. Later, the tokenizer is able to understand the difference
 * and do the right thing
 */
static QString escapeDoubleQuotes(QString && path);

// Called by KFileDialog
void KFileWidget::slotOk()
{
//     qDebug() << "slotOk\n";

    const QString locationEditCurrentText(KShell::tildeExpand(d->locationEditCurrentText()));

    QList<QUrl> locationEditCurrentTextList(d->tokenize(locationEditCurrentText));
    KFile::Modes mode = d->m_ops->mode();

    // if there is nothing to do, just return from here
    if (locationEditCurrentTextList.isEmpty()) {
        return;
    }

    // Make sure that one of the modes was provided
    if (!((mode & KFile::File) || (mode & KFile::Directory) || (mode & KFile::Files))) {
        mode |= KFile::File;
        // qDebug() << "No mode() provided";
    }

    // Clear the list as we are going to refill it
    d->m_urlList.clear();

    const bool directoryMode = (mode & KFile::Directory);
    const bool onlyDirectoryMode = directoryMode && !(mode & KFile::File) && !(mode & KFile::Files);

    // if we are on file mode, and the list of provided files/folder is greater than one, inform
    // the user about it
    if (locationEditCurrentTextList.count() > 1) {
        if (mode & KFile::File) {
            KMessageBox::sorry(this,
                               i18n("You can only select one file"),
                               i18n("More than one file provided"));
            return;
        }

        /**
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
        if (!d->m_differentHierarchyLevelItemsEntered) {     // avoid infinite recursion. running this
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
        /**
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
            && (isAbsoluteLocalPath(locationEditCurrentText) || containsProtocolSection(locationEditCurrentText))) {

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
                            if (!url.path().endsWith(QLatin1Char('/'))) {
                                url.setPath(url.path() + QLatin1Char('/'));
                            }
                        }
                    }
                } else {
                    const QUrl directory = url.adjusted(QUrl::RemoveFilename);
                    //Check if the folder exists
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
                locationEditCurrentTextList = { url };
            }
        }
    }

    // restore it
    d->m_differentHierarchyLevelItemsEntered = false;

    // locationEditCurrentTextList contains absolute paths
    // this is the general loop for the File and Files mode. Obviously we know
    // that the File mode will iterate only one time here
    QList<QUrl>::ConstIterator it = locationEditCurrentTextList.constBegin();
    bool filesInList = false;
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
            KMessageBox::sorry(this,
                               i18n("You can only select local files"),
                               i18n("Remote files not accepted"));
            return;
        }

        const auto &supportedSchemes = d->m_model->supportedSchemes();
        if (!supportedSchemes.isEmpty() && !supportedSchemes.contains(d->m_url.scheme())) {
            KMessageBox::sorry(this,
                               i18np("The selected URL uses an unsupported scheme. "
                                     "Please use the following scheme: %2",
                                     "The selected URL uses an unsupported scheme. "
                                     "Please use one of the following schemes: %2",
                                     supportedSchemes.size(),
                                     supportedSchemes.join(QLatin1String(", "))),
                               i18n("Unsupported URL scheme"));
            return;
        }

        // if we are given a folder when not on directory mode, let's get into it
        if (res && !directoryMode && statJob->statResult().isDir()) {
            // check if we were given more than one folder, in that case we don't know to which one
            // cd
            ++it;
            while (it != locationEditCurrentTextList.constEnd()) {
                QUrl checkUrl(*it);
                KIO::StatJob *checkStatJob = KIO::stat(checkUrl, KIO::HideProgressInfo);
                KJobWidgets::setWindow(checkStatJob, this);
                bool res = checkStatJob->exec();
                if (res && checkStatJob->statResult().isDir()) {
                    KMessageBox::sorry(this, i18n("More than one folder has been selected and this dialog does not accept folders, so it is not possible to decide which one to enter. Please select only one folder to list it."), i18n("More than one folder provided"));
                    return;
                } else if (res) {
                    filesInList = true;
                }
                ++it;
            }
            if (filesInList) {
                KMessageBox::information(this, i18n("At least one folder and one file has been selected. Selected files will be ignored and the selected folder will be listed"), i18n("Files and folders selected"));
            }
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
            filesInList = true;
        } else {
            KMessageBox::sorry(this, i18n("The file \"%1\" could not be found", url.toDisplayString(QUrl::PreferLocalFile)), i18n("Cannot open file"));
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
    QList<QUrl>::const_iterator it = list.begin();
    int atmost = d->m_locationEdit->maxItems(); //don't add more items than necessary
    for (; it != list.end() && atmost > 0; ++it) {
        const QUrl &url = *it;
        // we strip the last slash (-1) because KUrlComboBox does that as well
        // when operating in file-mode. If we wouldn't , dupe-finding wouldn't
        // work.
        QString file = url.isLocalFile() ? url.toLocalFile() : url.toDisplayString();

        // remove dupes
        for (int i = 1; i < d->m_locationEdit->count(); i++) {
            if (d->m_locationEdit->itemText(i) == file) {
                d->m_locationEdit->removeItem(i--);
                break;
            }
        }
        //FIXME I don't think this works correctly when the KUrlComboBox has some default urls.
        //KUrlComboBox should provide a function to add an url and rotate the existing ones, keeping
        //track of maxItems, and we shouldn't be able to insert items as we please.
        d->m_locationEdit->insertItem(1, file);
        atmost--;
    }

    d->writeViewConfig();
    d->saveRecentFiles();

    d->addToRecentDocuments();

    if (!(mode() & KFile::Files)) { // single selection
        Q_EMIT fileSelected(d->m_url);
    }

    d->m_ops->close();
}

void KFileWidgetPrivate::fileHighlighted(const KFileItem &i)
{
    if ((!i.isNull() && i.isDir()) ||
            (m_locationEdit->hasFocus() && !m_locationEdit->currentText().isEmpty())) { // don't disturb
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
    if (m_operationMode == KFileWidget::Saving) {
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
        setLocationText(i.url());
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

    setLocationText(list.urlList());
}

void KFileWidgetPrivate::setDummyHistoryEntry(const QString &text, const QIcon &icon,
        bool usePreviousPixmapIfNull)
{
    // Block m_locationEdit signals as setCurrentItem() will cause textChanged() to get
    // emitted, so slotLocationChanged() will be called. Make sure we don't clear the
    // KDirOperator's view-selection in there
    const QSignalBlocker blocker(m_locationEdit);

    bool dummyExists = m_dummyAdded;

    int cursorPosition = m_locationEdit->lineEdit()->cursorPosition();

    if (m_dummyAdded) {
        if (!icon.isNull()) {
            m_locationEdit->setItemIcon(0, icon);
            m_locationEdit->setItemText(0, text);
        } else {
            if (!usePreviousPixmapIfNull) {
                m_locationEdit->setItemIcon(0, QPixmap());
            }
            m_locationEdit->setItemText(0, text);
        }
    } else {
        if (!text.isEmpty()) {
            if (!icon.isNull()) {
                m_locationEdit->insertItem(0, icon, text);
            } else {
                if (!usePreviousPixmapIfNull) {
                    m_locationEdit->insertItem(0, QPixmap(), text);
                } else {
                    m_locationEdit->insertItem(0, text);
                }
            }
            m_dummyAdded = true;
            dummyExists = true;
        }
    }

    if (dummyExists && !text.isEmpty()) {
        m_locationEdit->setCurrentIndex(0);
    }

    m_locationEdit->lineEdit()->setCursorPosition(cursorPosition);
}

void KFileWidgetPrivate::removeDummyHistoryEntry()
{
    if (!m_dummyAdded) {
        return;
    }

    // Block m_locationEdit signals as setCurrentItem() will cause textChanged() to get
    // emitted, so slotLocationChanged() will be called. Make sure we don't clear the
    // KDirOperator's view-selection in there
    const QSignalBlocker blocker(m_locationEdit);

    if (m_locationEdit->count()) {
        m_locationEdit->removeItem(0);
    }
    m_locationEdit->setCurrentIndex(-1);
    m_dummyAdded = false;
}

void KFileWidgetPrivate::setLocationText(const QUrl &url)
{
    if (!url.isEmpty()) {
        if (!url.isRelative()) {
            const QUrl directory = url.adjusted(QUrl::RemoveFilename);
            if (!directory.path().isEmpty()) {
                q->setUrl(directory, false);
            } else {
                q->setUrl(url, false);
            }
        }

        const QIcon mimeTypeIcon = QIcon::fromTheme(KIO::iconNameForUrl(url), QIcon::fromTheme(QStringLiteral("application-octet-stream")));
        setDummyHistoryEntry(url.fileName(), mimeTypeIcon);
    } else {
        removeDummyHistoryEntry();
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

static QString escapeDoubleQuotes(QString && path) {
    // First escape the escape character that we are using
    path.replace(QStringLiteral("\\"), QStringLiteral("\\\\"));
    // Second, escape the quotes
    path.replace(QStringLiteral("\""), QStringLiteral("\\\""));
    return path;
}

void KFileWidgetPrivate::setLocationText(const QList<QUrl> &urlList)
{
    const QUrl currUrl = m_ops->url();

    if (urlList.count() > 1) {
        QString urls;
        for (const QUrl &url : urlList) {
            urls += QStringLiteral("\"%1\" ").arg(
                escapeDoubleQuotes(relativePathOrUrl(currUrl, url))
            );
        }
        urls.chop(1);

        setDummyHistoryEntry(urls, QIcon(), false);
    } else if (urlList.count() == 1) {
        const QIcon mimeTypeIcon = QIcon::fromTheme(KIO::iconNameForUrl(urlList[0]), QIcon::fromTheme(QStringLiteral("application-octet-stream")));
        setDummyHistoryEntry(
            escapeDoubleQuotes(relativePathOrUrl(currUrl, urlList[0])),
            mimeTypeIcon
        );
    } else {
        removeDummyHistoryEntry();
    }

    if (m_operationMode == KFileWidget::Saving) {
        setNonExtSelection();
    }
}

void KFileWidgetPrivate::updateLocationWhatsThis()
{
    QString whatsThisText;
    if (m_operationMode == KFileWidget::Saving) {
        whatsThisText = QLatin1String("<qt>") + i18n("This is the name to save the file as.") +
                        i18n(autocompletionWhatsThisText);
    } else if (m_ops->mode() & KFile::Files) {
        whatsThisText = QLatin1String("<qt>") + i18n("This is the list of files to open. More than "
                                      "one file can be specified by listing several "
                                      "files, separated by spaces.") +
                        i18n(autocompletionWhatsThisText);
    } else {
        whatsThisText = QLatin1String("<qt>") + i18n("This is the name of the file to open.") +
                        i18n(autocompletionWhatsThisText);
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
    QObject::connect(m_placesView, &KFilePlacesView::urlChanged, q, [this](const QUrl &url) { enterUrl(url); });

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

    QObject::connect(m_placesDock, &QDockWidget::visibilityChanged,
                     q, [this](bool visible) { togglePlacesPanel(visible, m_placesDock); });
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

void KFileWidgetPrivate::setLafBoxColumnWidth()
{
    // In order to perfectly align the filename widget with KDirOperator's icon view
    // - m_placesViewWidth needs to account for the size of the splitter handle
    // - the m_lafBox grid layout spacing should only affect the label, but not the line edit
    const int adjustment = m_placesViewSplitter->handleWidth() - m_lafBox->horizontalSpacing();
    m_lafBox->setColumnMinimumWidth(0, m_placesViewWidth + adjustment);
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

    QObject::connect(m_placesViewSplitter, &QSplitter::splitterMoved,
                     q, [this](int pos, int index) { placesViewSplitterMoved(pos, index); });
    m_placesViewSplitter->insertWidget(0, m_opsWidget);

    m_vbox = new QVBoxLayout();
    m_vbox->setContentsMargins(0, 0, 0, 0);
    m_boxLayout->addLayout(m_vbox);

    m_lafBox = new QGridLayout();

    m_lafBox->addWidget(m_locationLabel, 0, 0, Qt::AlignVCenter | Qt::AlignRight);
    m_lafBox->addWidget(m_locationEdit, 0, 1, Qt::AlignVCenter);
    m_lafBox->addWidget(m_okButton, 0, 2, Qt::AlignVCenter);

    m_lafBox->addWidget(m_filterLabel, 1, 0, Qt::AlignVCenter | Qt::AlignRight);
    m_lafBox->addWidget(m_filterWidget, 1, 1, Qt::AlignVCenter);
    m_lafBox->addWidget(m_cancelButton, 1, 2, Qt::AlignVCenter);

    m_lafBox->setColumnStretch(1, 4);

    m_vbox->addLayout(m_lafBox);

    // add the Automatically Select Extension checkbox
    m_vbox->addWidget(m_autoSelectExtCheckBox);

    q->setTabOrder(m_ops, m_autoSelectExtCheckBox);
    q->setTabOrder(m_autoSelectExtCheckBox, m_locationEdit);
    q->setTabOrder(m_locationEdit, m_filterWidget);
    q->setTabOrder(m_filterWidget, m_okButton);
    q->setTabOrder(m_okButton, m_cancelButton);
    q->setTabOrder(m_cancelButton, m_urlNavigator);
    q->setTabOrder(m_urlNavigator, m_ops);
}

void KFileWidgetPrivate::slotFilterChanged()
{
//     qDebug();

    m_filterDelayTimer.stop();

    QString filter = m_filterWidget->currentFilter();
    m_ops->clearFilter();

    if (filter.contains(QLatin1Char('/'))) {
        QStringList types = filter.split(QLatin1Char(' '), Qt::SkipEmptyParts);
        types.prepend(QStringLiteral("inode/directory"));
        m_ops->setMimeFilter(types);
    } else if (filter.contains(QLatin1Char('*')) || filter.contains(QLatin1Char('?')) || filter.contains(QLatin1Char('['))) {
        m_ops->setNameFilter(filter);
    } else {
        m_ops->setNameFilter(QLatin1Char('*') + filter.replace(QLatin1Char(' '), QLatin1Char('*')) + QLatin1Char('*'));
    }

    updateAutoSelectExtension();

    m_ops->updateDir();

    Q_EMIT q->filterChanged(filter);
}

void KFileWidget::setUrl(const QUrl &url, bool clearforward)
{
//     qDebug();

    d->m_ops->setUrl(url, clearforward);
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

    // is trigged in ctor before completion object is set
    KUrlCompletion *completion = dynamic_cast<KUrlCompletion *>(m_locationEdit->completionObject());
    if (completion) {
        completion->setDir(url);
    }

    if (m_placesView) {
        m_placesView->setUrl(url);
    }
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
    if (!u.path().isEmpty() && !u.path().endsWith(QLatin1Char('/'))) {
        u.setPath(u.path() + QLatin1Char('/'));
    }
    q->setUrl(u);

    // We need to check window()->focusWidget() instead of m_locationEdit->hasFocus
    // because when the window is showing up m_locationEdit
    // may still not have focus but it'll be the one that will have focus when the window
    // gets it and we don't want to steal its focus either
    if (q->window()->focusWidget() != m_locationEdit) {
        m_ops->setFocus();
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
                  i18n("The file \"%1\" already exists. Do you wish to overwrite it?",
                       url.fileName()), i18n("Overwrite File?"), KStandardGuiItem::overwrite(),
                  KStandardGuiItem::cancel(), QString(), KMessageBox::Notify | KMessageBox::Dangerous);

        if (ret != KMessageBox::Continue) {
            return false;
        }
        return true;
    }

    return true;
}

#if KIOFILEWIDGETS_BUILD_DEPRECATED_SINCE(5, 33)
void KFileWidget::setSelection(const QString &url)
{
//     qDebug() << "setSelection " << url;

    if (url.isEmpty()) {
        return;
    }

    QUrl u = d->getCompleteUrl(url);
    if (!u.isValid()) {
        // Relative path was treated as URL, but it was found to be invalid.
        qWarning() << url << " is not a correct argument for setSelection!";
        return;
    }

    setSelectedUrl(urlFromString(url));
}
#endif

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
    if (currentText.startsWith(QLatin1Char('/')))
        u.setPath(currentText);
    else
        u.setPath(concatPaths(m_ops->url().path(), currentText));
    m_ops->setCurrentItem(u);
    m_ops->blockSignals(false);
}

void KFileWidgetPrivate::fileCompletion(const QString &match)
{
//     qDebug();

    if (match.isEmpty() || m_locationEdit->currentText().contains(QLatin1Char('"'))) {
        return;
    }

    const QUrl url = urlFromString(match);
    const QIcon mimeTypeIcon = QIcon::fromTheme(KIO::iconNameForUrl(url), QIcon::fromTheme(QStringLiteral("application-octet-stream")));
    setDummyHistoryEntry(m_locationEdit->currentText(), mimeTypeIcon, !m_locationEdit->currentText().isEmpty());
}

void KFileWidgetPrivate::slotLocationChanged(const QString &text)
{
//     qDebug();

    m_locationEdit->lineEdit()->setModified(true);

    if (text.isEmpty() && m_ops->view()) {
        m_ops->view()->clearSelection();
    }

    if (text.isEmpty()) {
        removeDummyHistoryEntry();
    } else {
        setDummyHistoryEntry(text);
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
    QUrl u(m_ops->url().adjusted(QUrl::RemoveFilename));
    if (!u.path().endsWith(QLatin1Char('/'))) {
        u.setPath(u.path() + QLatin1Char('/'));
    }

    // A helper that creates, validates and appends a new url based
    // on the given filename.
    auto addUrl = [u, &urls](const QString &partial_name)
    {
        if (partial_name.trimmed().isEmpty()) {
            return;
        }

        // We have to use setPath here, so that something like "test#file"
        // isn't interpreted to have path "test" and fragment "file".
        QUrl partial_url;
        partial_url.setPath(partial_name);

        // This returns QUrl(partial_name) for absolute URLs.
        // Otherwise, returns the concatenated url.
        const QUrl finalUrl = u.resolved(partial_url);

        if (finalUrl.isValid()) {
            urls.append(finalUrl);
        } else {
            // This can happen in the first quote! (ex: ' "something here"')
            qCDebug(KIO_KFILEWIDGETS_FW) << "Discarding Invalid" << finalUrl;
        }
    };

    // An iterative approach here where we toggle the "escape" flag
    // if we hit `\`. If we hit `"` and the escape flag is false,
    // we split
    QString partial_name;
    bool escape = false;
    for(int i = 0; i < line.length(); i++) {
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
        if (ch.toLatin1() == '"') {
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
            KMessageBox::sorry(const_cast<KFileWidget *>(this),
                               i18n("You can only select local files."),
                               i18n("Remote Files Not Accepted"));
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
            QList<QUrl>::const_iterator it = urls.begin();
            while (it != urls.end()) {
                QUrl url = d->mostLocalUrl(*it);
                if (url.isLocalFile()) {
                    list.append(url.toLocalFile());
                }
                ++it;
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
    if (!d->m_hasView) {   // delayed view-creation
        Q_ASSERT(d);
        Q_ASSERT(d->m_ops);
        d->m_ops->setView(KFile::Default);
        d->m_ops->view()->setSizePolicy(QSizePolicy(QSizePolicy::Maximum, QSizePolicy::Maximum));
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
    if (watched == d->m_iconSizeSlider && keyEvent) {
        if (keyEvent->key() == Qt::Key_Left || keyEvent->key() == Qt::Key_Up ||
                keyEvent->key() == Qt::Key_Right || keyEvent->key() == Qt::Key_Down) {
            d->slotIconSizeSliderMoved(d->m_iconSizeSlider->value());
        }
    } else if (watched == d->m_locationEdit && event->type() == QEvent::KeyPress) {
        if (keyEvent->modifiers() & Qt::AltModifier) {
            switch (keyEvent->key()) {
            case Qt::Key_Up:
                d->m_ops->actionCollection()->action(QStringLiteral("up"))->trigger();
                break;
            case Qt::Key_Left:
                d->m_ops->actionCollection()->action(QStringLiteral("back"))->trigger();
                break;
            case Qt::Key_Right:
                d->m_ops->actionCollection()->action(QStringLiteral("forward"))->trigger();
                break;
            default:
                break;
            }
        }
    } else if (watched == d->m_ops && event->type() == QEvent::KeyPress &&
               (keyEvent->key() == Qt::Key_Return || keyEvent->key() == Qt::Key_Enter)) {
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
        d->m_filterWidget->setDefaultFilter(i18n("*|All Folders"));
    } else {
        d->m_filterWidget->setDefaultFilter(i18n("*|All Files"));
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

    KCompletion::CompletionMode cm = (KCompletion::CompletionMode)
                                     m_configGroup.readEntry(PathComboCompletionMode,
                                             static_cast<int>(KCompletion::CompletionPopup));
    if (cm != KCompletion::CompletionPopup) {
        combo->setCompletionMode(cm);
    }

    cm = (KCompletion::CompletionMode)
         m_configGroup.readEntry(LocationComboCompletionMode,
                               static_cast<int>(KCompletion::CompletionPopup));
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
    //saveDialogSize( tmpGroup, KConfigGroup::Persistent | KConfigGroup::Global );
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
    m_locationEdit->setUrls(m_configGroup.readPathEntry(RecentFiles, QStringList()), KUrlComboBox::RemoveBottom);
    m_locationEdit->setCurrentIndex(-1);
    m_locationEdit->blockSignals(oldState);

    KUrlComboBox *combo = m_urlNavigator->editor();
    combo->setUrls(m_configGroup.readPathEntry(RecentURLs, QStringList()), KUrlComboBox::RemoveTop);
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
    m_configGroup.writePathEntry(RecentFiles, m_locationEdit->urls());

    KUrlComboBox *pathCombo = m_urlNavigator->editor();
    m_configGroup.writePathEntry(RecentURLs, pathCombo->urls());
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
        actionCollection()->removeAction(actionCollection()->action(QStringLiteral("mkdir")));
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
        setLafBoxColumnWidth();
    }
}

void KFileWidgetPrivate::activateUrlNavigator()
{
//     qDebug();

    QLineEdit* lineEdit = m_urlNavigator->editor()->lineEdit();

    // If the text field currently has focus and everything is selected,
    // pressing the keyboard shortcut returns the whole thing to breadcrumb mode
    if (m_urlNavigator->isUrlEditable()
        && lineEdit->hasFocus()
        && lineEdit->selectedText() == lineEdit->text() ) {
        m_urlNavigator->setUrlEditable(false);
    } else {
        m_urlNavigator->setUrlEditable(true);
        m_urlNavigator->setFocus();
        lineEdit->selectAll();
    }
}

void KFileWidgetPrivate::zoomOutIconsSize()
{
    const int currValue = m_ops->iconSize();

    // Jump to the nearest standard size
    auto r_itEnd = m_stdIconSizes.crend();
    auto it = std::find_if(m_stdIconSizes.crbegin(), r_itEnd,
                 [currValue](KIconLoader::StdSizes size) { return size < currValue; });

    Q_ASSERT(it != r_itEnd);

    const int nearestSize = *it;

    m_iconSizeSlider->setValue(nearestSize);
    slotIconSizeSliderMoved(nearestSize);
}

void KFileWidgetPrivate::zoomInIconsSize()
{
    const int currValue = m_ops->iconSize();

    // Jump to the nearest standard size
    auto itEnd = m_stdIconSizes.cend();
    auto it = std::find_if(m_stdIconSizes.cbegin(), itEnd,
                 [currValue](KIconLoader::StdSizes size) { return size > currValue; });

    Q_ASSERT(it != itEnd);

    const int nearestSize = *it;

    m_iconSizeSlider->setValue(nearestSize);
    slotIconSizeSliderMoved(nearestSize);
}

void KFileWidgetPrivate::slotIconSizeChanged(int _value)
{
    m_ops->setIconSize(_value);

    switch (_value) {
    case KIconLoader::SizeSmall:
    case KIconLoader::SizeSmallMedium:
    case KIconLoader::SizeMedium:
    case KIconLoader::SizeLarge:
    case KIconLoader::SizeHuge:
    case KIconLoader::SizeEnormous:
        m_iconSizeSlider->setToolTip(i18n("Icon size: %1 pixels (standard size)", _value));
        break;
    default:
        m_iconSizeSlider->setToolTip(i18n("Icon size: %1 pixels", _value));
        break;
    }
}

void KFileWidgetPrivate::slotIconSizeSliderMoved(int _value)
{
    // Force this to be called in case this slot is called first on the
    // slider move.
    slotIconSizeChanged(_value);

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
    for (QStringList::ConstIterator it = patternList.begin();
            it != patternListEnd;
            ++it) {
//         qDebug() << "\t\ttry: \'" << (*it) << "\'";

        // is this pattern like "*.BMP" rather than useless things like:
        //
        // README
        // *.
        // *.*
        // *.JP*G
        // *.JP?
        // *.[Jj][Pp][Gg]
        if ((*it).startsWith(QLatin1String("*.")) &&
                (*it).length() > 2 &&
                (*it).indexOf(QLatin1Char('*'), 2) < 0 &&
                (*it).indexOf(QLatin1Char('?'), 2) < 0 &&
                (*it).indexOf(QLatin1Char('['), 2) < 0 &&
                (*it).indexOf(QLatin1Char(']'), 2) < 0) {
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

//QString KFileWidget::currentFilterExtension()
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

        QString filter = m_filterWidget->currentFilter();
        if (!filter.isEmpty()) {
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
            if (filter.indexOf(QLatin1Char('/')) < 0) {
                extensionList = filter.split(QLatin1Char(' '), Qt::SkipEmptyParts);
                defaultExtension = getExtensionFromPatternList(extensionList);
            }
            // e.g. "text/html"
            else {
                QMimeType mime = db.mimeTypeForName(filter);
                if (mime.isValid()) {
                    extensionList = mime.globPatterns();
                    defaultExtension = mime.preferredSuffix();
                    if (!defaultExtension.isEmpty()) {
                        defaultExtension.prepend(QLatin1Char('.'));
                    }
                }
            }

            if ((!currentExtension.isEmpty() && extensionList.contains(QLatin1String("*.") + currentExtension))
                    || filter == QLatin1String("application/octet-stream")) {
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
            m_autoSelectExtCheckBox->setText(i18n("Automatically select filename e&xtension (%1)",  m_extension));
            whatsThisExtension = i18n("the extension <b>%1</b>",  m_extension);

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
        m_autoSelectExtCheckBox->setWhatsThis(QLatin1String("<qt>") +
                                            i18n(
                                                "This option enables some convenient features for "
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
                                                "files more manageable."
                                                ,
                                                locationLabelText,
                                                locationLabelText,
                                                whatsThisExtension)
                                            + QLatin1String("</qt>")
                                           );

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

    QString urlStr = locationEditCurrentText();
    if (urlStr.isEmpty()) {
        return;
    }

    QUrl url = getCompleteUrl(urlStr);
//     qDebug() << "updateLocationEditExtension (" << url << ")";

    const int fileNameOffset = urlStr.lastIndexOf(QLatin1Char('/')) + 1;
    QString fileName = urlStr.mid(fileNameOffset);

    const int dot = fileName.lastIndexOf(QLatin1Char('.'));
    const int len = fileName.length();
    if (dot > 0 && // has an extension already and it's not a hidden file
            // like ".hidden" (but we do accept ".hidden.ext")
            dot != len - 1 // and not deliberately suppressing extension
       ) {
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
        if (lastExtension.length() && fileName.endsWith(lastExtension)) {
            fileName.chop(lastExtension.length());
        } else if (m_extension.length() && fileName.endsWith(m_extension)) {
            fileName.chop(m_extension.length());
        }
        // can only handle "single extensions"
        else {
            fileName.truncate(dot);
        }

        // add extension
        const QString newText = urlStr.leftRef(fileNameOffset) + fileName + m_extension;
        if (newText != locationEditCurrentText()) {
            m_locationEdit->setItemText(m_locationEdit->currentIndex(), newText);
            m_locationEdit->lineEdit()->setModified(true);
        }
    }
}

QString KFileWidgetPrivate::findMatchingFilter(const QString &filter, const QString &filename) const
{
     // e.g.: '*.foo *.bar|Foo type' -> '*.foo', '*.bar'
    const QStringList patterns = filter.left(filter.indexOf(QLatin1Char('|'))).split(QLatin1Char(' '),
                                             Qt::SkipEmptyParts);

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
// (this prevents you from accidently saving "file.kwd" as RTF, for example)
void KFileWidgetPrivate::updateFilter()
{
//     qDebug();

    if ((m_operationMode == KFileWidget::Saving) && (m_ops->mode() & KFile::File)) {
        QString urlStr = locationEditCurrentText();
        if (urlStr.isEmpty()) {
            return;
        }

        if (m_filterWidget->isMimeFilter()) {
            QMimeDatabase db;
            QMimeType mime = db.mimeTypeForFile(urlStr, QMimeDatabase::MatchExtension);
            if (mime.isValid() && !mime.isDefault()) {
                if (m_filterWidget->currentFilter() != mime.name() &&
                        m_filterWidget->filters().indexOf(mime.name()) != -1) {
                    m_filterWidget->setCurrentFilter(mime.name());
                }
            }
        } else {
            QString filename = urlStr.mid(urlStr.lastIndexOf(QLatin1Char('/')) + 1);     // only filename
            // accept any match to honor the user's selection; see later code handling the "*" match
            if (!findMatchingFilter(m_filterWidget->currentFilter(), filename).isEmpty()) {
                return;
            }
            const QStringList list = m_filterWidget->filters();
            for (const QString &filter : list) {
                QString match = findMatchingFilter(filter, filename);
                if (!match.isEmpty()) {
                    if (match != QLatin1String("*")) {   // never match the catch-all filter
                        m_filterWidget->setCurrentFilter(filter);
                    }
                    return; // do not repeat, could match a later filter
                }
            }
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
    const bool unspecifiedExtension = (dot <= 0);

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
    //don't add more than we need. KRecentDocument::add() is pretty slow

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

KActionCollection *KFileWidget::actionCollection() const
{
    return d->m_ops->actionCollection();
}

void KFileWidgetPrivate::togglePlacesPanel(bool show, QObject *sender)
{
    if (show) {
        initPlacesPanel();
        m_placesDock->show();
        setLafBoxColumnWidth();

        // check to see if they have a home item defined, if not show the home button
        QUrl homeURL;
        homeURL.setPath(QDir::homePath());
        KFilePlacesModel *model = static_cast<KFilePlacesModel *>(m_placesView->model());
        for (int rowIndex = 0; rowIndex < model->rowCount(); rowIndex++) {
            QModelIndex index = model->index(rowIndex, 0);
            QUrl url = model->url(index);

            if (homeURL.matches(url, QUrl::StripTrailingSlash)) {
                m_toolbar->removeAction(m_ops->actionCollection()->action(QStringLiteral("home")));
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

        QAction *homeAction = m_ops->actionCollection()->action(QStringLiteral("home"));
        QAction *reloadAction = m_ops->actionCollection()->action(QStringLiteral("reload"));
        if (!m_toolbar->actions().contains(homeAction)) {
            m_toolbar->insertAction(reloadAction, homeAction);
        }

        // reset the lafbox to not follow the width of the splitter
        m_lafBox->setColumnMinimumWidth(0, 0);
    }

    static_cast<KToggleAction *>(q->actionCollection()->action(QStringLiteral("togglePlacesPanel")))->setChecked(show);

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
        q->connect(m_bookmarkHandler, &KFileBookmarkHandler::openUrl, q, [this](const QString &path) { enterUrl(path); });
        m_bookmarkButton->setMenu(m_bookmarkHandler->menu());
    } else if (m_bookmarkHandler) {
        m_bookmarkButton->setMenu(nullptr);
        delete m_bookmarkHandler;
        m_bookmarkHandler = nullptr;
    }

    if (m_bookmarkButton) {
        m_bookmarkButton->setVisible(show);
    }

    static_cast<KToggleAction *>(q->actionCollection()->action(QStringLiteral("toggleBookmarks")))->setChecked(show);
}

// static, overloaded
QUrl KFileWidget::getStartUrl(const QUrl &startDir,
                              QString &recentDirClass)
{
    QString fileName;                   // result discarded
    return getStartUrl(startDir, recentDirClass, fileName);
}

// static, overloaded
QUrl KFileWidget::getStartUrl(const QUrl &startDir,
                              QString &recentDirClass,
                              QString &fileName)
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
            if (urlDir == QLatin1String("/")) {         // '1'..'4' above
                keyword = urlFile;
                fileName.clear();
            } else {                // '5' or '6' above
                keyword = urlDir.mid(1);
                fileName = urlFile;
            }

            if (startDir.query() == QLatin1String("global")) {
                recentDirClass = QStringLiteral("::%1").arg(keyword);
            } else {
                recentDirClass = QStringLiteral(":%1").arg(keyword);
            }

            ret = QUrl::fromLocalFile(KRecentDirs::dir(recentDirClass));
        } else {                    // not special "kfiledialog" URL
            // "foo.png" only gives us a file name, the default start dir will be used.
            // "file:foo.png" (from KHTML/webkit, due to fromPath()) means the same
            //   (and is the reason why we don't just use QUrl::isRelative()).

            // In all other cases (startDir contains a directory path, or has no
            // fileName for us anyway, such as smb://), startDir is indeed a dir url.

            if (!startDir.adjusted(QUrl::RemoveFilename | QUrl::StripTrailingSlash).path().isEmpty() ||
                    startDir.fileName().isEmpty()) {
                // can use start directory
                ret = startDir;             // will be checked by stat later
                // If we won't be able to list it (e.g. http), then use default
                if (!KProtocolManager::supportsListing(ret)) {
                    useDefaultStartDir = true;
                    fileName = startDir.fileName();
                }
            } else {                // file name only
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
            if (lastDirectory()->adjusted(QUrl::StripTrailingSlash) == home.adjusted(QUrl::StripTrailingSlash) ||
                    QDir::currentPath() != QDir::homePath() ||
                    !QDir(lastDirectory()->toLocalFile()).exists()) {
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
    QString label;
    QString whatsThisText;

    if (m_operationMode == KFileWidget::Saving && m_filterWidget->isMimeFilter()) {
        label = i18n("&File type:");
        whatsThisText = i18n("<qt>This is the file type selector. It is used to select the format that the file will be saved as.</qt>");
    } else {
        label = i18n("&Filter:");
        whatsThisText = i18n("<qt>This is the filter to apply to the file list. "
                         "File names that do not match the filter will not be shown.<p>"
                         "You may select from one of the preset filters in the "
                         "drop down menu, or you may enter a custom filter "
                         "directly into the text area.</p><p>"
                         "Wildcards such as * and ? are allowed.</p></qt>");
    }

    if (m_filterLabel) {
        m_filterLabel->setText(label);
        m_filterLabel->setWhatsThis(whatsThisText);
    }
    if (m_filterWidget) {
        m_filterWidget->setWhatsThis(whatsThisText);
    }
}

#if KIOFILEWIDGETS_BUILD_DEPRECATED_SINCE(5, 66)
KToolBar *KFileWidget::toolBar() const
{
    return d->m_toolbar;
}
#endif

void KFileWidget::setCustomWidget(QWidget *widget)
{
    delete d->m_bottomCustomWidget;
    d->m_bottomCustomWidget = widget;

    // add it to the dialog, below the filter list box.

    // Change the parent so that this widget is a child of the main widget
    d->m_bottomCustomWidget->setParent(this);

    d->m_vbox->addWidget(d->m_bottomCustomWidget);
    //d->m_vbox->addSpacing(3); // can't do this every time...

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
    d->m_lafBox->addWidget(label, 2, 0, Qt::AlignVCenter);
    d->m_lafBox->addWidget(widget, 2, 1, Qt::AlignVCenter);
}

KDirOperator *KFileWidget::dirOperator()
{
    return d->m_ops;
}

void KFileWidget::readConfig(KConfigGroup &group)
{
    d->m_configGroup = group;
    d->readViewConfig();
    d->readRecentFiles();
}

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
    QSize screenSize = QApplication::desktop()->availableGeometry(this).size();
    QSize minSize(screenSize / 2);
    QSize maxSize(screenSize * qreal(0.9));
    return (goodSize.expandedTo(minSize).boundedTo(maxSize));
}

void KFileWidget::setViewMode(KFile::FileView mode)
{
    d->m_ops->setView(mode);
    d->m_hasView = true;
}

void KFileWidget::setSupportedSchemes(const QStringList &schemes)
{
    d->m_model->setSupportedSchemes(schemes);
    d->m_ops->setSupportedSchemes(schemes);
    d->m_urlNavigator->setCustomProtocols(schemes);
}

QStringList KFileWidget::supportedSchemes() const
{
    return d->m_model->supportedSchemes();
}

#include "moc_kfilewidget.cpp"
