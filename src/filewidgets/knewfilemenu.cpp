/*
    This file is part of the KDE project
    SPDX-FileCopyrightText: 1998-2009 David Faure <faure@kde.org>
    SPDX-FileCopyrightText: 2003 Sven Leiber <s.leiber@web.de>

    SPDX-License-Identifier: LGPL-2.0-only OR LGPL-3.0-only
*/

#include "knewfilemenu.h"
#include "../utils_p.h"
#include "kfilewidgets_debug.h"
#include "knameandurlinputdialog.h"
#include "ui_knewfilemenu_newfiledialog.h"

#include <kdirnotify.h>
#include <kio/copyjob.h>
#include <kio/fileundomanager.h>
#include <kio/jobuidelegate.h>
#include <kio/mkdirjob.h>
#include <kio/mkpathjob.h>
#include <kio/namefinderjob.h>
#include <kio/statjob.h>
#include <kio/storedtransferjob.h>
#include <kpropertiesdialog.h>
#include <kprotocolinfo.h>
#include <kprotocolmanager.h>
#include <kurifilter.h>

#include <KCollapsibleGroupBox>
#include <KConfigGroup>
#include <KDesktopFile>
#include <KDirOperator>
#include <KDirWatch>
#include <KFileUtils>
#include <KIconDialog>
#include <KIconLoader>
#include <KJobWidgets>
#include <KLocalizedString>
#include <KMessageBox>
#include <KMessageWidget>
#include <KSharedConfig>
#include <KShell>

#include <QActionGroup>
#include <QDebug>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QFontDatabase>
#include <QLabel>
#include <QLineEdit>
#include <QList>
#include <QLoggingCategory>
#include <QMenu>
#include <QMimeDatabase>
#include <QPushButton>
#include <QStandardPaths>
#include <QTemporaryFile>
#include <QTimer>
#include <QVBoxLayout>

#ifdef Q_OS_WIN
#include <sys/utime.h>
#else
#include <utime.h>
#endif

#include <set>

static QString expandTilde(const QString &name, bool isfile = false)
{
    if (name.isEmpty() || name == QLatin1Char('~')) {
        return name;
    }

    QString expandedName;
    if (!isfile || name[0] == QLatin1Char('\\')) {
        expandedName = KShell::tildeExpand(name);
    }

    // If a tilde mark cannot be properly expanded, KShell::tildeExpand returns an empty string
    return !expandedName.isEmpty() ? expandedName : name;
}

static bool isDefaultFolderIcon(const QString &iconName)
{
    return iconName.isEmpty() || iconName == QLatin1String("folder") || iconName == QLatin1String("inode-directory");
}

static bool canPickFolderIcon(const QUrl &url)
{
    // TODO mostLocalUrl? But that would mean stat'ing when opening the dialog as opposed to only when accepting.
    return url.isLocalFile() || KProtocolInfo::protocolClass(url.scheme()) == QLatin1String(":local");
}

static KConfigGroup stateConfig()
{
    return KConfigGroup(KSharedConfig::openStateConfig(QStringLiteral("kiostaterc")), QStringLiteral("New File Menu"));
}

// Singleton, with data shared by all KNewFileMenu instances
class KNewFileMenuSingleton
{
public:
    KNewFileMenuSingleton()
        : dirWatch(nullptr)
        , filesParsed(false)
        , templatesList(nullptr)
        , templatesVersion(0)
    {
    }

    ~KNewFileMenuSingleton()
    {
        delete templatesList;
    }

    /*
     * Opens the desktop files and completes the Entry list
     * Input: the entry list. Output: the entry list ;-)
     */
    void parseFiles();

    enum EntryType {
        Unknown = 0, // Not parsed, i.e. we don't know
        LinkToTemplate, // A desktop file that points to a file or dir to copy
        Template, // A real file to copy as is (the KDE-1.x solution)
    };

    std::unique_ptr<KDirWatch> dirWatch;

    struct Entry {
        QString text;
        QString filePath; /// The displayed name in the context menu and the suggested filename. When using a .desktop file this is used to refer back to
                          /// it during parsing.
        QString templatePath; /// Where the file is copied from, the suggested file extension and whether the menu entries have a separator around them.
                              /// Same as filePath for Template.
        QString icon; /// The icon displayed in the context menu
        EntryType entryType; /// Defines if the created file will be a copy or a symbolic link
        QString comment; /// The prompt label asking for filename
        QString mimeType;
    };
    // NOTE: only filePath is known before we call parseFiles

    /*
     * List of all template files. It is important that they are in
     * the same order as the 'New' menu.
     */
    typedef QList<Entry> EntryList;

    /*
     * Set back to false each time new templates are found,
     * and to true on the first call to parseFiles
     */
    bool filesParsed;
    EntryList *templatesList;

    /*
     * Is increased when templatesList has been updated and
     * menu needs to be re-filled. Menus have their own version and compare it
     * to templatesVersion before showing up
     */
    int templatesVersion;
};

struct EntryInfo {
    QString key; /// Context menu order is the alphabetical order of this variable
    QString url;
    KNewFileMenuSingleton::Entry entry;
};

void KNewFileMenuSingleton::parseFiles()
{
    // qDebug();
    filesParsed = true;
    QMutableListIterator templIter(*templatesList);
    while (templIter.hasNext()) {
        KNewFileMenuSingleton::Entry &templ = templIter.next();
        const QString &filePath = templ.filePath;
        QString text;
        QString templatePath;
        // If a desktop file, then read the name from it.
        // Otherwise (or if no name in it?) use file name
        if (KDesktopFile::isDesktopFile(filePath)) {
            KDesktopFile desktopFile(filePath);
            if (desktopFile.noDisplay()) {
                templIter.remove();
                continue;
            }

            text = desktopFile.readName();
            templ.icon = desktopFile.readIcon();
            templ.comment = desktopFile.readComment();
            if (desktopFile.readType() == QLatin1String("Link")) {
                templatePath = desktopFile.desktopGroup().readPathEntry("URL", QString());
                if (templatePath.startsWith(QLatin1String("file:/"))) {
                    templatePath = QUrl(templatePath).toLocalFile();
                } else if (!templatePath.startsWith(QLatin1Char('/')) && !templatePath.startsWith(QLatin1String("__"))) {
                    // A relative path, then (that's the default in the files we ship)
                    const QStringView linkDir = QStringView(filePath).left(filePath.lastIndexOf(QLatin1Char('/')) + 1 /*keep / */);
                    // qDebug() << "linkDir=" << linkDir;
                    templatePath = linkDir + templatePath;
                }
            }
            if (templatePath.isEmpty()) {
                // No URL key, this is an old-style template
                templ.entryType = KNewFileMenuSingleton::Template;
                templ.templatePath = templ.filePath; // we'll copy the file
            } else {
                templ.entryType = KNewFileMenuSingleton::LinkToTemplate;
                templ.templatePath = templatePath;
            }
        }
        if (text.isEmpty()) {
            text = QUrl(filePath).fileName();
            const QLatin1String suffix(".desktop");
            if (text.endsWith(suffix)) {
                text.chop(suffix.size());
            }
        }
        templ.text = text;
        /*// qDebug() << "Updating entry with text=" << text
                        << "entryType=" << templ.entryType
                        << "templatePath=" << templ.templatePath;*/
    }
}

Q_GLOBAL_STATIC(KNewFileMenuSingleton, kNewMenuGlobals)

class KNewFileMenuCopyData
{
public:
    KNewFileMenuCopyData()
    {
        m_isSymlink = false;
    }
    QString chosenFileName() const
    {
        return m_chosenFileName;
    }

    // If empty, no copy is performed.
    QString sourceFileToCopy() const
    {
        return m_src;
    }
    QString tempFileToDelete() const
    {
        return m_tempFileToDelete;
    }
    bool m_isSymlink;

    QString m_chosenFileName;
    QString m_src;
    QString m_tempFileToDelete;
    QString m_templatePath;
};

class KNewFileMenuPrivate
{
public:
    explicit KNewFileMenuPrivate(KNewFileMenu *qq)
        : q(qq)
        , m_delayedSlotTextChangedTimer(new QTimer(q))
    {
        m_delayedSlotTextChangedTimer->setInterval(50);
        m_delayedSlotTextChangedTimer->setSingleShot(true);
    }

    bool checkSourceExists(const QString &src);

