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

#include "kfileplacesview.h"
#include "kfileplacesmodel.h"
#include "kfilebookmarkhandler_p.h"
#include "kurlcombobox.h"
#include "kurlnavigator.h"
#include "kfilepreviewgenerator.h"
#include "kfilewidgetdocktitlebar_p.h"
#include <config-kiofilewidgets.h>
#include <defaults-kfile.h>

#include <KActionCollection>
#include <KActionMenu>
#include <KConfigGroup>
#include <kdiroperator.h>
#include <kfilefiltercombo.h>
#include <kimagefilepreview.h>
#include <krecentdocument.h>
#include <KToolBar>
#include <kurlcompletion.h>
#include <kprotocolmanager.h>
#include <kio/job.h>
#include <kio/jobuidelegate.h>
#include <kio/scheduler.h>
#include <krecentdirs.h>
#include <QDebug>
#include <KLocalizedString>
#include <kfileitemdelegate.h>
#include <KSharedConfig>

#include <QCheckBox>
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
    explicit KFileWidgetPrivate(KFileWidget *widget)
        : q(widget),
          boxLayout(nullptr),
          placesDock(nullptr),
          placesView(nullptr),
          placesViewSplitter(nullptr),
          placesViewWidth(-1),
          labeledCustomWidget(nullptr),
          bottomCustomWidget(nullptr),
          autoSelectExtCheckBox(nullptr),
          operationMode(KFileWidget::Opening),
          bookmarkHandler(nullptr),
          toolbar(nullptr),
          locationEdit(nullptr),
          ops(nullptr),
          filterWidget(nullptr),
          autoSelectExtChecked(false),
          keepLocation(false),
          hasView(false),
          hasDefaultFilter(false),
          inAccept(false),
          dummyAdded(false),
          confirmOverwrite(false),
          differentHierarchyLevelItemsEntered(false),
          iconSizeSlider(nullptr),
          zoomOutAction(nullptr),
          zoomInAction(nullptr)
    {
    }

    ~KFileWidgetPrivate()
    {
        delete bookmarkHandler; // Should be deleted before ops!
        delete ops;
    }

    void updateLocationWhatsThis();
    void updateAutoSelectExtension();
    void initSpeedbar();
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
     * handles setting the locationEdit.
     */
    void multiSelectionChanged();

    /**
     * Returns the absolute version of the URL specified in locationEdit.
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
    void _k_slotLocationChanged(const QString &);
    void _k_urlEntered(const QUrl &);
    void _k_enterUrl(const QUrl &);
    void _k_enterUrl(const QString &);
    void _k_locationAccepted(const QString &);
    void _k_slotFilterChanged();
    void _k_fileHighlighted(const KFileItem &);
    void _k_fileSelected(const KFileItem &);
    void _k_slotLoadingFinished();
    void _k_fileCompletion(const QString &);
    void _k_toggleSpeedbar(bool);
    void _k_toggleBookmarks(bool);
    void _k_slotAutoSelectExtClicked();
    void _k_placesViewSplitterMoved(int, int);
    void _k_activateUrlNavigator();
    void _k_zoomOutIconsSize();
    void _k_zoomInIconsSize();
    void _k_slotIconSizeSliderMoved(int);
    void _k_slotIconSizeChanged(int);
    void _k_slotViewDoubleClicked(const QModelIndex&);
    void _k_slotViewKeyEnterReturnPressed();

    void addToRecentDocuments();

    QString locationEditCurrentText() const;

    /**
     * KIO::NetAccess::mostLocalUrl local replacement.
     * This method won't show any progress dialogs for stating, since
     * they are very annoying when stating.
     */
    QUrl mostLocalUrl(const QUrl &url);

    void setInlinePreviewShown(bool show);

    KFileWidget * const q;

    // the last selected url
    QUrl url;

    // now following all kind of widgets, that I need to rebuild
    // the geometry management
    QBoxLayout *boxLayout;
    QGridLayout *lafBox;
    QVBoxLayout *vbox;

    QLabel *locationLabel;
    QWidget *opsWidget;
    QWidget *pathSpacer;

    QLabel *filterLabel;
    KUrlNavigator *urlNavigator;
    QPushButton *okButton, *cancelButton;
    QDockWidget *placesDock;
    KFilePlacesView *placesView;
    QSplitter *placesViewSplitter;
    // caches the places view width. This value will be updated when the splitter
    // is moved. This allows us to properly set a value when the dialog itself
    // is resized
    int placesViewWidth;

    QWidget *labeledCustomWidget;
    QWidget *bottomCustomWidget;

    // Automatically Select Extension stuff
    QCheckBox *autoSelectExtCheckBox;
    QString extension; // current extension for this filter

    QList<KIO::StatJob *> statJobs;

    QList<QUrl> urlList; //the list of selected urls

    KFileWidget::OperationMode operationMode;

    // The file class used for KRecentDirs
    QString fileClass;

    KFileBookmarkHandler *bookmarkHandler;

    KActionMenu *bookmarkButton;

    KToolBar *toolbar;
    KUrlComboBox *locationEdit;
    KDirOperator *ops;
    KFileFilterCombo *filterWidget;
    QTimer filterDelayTimer;

    KFilePlacesModel *model;

    // whether or not the _user_ has checked the above box
    bool autoSelectExtChecked : 1;

    // indicates if the location edit should be kept or cleared when changing
    // directories
    bool keepLocation : 1;

    // the KDirOperators view is set in KFileWidget::show(), so to avoid
    // setting it again and again, we have this nice little boolean :)
    bool hasView : 1;

    bool hasDefaultFilter : 1; // necessary for the operationMode
    bool autoDirectoryFollowing : 1;
    bool inAccept : 1; // true between beginning and end of accept()
    bool dummyAdded : 1; // if the dummy item has been added. This prevents the combo from having a
    // blank item added when loaded
    bool confirmOverwrite : 1;
    bool differentHierarchyLevelItemsEntered;

    const std::array<KIconLoader::StdSizes, 6> stdIconSizes =
            {KIconLoader::SizeSmall, KIconLoader::SizeSmallMedium, KIconLoader::SizeMedium,
             KIconLoader::SizeLarge, KIconLoader::SizeHuge, KIconLoader::SizeEnormous};

    QSlider *iconSizeSlider;
    QAction *zoomOutAction;
    QAction *zoomInAction;

    // The group which stores app-specific settings. These settings are recent
    // files and urls. Visual settings (view mode, sorting criteria...) are not
    // app-specific and are stored in kdeglobals
    KConfigGroup configGroup;
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

    d->okButton = new QPushButton(this);
    KGuiItem::assign(d->okButton, KStandardGuiItem::ok());
    d->okButton->setDefault(true);
    d->cancelButton = new QPushButton(this);
    KGuiItem::assign(d->cancelButton, KStandardGuiItem::cancel());
    // The dialog shows them
    d->okButton->hide();
    d->cancelButton->hide();

    d->opsWidget = new QWidget(this);
    QVBoxLayout *opsWidgetLayout = new QVBoxLayout(d->opsWidget);
    opsWidgetLayout->setContentsMargins(0, 0, 0, 0);
    opsWidgetLayout->setSpacing(0);
    //d->toolbar = new KToolBar(this, true);
    d->toolbar = new KToolBar(d->opsWidget, true);
    d->toolbar->setObjectName(QStringLiteral("KFileWidget::toolbar"));
    d->toolbar->setMovable(false);
    opsWidgetLayout->addWidget(d->toolbar);

    d->model = new KFilePlacesModel(this);

    // Resolve this now so that a 'kfiledialog:' URL, if specified,
    // does not get inserted into the urlNavigator history.
    d->url = getStartUrl(startDir, d->fileClass, filename);
    startDir = d->url;

    // Don't pass startDir to the KUrlNavigator at this stage: as well as
    // the above, it may also contain a file name which should not get
    // inserted in that form into the old-style navigation bar history.
    // Wait until the KIO::stat has been done later.
    //
    // The stat cannot be done before this point, bug 172678.
    d->urlNavigator = new KUrlNavigator(d->model, QUrl(), d->opsWidget); //d->toolbar);
    d->urlNavigator->setPlacesSelectorVisible(false);
    opsWidgetLayout->addWidget(d->urlNavigator);

    d->ops = new KDirOperator(QUrl(), d->opsWidget);
    d->ops->installEventFilter(this);
    d->ops->setObjectName(QStringLiteral("KFileWidget::ops"));
    d->ops->setIsSaving(d->operationMode == Saving);
    d->ops->setNewFileMenuSelectDirWhenAlreadyExist(true);
    opsWidgetLayout->addWidget(d->ops);
    connect(d->ops, SIGNAL(urlEntered(QUrl)),
            SLOT(_k_urlEntered(QUrl)));
    connect(d->ops, SIGNAL(fileHighlighted(KFileItem)),
            SLOT(_k_fileHighlighted(KFileItem)));
    connect(d->ops, SIGNAL(fileSelected(KFileItem)),
            SLOT(_k_fileSelected(KFileItem)));
    connect(d->ops, SIGNAL(finishedLoading()),
            SLOT(_k_slotLoadingFinished()));
    connect(d->ops, SIGNAL(keyEnterReturnPressed()),
            SLOT(_k_slotViewKeyEnterReturnPressed()));

    d->ops->setupMenu(KDirOperator::SortActions |
                      KDirOperator::FileActions |
                      KDirOperator::ViewActions);
    KActionCollection *coll = d->ops->actionCollection();
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

    QAction *goToNavigatorAction = coll->addAction(QStringLiteral("gotonavigator"), this, SLOT(_k_activateUrlNavigator()));
    goToNavigatorAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_L));

    KToggleAction *showSidebarAction =
        new KToggleAction(i18n("Show Places Panel"), this);
    coll->addAction(QStringLiteral("toggleSpeedbar"), showSidebarAction);
    showSidebarAction->setShortcut(QKeySequence(Qt::Key_F9));
    connect(showSidebarAction, SIGNAL(toggled(bool)),
            SLOT(_k_toggleSpeedbar(bool)));

    KToggleAction *showBookmarksAction =
        new KToggleAction(i18n("Show Bookmarks Button"), this);
    coll->addAction(QStringLiteral("toggleBookmarks"), showBookmarksAction);
    connect(showBookmarksAction, SIGNAL(toggled(bool)),
            SLOT(_k_toggleBookmarks(bool)));

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
            d->ops, &KDirOperator::updateSelectionDependentActions);

    d->iconSizeSlider = new QSlider(this);
    d->iconSizeSlider->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Fixed);
    d->iconSizeSlider->setMinimumWidth(40);
    d->iconSizeSlider->setOrientation(Qt::Horizontal);
    d->iconSizeSlider->setMinimum(KIconLoader::SizeSmall);
    d->iconSizeSlider->setMaximum(KIconLoader::SizeEnormous);
    d->iconSizeSlider->installEventFilter(this);
    connect(d->iconSizeSlider, &QAbstractSlider::valueChanged,
            d->ops, &KDirOperator::setIconSize);
    connect(d->iconSizeSlider, SIGNAL(valueChanged(int)),
            this, SLOT(_k_slotIconSizeChanged(int)));
    connect(d->iconSizeSlider, SIGNAL(sliderMoved(int)),
            this, SLOT(_k_slotIconSizeSliderMoved(int)));
    connect(d->ops, &KDirOperator::currentIconSizeChanged, [this](int value) {
        d->iconSizeSlider->setValue(value);
        d->zoomOutAction->setDisabled(value <= d->iconSizeSlider->minimum());
        d->zoomInAction->setDisabled(value >= d->iconSizeSlider->maximum());
    });

    d->zoomOutAction = new QAction(QIcon::fromTheme(QStringLiteral("file-zoom-out")), i18n("Zoom out"), this);
    connect(d->zoomOutAction, SIGNAL(triggered()), SLOT(_k_zoomOutIconsSize()));
    d->zoomInAction = new QAction(QIcon::fromTheme(QStringLiteral("file-zoom-in")), i18n("Zoom in"), this);
    connect(d->zoomInAction, SIGNAL(triggered()), SLOT(_k_zoomInIconsSize()));

    d->bookmarkButton = new KActionMenu(QIcon::fromTheme(QStringLiteral("bookmarks")), i18n("Bookmarks"), this);
    d->bookmarkButton->setPopupMode(QToolButton::InstantPopup);
    coll->addAction(QStringLiteral("bookmark"), d->bookmarkButton);
    d->bookmarkButton->setWhatsThis(i18n("<qt>This button allows you to bookmark specific locations. "
                                        "Click on this button to open the bookmark menu where you may add, "
                                        "edit or select a bookmark.<br /><br />"
                                        "These bookmarks are specific to the file dialog, but otherwise operate "
                                        "like bookmarks elsewhere in KDE.</qt>"));

    QWidget *midSpacer = new QWidget(this);
    midSpacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    d->toolbar->addAction(coll->action(QStringLiteral("back")));
    d->toolbar->addAction(coll->action(QStringLiteral("forward")));
    d->toolbar->addAction(coll->action(QStringLiteral("up")));
    d->toolbar->addAction(coll->action(QStringLiteral("reload")));
    d->toolbar->addSeparator();
    d->toolbar->addAction(coll->action(QStringLiteral("icons view")));
    d->toolbar->addAction(coll->action(QStringLiteral("compact view")));
    d->toolbar->addAction(coll->action(QStringLiteral("details view")));
    d->toolbar->addSeparator();
    d->toolbar->addAction(coll->action(QStringLiteral("inline preview")));
    d->toolbar->addAction(coll->action(QStringLiteral("sorting menu")));
    d->toolbar->addAction(d->bookmarkButton);

    d->toolbar->addWidget(midSpacer);

    d->toolbar->addAction(d->zoomOutAction);
    d->toolbar->addWidget(d->iconSizeSlider);
    d->toolbar->addAction(d->zoomInAction);
    d->toolbar->addSeparator();
    d->toolbar->addAction(coll->action(QStringLiteral("mkdir")));
    d->toolbar->addAction(menu);

    d->toolbar->setToolButtonStyle(Qt::ToolButtonIconOnly);
    d->toolbar->setMovable(false);

    KUrlComboBox *pathCombo = d->urlNavigator->editor();
    KUrlCompletion *pathCompletionObj = new KUrlCompletion(KUrlCompletion::DirCompletion);
    pathCombo->setCompletionObject(pathCompletionObj);
    pathCombo->setAutoDeleteCompletionObject(true);

    connect(d->urlNavigator, SIGNAL(urlChanged(QUrl)),
            this,  SLOT(_k_enterUrl(QUrl)));
    connect(d->urlNavigator, &KUrlNavigator::returnPressed,
            d->ops, QOverload<>::of(&QWidget::setFocus));

    // the Location label/edit
    d->locationLabel = new QLabel(i18n("&Name:"), this);
    d->locationEdit = new KUrlComboBox(KUrlComboBox::Files, true, this);
    d->locationEdit->installEventFilter(this);
    // Properly let the dialog be resized (to smaller). Otherwise we could have
    // huge dialogs that can't be resized to smaller (it would be as big as the longest
    // item in this combo box). (ereslibre)
    d->locationEdit->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLength);
    connect(d->locationEdit, SIGNAL(editTextChanged(QString)),
            SLOT(_k_slotLocationChanged(QString)));

    d->updateLocationWhatsThis();
    d->locationLabel->setBuddy(d->locationEdit);

    KUrlCompletion *fileCompletionObj = new KUrlCompletion(KUrlCompletion::FileCompletion);
    d->locationEdit->setCompletionObject(fileCompletionObj);
    d->locationEdit->setAutoDeleteCompletionObject(true);
    connect(fileCompletionObj, SIGNAL(match(QString)),
            SLOT(_k_fileCompletion(QString)));

    connect(d->locationEdit, SIGNAL(returnPressed(QString)),
            this,  SLOT(_k_locationAccepted(QString)));

    // the Filter label/edit
    d->filterLabel = new QLabel(this);
    d->filterWidget = new KFileFilterCombo(this);
    d->updateFilterText();
    // Properly let the dialog be resized (to smaller). Otherwise we could have
    // huge dialogs that can't be resized to smaller (it would be as big as the longest
    // item in this combo box). (ereslibre)
    d->filterWidget->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLength);
    d->filterLabel->setBuddy(d->filterWidget);
    connect(d->filterWidget, SIGNAL(filterChanged()), SLOT(_k_slotFilterChanged()));

    d->filterDelayTimer.setSingleShot(true);
    d->filterDelayTimer.setInterval(300);
    connect(d->filterWidget, &QComboBox::editTextChanged, &d->filterDelayTimer, QOverload<>::of(&QTimer::start));
    connect(&d->filterDelayTimer, SIGNAL(timeout()), SLOT(_k_slotFilterChanged()));

    // the Automatically Select Extension checkbox
    // (the text, visibility etc. is set in updateAutoSelectExtension(), which is called by readConfig())
    d->autoSelectExtCheckBox = new QCheckBox(this);
    const int spacingHint = style()->pixelMetric(QStyle::PM_DefaultLayoutSpacing);
    d->autoSelectExtCheckBox->setStyleSheet(QStringLiteral("QCheckBox { padding-top: %1px; }").arg(spacingHint));
    connect(d->autoSelectExtCheckBox, SIGNAL(clicked()), SLOT(_k_slotAutoSelectExtClicked()));

    d->initGUI(); // activate GM

    // read our configuration
    KSharedConfig::Ptr config = KSharedConfig::openConfig();
    config->reparseConfiguration(); // grab newly added dirs by other processes (#403524)
    KConfigGroup group(config, ConfigGroup);
    readConfig(group);

    coll->action(QStringLiteral("inline preview"))->setChecked(d->ops->isInlinePreviewShown());
    d->iconSizeSlider->setValue(d->ops->iconSize());

    KFilePreviewGenerator *pg = d->ops->previewGenerator();
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

    d->ops->setUrl(startDir, true);
    d->urlNavigator->setLocationUrl(startDir);
    if (d->placesView) {
        d->placesView->setUrl(startDir);
    }

    // We have a file name either explicitly specified, or have checked that
    // we could stat it and it is not a directory.  Set it.
    if (!filename.isEmpty()) {
        QLineEdit *lineEdit = d->locationEdit->lineEdit();
        // qDebug() << "selecting filename" << filename;
        if (statRes) {
            d->setLocationText(QUrl(filename));
        } else {
            lineEdit->setText(filename);
            // Preserve this filename when clicking on the view (cf _k_fileHighlighted)
            lineEdit->setModified(true);
        }
        lineEdit->selectAll();
    }

    d->locationEdit->setFocus();
}