    /*
     * The strategy used for other desktop files than Type=Link. Example: Application, Device.
     */
    void executeOtherDesktopFile(const KNewFileMenuSingleton::Entry &entry);

    /*
     * The strategy used for "real files or directories" (the common case)
     */
    void executeRealFileOrDir(const KNewFileMenuSingleton::Entry &entry);

    /*
     * Actually performs file handling. Reads in m_copyData for needed data, that has been collected by execute*() before
     */
    void executeStrategy();

    /*
     * The strategy used when creating a symlink
     */
    void executeSymLink(const KNewFileMenuSingleton::Entry &entry);

    /*
     * The strategy used for "url" desktop files
     */
    void executeUrlDesktopFile(const KNewFileMenuSingleton::Entry &entry);

    /*
     * Fills the menu from the templates list.
     */
    void fillMenu();

    /*
     * Tries to map a local URL for the given URL.
     */
    QUrl mostLocalUrl(const QUrl &url);

    /*
     * Just clears the string buffer d->m_text, but I need a slot for this to occur
     */
    void slotAbortDialog();

    /*
     * Called when New->* is clicked
     */
    void slotActionTriggered(QAction *action);

    /*
     * Shows a dialog asking the user to enter a name when creating a new folder.
     */
    void showNewDirNameDlg(const QString &name);

    /*
     * Callback function that reads in directory name from dialog and processes it
     */
    void slotCreateDirectory();

    /*
     * Fills the templates list.
     */
    void slotFillTemplates();

    /*
     * Called when accepting the KPropertiesDialog (for "other desktop files")
     */
    void _k_slotOtherDesktopFile(KPropertiesDialog *sender);

    /*
     * Called when closing the KPropertiesDialog is closed (whichever way, accepted and rejected)
     */
    void slotOtherDesktopFileClosed();

    /*
     * Callback in KNewFileMenu for the RealFile Dialog. Handles dialog input and gives over
     * to executeStrategy()
     */
    void slotRealFileOrDir();

    /*
     * Delay calls to _k_slotTextChanged
     */
    void _k_delayedSlotTextChanged();

    /*
     * Dialogs use this slot to write the changed string into KNewFile menu when the user
     * changes touches them
     */
    void _k_slotTextChanged(const QString &text);

    /*
     * Callback in KNewFileMenu for the Symlink Dialog. Handles dialog input and gives over
     * to executeStrategy()
     */
    void slotSymLink();

    /*
     * Callback in KNewFileMenu for the Url/Desktop Dialog. Handles dialog input and gives over
     * to executeStrategy()
     */
    void slotUrlDesktopFile();

    /*
     * Callback to check if a file/directory with the same name as the one being created, exists
     */
    void _k_slotStatResult(KJob *job);

    void _k_slotAccepted();

    /*
     * Initializes m_fileDialog and the other widgets that are included in it. Mainly to reduce
     * code duplication in showNewDirNameDlg() and executeRealFileOrDir().
     */
    void initDialog();

    /**
     * Sets the file/folder icon in the new file dialog.
     */
    void setIcon(const QIcon &icon);

    QAction *m_newFolderShortcutAction = nullptr;
    QAction *m_newFileShortcutAction = nullptr;

    KActionMenu *m_menuDev = nullptr;
    int m_menuItemsVersion = 0;
    QAction *m_newDirAction = nullptr;
    QDialog *m_fileDialog = nullptr;
    KMessageWidget *m_messageWidget = nullptr;
    QLabel *m_label = nullptr;
    QLabel *m_iconLabel = nullptr;
    QLineEdit *m_lineEdit = nullptr;
    KCollapsibleGroupBox *m_chooseIconBox = nullptr;
    QGridLayout *m_folderIconGrid = nullptr;
    // Exclusive QButtonGroup doesn't allow no button be checked...
    QActionGroup *m_iconGroup = nullptr;
    QPushButton *m_chooseIconButton = nullptr;
    QDialogButtonBox *m_buttonBox = nullptr;

    // This is used to allow _k_slotTextChanged to know whether it's being used to
    // create a file or a directory without duplicating code across two functions
    bool m_creatingDirectory = false;
    bool m_modal = true;

    /*
     * The action group that our actions belong to
     */
    QActionGroup *m_newMenuGroup = nullptr;
    QWidget *m_parentWidget = nullptr;

    /*
     * When the user pressed the right mouse button over an URL a popup menu
     * is displayed. The URL belonging to this popup menu is stored here.
     * For all intents and purposes this is the current directory where the menu is
     * opened.
     * TODO KF6 make it a single QUrl.
     */
    QList<QUrl> m_popupFiles;

    QStringList m_supportedMimeTypes;
    QString m_tempFileToDelete; // set when a tempfile was created for a Type=URL desktop file
    QString m_text;
    QString m_windowTitle;

    KNewFileMenuSingleton::Entry *m_firstFileEntry = nullptr;

    KNewFileMenu *const q;

    KNewFileMenuCopyData m_copyData;

    /*
     * Use to delay a bit feedback to user
     */
    QTimer *m_delayedSlotTextChangedTimer;

    QUrl m_baseUrl;

    bool m_selectDirWhenAlreadyExists = false;
    bool m_acceptedPressed = false;
    bool m_statRunning = false;
    bool m_isCreateDirectoryRunning = false;
    bool m_isCreateFileRunning = false;
};

void KNewFileMenuPrivate::_k_slotAccepted()
{
    if (m_statRunning || m_delayedSlotTextChangedTimer->isActive()) {
        // stat is running or _k_slotTextChanged has not been called already
        // delay accept until stat has been run
        m_acceptedPressed = true;

        if (m_delayedSlotTextChangedTimer->isActive()) {
            m_delayedSlotTextChangedTimer->stop();
            _k_slotTextChanged(m_lineEdit->text());
        }
    } else {
        m_fileDialog->accept();
    }
}

void KNewFileMenuPrivate::initDialog()
{
    m_fileDialog = new QDialog(m_parentWidget);
    m_fileDialog->setAttribute(Qt::WA_DeleteOnClose);
    m_fileDialog->setModal(m_modal);

    Ui_NewFileDialog ui;
    ui.setupUi(m_fileDialog);

    m_messageWidget = ui.messageWidget;
    m_label = ui.label;
    m_iconLabel = ui.iconLabel;
    m_lineEdit = ui.lineEdit;
    m_chooseIconBox = ui.chooseIconBox;
    m_folderIconGrid = ui.folderIconGrid;
    m_buttonBox = ui.buttonBox;
    m_chooseIconButton = ui.chooseIconButton;

    ui.iconHintLabel->setFont(QFontDatabase::systemFont(QFontDatabase::SmallestReadableFont));

    m_iconLabel->hide();
    m_chooseIconBox->hide();
    m_messageWidget->hide();

    QObject::connect(m_buttonBox, &QDialogButtonBox::accepted, [this]() {
        _k_slotAccepted();
    });
    QObject::connect(m_buttonBox, &QDialogButtonBox::rejected, m_fileDialog, &QDialog::reject);

    QObject::connect(m_fileDialog, &QDialog::finished, m_fileDialog, [this] {
        m_statRunning = false;
    });
}

void KNewFileMenuPrivate::setIcon(const QIcon &icon)
{
    m_iconLabel->setProperty("iconName", icon.name());
    if (!icon.isNull()) {
        const QSize iconSize{KIconLoader::SizeHuge, KIconLoader::SizeHuge};
        m_iconLabel->setPixmap(icon.pixmap(iconSize, m_fileDialog->devicePixelRatioF()));
    }
    m_iconLabel->setVisible(!icon.isNull());
}

bool KNewFileMenuPrivate::checkSourceExists(const QString &src)
{
    if (!QFile::exists(src)) {
        qWarning() << src << "doesn't exist";

        QDialog *dialog = new QDialog(m_parentWidget);
        dialog->setWindowTitle(i18n("Sorry"));
        dialog->setObjectName(QStringLiteral("sorry"));
        dialog->setModal(q->isModal());
        dialog->setAttribute(Qt::WA_DeleteOnClose);

        QDialogButtonBox *box = new QDialogButtonBox(dialog);
        box->setStandardButtons(QDialogButtonBox::Ok);

        KMessageBox::createKMessageBox(dialog,
                                       box,
                                       QMessageBox::Warning,
                                       i18n("<qt>The template file <b>%1</b> does not exist.</qt>", src),
                                       QStringList(),
                                       QString(),
                                       nullptr,
                                       KMessageBox::NoExec);

        dialog->show();

        return false;
    }
    return true;
}

void KNewFileMenuPrivate::executeOtherDesktopFile(const KNewFileMenuSingleton::Entry &entry)
{
    if (!checkSourceExists(entry.templatePath)) {
        return;
    }

    for (const auto &url : std::as_const(m_popupFiles)) {
        QString text = entry.text;
        text.remove(QStringLiteral("...")); // the ... is fine for the menu item but not for the default filename
        text = text.trimmed(); // In some languages, there is a space in front of "...", see bug 268895
        // KDE5 TODO: remove the "..." from link*.desktop files and use i18n("%1...") when making
        // the action.
        QString name = text;
        text.append(QStringLiteral(".desktop"));

        const QUrl directory = mostLocalUrl(url);
        const QUrl defaultFile = QUrl::fromLocalFile(directory.toLocalFile() + QLatin1Char('/') + KIO::encodeFileName(text));
        if (defaultFile.isLocalFile() && QFile::exists(defaultFile.toLocalFile())) {
            text = KFileUtils::suggestName(directory, text);
        }

        QUrl templateUrl;
        bool usingTemplate = false;
        if (entry.templatePath.startsWith(QLatin1String(":/"))) {
            QTemporaryFile *tmpFile = QTemporaryFile::createNativeFile(entry.templatePath);
            tmpFile->setAutoRemove(false);
            QString tempFileName = tmpFile->fileName();
            tmpFile->close();

            KDesktopFile df(tempFileName);
            KConfigGroup group = df.desktopGroup();
            group.writeEntry("Name", name);
            templateUrl = QUrl::fromLocalFile(tempFileName);
            m_tempFileToDelete = tempFileName;
            usingTemplate = true;
        } else {
            templateUrl = QUrl::fromLocalFile(entry.templatePath);
        }
        KPropertiesDialog *dlg = new KPropertiesDialog(templateUrl, directory, text, m_parentWidget);
        dlg->setModal(q->isModal());
        dlg->setAttribute(Qt::WA_DeleteOnClose);
        QObject::connect(dlg, &KPropertiesDialog::applied, q, [this, dlg]() {
            _k_slotOtherDesktopFile(dlg);
        });
        if (usingTemplate) {
            QObject::connect(dlg, &KPropertiesDialog::propertiesClosed, q, [this]() {
                slotOtherDesktopFileClosed();
            });
        }
        dlg->show();
    }
    // We don't set m_src here -> there will be no copy, we are done.
}

void KNewFileMenuPrivate::executeRealFileOrDir(const KNewFileMenuSingleton::Entry &entry)
{
    Q_EMIT q->fileCreationStarted(QUrl(entry.filePath));

    initDialog();

    const auto getSelectionLength = [](const QString &text) {
        // Select the text without MIME-type extension
        int selectionLength = text.length();

        QMimeDatabase db;
        const QString extension = db.suffixForFileName(text);
        if (extension.isEmpty()) {
            // For an unknown extension just exclude the extension after
            // the last point. This does not work for multiple extensions like
            // *.tar.gz but usually this is anyhow a known extension.
            selectionLength = text.lastIndexOf(QLatin1Char('.'));

            // If no point could be found, use whole text length for selection.
            if (selectionLength < 1) {
                selectionLength = text.length();
            }

        } else {
            selectionLength -= extension.length() + 1;
        }

        return selectionLength;
    };

    // The template is not a desktop file
    // Prompt the user to set the destination filename
    QString text = entry.text;
    text.remove(QStringLiteral("...")); // the ... is fine for the menu item but not for the default filename
    text = text.trimmed(); // In some languages, there is a space in front of "...", see bug 268895
    // add the extension (from the templatePath), should work with .txt, .html and with ".tar.gz"... etc
    const QString fileName = entry.templatePath.mid(entry.templatePath.lastIndexOf(QLatin1Char('/')));
    const int dotIndex = getSelectionLength(fileName);
    text += dotIndex > 0 ? fileName.mid(dotIndex) : QString();

    m_copyData.m_src = entry.templatePath;

    const QUrl directory = mostLocalUrl(m_popupFiles.first());
    m_baseUrl = directory;
    const QUrl defaultFile = QUrl::fromLocalFile(directory.toLocalFile() + QLatin1Char('/') + KIO::encodeFileName(text));
    if (defaultFile.isLocalFile() && QFile::exists(defaultFile.toLocalFile())) {
        text = KFileUtils::suggestName(directory, text);
    }

    m_label->setText(entry.comment);
    setIcon(QIcon::fromTheme(entry.icon));

    m_lineEdit->setText(text);

    m_creatingDirectory = false;
    _k_slotTextChanged(text);
    QObject::connect(m_lineEdit, &QLineEdit::textChanged, q, [this]() {
        _k_delayedSlotTextChanged();
    });
    m_delayedSlotTextChangedTimer->callOnTimeout(m_lineEdit, [this]() {
        _k_slotTextChanged(m_lineEdit->text());
    });

    QObject::connect(m_fileDialog, &QDialog::accepted, q, [this]() {
        slotRealFileOrDir();
    });
    QObject::connect(m_fileDialog, &QDialog::rejected, q, [this]() {
        slotAbortDialog();
    });

    m_fileDialog->show();

    const int firstDotInBaseName = getSelectionLength(text);
    m_lineEdit->setSelection(0, firstDotInBaseName > 0 ? firstDotInBaseName : text.size());

    m_lineEdit->setFocus();
}

void KNewFileMenuPrivate::executeSymLink(const KNewFileMenuSingleton::Entry &entry)
{
    KNameAndUrlInputDialog *dlg = new KNameAndUrlInputDialog(i18n("Name for new link:"), entry.comment, m_popupFiles.first(), m_parentWidget);
    dlg->setModal(q->isModal());
    dlg->setAttribute(Qt::WA_DeleteOnClose);
    dlg->setWindowTitle(i18n("Create Symlink"));
    m_fileDialog = dlg;
    QObject::connect(dlg, &QDialog::accepted, q, [this]() {
        slotSymLink();
    });
    dlg->show();
}

void KNewFileMenuPrivate::executeStrategy()
{
    m_tempFileToDelete = m_copyData.tempFileToDelete();
    const QString src = m_copyData.sourceFileToCopy();
    QString chosenFileName = expandTilde(m_copyData.chosenFileName(), true);

    if (src.isEmpty()) {
        return;
    }
    QUrl uSrc(QUrl::fromLocalFile(src));

    // In case the templates/.source directory contains symlinks, resolve
    // them to the target files. Fixes bug #149628.
    KFileItem item(uSrc, QString(), KFileItem::Unknown);
    if (item.isLink()) {
        uSrc.setPath(item.linkDest());
    }

    // The template is not a desktop file [or it's a URL one] >>> Copy it
    for (const auto &u : std::as_const(m_popupFiles)) {
        QUrl dest = u;
        dest.setPath(Utils::concatPaths(dest.path(), KIO::encodeFileName(chosenFileName)));

        QList<QUrl> lstSrc;
        lstSrc.append(uSrc);
        KIO::Job *kjob;
        if (m_copyData.m_isSymlink) {
            KIO::CopyJob *linkJob = KIO::linkAs(uSrc, dest);
            kjob = linkJob;
            KIO::FileUndoManager::self()->recordCopyJob(linkJob);
        } else if (src.startsWith(QLatin1String(":/"))) {
            QFile srcFile(src);
            if (!srcFile.open(QIODevice::ReadOnly)) {
                return;
            }
            // The QFile won't live long enough for the job, so let's buffer the contents
            const QByteArray srcBuf(srcFile.readAll());
            KIO::StoredTransferJob *putJob = KIO::storedPut(srcBuf, dest, -1);
            kjob = putJob;
            KIO::FileUndoManager::self()->recordJob(KIO::FileUndoManager::Put, QList<QUrl>(), dest, putJob);
        } else {
            // qDebug() << "KIO::copyAs(" << uSrc.url() << "," << dest.url() << ")";
            KIO::CopyJob *job = KIO::copyAs(uSrc, dest);
            job->setDefaultPermissions(true);
            kjob = job;
            KIO::FileUndoManager::self()->recordCopyJob(job);
        }
        KJobWidgets::setWindow(kjob, m_parentWidget);
        QObject::connect(kjob, &KJob::result, q, &KNewFileMenu::slotResult);
    }
}