KFileWidget::~KFileWidget()
{
    KSharedConfig::Ptr config = KSharedConfig::openConfig();
    config->sync();

    delete d;
}

void KFileWidget::setLocationLabel(const QString &text)
{
    d->locationLabel->setText(text);
}

void KFileWidget::setFilter(const QString &filter)
{
    int pos = filter.indexOf(QLatin1Char('/'));

    // Check for an un-escaped '/', if found
    // interpret as a MIME filter.

    if (pos > 0 && filter[pos - 1] != QLatin1Char('\\')) {
#if QT_VERSION < QT_VERSION_CHECK(5, 15, 0)
        QStringList filters = filter.split(QLatin1Char(' '), QString::SkipEmptyParts);
#else
        QStringList filters = filter.split(QLatin1Char(' '), Qt::SkipEmptyParts);
#endif
        setMimeFilter(filters);
        return;
    }

    // Strip the escape characters from
    // escaped '/' characters.

    QString copy(filter);
    for (pos = 0; (pos = copy.indexOf(QLatin1String("\\/"), pos)) != -1; ++pos) {
        copy.remove(pos, 1);
    }

    d->ops->clearFilter();
    d->filterWidget->setFilter(copy);
    d->ops->setNameFilter(d->filterWidget->currentFilter());
    d->ops->updateDir();
    d->hasDefaultFilter = false;
    d->filterWidget->setEditable(true);

    d->updateAutoSelectExtension();
}

QString KFileWidget::currentFilter() const
{
    return d->filterWidget->currentFilter();
}

void KFileWidget::setMimeFilter(const QStringList &mimeTypes,
                                const QString &defaultType)
{
    d->filterWidget->setMimeFilter(mimeTypes, defaultType);

#if QT_VERSION < QT_VERSION_CHECK(5, 15, 0)
    QStringList types = d->filterWidget->currentFilter().split(QLatin1Char(' '), QString::SkipEmptyParts); //QStringList::split(" ", d->filterWidget->currentFilter());
#else
    QStringList types = d->filterWidget->currentFilter().split(QLatin1Char(' '), Qt::SkipEmptyParts); //QStringList::split(" ", d->filterWidget->currentFilter());
#endif
    types.append(QStringLiteral("inode/directory"));
    d->ops->clearFilter();
    d->ops->setMimeFilter(types);
    d->hasDefaultFilter = !defaultType.isEmpty();
    d->filterWidget->setEditable(!d->hasDefaultFilter ||
                                 d->operationMode != Saving);

    d->updateAutoSelectExtension();
    d->updateFilterText();
}

void KFileWidget::clearFilter()
{
    d->filterWidget->setFilter(QString());
    d->ops->clearFilter();
    d->hasDefaultFilter = false;
    d->filterWidget->setEditable(true);

    d->updateAutoSelectExtension();
}

QString KFileWidget::currentMimeFilter() const
{
    int i = d->filterWidget->currentIndex();
    if (d->filterWidget->showsAllTypes() && i == 0) {
        return QString();    // The "all types" item has no MIME type
    }

    return d->filterWidget->filters().at(i);
}

QMimeType KFileWidget::currentFilterMimeType()
{
    QMimeDatabase db;
    return db.mimeTypeForName(currentMimeFilter());
}

void KFileWidget::setPreviewWidget(KPreviewWidgetBase *w)
{
    d->ops->setPreviewWidget(w);
    d->ops->clearHistory();
    d->hasView = true;
}