void KNewFileMenuPrivate::executeUrlDesktopFile(const KNewFileMenuSingleton::Entry &entry)
{
    KNameAndUrlInputDialog *dlg = new KNameAndUrlInputDialog(i18n("Name for new link:"), entry.comment, m_popupFiles.first(), m_parentWidget);
    m_copyData.m_templatePath = entry.templatePath;
    dlg->setModal(q->isModal());
    dlg->setAttribute(Qt::WA_DeleteOnClose);
    dlg->setWindowTitle(i18n("Create link to URL"));
    m_fileDialog = dlg;
    QObject::connect(dlg, &QDialog::accepted, q, [this]() {
        slotUrlDesktopFile();
    });
    dlg->show();
}

void KNewFileMenuPrivate::fillMenu()
{
    QMenu *menu = q->menu();
    menu->clear();
    m_menuDev->menu()->clear();
    m_newDirAction = nullptr;

    std::set<QString> seenTexts;
    QString lastTemplatePath;
    // these shall be put at special positions
    QAction *linkURL = nullptr;
    QAction *linkApp = nullptr;
    QAction *linkPath = nullptr;

    KNewFileMenuSingleton *s = kNewMenuGlobals();
    int idx = 0;
    for (auto &entry : *s->templatesList) {
        ++idx;
        if (entry.entryType != KNewFileMenuSingleton::Unknown) {
            // There might be a .desktop for that one already.

            // In fact, we skip any second item that has the same text as another one.
            // Duplicates in a menu look bad in any case.
            const auto [it, isInserted] = seenTexts.insert(entry.text);
            if (isInserted) {
                // const KNewFileMenuSingleton::Entry entry = templatesList->at(i-1);

                const QString templatePath = entry.templatePath;
                // The best way to identify the "Create Directory", "Link to Location", "Link to Application" was the template
                if (templatePath.endsWith(QLatin1String("emptydir"))) {
                    QAction *act = new QAction(q);
                    m_newDirAction = act;
                    act->setIcon(QIcon::fromTheme(entry.icon));
                    act->setText(i18nc("@item:inmenu Create New", "%1", entry.text));
                    act->setActionGroup(m_newMenuGroup);

                    // If there is a shortcut action copy its shortcut
                    if (m_newFolderShortcutAction) {
                        act->setShortcuts(m_newFolderShortcutAction->shortcuts());
                        // Both actions have now the same shortcut, so this will prevent the "Ambiguous shortcut detected" dialog.
                        act->setShortcutContext(Qt::WidgetShortcut);
                        // We also need to react to shortcut changes.
                        QObject::connect(m_newFolderShortcutAction, &QAction::changed, act, [act, this]() {
                            act->setShortcuts(m_newFolderShortcutAction->shortcuts());
                        });
                    }

                    menu->addAction(act);
                    menu->addSeparator();
                } else {
                    if (lastTemplatePath.startsWith(QDir::homePath()) && !templatePath.startsWith(QDir::homePath())) {
                        menu->addSeparator();
                    }
                    if (!m_supportedMimeTypes.isEmpty()) {
                        bool keep = false;

                        // We need to do MIME type filtering, for real files.
                        const bool createSymlink = entry.templatePath == QLatin1String("__CREATE_SYMLINK__");
                        if (createSymlink) {
                            keep = true;
                        } else if (!KDesktopFile::isDesktopFile(entry.templatePath)) {
                            // Determine MIME type on demand
                            QMimeDatabase db;
                            QMimeType mime;
                            if (entry.mimeType.isEmpty()) {
                                mime = db.mimeTypeForFile(entry.templatePath);
                                // qDebug() << entry.templatePath << "is" << mime.name();
                                entry.mimeType = mime.name();
                            } else {
                                mime = db.mimeTypeForName(entry.mimeType);
                            }
                            for (const QString &supportedMime : std::as_const(m_supportedMimeTypes)) {
                                if (mime.inherits(supportedMime)) {
                                    keep = true;
                                    break;
                                }
                            }
                        }

                        if (!keep) {
                            // qDebug() << "Not keeping" << entry.templatePath;
                            continue;
                        }
                    }

                    QAction *act = new QAction(q);
                    act->setData(idx);
                    act->setIcon(QIcon::fromTheme(entry.icon));
                    act->setText(i18nc("@item:inmenu Create New", "%1", entry.text));
                    act->setActionGroup(m_newMenuGroup);

                    // qDebug() << templatePath << entry.filePath;

                    if (templatePath.endsWith(QLatin1String("/URL.desktop"))) {
                        linkURL = act;
                    } else if (templatePath.endsWith(QLatin1String("/Program.desktop"))) {
                        linkApp = act;
                    } else if (entry.filePath.endsWith(QLatin1String("/linkPath.desktop"))) {
                        linkPath = act;
                    } else if (KDesktopFile::isDesktopFile(templatePath)) {
                        KDesktopFile df(templatePath);
                        if (df.readType() == QLatin1String("FSDevice")) {
                            m_menuDev->menu()->addAction(act);
                        } else {
                            menu->addAction(act);
                        }
                    } else {
                        if (!m_firstFileEntry) {
                            m_firstFileEntry = &entry;

                            // If there is a shortcut action copy its shortcut
                            if (m_newFileShortcutAction) {
                                act->setShortcuts(m_newFileShortcutAction->shortcuts());
                                // Both actions have now the same shortcut, so this will prevent the "Ambiguous shortcut detected" dialog.
                                act->setShortcutContext(Qt::WidgetShortcut);
                                // We also need to react to shortcut changes.
                                QObject::connect(m_newFileShortcutAction, &QAction::changed, act, [act, this]() {
                                    act->setShortcuts(m_newFileShortcutAction->shortcuts());
                                });
                            }
                        }
                        menu->addAction(act);
                    }
                }
            }
            lastTemplatePath = entry.templatePath;
        } else { // Separate system from personal templates
            Q_ASSERT(entry.entryType != 0);
            menu->addSeparator();
        }
    }

    if (m_supportedMimeTypes.isEmpty()) {
        menu->addSeparator();
        if (linkURL) {
            menu->addAction(linkURL);
        }
        if (linkPath) {
            menu->addAction(linkPath);
        }
        if (linkApp) {
            menu->addAction(linkApp);
        }
        Q_ASSERT(m_menuDev);
        if (!m_menuDev->menu()->isEmpty()) {
            menu->addAction(m_menuDev);
        }
    }
}

QUrl KNewFileMenuPrivate::mostLocalUrl(const QUrl &url)
{
    if (url.isLocalFile() || KProtocolInfo::protocolClass(url.scheme()) != QLatin1String(":local")) {
        return url;
    }

    KIO::StatJob *job = KIO::mostLocalUrl(url);
    KJobWidgets::setWindow(job, m_parentWidget);

    return job->exec() ? job->mostLocalUrl() : url;
}

void KNewFileMenuPrivate::slotAbortDialog()
{
    m_text = QString();
    if (m_creatingDirectory) {
        Q_EMIT q->directoryCreationRejected(m_baseUrl);
    } else {
        Q_EMIT q->fileCreationRejected(m_baseUrl);
    }
}

void KNewFileMenuPrivate::slotActionTriggered(QAction *action)
{
    q->trigger(); // was for kdesktop's slotNewMenuActivated() in kde3 times. Can't hurt to keep it...

    if (action == m_newDirAction) {
        q->createDirectory();
        return;
    }
    const int id = action->data().toInt();
    Q_ASSERT(id > 0);

    KNewFileMenuSingleton *s = kNewMenuGlobals();
    const KNewFileMenuSingleton::Entry entry = s->templatesList->at(id - 1);

    const bool createSymlink = entry.templatePath == QLatin1String("__CREATE_SYMLINK__");

    m_copyData = KNewFileMenuCopyData();

    if (createSymlink) {
        m_copyData.m_isSymlink = true;
        executeSymLink(entry);
    } else if (KDesktopFile::isDesktopFile(entry.templatePath)) {
        KDesktopFile df(entry.templatePath);
        if (df.readType() == QLatin1String("Link")) {
            executeUrlDesktopFile(entry);
        } else { // any other desktop file (Device, App, etc.)
            executeOtherDesktopFile(entry);
        }
    } else {
        executeRealFileOrDir(entry);
    }
}