QUrl KFileWidgetPrivate::getCompleteUrl(const QString &_url) const
{
//     qDebug() << "got url " << _url;

    const QString url = KShell::tildeExpand(_url);
    QUrl u;

    if (isAbsoluteLocalPath(url)) {
        u = QUrl::fromLocalFile(url);
    } else {
        QUrl relativeUrlTest(ops->url());
        relativeUrlTest.setPath(concatPaths(relativeUrlTest.path(), url));
        if (!ops->dirLister()->findByUrl(relativeUrlTest).isNull() ||
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
    KFile::Modes mode = d->ops->mode();

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
    d->urlList.clear();

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
        if (!d->differentHierarchyLevelItemsEntered) {     // avoid infinite recursion. running this
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

            d->ops->setUrl(topMostUrl, true);
            const bool signalsBlocked = d->locationEdit->lineEdit()->blockSignals(true);
            d->locationEdit->lineEdit()->setText(QStringLiteral("\"%1\"").arg(stringList.join(QStringLiteral("\" \""))));
            d->locationEdit->lineEdit()->blockSignals(signalsBlocked);

            d->differentHierarchyLevelItemsEntered = true;
            slotOk();
            return;
        }
        /**
          * end multi relative urls
          */
    } else if (!locationEditCurrentTextList.isEmpty()) {
        // if we are on file or files mode, and we have an absolute url written by
        // the user, convert it to relative
        if (!locationEditCurrentText.isEmpty() && !(mode & KFile::Directory)
            && (isAbsoluteLocalPath(locationEditCurrentText) || containsProtocolSection(locationEditCurrentText))) {

            QString fileName;
            QUrl url = urlFromString(locationEditCurrentText);
            if (d->operationMode == Opening) {
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
            d->ops->setUrl(url, true);
            const bool signalsBlocked = d->locationEdit->lineEdit()->blockSignals(true);
            d->locationEdit->lineEdit()->setText(fileName);
            d->locationEdit->lineEdit()->blockSignals(signalsBlocked);
            slotOk();
            return;
        }
    }

    // restore it
    d->differentHierarchyLevelItemsEntered = false;

    // locationEditCurrentTextList contains absolute paths
    // this is the general loop for the File and Files mode. Obviously we know
    // that the File mode will iterate only one time here
    bool directoryMode = (mode & KFile::Directory);
    bool onlyDirectoryMode = directoryMode && !(mode & KFile::File) && !(mode & KFile::Files);
    QList<QUrl>::ConstIterator it = locationEditCurrentTextList.constBegin();
    bool filesInList = false;
    while (it != locationEditCurrentTextList.constEnd()) {
        QUrl url(*it);

        if (d->operationMode == Saving && !directoryMode) {
            d->appendExtension(url);
        }

        d->url = url;
        KIO::StatJob *statJob = KIO::stat(url, KIO::HideProgressInfo);
        KJobWidgets::setWindow(statJob, this);
        int res = statJob->exec();

        if (!KUrlAuthorized::authorizeUrlAction(QStringLiteral("open"), QUrl(), url)) {
            QString msg = KIO::buildErrorString(KIO::ERR_ACCESS_DENIED, d->url.toDisplayString());
            KMessageBox::error(this, msg);
            return;
        }

        // if we are on local mode, make sure we haven't got a remote base url
        if ((mode & KFile::LocalOnly) && !d->mostLocalUrl(d->url).isLocalFile()) {
            KMessageBox::sorry(this,
                               i18n("You can only select local files"),
                               i18n("Remote files not accepted"));
            return;
        }

        const auto &supportedSchemes = d->model->supportedSchemes();
        if (!supportedSchemes.isEmpty() && !supportedSchemes.contains(d->url.scheme())) {
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
            d->ops->setUrl(url, true);
            const bool signalsBlocked = d->locationEdit->lineEdit()->blockSignals(true);
            d->locationEdit->lineEdit()->setText(QString());
            d->locationEdit->lineEdit()->blockSignals(signalsBlocked);
            return;
        } else if (res && onlyDirectoryMode && !statJob->statResult().isDir()) {
            // if we are given a file when on directory only mode, reject it
            return;
        } else if (!(mode & KFile::ExistingOnly) || res) {
            // if we don't care about ExistingOnly flag, add the file even if
            // it doesn't exist. If we care about it, don't add it to the list
            if (!onlyDirectoryMode || (res && statJob->statResult().isDir())) {
                d->urlList << url;
            }
            filesInList = true;
        } else {
            KMessageBox::sorry(this, i18n("The file \"%1\" could not be found", url.toDisplayString(QUrl::PreferLocalFile)), i18n("Cannot open file"));
            return; // do not emit accepted() if we had ExistingOnly flag and stat failed
        }

        if ((d->operationMode == Saving) && d->confirmOverwrite && !d->toOverwrite(url)) {
            return;
        }

        ++it;
    }

    // if we have reached this point and we didn't return before, that is because
    // we want this dialog to be accepted
    emit accepted();
}

void KFileWidget::accept()
{
    d->inAccept = true;

    *lastDirectory() = d->ops->url();
    if (!d->fileClass.isEmpty()) {
        KRecentDirs::add(d->fileClass, d->ops->url().toString());
    }

    // clear the topmost item, we insert it as full path later on as item 1
    d->locationEdit->setItemText(0, QString());

    const QList<QUrl> list = selectedUrls();
    QList<QUrl>::const_iterator it = list.begin();
    int atmost = d->locationEdit->maxItems(); //don't add more items than necessary
    for (; it != list.end() && atmost > 0; ++it) {
        const QUrl &url = *it;
        // we strip the last slash (-1) because KUrlComboBox does that as well
        // when operating in file-mode. If we wouldn't , dupe-finding wouldn't
        // work.
        QString file = url.isLocalFile() ? url.toLocalFile() : url.toDisplayString();

        // remove dupes
        for (int i = 1; i < d->locationEdit->count(); i++) {
            if (d->locationEdit->itemText(i) == file) {
                d->locationEdit->removeItem(i--);
                break;
            }
        }
        //FIXME I don't think this works correctly when the KUrlComboBox has some default urls.
        //KUrlComboBox should provide a function to add an url and rotate the existing ones, keeping
        //track of maxItems, and we shouldn't be able to insert items as we please.
        d->locationEdit->insertItem(1, file);
        atmost--;
    }

    d->writeViewConfig();
    d->saveRecentFiles();

    d->addToRecentDocuments();

    if (!(mode() & KFile::Files)) { // single selection
        emit fileSelected(d->url);
    }

    d->ops->close();
}

void KFileWidgetPrivate::_k_fileHighlighted(const KFileItem &i)
{
    if ((!i.isNull() && i.isDir()) ||
            (locationEdit->hasFocus() && !locationEdit->currentText().isEmpty())) { // don't disturb
        return;
    }

    const bool modified = locationEdit->lineEdit()->isModified();

    if (!(ops->mode() & KFile::Files)) {
        if (i.isNull()) {
            if (!modified) {
                setLocationText(QUrl());
            }
            return;
        }

        url = i.url();

        if (!locationEdit->hasFocus()) { // don't disturb while editing
            setLocationText(url);
        }

        emit q->fileHighlighted(url);
    } else {
        multiSelectionChanged();
        emit q->selectionChanged();
    }

    locationEdit->lineEdit()->setModified(false);

    // When saving, and when double-click mode is being used, highlight the
    // filename after a file is single-clicked so the user has a chance to quickly
    // rename it if desired
    // Note that double-clicking will override this and overwrite regardless of
    // single/double click mouse setting (see _k_slotViewDoubleClicked() )
    if (operationMode == KFileWidget::Saving) {
        locationEdit->setFocus();
    }
}

void KFileWidgetPrivate::_k_fileSelected(const KFileItem &i)
{
    if (!i.isNull() && i.isDir()) {
        return;
    }

    if (!(ops->mode() & KFile::Files)) {
        if (i.isNull()) {
            setLocationText(QUrl());
            return;
        }
        setLocationText(i.url());
    } else {
        multiSelectionChanged();
        emit q->selectionChanged();
    }

    // Same as above in _k_fileHighlighted(), but for single-click mode
    if (operationMode == KFileWidget::Saving) {
        locationEdit->setFocus();
    } else {
        q->slotOk();
    }
}

// I know it's slow to always iterate thru the whole filelist
// (d->ops->selectedItems()), but what can we do?
void KFileWidgetPrivate::multiSelectionChanged()
{
    if (locationEdit->hasFocus() && !locationEdit->currentText().isEmpty()) { // don't disturb
        return;
    }

    const KFileItemList list = ops->selectedItems();

    if (list.isEmpty()) {
        setLocationText(QUrl());
        return;
    }

    setLocationText(list.urlList());
}

void KFileWidgetPrivate::setDummyHistoryEntry(const QString &text, const QIcon &icon,
        bool usePreviousPixmapIfNull)
{
    // setCurrentItem() will cause textChanged() being emitted,
    // so slotLocationChanged() will be called. Make sure we don't clear
    // the KDirOperator's view-selection in there
    QObject::disconnect(locationEdit, SIGNAL(editTextChanged(QString)),
                        q, SLOT(_k_slotLocationChanged(QString)));

    bool dummyExists = dummyAdded;

    int cursorPosition = locationEdit->lineEdit()->cursorPosition();

    if (dummyAdded) {
        if (!icon.isNull()) {
            locationEdit->setItemIcon(0, icon);
            locationEdit->setItemText(0, text);
        } else {
            if (!usePreviousPixmapIfNull) {
                locationEdit->setItemIcon(0, QPixmap());
            }
            locationEdit->setItemText(0, text);
        }
    } else {
        if (!text.isEmpty()) {
            if (!icon.isNull()) {
                locationEdit->insertItem(0, icon, text);
            } else {
                if (!usePreviousPixmapIfNull) {
                    locationEdit->insertItem(0, QPixmap(), text);
                } else {
                    locationEdit->insertItem(0, text);
                }
            }
            dummyAdded = true;
            dummyExists = true;
        }
    }

    if (dummyExists && !text.isEmpty()) {
        locationEdit->setCurrentIndex(0);
    }

    locationEdit->lineEdit()->setCursorPosition(cursorPosition);

    QObject::connect(locationEdit, SIGNAL(editTextChanged(QString)),
                     q, SLOT(_k_slotLocationChanged(QString)));
}

void KFileWidgetPrivate::removeDummyHistoryEntry()
{
    if (!dummyAdded) {
        return;
    }

    // setCurrentItem() will cause textChanged() being emitted,
    // so slotLocationChanged() will be called. Make sure we don't clear
    // the KDirOperator's view-selection in there
    QObject::disconnect(locationEdit, SIGNAL(editTextChanged(QString)),
                        q, SLOT(_k_slotLocationChanged(QString)));

    if (locationEdit->count()) {
        locationEdit->removeItem(0);
    }
    locationEdit->setCurrentIndex(-1);
    dummyAdded = false;

    QObject::connect(locationEdit, SIGNAL(editTextChanged(QString)),
                     q, SLOT(_k_slotLocationChanged(QString)));
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

    if (operationMode == KFileWidget::Saving) {
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
    const QUrl currUrl = ops->url();

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

    if (operationMode == KFileWidget::Saving) {
        setNonExtSelection();
    }
}

void KFileWidgetPrivate::updateLocationWhatsThis()
{
    QString whatsThisText;
    if (operationMode == KFileWidget::Saving) {
        whatsThisText = QLatin1String("<qt>") + i18n("This is the name to save the file as.") +
                        i18n(autocompletionWhatsThisText);
    } else if (ops->mode() & KFile::Files) {
        whatsThisText = QLatin1String("<qt>") + i18n("This is the list of files to open. More than "
                                      "one file can be specified by listing several "
                                      "files, separated by spaces.") +
                        i18n(autocompletionWhatsThisText);
    } else {
        whatsThisText = QLatin1String("<qt>") + i18n("This is the name of the file to open.") +
                        i18n(autocompletionWhatsThisText);
    }

    locationLabel->setWhatsThis(whatsThisText);
    locationEdit->setWhatsThis(whatsThisText);
}

void KFileWidgetPrivate::initSpeedbar()
{
    if (placesDock) {
        return;
    }

    placesDock = new QDockWidget(i18nc("@title:window", "Places"), q);
    placesDock->setFeatures(QDockWidget::NoDockWidgetFeatures);
    placesDock->setTitleBarWidget(new KDEPrivate::KFileWidgetDockTitleBar(placesDock));

    placesView = new KFilePlacesView(placesDock);
    placesView->setModel(model);
    placesView->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    placesView->setObjectName(QStringLiteral("url bar"));
    QObject::connect(placesView, SIGNAL(urlChanged(QUrl)),
                     q, SLOT(_k_enterUrl(QUrl)));

    // need to set the current url of the urlbar manually (not via urlEntered()
    // here, because the initial url of KDirOperator might be the same as the
    // one that will be set later (and then urlEntered() won't be emitted).
    // TODO: KDE5 ### REMOVE THIS when KDirOperator's initial URL (in the c'tor) is gone.
    placesView->setUrl(url);

    placesDock->setWidget(placesView);
    placesViewSplitter->insertWidget(0, placesDock);

    // initialize the size of the splitter
    placesViewWidth = configGroup.readEntry(SpeedbarWidth, placesView->sizeHint().width());

    // Needed for when the dialog is shown with the places panel initially hidden
    setPlacesViewSplitterSizes();

    QObject::connect(placesDock, SIGNAL(visibilityChanged(bool)),
                     q, SLOT(_k_toggleSpeedbar(bool)));
}

void KFileWidgetPrivate::setPlacesViewSplitterSizes()
{
    if (placesViewWidth > 0) {
        QList<int> sizes = placesViewSplitter->sizes();
        sizes[0] = placesViewWidth;
        sizes[1] = q->width() - placesViewWidth - placesViewSplitter->handleWidth();
        placesViewSplitter->setSizes(sizes);
    }
}

void KFileWidgetPrivate::setLafBoxColumnWidth()
{
    // In order to perfectly align the filename widget with KDirOperator's icon view
    // - placesViewWidth needs to account for the size of the splitter handle
    // - the lafBox grid layout spacing should only affect the label, but not the line edit
    const int adjustment = placesViewSplitter->handleWidth() - lafBox->horizontalSpacing();
    lafBox->setColumnMinimumWidth(0, placesViewWidth + adjustment);
}

void KFileWidgetPrivate::initGUI()
{
    delete boxLayout; // deletes all sub layouts

    boxLayout = new QVBoxLayout(q);
    boxLayout->setContentsMargins(0, 0, 0, 0); // no additional margin to the already existing

    placesViewSplitter = new QSplitter(q);
    placesViewSplitter->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    placesViewSplitter->setChildrenCollapsible(false);
    boxLayout->addWidget(placesViewSplitter);

    QObject::connect(placesViewSplitter, SIGNAL(splitterMoved(int,int)),
                     q, SLOT(_k_placesViewSplitterMoved(int,int)));
    placesViewSplitter->insertWidget(0, opsWidget);

    vbox = new QVBoxLayout();
    vbox->setContentsMargins(0, 0, 0, 0);
    boxLayout->addLayout(vbox);

    lafBox = new QGridLayout();

    lafBox->addWidget(locationLabel, 0, 0, Qt::AlignVCenter | Qt::AlignRight);
    lafBox->addWidget(locationEdit, 0, 1, Qt::AlignVCenter);
    lafBox->addWidget(okButton, 0, 2, Qt::AlignVCenter);

    lafBox->addWidget(filterLabel, 1, 0, Qt::AlignVCenter | Qt::AlignRight);
    lafBox->addWidget(filterWidget, 1, 1, Qt::AlignVCenter);
    lafBox->addWidget(cancelButton, 1, 2, Qt::AlignVCenter);

    lafBox->setColumnStretch(1, 4);

    vbox->addLayout(lafBox);

    // add the Automatically Select Extension checkbox
    vbox->addWidget(autoSelectExtCheckBox);

    q->setTabOrder(ops, autoSelectExtCheckBox);
    q->setTabOrder(autoSelectExtCheckBox, locationEdit);
    q->setTabOrder(locationEdit, filterWidget);
    q->setTabOrder(filterWidget, okButton);
    q->setTabOrder(okButton, cancelButton);
    q->setTabOrder(cancelButton, urlNavigator);
    q->setTabOrder(urlNavigator, ops);
}

void KFileWidgetPrivate::_k_slotFilterChanged()
{
//     qDebug();

    filterDelayTimer.stop();

    QString filter = filterWidget->currentFilter();
    ops->clearFilter();

    if (filter.contains(QLatin1Char('/'))) {
#if QT_VERSION < QT_VERSION_CHECK(5, 15, 0)
        QStringList types = filter.split(QLatin1Char(' '), QString::SkipEmptyParts);
#else
        QStringList types = filter.split(QLatin1Char(' '), Qt::SkipEmptyParts);
#endif
        types.prepend(QStringLiteral("inode/directory"));
        ops->setMimeFilter(types);
    } else if (filter.contains(QLatin1Char('*')) || filter.contains(QLatin1Char('?')) || filter.contains(QLatin1Char('['))) {
        ops->setNameFilter(filter);
    } else {
        ops->setNameFilter(QLatin1Char('*') + filter.replace(QLatin1Char(' '), QLatin1Char('*')) + QLatin1Char('*'));
    }

    updateAutoSelectExtension();

    ops->updateDir();

    emit q->filterChanged(filter);
}

void KFileWidget::setUrl(const QUrl &url, bool clearforward)
{
//     qDebug();

    d->ops->setUrl(url, clearforward);
}

// Protected
void KFileWidgetPrivate::_k_urlEntered(const QUrl &url)
{
//     qDebug();

    KUrlComboBox *pathCombo = urlNavigator->editor();
    if (pathCombo->count() != 0) { // little hack
        pathCombo->setUrl(url);
    }

    bool blocked = locationEdit->blockSignals(true);
    if (keepLocation) {
        const QUrl currentUrl = urlFromString(locationEditCurrentText());
        // iconNameForUrl will get the icon or fallback to a generic one
        locationEdit->setItemIcon(0, QIcon::fromTheme(KIO::iconNameForUrl(currentUrl)));
        // Preserve the text when clicking on the view (cf _k_fileHighlighted)
        locationEdit->lineEdit()->setModified(true);
    }

    locationEdit->blockSignals(blocked);

    urlNavigator->setLocationUrl(url);

    // is trigged in ctor before completion object is set
    KUrlCompletion *completion = dynamic_cast<KUrlCompletion *>(locationEdit->completionObject());
    if (completion) {
        completion->setDir(url);
    }

    if (placesView) {
        placesView->setUrl(url);
    }
}

void KFileWidgetPrivate::_k_locationAccepted(const QString &url)
{
    Q_UNUSED(url);
//     qDebug();
    q->slotOk();
}

void KFileWidgetPrivate::_k_enterUrl(const QUrl &url)
{
//     qDebug();

    // append '/' if needed: url combo does not add it
    // tokenize() expects it because it uses QUrl::adjusted(QUrl::RemoveFilename)
    QUrl u(url);
    if (!u.path().isEmpty() && !u.path().endsWith(QLatin1Char('/'))) {
        u.setPath(u.path() + QLatin1Char('/'));
    }
    q->setUrl(u);

    // We need to check window()->focusWidget() instead of locationEdit->hasFocus
    // because when the window is showing up locationEdit
    // may still not have focus but it'll be the one that will have focus when the window
    // gets it and we don't want to steal its focus either
    if (q->window()->focusWidget() != locationEdit) {
        ops->setFocus();
    }
}

void KFileWidgetPrivate::_k_enterUrl(const QString &url)
{
//     qDebug();

    _k_enterUrl(urlFromString(KUrlCompletion::replacedPath(url, true, true)));
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

void KFileWidgetPrivate::_k_slotLoadingFinished()
{
    const QString currentText = locationEdit->currentText();
    if (currentText.isEmpty()) {
        return;
    }

    ops->blockSignals(true);
    QUrl u(ops->url());
    if (currentText.startsWith(QLatin1Char('/')))
        u.setPath(currentText);
    else
        u.setPath(concatPaths(ops->url().path(), currentText));
    ops->setCurrentItem(u);
    ops->blockSignals(false);
}

void KFileWidgetPrivate::_k_fileCompletion(const QString &match)
{
//     qDebug();

    if (match.isEmpty() || locationEdit->currentText().contains(QLatin1Char('"'))) {
        return;
    }

    const QUrl url = urlFromString(match);
    const QIcon mimeTypeIcon = QIcon::fromTheme(KIO::iconNameForUrl(url), QIcon::fromTheme(QStringLiteral("application-octet-stream")));
    setDummyHistoryEntry(locationEdit->currentText(), mimeTypeIcon, !locationEdit->currentText().isEmpty());
}

void KFileWidgetPrivate::_k_slotLocationChanged(const QString &text)
{
//     qDebug();

    locationEdit->lineEdit()->setModified(true);

    if (text.isEmpty() && ops->view()) {
        ops->view()->clearSelection();
    }

    if (text.isEmpty()) {
        removeDummyHistoryEntry();
    } else {
        setDummyHistoryEntry(text);
    }

    if (!locationEdit->lineEdit()->text().isEmpty()) {
        const QList<QUrl> urlList(tokenize(text));
        ops->setCurrentItems(urlList);
    }

    updateFilter();
}

QUrl KFileWidget::selectedUrl() const
{
//     qDebug();

    if (d->inAccept) {
        return d->url;
    } else {
        return QUrl();
    }
}

QList<QUrl> KFileWidget::selectedUrls() const
{
//     qDebug();

    QList<QUrl> list;
    if (d->inAccept) {
        if (d->ops->mode() & KFile::Files) {
            list = d->urlList;
        } else {
            list.append(d->url);
        }
    }
    return list;
}

QList<QUrl> KFileWidgetPrivate::tokenize(const QString &line) const
{
    qCDebug(KIO_KFILEWIDGETS_FW) << "Tokenizing:" << line;

    QList<QUrl> urls;
    QUrl u(ops->url().adjusted(QUrl::RemoveFilename));
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

    if (d->inAccept) {
        const QUrl url = d->mostLocalUrl(d->url);
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

    if (d->inAccept) {
        if (d->ops->mode() & KFile::Files) {
            const QList<QUrl> urls = d->urlList;
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
            if (d->url.isLocalFile()) {
                list.append(d->url.toLocalFile());
            }
        }
    }

    return list;
}

QUrl KFileWidget::baseUrl() const
{
    return d->ops->url();
}

void KFileWidget::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);

    if (d->placesDock) {
        // we don't want our places dock actually changing size when we resize
        // and qt doesn't make it easy to enforce such a thing with QSplitter
        d->setPlacesViewSplitterSizes();
    }
}

void KFileWidget::showEvent(QShowEvent *event)
{
    if (!d->hasView) {   // delayed view-creation
        Q_ASSERT(d);
        Q_ASSERT(d->ops);
        d->ops->setView(KFile::Default);
        d->ops->view()->setSizePolicy(QSizePolicy(QSizePolicy::Maximum, QSizePolicy::Maximum));
        d->hasView = true;

        connect(d->ops->view(), SIGNAL(doubleClicked(QModelIndex)), this, SLOT(_k_slotViewDoubleClicked(QModelIndex)));
    }
    d->ops->clearHistory();

    QWidget::showEvent(event);
}

bool KFileWidget::eventFilter(QObject *watched, QEvent *event)
{
    const bool res = QWidget::eventFilter(watched, event);

    QKeyEvent *keyEvent = dynamic_cast<QKeyEvent *>(event);
    if (watched == d->iconSizeSlider && keyEvent) {
        if (keyEvent->key() == Qt::Key_Left || keyEvent->key() == Qt::Key_Up ||
                keyEvent->key() == Qt::Key_Right || keyEvent->key() == Qt::Key_Down) {
            d->_k_slotIconSizeSliderMoved(d->iconSizeSlider->value());
        }
    } else if (watched == d->locationEdit && event->type() == QEvent::KeyPress) {
        if (keyEvent->modifiers() & Qt::AltModifier) {
            switch (keyEvent->key()) {
            case Qt::Key_Up:
                d->ops->actionCollection()->action(QStringLiteral("up"))->trigger();
                break;
            case Qt::Key_Left:
                d->ops->actionCollection()->action(QStringLiteral("back"))->trigger();
                break;
            case Qt::Key_Right:
                d->ops->actionCollection()->action(QStringLiteral("forward"))->trigger();
                break;
            default:
                break;
            }
        }
    } else if (watched == d->ops && event->type() == QEvent::KeyPress &&
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

    d->ops->setMode(m);
    if (d->ops->dirOnlyMode()) {
        d->filterWidget->setDefaultFilter(i18n("*|All Folders"));
    } else {
        d->filterWidget->setDefaultFilter(i18n("*|All Files"));
    }

    d->updateAutoSelectExtension();
}

KFile::Modes KFileWidget::mode() const
{
    return d->ops->mode();
}

void KFileWidgetPrivate::readViewConfig()
{
    ops->setViewConfig(configGroup);
    ops->readConfig(configGroup);
    KUrlComboBox *combo = urlNavigator->editor();

    autoDirectoryFollowing = configGroup.readEntry(AutoDirectoryFollowing,
                             DefaultDirectoryFollowing);

    KCompletion::CompletionMode cm = (KCompletion::CompletionMode)
                                     configGroup.readEntry(PathComboCompletionMode,
                                             static_cast<int>(KCompletion::CompletionPopup));
    if (cm != KCompletion::CompletionPopup) {
        combo->setCompletionMode(cm);
    }

    cm = (KCompletion::CompletionMode)
         configGroup.readEntry(LocationComboCompletionMode,
                               static_cast<int>(KCompletion::CompletionPopup));
    if (cm != KCompletion::CompletionPopup) {
        locationEdit->setCompletionMode(cm);
    }

    // show or don't show the speedbar
    _k_toggleSpeedbar(configGroup.readEntry(ShowSpeedbar, true));

    // show or don't show the bookmarks
    _k_toggleBookmarks(configGroup.readEntry(ShowBookmarks, false));

    // does the user want Automatically Select Extension?
    autoSelectExtChecked = configGroup.readEntry(AutoSelectExtChecked, DefaultAutoSelectExtChecked);
    updateAutoSelectExtension();

    // should the URL navigator use the breadcrumb navigation?
    urlNavigator->setUrlEditable(!configGroup.readEntry(BreadcrumbNavigation, true));

    // should the URL navigator show the full path?
    urlNavigator->setShowFullPath(configGroup.readEntry(ShowFullPath, false));

    int w1 = q->minimumSize().width();
    int w2 = toolbar->sizeHint().width();
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

    KUrlComboBox *pathCombo = urlNavigator->editor();
    //saveDialogSize( tmpGroup, KConfigGroup::Persistent | KConfigGroup::Global );
    tmpGroup.writeEntry(PathComboCompletionMode, static_cast<int>(pathCombo->completionMode()));
    tmpGroup.writeEntry(LocationComboCompletionMode, static_cast<int>(locationEdit->completionMode()));

    const bool showSpeedbar = placesDock && !placesDock->isHidden();
    tmpGroup.writeEntry(ShowSpeedbar, showSpeedbar);
    if (placesViewWidth > 0) {
        tmpGroup.writeEntry(SpeedbarWidth, placesViewWidth);
    }

    tmpGroup.writeEntry(ShowBookmarks, bookmarkHandler != nullptr);
    tmpGroup.writeEntry(AutoSelectExtChecked, autoSelectExtChecked);
    tmpGroup.writeEntry(BreadcrumbNavigation, !urlNavigator->isUrlEditable());
    tmpGroup.writeEntry(ShowFullPath, urlNavigator->showFullPath());

    ops->writeConfig(tmpGroup);

    // Copy saved settings to kdeglobals
    tmpGroup.copyTo(&configGroup, KConfigGroup::Persistent | KConfigGroup::Global);
}

void KFileWidgetPrivate::readRecentFiles()
{
//     qDebug();

    QObject::disconnect(locationEdit, SIGNAL(editTextChanged(QString)),
                        q, SLOT(_k_slotLocationChanged(QString)));

    locationEdit->setMaxItems(configGroup.readEntry(RecentFilesNumber, DefaultRecentURLsNumber));
    locationEdit->setUrls(configGroup.readPathEntry(RecentFiles, QStringList()),
                          KUrlComboBox::RemoveBottom);
    locationEdit->setCurrentIndex(-1);

    QObject::connect(locationEdit, SIGNAL(editTextChanged(QString)),
                     q, SLOT(_k_slotLocationChanged(QString)));

    KUrlComboBox *combo = urlNavigator->editor();
    combo->setUrls(configGroup.readPathEntry(RecentURLs, QStringList()), KUrlComboBox::RemoveTop);
    combo->setMaxItems(configGroup.readEntry(RecentURLsNumber, DefaultRecentURLsNumber));
    combo->setUrl(ops->url());
    // since we delayed this moment, initialize the directory of the completion object to
    // our current directory (that was very probably set on the constructor)
    KUrlCompletion *completion = dynamic_cast<KUrlCompletion *>(locationEdit->completionObject());
    if (completion) {
        completion->setDir(ops->url());
    }

}

void KFileWidgetPrivate::saveRecentFiles()
{
//     qDebug();
    configGroup.writePathEntry(RecentFiles, locationEdit->urls());

    KUrlComboBox *pathCombo = urlNavigator->editor();
    configGroup.writePathEntry(RecentURLs, pathCombo->urls());
}

QPushButton *KFileWidget::okButton() const
{
    return d->okButton;
}

QPushButton *KFileWidget::cancelButton() const
{
    return d->cancelButton;
}

// Called by KFileDialog
void KFileWidget::slotCancel()
{
    d->writeViewConfig();
    d->ops->close();
}

void KFileWidget::setKeepLocation(bool keep)
{
    d->keepLocation = keep;
}

bool KFileWidget::keepsLocation() const
{
    return d->keepLocation;
}

void KFileWidget::setOperationMode(OperationMode mode)
{
//     qDebug();

    d->operationMode = mode;
    d->keepLocation = (mode == Saving);
    d->filterWidget->setEditable(!d->hasDefaultFilter || mode != Saving);
    if (mode == Opening) {
        // don't use KStandardGuiItem::open() here which has trailing ellipsis!
        d->okButton->setText(i18n("&Open"));
        d->okButton->setIcon(QIcon::fromTheme(QStringLiteral("document-open")));
        // hide the new folder actions...usability team says they shouldn't be in open file dialog
        actionCollection()->removeAction(actionCollection()->action(QStringLiteral("mkdir")));
    } else if (mode == Saving) {
        KGuiItem::assign(d->okButton, KStandardGuiItem::save());
        d->setNonExtSelection();
    } else {
        KGuiItem::assign(d->okButton, KStandardGuiItem::ok());
    }
    d->updateLocationWhatsThis();
    d->updateAutoSelectExtension();

    if (d->ops) {
        d->ops->setIsSaving(mode == Saving);
    }
    d->updateFilterText();
}

KFileWidget::OperationMode KFileWidget::operationMode() const
{
    return d->operationMode;
}

void KFileWidgetPrivate::_k_slotAutoSelectExtClicked()
{
//     qDebug() << "slotAutoSelectExtClicked(): "
//                          << autoSelectExtCheckBox->isChecked() << endl;

    // whether the _user_ wants it on/off
    autoSelectExtChecked = autoSelectExtCheckBox->isChecked();

    // update the current filename's extension
    updateLocationEditExtension(extension /* extension hasn't changed */);
}

void KFileWidgetPrivate::_k_placesViewSplitterMoved(int pos, int index)
{
//     qDebug();

    // we need to record the size of the splitter when the splitter changes size
    // so we can keep the places box the right size!
    if (placesDock && index == 1) {
        placesViewWidth = pos;
//         qDebug() << "setting lafBox minwidth to" << placesViewWidth;
        setLafBoxColumnWidth();
    }
}

void KFileWidgetPrivate::_k_activateUrlNavigator()
{
//     qDebug();

    QLineEdit* lineEdit = urlNavigator->editor()->lineEdit();

    // If the text field currently has focus and everything is selected,
    // pressing the keyboard shortcut returns the whole thing to breadcrumb mode
    if (urlNavigator->isUrlEditable()
        && lineEdit->hasFocus()
        && lineEdit->selectedText() == lineEdit->text() ) {
        urlNavigator->setUrlEditable(false);
    } else {
        urlNavigator->setUrlEditable(true);
        urlNavigator->setFocus();
        lineEdit->selectAll();
    }
}

void KFileWidgetPrivate::_k_zoomOutIconsSize()
{
    const int currValue = ops->iconSize();

    // Jump to the nearest standard size
    auto r_itEnd = stdIconSizes.crend();
    auto it = std::find_if(stdIconSizes.crbegin(), r_itEnd,
                 [currValue](KIconLoader::StdSizes size) { return size < currValue; });

    Q_ASSERT(it != r_itEnd);

    const int nearestSize = *it;

    iconSizeSlider->setValue(nearestSize);
    _k_slotIconSizeSliderMoved(nearestSize);
}

void KFileWidgetPrivate::_k_zoomInIconsSize()
{
    const int currValue = ops->iconSize();

    // Jump to the nearest standard size
    auto itEnd = stdIconSizes.cend();
    auto it = std::find_if(stdIconSizes.cbegin(), itEnd,
                 [currValue](KIconLoader::StdSizes size) { return size > currValue; });

    Q_ASSERT(it != itEnd);

    const int nearestSize = *it;

    iconSizeSlider->setValue(nearestSize);
    _k_slotIconSizeSliderMoved(nearestSize);
}

void KFileWidgetPrivate::_k_slotIconSizeChanged(int _value)
{
    switch (_value) {
    case KIconLoader::SizeSmall:
    case KIconLoader::SizeSmallMedium:
    case KIconLoader::SizeMedium:
    case KIconLoader::SizeLarge:
    case KIconLoader::SizeHuge:
    case KIconLoader::SizeEnormous:
        iconSizeSlider->setToolTip(i18n("Icon size: %1 pixels (standard size)", _value));
        break;
    default:
        iconSizeSlider->setToolTip(i18n("Icon size: %1 pixels", _value));
        break;
    }
}

void KFileWidgetPrivate::_k_slotIconSizeSliderMoved(int _value)
{
    // Force this to be called in case this slot is called first on the
    // slider move.
    _k_slotIconSizeChanged(_value);

    QPoint global(iconSizeSlider->rect().topLeft());
    global.ry() += iconSizeSlider->height() / 2;
    QHelpEvent toolTipEvent(QEvent::ToolTip, QPoint(0, 0), iconSizeSlider->mapToGlobal(global));
    QApplication::sendEvent(iconSizeSlider, &toolTipEvent);
}

void KFileWidgetPrivate::_k_slotViewDoubleClicked(const QModelIndex &index)
{
    // double clicking to save should only work on files
    if (operationMode == KFileWidget::Saving && index.isValid() && ops->selectedItems().constFirst().isFile()) {
        q->slotOk();
    }
}

void KFileWidgetPrivate::_k_slotViewKeyEnterReturnPressed()
{
    // an enter/return event occurred in the view
    // when we are saving one file and there is no selection in the view (otherwise we get an activated event)
    if (operationMode == KFileWidget::Saving && (ops->mode() & KFile::File) && ops->selectedItems().isEmpty()) {
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
//    return d->extension;
//}

void KFileWidgetPrivate::updateAutoSelectExtension()
{
    if (!autoSelectExtCheckBox) {
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
    QString lastExtension = extension;
    extension.clear();

    // Automatically Select Extension is only valid if the user is _saving_ a _file_
    if ((operationMode == KFileWidget::Saving) && (ops->mode() & KFile::File)) {
        //
        // Get an extension from the filter
        //

        QString filter = filterWidget->currentFilter();
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
#if QT_VERSION < QT_VERSION_CHECK(5, 15, 0)
                extensionList = filter.split(QLatin1Char(' '), QString::SkipEmptyParts);
#else
                extensionList = filter.split(QLatin1Char(' '), Qt::SkipEmptyParts);
#endif
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
                extension = QLatin1Char('.') + currentExtension;
            } else {
                extension = defaultExtension;
            }

            // qDebug() << "List:" << extensionList << "auto-selected extension:" << extension;
        }

        //
        // GUI: checkbox
        //

        QString whatsThisExtension;
        if (!extension.isEmpty()) {
            // remember: sync any changes to the string with below
            autoSelectExtCheckBox->setText(i18n("Automatically select filename e&xtension (%1)",  extension));
            whatsThisExtension = i18n("the extension <b>%1</b>",  extension);

            autoSelectExtCheckBox->setEnabled(true);
            autoSelectExtCheckBox->setChecked(autoSelectExtChecked);
        } else {
            // remember: sync any changes to the string with above
            autoSelectExtCheckBox->setText(i18n("Automatically select filename e&xtension"));
            whatsThisExtension = i18n("a suitable extension");

            autoSelectExtCheckBox->setChecked(false);
            autoSelectExtCheckBox->setEnabled(false);
        }

        const QString locationLabelText = stripUndisplayable(locationLabel->text());
        autoSelectExtCheckBox->setWhatsThis(QLatin1String("<qt>") +
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

        autoSelectExtCheckBox->show();

        // update the current filename's extension
        updateLocationEditExtension(lastExtension);
    }
    // Automatically Select Extension not valid
    else {
        autoSelectExtCheckBox->setChecked(false);
        autoSelectExtCheckBox->hide();
    }
}

// Updates the extension of the filename specified in d->locationEdit if the
// Automatically Select Extension feature is enabled.
// (this prevents you from accidentally saving "file.kwd" as RTF, for example)
void KFileWidgetPrivate::updateLocationEditExtension(const QString &lastExtension)
{
    if (!autoSelectExtCheckBox->isChecked() || extension.isEmpty()) {
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
        } else if (extension.length() && fileName.endsWith(extension)) {
            fileName.chop(extension.length());
        }
        // can only handle "single extensions"
        else {
            fileName.truncate(dot);
        }

        // add extension
        const QString newText = urlStr.leftRef(fileNameOffset) + fileName + extension;
        if (newText != locationEditCurrentText()) {
            locationEdit->setItemText(locationEdit->currentIndex(), newText);
            locationEdit->lineEdit()->setModified(true);
        }
    }
}

QString KFileWidgetPrivate::findMatchingFilter(const QString &filter, const QString &filename) const
{
     // e.g.: '*.foo *.bar|Foo type' -> '*.foo', '*.bar'
    const QStringList patterns = filter.left(filter.indexOf(QLatin1Char('|'))).split(QLatin1Char(' '),
#if QT_VERSION < QT_VERSION_CHECK(5, 15, 0)
                                             QString::SkipEmptyParts);
#else
                                             Qt::SkipEmptyParts);
#endif

    QRegularExpression rx;
    for (const QString &p : patterns) {
        rx.setPattern(QRegularExpression::wildcardToRegularExpression(p));
        if (rx.match(filename).hasMatch()) {
            return p;
        }
    }
    return QString();
}

// Updates the filter if the extension of the filename specified in d->locationEdit is changed
// (this prevents you from accidently saving "file.kwd" as RTF, for example)
void KFileWidgetPrivate::updateFilter()
{
//     qDebug();

    if ((operationMode == KFileWidget::Saving) && (ops->mode() & KFile::File)) {
        QString urlStr = locationEditCurrentText();
        if (urlStr.isEmpty()) {
            return;
        }

        if (filterWidget->isMimeFilter()) {
            QMimeDatabase db;
            QMimeType mime = db.mimeTypeForFile(urlStr, QMimeDatabase::MatchExtension);
            if (mime.isValid() && !mime.isDefault()) {
                if (filterWidget->currentFilter() != mime.name() &&
                        filterWidget->filters().indexOf(mime.name()) != -1) {
                    filterWidget->setCurrentFilter(mime.name());
                }
            }
        } else {
            QString filename = urlStr.mid(urlStr.lastIndexOf(QLatin1Char('/')) + 1);     // only filename
            // accept any match to honor the user's selection; see later code handling the "*" match
            if (!findMatchingFilter(filterWidget->currentFilter(), filename).isEmpty()) {
                return;
            }
            const QStringList list = filterWidget->filters();
            for (const QString &filter : list) {
                QString match = findMatchingFilter(filter, filename);
                if (!match.isEmpty()) {
                    if (match != QLatin1String("*")) {   // never match the catch-all filter
                        filterWidget->setCurrentFilter(filter);
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

    if (!autoSelectExtCheckBox->isChecked() || extension.isEmpty()) {
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
        // This allows lazy people to have autoSelectExtCheckBox->isChecked
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
//         qDebug() << "\tappending extension \'" << extension << "\'...";
        url = url.adjusted(QUrl::RemoveFilename); // keeps trailing slash
        url.setPath(url.path() + fileName + extension);
//         qDebug() << "\tsaving as \'" << url << "\'";
    }
}

// adds the selected files/urls to 'recent documents'
void KFileWidgetPrivate::addToRecentDocuments()
{
    int m = ops->mode();
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
    return d->locationEdit;
}

KFileFilterCombo *KFileWidget::filterWidget() const
{
    return d->filterWidget;
}

KActionCollection *KFileWidget::actionCollection() const
{
    return d->ops->actionCollection();
}

void KFileWidgetPrivate::_k_toggleSpeedbar(bool show)
{
    if (show) {
        initSpeedbar();
        placesDock->show();
        setLafBoxColumnWidth();

        // check to see if they have a home item defined, if not show the home button
        QUrl homeURL;
        homeURL.setPath(QDir::homePath());
        KFilePlacesModel *model = static_cast<KFilePlacesModel *>(placesView->model());
        for (int rowIndex = 0; rowIndex < model->rowCount(); rowIndex++) {
            QModelIndex index = model->index(rowIndex, 0);
            QUrl url = model->url(index);

            if (homeURL.matches(url, QUrl::StripTrailingSlash)) {
                toolbar->removeAction(ops->actionCollection()->action(QStringLiteral("home")));
                break;
            }
        }
    } else {
        if (q->sender() == placesDock && placesDock && placesDock->isVisibleTo(q)) {
            // we didn't *really* go away! the dialog was simply hidden or
            // we changed virtual desktops or ...
            return;
        }

        if (placesDock) {
            placesDock->hide();
        }

        QAction *homeAction = ops->actionCollection()->action(QStringLiteral("home"));
        QAction *reloadAction = ops->actionCollection()->action(QStringLiteral("reload"));
        if (!toolbar->actions().contains(homeAction)) {
            toolbar->insertAction(reloadAction, homeAction);
        }

        // reset the lafbox to not follow the width of the splitter
        lafBox->setColumnMinimumWidth(0, 0);
    }

    static_cast<KToggleAction *>(q->actionCollection()->action(QStringLiteral("toggleSpeedbar")))->setChecked(show);

    // if we don't show the places panel, at least show the places menu
    urlNavigator->setPlacesSelectorVisible(!show);
}

void KFileWidgetPrivate::_k_toggleBookmarks(bool show)
{
    if (show) {
        if (bookmarkHandler) {
            return;
        }
        bookmarkHandler = new KFileBookmarkHandler(q);
        q->connect(bookmarkHandler, SIGNAL(openUrl(QString)),
                   SLOT(_k_enterUrl(QString)));
        bookmarkButton->setMenu(bookmarkHandler->menu());
    } else if (bookmarkHandler) {
        bookmarkButton->setMenu(nullptr);
        delete bookmarkHandler;
        bookmarkHandler = nullptr;
    }

    if (bookmarkButton) {
        bookmarkButton->setVisible(show);
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
        locationEdit->lineEdit()->setSelection(0, filename.length() - extension.length() - 1);
    } else {
        int lastDot = filename.lastIndexOf(QLatin1Char('.'));
        if (lastDot > 0) {
            locationEdit->lineEdit()->setSelection(0, lastDot);
        } else {
            locationEdit->lineEdit()->selectAll();
        }
    }
}

// Sets the filter text to "File type" if the dialog is saving and a MIME type
// filter has been set; otherwise, the text is "Filter:"
void KFileWidgetPrivate::updateFilterText()
{
    QString label;
    QString whatsThisText;

    if (operationMode == KFileWidget::Saving && filterWidget->isMimeFilter()) {
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

    if (filterLabel) {
        filterLabel->setText(label);
        filterLabel->setWhatsThis(whatsThisText);
    }
    if (filterWidget) {
        filterWidget->setWhatsThis(whatsThisText);
    }
}

#if KIOFILEWIDGETS_BUILD_DEPRECATED_SINCE(5, 66)
KToolBar *KFileWidget::toolBar() const
{
    return d->toolbar;
}
#endif

void KFileWidget::setCustomWidget(QWidget *widget)
{
    delete d->bottomCustomWidget;
    d->bottomCustomWidget = widget;

    // add it to the dialog, below the filter list box.

    // Change the parent so that this widget is a child of the main widget
    d->bottomCustomWidget->setParent(this);

    d->vbox->addWidget(d->bottomCustomWidget);
    //d->vbox->addSpacing(3); // can't do this every time...

    // FIXME: This should adjust the tab orders so that the custom widget
    // comes after the Cancel button. The code appears to do this, but the result
    // somehow screws up the tab order of the file path combo box. Not a major
    // problem, but ideally the tab order with a custom widget should be
    // the same as the order without one.
    setTabOrder(d->cancelButton, d->bottomCustomWidget);
    setTabOrder(d->bottomCustomWidget, d->urlNavigator);
}

void KFileWidget::setCustomWidget(const QString &text, QWidget *widget)
{
    delete d->labeledCustomWidget;
    d->labeledCustomWidget = widget;

    QLabel *label = new QLabel(text, this);
    label->setAlignment(Qt::AlignRight);
    d->lafBox->addWidget(label, 2, 0, Qt::AlignVCenter);
    d->lafBox->addWidget(widget, 2, 1, Qt::AlignVCenter);
}

KDirOperator *KFileWidget::dirOperator()
{
    return d->ops;
}

void KFileWidget::readConfig(KConfigGroup &group)
{
    d->configGroup = group;
    d->readViewConfig();
    d->readRecentFiles();
}

QString KFileWidgetPrivate::locationEditCurrentText() const
{
    return QDir::fromNativeSeparators(locationEdit->currentText());
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
    ops->setInlinePreviewShown(show);
}

void KFileWidget::setConfirmOverwrite(bool enable)
{
    d->confirmOverwrite = enable;
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
    d->ops->setView(mode);
    d->hasView = true;
}

void KFileWidget::setSupportedSchemes(const QStringList &schemes)
{
    d->model->setSupportedSchemes(schemes);
    d->ops->setSupportedSchemes(schemes);
    d->urlNavigator->setCustomProtocols(schemes);
}

QStringList KFileWidget::supportedSchemes() const
{
    return d->model->supportedSchemes();
}

#include "moc_kfilewidget.cpp"