void KNewFileMenuPrivate::slotCreateDirectory()
{
    // Automatically trim trailing spaces since they're pretty much always
    // unintentional and can cause issues on Windows in shared environments
    while (m_text.endsWith(QLatin1Char(' '))) {
        m_text.chop(1);
    }

    QUrl url;
    QUrl baseUrl = m_popupFiles.first();

    QString name = expandTilde(m_text);

    if (!name.isEmpty()) {
        if (Utils::isAbsoluteLocalPath(name)) {
            url = QUrl::fromLocalFile(name);
        } else {
            url = baseUrl;
            url.setPath(Utils::concatPaths(url.path(), name));
        }
    }

    KIO::Job *job;
    if (name.contains(QLatin1Char('/'))) {
        // If the name contains any slashes, use mkpath so that a/b/c works.
        job = KIO::mkpath(url, baseUrl);
        KIO::FileUndoManager::self()->recordJob(KIO::FileUndoManager::Mkpath, QList<QUrl>(), url, job);
    } else {
        // If not, use mkdir so it will fail if the name of an existing folder was used
        job = KIO::mkdir(url);
        KIO::FileUndoManager::self()->recordJob(KIO::FileUndoManager::Mkdir, QList<QUrl>(), url, job);
    }
    job->setProperty("newDirectoryURL", url);
    if (canPickFolderIcon(url)) {
        KConfigGroup cfg = stateConfig();
        cfg.writeEntry("ShowFolderIconPicker", m_chooseIconBox->isExpanded());

        const QString customIconName = m_iconLabel->property("iconName").toString();
        if (!isDefaultFolderIcon(customIconName)) {
            job->setProperty("newDirectoryIconName", customIconName);

            QStringList icons = cfg.readEntry("FolderIcons", QStringList());
            // Move to the end of the list.
            icons.removeOne(customIconName);
            icons.append(customIconName);
            cfg.writeEntry("FolderIcons", icons);
        }
    }
    job->uiDelegate()->setAutoErrorHandlingEnabled(true);
    KJobWidgets::setWindow(job, m_parentWidget);

    if (job) {
        // We want the error handling to be done by slotResult so that subclasses can reimplement it
        job->uiDelegate()->setAutoErrorHandlingEnabled(false);
        QObject::connect(job, &KJob::result, q, &KNewFileMenu::slotResult);
    }
    slotAbortDialog();
}

static QStringList getInstalledTemplates()
{
    QStringList list = QStandardPaths::locateAll(QStandardPaths::GenericDataLocation, QStringLiteral("templates"), QStandardPaths::LocateDirectory);
    QString templateFolder = QStandardPaths::standardLocations(QStandardPaths::TemplatesLocation).value(0);
    static bool templateWarningShown = false;
    // Some distros set TemplatesLocation to home dir, which means it hasn't been set up and should be ignored
    // Otherwise everything in the home folder will be used as a template
    if (templateFolder != QDir::homePath()) {
        list << templateFolder;
    } else if (!templateWarningShown) {
        qCWarning(KFILEWIDGETS_LOG) << "Your 'templates' folder is set to your home folder. "
                                       "This is probably an error in your settings. Ignoring it. "
                                       "You can change the setting by running `systemsettings kcm_desktoppaths`. ";
        templateWarningShown = true;
    }
    return list;
}

static QStringList getTemplateFilePaths(const QStringList &templates)
{
    QDir dir;
    QStringList files;
    for (const QString &path : templates) {
        dir.setPath(path);
        const QStringList entryList = dir.entryList(QDir::NoDotAndDotDot | QDir::AllEntries);
        files.reserve(files.size() + entryList.size());
        for (const QString &entry : entryList) {
            const QString file = Utils::concatPaths(dir.path(), entry);
            files.append(file);
        }
    }
    return files;
}

void KNewFileMenuPrivate::slotFillTemplates()
{
    KNewFileMenuSingleton *instance = kNewMenuGlobals();
    // qDebug();

    const QStringList installedTemplates = getInstalledTemplates();
    const QStringList qrcTemplates{QStringLiteral(":/kio5/newfile-templates")};
    const QStringList templates = qrcTemplates + installedTemplates;

    // Ensure any changes in the templates dir will call this
    if (!instance->dirWatch) {
        instance->dirWatch = std::make_unique<KDirWatch>();
        for (const QString &dir : installedTemplates) {
            instance->dirWatch->addDir(dir);
        }

        auto slotFunc = [this]() {
            slotFillTemplates();
        };
        QObject::connect(instance->dirWatch.get(), &KDirWatch::dirty, q, slotFunc);
        QObject::connect(instance->dirWatch.get(), &KDirWatch::created, q, slotFunc);
        QObject::connect(instance->dirWatch.get(), &KDirWatch::deleted, q, slotFunc);
        // Ok, this doesn't cope with new dirs in XDG_DATA_DIRS, but that's another story
    }

    // Look into "templates" dirs.
    QStringList files = getTemplateFilePaths(templates);

    // Remove files that begin with a dot.
    // dir.entryList(QDir::NoDotAndDotDot | QDir::AllEntries) does not disregard internal files that
    // start with a dot like :/kio5/newfile-templates/.source
    auto removeFunc = [](const QString &path) {
        QFileInfo fileinfo(path);
        return fileinfo.fileName().startsWith(QLatin1Char('.'));
    };
    files.erase(std::remove_if(files.begin(), files.end(), removeFunc), files.end());

    // Ensure desktop files are always before template files
    // This ensures consistent behavior
    std::partition(files.begin(), files.end(), [](const QString &a) {
        return a.endsWith(QStringLiteral(".desktop"));
    });

    std::vector<EntryInfo> uniqueEntries;
    QMimeDatabase db;
    for (const QString &file : files) {
        // qDebug() << file;
        KNewFileMenuSingleton::Entry entry;
        entry.entryType = KNewFileMenuSingleton::Unknown; // not parsed yet
        QString url;
        QString key;

        if (file.endsWith(QLatin1String(".desktop"))) {
            entry.filePath = file;
            const KDesktopFile config(file);
            url = config.desktopGroup().readEntry("URL");
            key = config.desktopGroup().readEntry("Name");
        }
        // Preparse non-.desktop files
        else {
            QFileInfo fileinfo(file);
            url = file;
            key = fileinfo.fileName();
            entry.entryType = KNewFileMenuSingleton::Template;
            entry.text = fileinfo.baseName();
            entry.filePath = fileinfo.completeBaseName();
            entry.templatePath = file;
            QMimeType mime = db.mimeTypeForFile(file);
            entry.mimeType = mime.name();
            entry.icon = mime.iconName();
            entry.comment = i18nc("@label:textbox Prompt for new file of type", "Enter %1 filename:", mime.comment());
        }
        // Put Directory first in the list (a bit hacky),
        // and TextFile before others because it's the most used one.
        // This also sorts by user-visible name.
        // The rest of the re-ordering is done in fillMenu.
        if (file.endsWith(QLatin1String("Directory.desktop"))) {
            key.prepend(QLatin1Char('0'));
        } else if (file.startsWith(QDir::homePath())) {
            key.prepend(QLatin1Char('1'));
        } else if (file.endsWith(QLatin1String("TextFile.desktop"))) {
            key.prepend(QLatin1Char('2'));
        } else {
            key.prepend(QLatin1Char('3'));
        }

        EntryInfo eInfo = {key, url, entry};
        auto it = std::find_if(uniqueEntries.begin(), uniqueEntries.end(), [&url](const EntryInfo &info) {
            return url == info.url;
        });

        if (it != uniqueEntries.cend()) {
            *it = eInfo;
        } else {
            uniqueEntries.push_back(eInfo);
        }
    }

    std::sort(uniqueEntries.begin(), uniqueEntries.end(), [](const EntryInfo &a, const EntryInfo &b) {
        return a.key < b.key;
    });

    ++instance->templatesVersion;
    instance->filesParsed = false;

    instance->templatesList->clear();

    instance->templatesList->reserve(uniqueEntries.size());
    for (const auto &info : uniqueEntries) {
        instance->templatesList->append(info.entry);
    };
}

void KNewFileMenuPrivate::_k_slotOtherDesktopFile(KPropertiesDialog *sender)
{
    // The properties dialog took care of the copying, so we're done
    Q_EMIT q->fileCreated(sender->url());
}

void KNewFileMenuPrivate::slotOtherDesktopFileClosed()
{
    QFile::remove(m_tempFileToDelete);
}

void KNewFileMenuPrivate::slotRealFileOrDir()
{
    // Automatically trim trailing spaces since they're pretty much always
    // unintentional and can cause issues on Windows in shared environments
    while (m_text.endsWith(QLatin1Char(' '))) {
        m_text.chop(1);
    }
    m_copyData.m_chosenFileName = m_text;
    slotAbortDialog();
    executeStrategy();
}

void KNewFileMenuPrivate::slotSymLink()
{
    KNameAndUrlInputDialog *dlg = static_cast<KNameAndUrlInputDialog *>(m_fileDialog);

    m_copyData.m_chosenFileName = dlg->name(); // no path
    const QString linkTarget = dlg->urlText();

    if (m_copyData.m_chosenFileName.isEmpty() || linkTarget.isEmpty()) {
        return;
    }

    m_copyData.m_src = linkTarget;
    executeStrategy();
}

void KNewFileMenuPrivate::_k_delayedSlotTextChanged()
{
    m_delayedSlotTextChangedTimer->start();
    m_buttonBox->button(QDialogButtonBox::Ok)->setEnabled(!m_lineEdit->text().isEmpty());
}

void KNewFileMenuPrivate::_k_slotTextChanged(const QString &text)
{
    // Validate input, displaying a KMessageWidget for questionable names

    if (text.isEmpty()) {
        m_messageWidget->hide();
        m_buttonBox->button(QDialogButtonBox::Ok)->setEnabled(false);
    }

    // Don't allow creating folders that would mask . or ..
    else if (text == QLatin1Char('.') || text == QLatin1String("..")) {
        m_messageWidget->setText(
            xi18nc("@info", "The name <filename>%1</filename> cannot be used because it is reserved for use by the operating system.", text));
        m_messageWidget->setMessageType(KMessageWidget::Error);
        m_messageWidget->animatedShow();
        m_buttonBox->button(QDialogButtonBox::Ok)->setEnabled(false);
    }

    // File or folder would be hidden; show warning
    else if (text.startsWith(QLatin1Char('.'))) {
        m_messageWidget->setText(xi18nc("@info", "The name <filename>%1</filename> starts with a dot, so it will be hidden by default.", text));
        m_messageWidget->setMessageType(KMessageWidget::Warning);
        m_messageWidget->animatedShow();
    }

    // File or folder begins with a space; show warning
    else if (text.startsWith(QLatin1Char(' '))) {
        m_messageWidget->setText(xi18nc("@info",
                                        "The name <filename>%1</filename> starts with a space, which will result in it being shown before other items when "
                                        "sorting alphabetically, among other potential oddities.",
                                        text));
        m_messageWidget->setMessageType(KMessageWidget::Warning);
        m_messageWidget->animatedShow();
    }
#ifndef Q_OS_WIN
    // Inform the user that slashes in folder names create a directory tree
    else if (text.contains(QLatin1Char('/'))) {
        if (m_creatingDirectory) {
            QStringList folders = text.split(QLatin1Char('/'));
            if (!folders.isEmpty()) {
                if (folders.first().isEmpty()) {
                    folders.removeFirst();
                }
            }
            QString label;
            if (folders.count() > 1) {
                label = i18n("Using slashes in folder names will create sub-folders, like so:");
                QString indentation = QString();
                for (const QString &folder : std::as_const(folders)) {
                    label.append(QLatin1Char('\n'));
                    label.append(indentation);
                    label.append(folder);
                    label.append(QStringLiteral("/"));
                    indentation.append(QStringLiteral("    "));
                }
            } else {
                label = i18n("Using slashes in folder names will create sub-folders.");
            }
            m_messageWidget->setText(label);
            m_messageWidget->setMessageType(KMessageWidget::Information);
            m_messageWidget->animatedShow();
        }
    }
#endif

#ifdef Q_OS_WIN
    // Slashes and backslashes are not allowed in Windows filenames; show error
    else if (text.contains(QLatin1Char('/'))) {
        m_messageWidget->setText(i18n("Slashes cannot be used in file and folder names."));
        m_messageWidget->setMessageType(KMessageWidget::Error);
        m_messageWidget->animatedShow();
        m_buttonBox->button(QDialogButtonBox::Ok)->setEnabled(false);
    } else if (text.contains(QLatin1Char('\\'))) {
        m_messageWidget->setText(i18n("Backslashes cannot be used in file and folder names."));
        m_messageWidget->setMessageType(KMessageWidget::Error);
        m_messageWidget->animatedShow();
        m_buttonBox->button(QDialogButtonBox::Ok)->setEnabled(false);
    }
#endif

    // Using a tilde to begin a file or folder name is not recommended
    else if (text.startsWith(QLatin1Char('~'))) {
#ifndef Q_OS_WIN
        const bool wasExpanded = !KShell::tildeExpand(text).startsWith(QStringLiteral("~"));
        if (wasExpanded && text.length() > 1) {
            m_messageWidget->setText(
                xi18nc("@error",
                       "Creating a file or folder with the name <filename>%1</filename> is not possible since it would overlap with the location of a user's home folder.",
                       text,
                       KShell::tildeExpand(text)));
            m_messageWidget->setMessageType(KMessageWidget::Error);
            m_messageWidget->animatedShow();
            m_buttonBox->button(QDialogButtonBox::Ok)->setEnabled(false);
        } else {
#endif
            m_messageWidget->setText(i18n(
                "Starting a file or folder name with a tilde is not recommended because it may be confusing or dangerous when using the terminal to delete "
                "things."));
            m_messageWidget->setMessageType(KMessageWidget::Warning);
            m_messageWidget->animatedShow();
#ifndef Q_OS_WIN
            m_buttonBox->button(QDialogButtonBox::Ok)->setEnabled(true);
        }
#endif
    } else {
        m_messageWidget->hide();
    }

    if (!text.isEmpty()) {
        // Check file does not already exists
        m_statRunning = true;
        QUrl url;
        if (m_creatingDirectory && text.at(0) == QLatin1Char('~')) {
            url = QUrl::fromUserInput(KShell::tildeExpand(text));
        } else {
            url = QUrl(m_baseUrl.toString() + QLatin1Char('/') + text);
        }
        KIO::StatJob *job = KIO::stat(url, KIO::StatJob::StatSide::DestinationSide, KIO::StatDetail::StatBasic, KIO::HideProgressInfo);
        QObject::connect(job, &KJob::result, m_fileDialog, [this](KJob *job) {
            _k_slotStatResult(job);
        });
        job->start();
    }

    m_text = text;
}

void KNewFileMenu::setSelectDirWhenAlreadyExist(bool shouldSelectExistingDir)
{
    d->m_selectDirWhenAlreadyExists = shouldSelectExistingDir;
}

void KNewFileMenuPrivate::_k_slotStatResult(KJob *job)
{
    m_statRunning = false;
    KIO::StatJob *statJob = static_cast<KIO::StatJob *>(job);
    // ignore stat Result when the lineEdit has changed
    const QUrl url = statJob->url().adjusted(QUrl::StripTrailingSlash);
    if (m_creatingDirectory && m_lineEdit->text().startsWith(QLatin1Char('~'))) {
        if (url.path() != KShell::tildeExpand(m_lineEdit->text())) {
            return;
        }
    } else if (url.fileName() != m_lineEdit->text()) {
        return;
    }
    bool accepted = m_acceptedPressed;
    m_acceptedPressed = false;
    auto error = job->error();
    if (error) {
        if (error == KIO::ERR_DOES_NOT_EXIST) {
            // fine for file creation
            if (accepted) {
                m_fileDialog->accept();
            }
        } else {
            qWarning() << error << job->errorString();
        }
    } else {
        bool shouldEnable = false;
        KMessageWidget::MessageType messageType = KMessageWidget::Error;

        const KIO::UDSEntry &entry = statJob->statResult();
        if (entry.isDir()) {
            if (m_selectDirWhenAlreadyExists && m_creatingDirectory) {
                // allow "overwrite" of dir
                messageType = KMessageWidget::Information;
                shouldEnable = true;
            }
            m_messageWidget->setText(xi18nc("@info", "A directory with name <filename>%1</filename> already exists.", m_text));
        } else {
            m_messageWidget->setText(xi18nc("@info", "A file with name <filename>%1</filename> already exists.", m_text));
        }
        m_messageWidget->setMessageType(messageType);
        m_messageWidget->animatedShow();
        m_buttonBox->button(QDialogButtonBox::Ok)->setEnabled(shouldEnable);

        if (accepted && shouldEnable) {
            m_fileDialog->accept();
        }
    }
}

void KNewFileMenuPrivate::slotUrlDesktopFile()
{
    KNameAndUrlInputDialog *dlg = static_cast<KNameAndUrlInputDialog *>(m_fileDialog);
    QString name = dlg->name();
    const QLatin1String ext(".desktop");
    if (!name.endsWith(ext)) {
        name += ext;
    }
    m_copyData.m_chosenFileName = name; // no path
    QUrl linkUrl = dlg->url();

    // Filter user input so that short uri entries, e.g. www.kde.org, are
    // handled properly. This not only makes the icon detection below work
    // properly, but opening the URL link where the short uri will not be
    // sent to the application (opening such link Konqueror fails).
    KUriFilterData uriData;
    uriData.setData(linkUrl); // the url to put in the file
    uriData.setCheckForExecutables(false);

    if (KUriFilter::self()->filterUri(uriData, QStringList{QStringLiteral("kshorturifilter")})) {
        linkUrl = uriData.uri();
    }

    if (m_copyData.m_chosenFileName.isEmpty() || linkUrl.isEmpty()) {
        return;
    }

    // It's a "URL" desktop file; we need to make a temp copy of it, to modify it
    // before copying it to the final destination [which could be a remote protocol]
    QTemporaryFile tmpFile;
    tmpFile.setAutoRemove(false); // done below
    if (!tmpFile.open()) {
        qCritical() << "Couldn't create temp file!";
        return;
    }

    if (!checkSourceExists(m_copyData.m_templatePath)) {
        return;
    }

    // First copy the template into the temp file
    QFile file(m_copyData.m_templatePath);
    if (!file.open(QIODevice::ReadOnly)) {
        qCritical() << "Couldn't open template" << m_copyData.m_templatePath;
        return;
    }
    const QByteArray data = file.readAll();
    tmpFile.write(data);
    const QString tempFileName = tmpFile.fileName();
    Q_ASSERT(!tempFileName.isEmpty());
    tmpFile.close();
    file.close();

    KDesktopFile df(tempFileName);
    KConfigGroup group = df.desktopGroup();

    if (linkUrl.isLocalFile()) {
        KFileItem fi(linkUrl);
        group.writeEntry("Icon", fi.iconName());
    } else {
        group.writeEntry("Icon", KProtocolInfo::icon(linkUrl.scheme()));
    }

    group.writePathEntry("URL", linkUrl.toDisplayString());
    group.writeEntry("Name", dlg->name()); // Used as user-visible name by kio_desktop
    df.sync();

    m_copyData.m_src = tempFileName;
    m_copyData.m_tempFileToDelete = tempFileName;

    executeStrategy();
}

KNewFileMenu::KNewFileMenu(QObject *parent)
    : KActionMenu(QIcon::fromTheme(QStringLiteral("document-new")), i18n("Create New"), parent)
    , d(std::make_unique<KNewFileMenuPrivate>(this))
{
    // Don't fill the menu yet
    // We'll do that in checkUpToDate (should be connected to aboutToShow)
    d->m_newMenuGroup = new QActionGroup(this);
    connect(d->m_newMenuGroup, &QActionGroup::triggered, this, [this](QAction *action) {
        d->slotActionTriggered(action);
    });

    // Connect directory creation signals
    connect(this, &KNewFileMenu::directoryCreationStarted, this, [this] {
        d->m_isCreateDirectoryRunning = true;
    });
    connect(this, &KNewFileMenu::directoryCreated, this, [this] {
        d->m_isCreateDirectoryRunning = false;
    });
    connect(this, &KNewFileMenu::directoryCreationRejected, this, [this] {
        d->m_isCreateDirectoryRunning = false;
    });

    // Connect file creation signals
    connect(this, &KNewFileMenu::fileCreationStarted, this, [this] {
        d->m_isCreateFileRunning = true;
    });
    connect(this, &KNewFileMenu::fileCreated, this, [this] {
        d->m_isCreateFileRunning = false;
    });
    connect(this, &KNewFileMenu::fileCreationRejected, this, [this] {
        d->m_isCreateFileRunning = false;
    });

    d->m_parentWidget = qobject_cast<QWidget *>(parent);
    d->m_newDirAction = nullptr;

    d->m_menuDev = new KActionMenu(QIcon::fromTheme(QStringLiteral("drive-removable-media")), i18n("Link to Device"), this);
}

KNewFileMenu::~KNewFileMenu() = default;

void KNewFileMenu::checkUpToDate()
{
    KNewFileMenuSingleton *s = kNewMenuGlobals();
    // qDebug() << this << "m_menuItemsVersion=" << d->m_menuItemsVersion
    //              << "s->templatesVersion=" << s->templatesVersion;
    if (d->m_menuItemsVersion < s->templatesVersion || s->templatesVersion == 0) {
        // qDebug() << "recreating actions";
        // We need to clean up the action collection
        // We look for our actions using the group
        qDeleteAll(d->m_newMenuGroup->actions());

        if (!s->templatesList) { // No templates list up to now
            s->templatesList = new KNewFileMenuSingleton::EntryList;
            d->slotFillTemplates();
            s->parseFiles();
        }

        // This might have been already done for other popupmenus,
        // that's the point in s->filesParsed.
        if (!s->filesParsed) {
            s->parseFiles();
        }

        d->fillMenu();

        d->m_menuItemsVersion = s->templatesVersion;
    }
}

void KNewFileMenu::createDirectory()
{
    if (d->m_popupFiles.isEmpty()) {
        return;
    }

    d->m_baseUrl = d->m_popupFiles.first();

    if (d->m_isCreateDirectoryRunning) {
        qCWarning(KFILEWIDGETS_LOG) << "Directory creation is already running for " << d->m_baseUrl;
    }

    QString name = !d->m_text.isEmpty() ? d->m_text : i18nc("Default name for a new folder", "New Folder");

    auto nameJob = new KIO::NameFinderJob(d->m_baseUrl, name, this);
    connect(nameJob, &KJob::result, this, [nameJob, name, this]() mutable {
        if (!nameJob->error()) {
            d->m_baseUrl = nameJob->baseUrl();
            name = nameJob->finalName();
        }
        d->showNewDirNameDlg(name);
    });
    nameJob->start();
    Q_EMIT directoryCreationStarted(d->m_baseUrl);
}

bool KNewFileMenu::isCreateDirectoryRunning()
{
    return d->m_isCreateDirectoryRunning;
}

void KNewFileMenuPrivate::showNewDirNameDlg(const QString &name)
{
    initDialog();

    m_fileDialog->setWindowTitle(m_windowTitle.isEmpty() ? i18nc("@title:window", "Create New Folder") : m_windowTitle);

    m_label->setText(i18n("Create new folder in %1:", m_baseUrl.toDisplayString(QUrl::PreferLocalFile | QUrl::StripTrailingSlash)));

    m_lineEdit->setText(name);
    m_lineEdit->setPlaceholderText(i18nc("@info:placeholder", "Enter folder name"));

    const QString defaultFolderIconName = QStringLiteral("inode-directory");
    setIcon(QIcon::fromTheme(defaultFolderIconName));

    if (canPickFolderIcon(m_baseUrl)) {
        m_iconGroup = new QActionGroup{m_fileDialog};
        m_iconGroup->setExclusionPolicy(QActionGroup::ExclusionPolicy::ExclusiveOptional);

        static constexpr int s_folderIconsCount = 2 * 10 - 1; // default icon is always added.

        int x = 0;
        int y = 0;
        QStringList icons = {// colors.
                             // default folder icon goes here.
                             QStringLiteral("folder-red"),
                             QStringLiteral("folder-yellow"),
                             QStringLiteral("folder-orange"),
                             QStringLiteral("folder-green"),
                             QStringLiteral("folder-cyan"),
                             QStringLiteral("folder-blue"),
                             QStringLiteral("folder-violet"),
                             QStringLiteral("folder-brown"),
                             QStringLiteral("folder-grey"),
                             // emblems.
                             QStringLiteral("folder-bookmark"),
                             QStringLiteral("folder-cloud"),
                             QStringLiteral("folder-development"),
                             QStringLiteral("folder-games"),
                             QStringLiteral("folder-mail"),
                             QStringLiteral("folder-music"),
                             QStringLiteral("folder-print"),
                             QStringLiteral("folder-tar"),
                             QStringLiteral("folder-temp"),
                             QStringLiteral("folder-important")};

        const QStringList storedFolderIcons = stateConfig().readEntry(QStringLiteral("FolderIcons"), QStringList());
        for (const QString &icon : storedFolderIcons) {
            if (!icons.contains(icon)) {
                icons.append(icon);
            }
        }

        while (icons.size() > s_folderIconsCount) {
            icons.removeFirst();
        }

        icons.prepend(defaultFolderIconName);

        QWidget *lastWidget = m_chooseIconBox;

        for (const QString &icon : icons) {
            const bool isFirstButton = (x == 0 && y == 0);

            auto *action = new QAction{m_iconGroup};
            action->setIcon(QIcon::fromTheme(icon));

            const QString displayName = isFirstButton ? i18n("Default") : icon;
            action->setToolTip(displayName);
            action->setCheckable(true);
            action->setChecked(isFirstButton);

            auto *button = new QToolButton{m_fileDialog};
            button->setDefaultAction(action);
            button->setToolButtonStyle(Qt::ToolButtonIconOnly);
            button->setIconSize(QSize(KIconLoader::SizeMedium, KIconLoader::SizeMedium));

            QWidget::setTabOrder(lastWidget, button);
            lastWidget = button;

            m_folderIconGrid->addWidget(button, y, x++);
            if (x == icons.size() / 2) {
                x = 0;
                ++y;
            }
        }

        QWidget::setTabOrder(lastWidget, m_chooseIconButton);

        QObject::connect(m_iconGroup, &QActionGroup::triggered, q, [this](QAction *action) {
            setIcon(action->icon());
            // We need ExclusiveOptional so that custom icon can have no button checked
            // but we never want the user uncheck a button manually.
            action->setChecked(true);
        });
        QObject::connect(m_chooseIconButton, &QPushButton::clicked, q, [this] {
            KIconDialog dialog{m_fileDialog};
            dialog.setup(KIconLoader::Desktop, KIconLoader::Place);
            const QString currentIconName = m_iconLabel->property("iconName").toString();
            if (!isDefaultFolderIcon(currentIconName)) {
                dialog.setSelectedIcon(currentIconName);
            }
            const QString iconName = dialog.openDialog();
            if (iconName.isEmpty()) {
                return;
            }

            if (isDefaultFolderIcon(iconName)) {
                m_iconGroup->actions().first()->setChecked(true);
            } else {
                const auto actions = m_iconGroup->actions();
                for (auto *action : actions) {
                    // No break so none are checked when no preset was found.
                    action->setChecked(action->icon().name() == iconName);
                }
            }

            // setChecked does not emit triggered, update the icon manually.
            if (m_iconGroup->checkedAction()) {
                setIcon(m_iconGroup->checkedAction()->icon());
            } else {
                setIcon(QIcon::fromTheme(iconName));
            }
        });

        if (stateConfig().readEntry("ShowFolderIconPicker", false)) {
            m_chooseIconBox->setExpanded(true);
        }
        m_chooseIconBox->show();
    }

    m_creatingDirectory = true;
    _k_slotTextChanged(name); // have to save string in m_text in case user does not touch dialog
    QObject::connect(m_lineEdit, &QLineEdit::textChanged, q, [this]() {
        _k_delayedSlotTextChanged();
    });
    m_delayedSlotTextChangedTimer->callOnTimeout(m_lineEdit, [this]() {
        _k_slotTextChanged(m_lineEdit->text());
    });

    QObject::connect(m_fileDialog, &QDialog::accepted, q, [this]() {
        slotCreateDirectory();
    });
    QObject::connect(m_fileDialog, &QDialog::rejected, q, [this]() {
        slotAbortDialog();
    });

    m_fileDialog->show();
    m_lineEdit->selectAll();
    m_lineEdit->setFocus();
}

void KNewFileMenu::createFile()
{
    if (d->m_popupFiles.isEmpty()) {
        Q_EMIT fileCreationRejected(QUrl());
        return;
    }

    checkUpToDate();
    if (!d->m_firstFileEntry) {
        Q_EMIT fileCreationRejected(QUrl());
        return;
    }

    if (!d->m_isCreateFileRunning) {
        d->executeRealFileOrDir(*d->m_firstFileEntry);
    } else {
        qCWarning(KFILEWIDGETS_LOG) << "File creation is already running for " << d->m_firstFileEntry;
    }
}

bool KNewFileMenu::isCreateFileRunning()
{
    return d->m_isCreateFileRunning;
}

bool KNewFileMenu::isModal() const
{
    return d->m_modal;
}

void KNewFileMenu::setModal(bool modal)
{
    d->m_modal = modal;
}

void KNewFileMenu::setParentWidget(QWidget *parentWidget)
{
    d->m_parentWidget = parentWidget;
}

void KNewFileMenu::setSupportedMimeTypes(const QStringList &mime)
{
    d->m_supportedMimeTypes = mime;
}

void KNewFileMenu::setWindowTitle(const QString &title)
{
    d->m_windowTitle = title;
}

void KNewFileMenu::slotResult(KJob *job)
{
    if (job->error()) {
        if (job->error() == KIO::ERR_DIR_ALREADY_EXIST && d->m_selectDirWhenAlreadyExists) {
            auto *simpleJob = ::qobject_cast<KIO::SimpleJob *>(job);
            if (simpleJob) {
                const QUrl jobUrl = simpleJob->url();
                // Select the existing dir
                Q_EMIT selectExistingDir(jobUrl);
            }
        } else { // All other errors
            static_cast<KIO::Job *>(job)->uiDelegate()->showErrorMessage();
        }
    } else {
        // Was this a copy or a mkdir?
        if (job->property("newDirectoryURL").isValid()) {
            QUrl newDirectoryURL = job->property("newDirectoryURL").toUrl();
            const QString newDirectoryIconName = job->property("newDirectoryIconName").toString();

            // Apply custom folder icon, if applicable.
            if (!isDefaultFolderIcon(newDirectoryIconName)) {
                const QUrl localUrl = d->mostLocalUrl(newDirectoryURL);
                KDesktopFile desktopFile{localUrl.toLocalFile() + QLatin1String("/.directory")};
                desktopFile.desktopGroup().writeEntry(QStringLiteral("Icon"), newDirectoryIconName);
            }
            Q_EMIT directoryCreated(newDirectoryURL);
        } else {
            KIO::CopyJob *copyJob = ::qobject_cast<KIO::CopyJob *>(job);
            if (copyJob) {
                const QUrl destUrl = copyJob->destUrl();
                const QUrl localUrl = d->mostLocalUrl(destUrl);
                if (localUrl.isLocalFile()) {
                    // Normal (local) file. Need to "touch" it, kio_file copied the mtime.
                    (void)::utime(QFile::encodeName(localUrl.toLocalFile()).constData(), nullptr);
                }
                Q_EMIT fileCreated(destUrl);
            } else if (KIO::SimpleJob *simpleJob = ::qobject_cast<KIO::SimpleJob *>(job)) {
                // Called in the storedPut() case
#ifdef WITH_QTDBUS
                org::kde::KDirNotify::emitFilesAdded(simpleJob->url().adjusted(QUrl::RemoveFilename | QUrl::StripTrailingSlash));
#endif
                Q_EMIT fileCreated(simpleJob->url());
            }
        }
    }
    if (!d->m_tempFileToDelete.isEmpty()) {
        QFile::remove(d->m_tempFileToDelete);
    }
}

QStringList KNewFileMenu::supportedMimeTypes() const
{
    return d->m_supportedMimeTypes;
}

void KNewFileMenu::setWorkingDirectory(const QUrl &directory)
{
    d->m_popupFiles = {directory};

    if (directory.isEmpty()) {
        d->m_newMenuGroup->setEnabled(false);
    } else {
        if (KProtocolManager::supportsWriting(directory)) {
            d->m_newMenuGroup->setEnabled(true);
            if (d->m_newDirAction) {
                d->m_newDirAction->setEnabled(KProtocolManager::supportsMakeDir(directory)); // e.g. trash:/
            }
        } else {
            d->m_newMenuGroup->setEnabled(true);
        }
    }
}

QUrl KNewFileMenu::workingDirectory() const
{
    return d->m_popupFiles.isEmpty() ? QUrl() : d->m_popupFiles.first();
}

void KNewFileMenu::setNewFolderShortcutAction(QAction *action)
{
    d->m_newFolderShortcutAction = action;
}

void KNewFileMenu::setNewFileShortcutAction(QAction *action)
{
    d->m_newFileShortcutAction = action;
}

#include "moc_knewfilemenu.cpp"
