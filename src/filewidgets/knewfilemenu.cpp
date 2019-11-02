/* This file is part of the KDE project
   Copyright (C) 1998-2009 David Faure <faure@kde.org>
                 2003      Sven Leiber <s.leiber@web.de>

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public
   License as published by the Free Software Foundation; either
   version 2 or at your option version 3.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public License
   along with this library; see the file COPYING.LIB.  If not, write to
   the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
   Boston, MA 02110-1301, USA.
*/

#include "knewfilemenu.h"
#include "../pathhelpers_p.h"
#include "knameandurlinputdialog.h"

#include <QDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QVBoxLayout>
#include <QList>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QPushButton>
#include <QStandardPaths>

#include <qtemporaryfile.h>
#include <kactioncollection.h>
#include <kconfiggroup.h>
#include <QDebug>
#include <kdesktopfile.h>
#include <kdirwatch.h>
#include <kdirnotify.h>
#include <kjobwidgets.h>
#include <klocalizedstring.h>
#include <kmessagebox.h>
#include <kmessagewidget.h>
#include <kprotocolinfo.h>
#include <kprotocolmanager.h>
#include <krun.h>
#include <kshell.h>
#include <kio/job.h>
#include <kio/copyjob.h>
#include <kio/jobuidelegate.h>
#include <kio/fileundomanager.h>
#include <kio/mkpathjob.h>
#include <kurifilter.h>
#include <kfileutils.h>

#include <kpropertiesdialog.h>
#include <qmimedatabase.h>
#ifdef Q_OS_WIN
#include <sys/utime.h>
#else
#include <utime.h>
#endif

static QString expandTilde(const QString &name, bool isfile = false)
{
    if (!name.isEmpty() && (!isfile || name[0] == QLatin1Char('\\'))) {
        const QString expandedName = KShell::tildeExpand(name);
        // When a tilde mark cannot be properly expanded, the above call
        // returns an empty string...
        if (!expandedName.isEmpty()) {
            return expandedName;
        }
    }

    return name;
}

// Singleton, with data shared by all KNewFileMenu instances
class KNewFileMenuSingleton
{
public:
    KNewFileMenuSingleton()
        : dirWatch(nullptr),
          filesParsed(false),
          templatesList(nullptr),
          templatesVersion(0)
    {
    }

    ~KNewFileMenuSingleton()
    {
        delete dirWatch;
        delete templatesList;
    }

    /**
     * Opens the desktop files and completes the Entry list
     * Input: the entry list. Output: the entry list ;-)
     */
    void parseFiles();

    /**
     * For entryType
     * LINKTOTEMPLATE: a desktop file that points to a file or dir to copy
     * TEMPLATE: a real file to copy as is (the KDE-1.x solution)
     * SEPARATOR: to put a separator in the menu
     * 0 means: not parsed, i.e. we don't know
     */
    enum EntryType { Unknown, LinkToTemplate = 1, Template, Separator };

    KDirWatch *dirWatch;

    struct Entry {
        QString text;
        QString filePath; // empty for Separator
        QString templatePath; // same as filePath for Template
        QString icon;
        EntryType entryType;
        QString comment;
        QString mimeType;
    };
    // NOTE: only filePath is known before we call parseFiles

    /**
     * List of all template files. It is important that they are in
     * the same order as the 'New' menu.
     */
    typedef QList<Entry> EntryList;

    /**
     * Set back to false each time new templates are found,
     * and to true on the first call to parseFiles
     */
    bool filesParsed;
    EntryList *templatesList;

    /**
     * Is increased when templatesList has been updated and
     * menu needs to be re-filled. Menus have their own version and compare it
     * to templatesVersion before showing up
     */
    int templatesVersion;
};

void KNewFileMenuSingleton::parseFiles()
{
    //qDebug();
    filesParsed = true;
    QMutableListIterator<KNewFileMenuSingleton::Entry> templIter(*templatesList);
    while (templIter.hasNext()) {
        KNewFileMenuSingleton::Entry &templ = templIter.next();
        const QString filePath = templ.filePath;
        if (!filePath.isEmpty()) {
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
                QString type = desktopFile.readType();
                if (type == QLatin1String("Link")) {
                    templatePath = desktopFile.desktopGroup().readPathEntry("URL", QString());
                    if (templatePath[0] != QLatin1Char('/') && !templatePath.startsWith(QLatin1String("__"))) {
                        if (templatePath.startsWith(QLatin1String("file:/"))) {
                            templatePath = QUrl(templatePath).toLocalFile();
                        } else {
                            // A relative path, then (that's the default in the files we ship)
                            const QStringRef linkDir = filePath.leftRef(filePath.lastIndexOf(QLatin1Char('/')) + 1 /*keep / */);
                            //qDebug() << "linkDir=" << linkDir;
                            templatePath = linkDir + templatePath;
                        }
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
                if (text.endsWith(QLatin1String(".desktop"))) {
                    text.chop(8);
                }
            }
            templ.text = text;
            /*// qDebug() << "Updating entry with text=" << text
                          << "entryType=" << templ.entryType
                          << "templatePath=" << templ.templatePath;*/
        } else {
            templ.entryType = KNewFileMenuSingleton::Separator;
        }
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
    explicit KNewFileMenuPrivate(KActionCollection *collection, KNewFileMenu *qq)
        : m_actionCollection(collection),
          q(qq)
    {}

    bool checkSourceExists(const QString &src);

    /**
      * The strategy used for other desktop files than Type=Link. Example: Application, Device.
      */
    void executeOtherDesktopFile(const KNewFileMenuSingleton::Entry &entry);

    /**
      * The strategy used for "real files or directories" (the common case)
      */
    void executeRealFileOrDir(const KNewFileMenuSingleton::Entry &entry);

    /**
      * Actually performs file handling. Reads in m_copyData for needed data, that has been collected by execute*() before
      */
    void executeStrategy();

    /**
      * The strategy used when creating a symlink
      */
    void executeSymLink(const KNewFileMenuSingleton::Entry &entry);

    /**
      * The strategy used for "url" desktop files
      */
    void executeUrlDesktopFile(const KNewFileMenuSingleton::Entry &entry);

    /**
     * Fills the menu from the templates list.
     */
    void fillMenu();

    /**
     * Tries to map a local URL for the given URL.
     */
    QUrl mostLocalUrl(const QUrl &url);

    /**
      * Just clears the string buffer d->m_text, but I need a slot for this to occur
      */
    void _k_slotAbortDialog();

    /**
     * Called when New->* is clicked
     */
    void _k_slotActionTriggered(QAction *action);

    /**
     * Callback function that reads in directory name from dialog and processes it
     */
    void _k_slotCreateDirectory(bool writeHiddenDir = false);

    /**
     * Callback function that reads in directory name from dialog and processes it. This will wirte
     * a hidden directory without further questions
     */
    void _k_slotCreateHiddenDirectory();

    /**
     * Fills the templates list.
     */
    void _k_slotFillTemplates();

    /**
      * Called when accepting the KPropertiesDialog (for "other desktop files")
      */
    void _k_slotOtherDesktopFile();

    /**
     * Called when closing the KPropertiesDialog is closed (whichever way, accepted and rejected)
     */
    void _k_slotOtherDesktopFileClosed();

    /**
      * Callback in KNewFileMenu for the RealFile Dialog. Handles dialog input and gives over
      * to executeStrategy()
      */
    void _k_slotRealFileOrDir();

    /**
      * Dialogs use this slot to write the changed string into KNewFile menu when the user
      * changes touches them
      */
    void _k_slotTextChanged(const QString &text);

    /**
      * Callback in KNewFileMenu for the Symlink Dialog. Handles dialog input and gives over
      * to executeStrategy()
      */
    void _k_slotSymLink();

    /**
      * Callback in KNewFileMenu for the Url/Desktop Dialog. Handles dialog input and gives over
      * to executeStrategy()
      */
    void _k_slotUrlDesktopFile();

    KActionCollection *m_actionCollection;
    QDialog *m_fileDialog;

    KActionMenu *m_menuDev;
    int m_menuItemsVersion = 0;
    QAction *m_newDirAction;
    KMessageWidget* m_messageWidget = nullptr;
    QDialogButtonBox* m_buttonBox = nullptr;

    // This is used to allow _k_slotTextChanged to know whether it's being used to
    // create a file or a directory without duplicating code across two functions
    bool m_creatingDirectory = false;
    bool m_viewShowsHiddenFiles = false;
    bool m_modal = true;

    /**
     * The action group that our actions belong to
     */
    QActionGroup *m_newMenuGroup;
    QWidget *m_parentWidget;

    /**
     * When the user pressed the right mouse button over an URL a popup menu
     * is displayed. The URL belonging to this popup menu is stored here.
     */
    QList<QUrl> m_popupFiles;

    QStringList m_supportedMimeTypes;
    QString m_tempFileToDelete; // set when a tempfile was created for a Type=URL desktop file
    QString m_text;

    KNewFileMenuSingleton::Entry *m_firstFileEntry = nullptr;

    KNewFileMenu * const q;

    KNewFileMenuCopyData m_copyData;
};

bool KNewFileMenuPrivate::checkSourceExists(const QString &src)
{
    if (!QFile::exists(src)) {
        qWarning() << src << "doesn't exist";

        QDialog *dialog = new QDialog(m_parentWidget);
        dialog->setWindowTitle(i18n("Sorry"));
        dialog->setObjectName(QStringLiteral("sorry"));
        dialog->setModal(q->isModal());
        dialog->setAttribute(Qt::WA_DeleteOnClose);

        m_buttonBox = new QDialogButtonBox(dialog);
        m_buttonBox->setStandardButtons(QDialogButtonBox::Ok);

        KMessageBox::createKMessageBox(dialog, m_buttonBox, QMessageBox::Warning,
                                       i18n("<qt>The template file <b>%1</b> does not exist.</qt>", src),
                                       QStringList(), QString(), nullptr, KMessageBox::NoExec);

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

    QList<QUrl>::const_iterator it = m_popupFiles.constBegin();
    for (; it != m_popupFiles.constEnd(); ++it) {
        QString text = entry.text;
        text.remove(QStringLiteral("...")); // the ... is fine for the menu item but not for the default filename
        text = text.trimmed(); // In some languages, there is a space in front of "...", see bug 268895
        // KDE5 TODO: remove the "..." from link*.desktop files and use i18n("%1...") when making
        // the action.
        QString name = text;
        text.append(QStringLiteral(".desktop"));

        const QUrl directory = mostLocalUrl(*it);
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
        QDialog *dlg = new KPropertiesDialog(templateUrl, directory, text, m_parentWidget);
        dlg->setModal(q->isModal());
        dlg->setAttribute(Qt::WA_DeleteOnClose);
        QObject::connect(dlg, SIGNAL(applied()), q, SLOT(_k_slotOtherDesktopFile()));
        if (usingTemplate) {
            QObject::connect(dlg, SIGNAL(propertiesClosed()), q, SLOT(_k_slotOtherDesktopFileClosed()));
        }
        dlg->show();
    }
    // We don't set m_src here -> there will be no copy, we are done.
}

void KNewFileMenuPrivate::executeRealFileOrDir(const KNewFileMenuSingleton::Entry &entry)
{
    // The template is not a desktop file
    // Show the small dialog for getting the destination filename
    QString text = entry.text;
    text.remove(QStringLiteral("...")); // the ... is fine for the menu item but not for the default filename
    text = text.trimmed(); // In some languages, there is a space in front of "...", see bug 268895
    m_copyData.m_src = entry.templatePath;

    const QUrl directory = mostLocalUrl(m_popupFiles.first());
    const QUrl defaultFile = QUrl::fromLocalFile(directory.toLocalFile() + QLatin1Char('/') + KIO::encodeFileName(text));
    if (defaultFile.isLocalFile() && QFile::exists(defaultFile.toLocalFile())) {
        text = KFileUtils::suggestName(directory, text);
    }

    QDialog *fileDialog = new QDialog(m_parentWidget);
    fileDialog->setAttribute(Qt::WA_DeleteOnClose);
    fileDialog->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    fileDialog->setModal(q->isModal());
    m_fileDialog = fileDialog;

    QVBoxLayout *layout = new QVBoxLayout;
    layout->setSizeConstraint(QLayout::SetFixedSize);

    m_messageWidget = new KMessageWidget(fileDialog);
    m_messageWidget->setCloseButtonVisible(false);
    m_messageWidget->setWordWrap(true);
    m_messageWidget->hide();
    QLabel *label = new QLabel(entry.comment, fileDialog);

    QLineEdit *lineEdit = new QLineEdit(fileDialog);
    lineEdit->setClearButtonEnabled(true);
    lineEdit->setText(text);
    lineEdit->setMinimumWidth(400);

    m_buttonBox = new QDialogButtonBox(fileDialog);
    m_buttonBox->setStandardButtons(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    QObject::connect(m_buttonBox, &QDialogButtonBox::accepted, fileDialog, &QDialog::accept);
    QObject::connect(m_buttonBox, &QDialogButtonBox::rejected, fileDialog, &QDialog::reject);

    m_creatingDirectory = false;
    _k_slotTextChanged(text);
    QObject::connect(lineEdit, SIGNAL(textChanged(QString)), q, SLOT(_k_slotTextChanged(QString)));

    layout->addWidget(label);
    layout->addWidget(lineEdit);
    layout->addWidget(m_buttonBox);
    layout->addWidget(m_messageWidget);
    layout->addStretch();

    fileDialog->setLayout(layout);
    QObject::connect(fileDialog, SIGNAL(accepted()), q, SLOT(_k_slotRealFileOrDir()));
    QObject::connect(fileDialog, SIGNAL(rejected()), q, SLOT(_k_slotAbortDialog()));

    fileDialog->show();
    lineEdit->selectAll();
    lineEdit->setFocus();
}

void KNewFileMenuPrivate::executeSymLink(const KNewFileMenuSingleton::Entry &entry)
{
    KNameAndUrlInputDialog *dlg = new KNameAndUrlInputDialog(i18n("File name:"), entry.comment, m_popupFiles.first(), m_parentWidget);
    dlg->setModal(q->isModal());
    dlg->setAttribute(Qt::WA_DeleteOnClose);
    dlg->setWindowTitle(i18n("Create Symlink"));
    m_fileDialog = dlg;
    QObject::connect(dlg, SIGNAL(accepted()), q, SLOT(_k_slotSymLink()));
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

    if (!m_copyData.m_isSymlink) {
        // If the file is not going to be detected as a desktop file, due to a
        // known extension (e.g. ".pl"), append ".desktop". #224142.
        QFile srcFile(uSrc.toLocalFile());
        if (srcFile.open(QIODevice::ReadOnly)) {
            QMimeDatabase db;
            QMimeType wantedMime = db.mimeTypeForUrl(uSrc);
            QMimeType mime = db.mimeTypeForFileNameAndData(m_copyData.m_chosenFileName, srcFile.read(1024));
            //qDebug() << "mime=" << mime->name() << "wantedMime=" << wantedMime->name();
            if (!mime.inherits(wantedMime.name()))
                if (!wantedMime.preferredSuffix().isEmpty()) {
                    chosenFileName += QLatin1Char('.') + wantedMime.preferredSuffix();
                }
        }
    }

    // The template is not a desktop file [or it's a URL one]
    // Copy it.
    QList<QUrl>::const_iterator it = m_popupFiles.constBegin();
    for (; it != m_popupFiles.constEnd(); ++it) {
        QUrl dest = *it;
        dest.setPath(concatPaths(dest.path(), KIO::encodeFileName(chosenFileName)));

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
            KIO::StoredTransferJob* putJob = KIO::storedPut(srcBuf, dest, -1);
            kjob = putJob;
            KIO::FileUndoManager::self()->recordJob(KIO::FileUndoManager::Put, QList<QUrl>(), dest, putJob);
        } else {
            //qDebug() << "KIO::copyAs(" << uSrc.url() << "," << dest.url() << ")";
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
    KNameAndUrlInputDialog *dlg = new KNameAndUrlInputDialog(i18n("File name:"), entry.comment, m_popupFiles.first(), m_parentWidget);
    m_copyData.m_templatePath = entry.templatePath;
    dlg->setModal(q->isModal());
    dlg->setAttribute(Qt::WA_DeleteOnClose);
    dlg->setWindowTitle(i18n("Create link to URL"));
    m_fileDialog = dlg;
    QObject::connect(dlg, SIGNAL(accepted()), q, SLOT(_k_slotUrlDesktopFile()));
    dlg->show();
}

void KNewFileMenuPrivate::fillMenu()
{
    QMenu *menu = q->menu();
    menu->clear();
    m_menuDev->menu()->clear();
    m_newDirAction = nullptr;

    QSet<QString> seenTexts;
    QString lastTemplatePath;
    // these shall be put at special positions
    QAction *linkURL = nullptr;
    QAction *linkApp = nullptr;
    QAction *linkPath = nullptr;

    KNewFileMenuSingleton *s = kNewMenuGlobals();
    int i = 1;
    KNewFileMenuSingleton::EntryList::iterator templ = s->templatesList->begin();
    const KNewFileMenuSingleton::EntryList::iterator templ_end = s->templatesList->end();
    for (; templ != templ_end; ++templ, ++i) {
        KNewFileMenuSingleton::Entry &entry = *templ;
        if (entry.entryType != KNewFileMenuSingleton::Separator) {
            // There might be a .desktop for that one already, if it's a kdelnk
            // This assumes we read .desktop files before .kdelnk files ...

            // In fact, we skip any second item that has the same text as another one.
            // Duplicates in a menu look bad in any case.

            const bool bSkip = seenTexts.contains(entry.text);
            if (bSkip) {
                // qDebug() << "skipping" << entry.filePath;
            } else {
                seenTexts.insert(entry.text);
                //const KNewFileMenuSingleton::Entry entry = templatesList->at(i-1);

                const QString templatePath = entry.templatePath;
                // The best way to identify the "Create Directory", "Link to Location", "Link to Application" was the template
                if (templatePath.endsWith(QLatin1String("emptydir"))) {
                    QAction *act = new QAction(q);
                    m_newDirAction = act;
                    act->setIcon(QIcon::fromTheme(entry.icon));
                    act->setText(i18nc("@item:inmenu Create New", "%1", entry.text));
                    act->setActionGroup(m_newMenuGroup);

                    // If there is a shortcut available in the action collection, use it.
                    QAction *act2 = m_actionCollection->action(QStringLiteral("create_dir"));
                    if (act2) {
                        act->setShortcuts(act2->shortcuts());
                        // Both actions have now the same shortcut, so this will prevent the "Ambiguous shortcut detected" dialog.
                        act->setShortcutContext(Qt::WidgetShortcut);
                        // We also need to react to shortcut changes.
                        QObject::connect(act2, &QAction::changed, act, [=]() {
                            act->setShortcuts(act2->shortcuts());
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

                        // We need to do mimetype filtering, for real files.
                        const bool createSymlink = entry.templatePath == QLatin1String("__CREATE_SYMLINK__");
                        if (createSymlink) {
                            keep = true;
                        } else if (!KDesktopFile::isDesktopFile(entry.templatePath)) {

                            // Determine mimetype on demand
                            QMimeDatabase db;
                            QMimeType mime;
                            if (entry.mimeType.isEmpty()) {
                                mime = db.mimeTypeForFile(entry.templatePath);
                                //qDebug() << entry.templatePath << "is" << mime.name();
                                entry.mimeType = mime.name();
                            } else {
                                mime = db.mimeTypeForName(entry.mimeType);
                            }
                            for (const QString &supportedMime : qAsConst(m_supportedMimeTypes)) {
                                if (mime.inherits(supportedMime)) {
                                    keep = true;
                                    break;
                                }
                            }
                        }

                        if (!keep) {
                            //qDebug() << "Not keeping" << entry.templatePath;
                            continue;
                        }
                    }

                    QAction *act = new QAction(q);
                    act->setData(i);
                    act->setIcon(QIcon::fromTheme(entry.icon));
                    act->setText(i18nc("@item:inmenu Create New", "%1", entry.text));
                    act->setActionGroup(m_newMenuGroup);

                    //qDebug() << templatePath << entry.filePath;

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
                            // If there is a shortcut available in the action collection, use it.
                            QAction *act2 = m_actionCollection->action(QStringLiteral("create_file"));
                            if (act2) {
                                act->setShortcuts(act2->shortcuts());
                                // Both actions have now the same shortcut, so this will prevent the "Ambiguous shortcut detected" dialog.
                                act->setShortcutContext(Qt::WidgetShortcut);
                                // We also need to react to shortcut changes.
                                QObject::connect(act2, &QAction::changed, act, [=]() {
                                    act->setShortcuts(act2->shortcuts());
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
    if (url.isLocalFile()) {
        return url;
    }

    KIO::StatJob *job = KIO::stat(url);
    KJobWidgets::setWindow(job, m_parentWidget);

    if (!job->exec()) {
        return url;
    }

    KIO::UDSEntry entry = job->statResult();
    const QString path = entry.stringValue(KIO::UDSEntry::UDS_LOCAL_PATH);

    return path.isEmpty() ? url : QUrl::fromLocalFile(path);
}

void KNewFileMenuPrivate::_k_slotAbortDialog()
{
    m_text = QString();
}

void KNewFileMenuPrivate::_k_slotActionTriggered(QAction *action)
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

void KNewFileMenuPrivate::_k_slotCreateDirectory(bool writeHiddenDir)
{
    QUrl url;
    QUrl baseUrl = m_popupFiles.first();

    QString name = expandTilde(m_text);

    if (!name.isEmpty()) {
        if (QDir::isAbsolutePath(name)) {
            url = QUrl::fromLocalFile(name);
        } else {
            if (name == QLatin1Char('.') || name == QLatin1String("..")) {
                KGuiItem enterNewNameGuiItem(KStandardGuiItem::ok());
                enterNewNameGuiItem.setText(i18nc("@action:button", "Enter a Different Name"));
                enterNewNameGuiItem.setIcon(QIcon::fromTheme(QStringLiteral("edit-rename")));

                QDialog *confirmDialog = new QDialog(m_parentWidget);
                confirmDialog->setWindowTitle(i18n("Invalid Directory Name"));
                confirmDialog->setModal(m_modal);
                confirmDialog->setAttribute(Qt::WA_DeleteOnClose);

                m_buttonBox = new QDialogButtonBox(confirmDialog);
                m_buttonBox->setStandardButtons(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
                KGuiItem::assign(m_buttonBox->button(QDialogButtonBox::Ok), enterNewNameGuiItem);

                KMessageBox::createKMessageBox(confirmDialog, m_buttonBox, QMessageBox::Critical,
                                   xi18nc("@info", "Could not create a folder with the name <filename>%1</filename><nl/>because it is reserved for use by the operating system.", name),
                                   QStringList(),
                                   QString(),
                                   nullptr,
                                   KMessageBox::NoExec,
                                   QString());

                m_creatingDirectory = true;
                QObject::connect(m_buttonBox, &QDialogButtonBox::accepted, q, &KNewFileMenu::createDirectory);
                m_fileDialog = confirmDialog;
                confirmDialog->show();
                _k_slotAbortDialog();
                return;
            }
            url = baseUrl;
            url.setPath(concatPaths(url.path(), name));
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
    job->uiDelegate()->setAutoErrorHandlingEnabled(true);
    KJobWidgets::setWindow(job, m_parentWidget);

    if (job) {
        // We want the error handling to be done by slotResult so that subclasses can reimplement it
        job->uiDelegate()->setAutoErrorHandlingEnabled(false);
        QObject::connect(job, &KJob::result, q, &KNewFileMenu::slotResult);
    }
    _k_slotAbortDialog();
}

void KNewFileMenuPrivate::_k_slotCreateHiddenDirectory()
{
    _k_slotCreateDirectory(true);
}

struct EntryWithName {
    QString key;
    KNewFileMenuSingleton::Entry entry;
};

void KNewFileMenuPrivate::_k_slotFillTemplates()
{
    KNewFileMenuSingleton *s = kNewMenuGlobals();
    //qDebug();

    const QStringList qrcTemplates = { QStringLiteral(":/kio5/newfile-templates") };
    QStringList installedTemplates = { QStandardPaths::locateAll(QStandardPaths::GenericDataLocation, QStringLiteral("templates"), QStandardPaths::LocateDirectory) };
    // Qt does not provide an easy way to receive the xdg dir for templates so we have to find it on our own
    #ifdef Q_OS_UNIX
        QString xdgUserDirs = QStandardPaths::locate(QStandardPaths::ConfigLocation, QStringLiteral("user-dirs.dirs"), QStandardPaths::LocateFile);
        QFile xdgUserDirsFile(xdgUserDirs);
        if (!xdgUserDirs.isEmpty() && xdgUserDirsFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
            QTextStream in(&xdgUserDirsFile);
            while (!in.atEnd()) {
                QString line = in.readLine();
                if (line.startsWith(QLatin1String("XDG_TEMPLATES_DIR="))) {
                    QString xdgTemplates = line.mid(19, line.size()-20);
                    xdgTemplates.replace(QStringLiteral("$HOME"), QDir::homePath());
                    QDir xdgTemplatesDir(xdgTemplates);
                    if (xdgTemplatesDir.exists()) {
                        installedTemplates << xdgTemplates;
                    }
                    break;
                }
            }
        }
    #endif

    const QStringList templates = qrcTemplates + installedTemplates;

    // Ensure any changes in the templates dir will call this
    if (! s->dirWatch) {
        s->dirWatch = new KDirWatch;
        for (const QString &dir : qAsConst(installedTemplates)) {
            s->dirWatch->addDir(dir);
        }
        QObject::connect(s->dirWatch, SIGNAL(dirty(QString)),
                         q, SLOT(_k_slotFillTemplates()));
        QObject::connect(s->dirWatch, SIGNAL(created(QString)),
                         q, SLOT(_k_slotFillTemplates()));
        QObject::connect(s->dirWatch, SIGNAL(deleted(QString)),
                         q, SLOT(_k_slotFillTemplates()));
        // Ok, this doesn't cope with new dirs in XDG_DATA_DIRS, but that's another story
    }
    ++s->templatesVersion;
    s->filesParsed = false;

    s->templatesList->clear();

    // Look into "templates" dirs.
    QStringList files;
    QDir dir;

    for (const QString &path : templates) {
        dir.setPath(path);
        const QStringList &entryList(dir.entryList(QStringList() << QStringLiteral("*.desktop"), QDir::Files));
        files.reserve(files.size() + entryList.size());
        for (const QString &entry : entryList) {
            const QString file = concatPaths(dir.path(), entry);
            files.append(file);
        }
    }

    QMap<QString, KNewFileMenuSingleton::Entry> slist; // used for sorting
    QMap<QString, EntryWithName> ulist; // entries with unique URLs
    for (const QString &file : qAsConst(files)) {
        //qDebug() << file;
        if (file[0] != QLatin1Char('.')) {
            KNewFileMenuSingleton::Entry e;
            e.filePath = file;
            e.entryType = KNewFileMenuSingleton::Unknown; // not parsed yet

            // Put Directory first in the list (a bit hacky),
            // and TextFile before others because it's the most used one.
            // This also sorts by user-visible name.
            // The rest of the re-ordering is done in fillMenu.
            const KDesktopFile config(file);
            QString url = config.desktopGroup().readEntry("URL");
            QString key = config.desktopGroup().readEntry("Name");
            if (file.endsWith(QLatin1String("Directory.desktop"))) {
                key.prepend(QLatin1Char('0'));
            } else if (file.startsWith(QDir::homePath())) {
                key.prepend(QLatin1Char('1'));
            } else if (file.endsWith(QLatin1String("TextFile.desktop"))) {
                key.prepend(QLatin1Char('2'));
            } else {
                key.prepend(QLatin1Char('3'));
            }
            EntryWithName en = { key, e };
            if (ulist.contains(url)) {
                ulist.remove(url);
            }
            ulist.insert(url, en);
        }
    }
    QMap<QString, EntryWithName>::iterator it = ulist.begin();
    for (; it != ulist.end(); ++it) {
        EntryWithName ewn = *it;
        slist.insert(ewn.key, ewn.entry);
    }
    (*s->templatesList) += slist.values();
}

void KNewFileMenuPrivate::_k_slotOtherDesktopFile()
{
    // The properties dialog took care of the copying, so we're done
    KPropertiesDialog *dialog = qobject_cast<KPropertiesDialog *>(q->sender());
    emit q->fileCreated(dialog->url());
}

void KNewFileMenuPrivate::_k_slotOtherDesktopFileClosed()
{
    QFile::remove(m_tempFileToDelete);
}

void KNewFileMenuPrivate::_k_slotRealFileOrDir()
{
    m_copyData.m_chosenFileName = m_text;
    _k_slotAbortDialog();
    executeStrategy();
}

void KNewFileMenuPrivate::_k_slotSymLink()
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

void KNewFileMenuPrivate::_k_slotTextChanged(const QString &text)
{
    m_buttonBox->button(QDialogButtonBox::Ok)->setEnabled(true);
    // Validate input, displaying a KMessageWidget for questionable names

    if (text.length() == 0) {
        m_messageWidget->hide();
        m_buttonBox->button(QDialogButtonBox::Ok)->setEnabled(false);
    }

    // Don't allow creating folders that would mask . or ..
    else if (text == QLatin1Char('.') || text == QLatin1String("..")) {
        m_messageWidget->setText(xi18nc("@info", "The name <filename>%1</filename> cannot be used because it is reserved for use by the operating system.", text));
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
                for (const QString &folder : qAsConst(folders)) {
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
    }
    else if (text.contains(QLatin1Char('\\'))) {
        m_messageWidget->setText(i18n("Backslashes cannot be used in file and folder names."));
        m_messageWidget->setMessageType(KMessageWidget::Error);
        m_messageWidget->animatedShow();
        m_buttonBox->button(QDialogButtonBox::Ok)->setEnabled(false);
    }
#endif

    // Using a tilde to begin a file or folder name is not recommended
    else if (text.startsWith(QLatin1Char('~'))) {
        m_messageWidget->setText(i18n("Starting a file or folder name with a tilde is not recommended because it may be confusing or dangerous when using the terminal to delete things."));
        m_messageWidget->setMessageType(KMessageWidget::Warning);
        m_messageWidget->animatedShow();
    }

    else {
        m_messageWidget->hide();
    }
    m_text = text;
}

void KNewFileMenuPrivate::_k_slotUrlDesktopFile()
{
    KNameAndUrlInputDialog *dlg = static_cast<KNameAndUrlInputDialog *>(m_fileDialog);

    m_copyData.m_chosenFileName = dlg->name(); // no path
    QUrl linkUrl = dlg->url();

    // Filter user input so that short uri entries, e.g. www.kde.org, are
    // handled properly. This not only makes the icon detection below work
    // properly, but opening the URL link where the short uri will not be
    // sent to the application (opening such link Konqueror fails).
    KUriFilterData uriData;
    uriData.setData(linkUrl); // the url to put in the file
    uriData.setCheckForExecutables(false);

    if (KUriFilter::self()->filterUri(uriData, QStringList() << QStringLiteral("kshorturifilter"))) {
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
    group.writeEntry("Icon", KProtocolInfo::icon(linkUrl.scheme()));
    group.writePathEntry("URL", linkUrl.toDisplayString());
    df.sync();

    m_copyData.m_src = tempFileName;
    m_copyData.m_tempFileToDelete = tempFileName;

    executeStrategy();
}

KNewFileMenu::KNewFileMenu(KActionCollection *collection, const QString &name, QObject *parent)
    : KActionMenu(QIcon::fromTheme(QStringLiteral("document-new")), i18n("Create New"), parent),
      d(new KNewFileMenuPrivate(collection, this))
{
    // Don't fill the menu yet
    // We'll do that in checkUpToDate (should be connected to aboutToShow)
    d->m_newMenuGroup = new QActionGroup(this);
    connect(d->m_newMenuGroup, SIGNAL(triggered(QAction*)), this, SLOT(_k_slotActionTriggered(QAction*)));
    d->m_parentWidget = qobject_cast<QWidget *>(parent);
    d->m_newDirAction = nullptr;

    if (d->m_actionCollection) {
        d->m_actionCollection->addAction(name, this);
    }

    d->m_menuDev = new KActionMenu(QIcon::fromTheme(QStringLiteral("drive-removable-media")), i18n("Link to Device"), this);
}

KNewFileMenu::~KNewFileMenu()
{
    //qDebug() << this;
    delete d;
}

void KNewFileMenu::checkUpToDate()
{
    KNewFileMenuSingleton *s = kNewMenuGlobals();
    //qDebug() << this << "m_menuItemsVersion=" << d->m_menuItemsVersion
    //              << "s->templatesVersion=" << s->templatesVersion;
    if (d->m_menuItemsVersion < s->templatesVersion || s->templatesVersion == 0) {
        //qDebug() << "recreating actions";
        // We need to clean up the action collection
        // We look for our actions using the group
        qDeleteAll(d->m_newMenuGroup->actions());

        if (!s->templatesList) { // No templates list up to now
            s->templatesList = new KNewFileMenuSingleton::EntryList;
            d->_k_slotFillTemplates();
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

    QUrl baseUrl = d->m_popupFiles.first();

    KIO::StatJob *job = KIO::mostLocalUrl(baseUrl);

    if (job->exec()) {
        baseUrl = job->mostLocalUrl();
    }

    QString name = d->m_text.isEmpty() ? i18nc("Default name for a new folder", "New Folder") :
                   d->m_text;

    if (baseUrl.isLocalFile() && QFileInfo::exists(baseUrl.toLocalFile() + QLatin1Char('/') + name)) {
        name = KFileUtils::suggestName(baseUrl, name);
    }

    QDialog *fileDialog = new QDialog(d->m_parentWidget);
    fileDialog->setModal(isModal());
    fileDialog->setAttribute(Qt::WA_DeleteOnClose);
    fileDialog->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Minimum);
    fileDialog->setWindowTitle(i18nc("@title:window", "New Folder"));
    d->m_fileDialog = fileDialog;

    QVBoxLayout *layout = new QVBoxLayout;
    layout->setSizeConstraint(QLayout::SetFixedSize);

    d->m_messageWidget = new KMessageWidget(fileDialog);
    d->m_messageWidget->setCloseButtonVisible(false);
    d->m_messageWidget->setWordWrap(true);
    d->m_messageWidget->hide();
    QLabel *label = new QLabel(i18n("Create new folder in %1:", baseUrl.toDisplayString(QUrl::PreferLocalFile)), fileDialog);

    QLineEdit *lineEdit = new QLineEdit(fileDialog);
    lineEdit->setClearButtonEnabled(true);
    lineEdit->setText(name);
    lineEdit->setMinimumWidth(400);

    d->m_buttonBox = new QDialogButtonBox(fileDialog);
    d->m_buttonBox->setStandardButtons(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    QObject::connect(d->m_buttonBox, &QDialogButtonBox::accepted, fileDialog, &QDialog::accept);
    QObject::connect(d->m_buttonBox, &QDialogButtonBox::rejected, fileDialog, &QDialog::reject);

    d->m_creatingDirectory = true;
    d->_k_slotTextChanged(name); // have to save string in d->m_text in case user does not touch dialog
    connect(lineEdit, SIGNAL(textChanged(QString)), this, SLOT(_k_slotTextChanged(QString)));

    layout->addWidget(label);
    layout->addWidget(lineEdit);
    layout->addWidget(d->m_buttonBox);
    layout->addWidget(d->m_messageWidget);
    layout->addStretch();

    fileDialog->setLayout(layout);
    connect(fileDialog, SIGNAL(accepted()), this, SLOT(_k_slotCreateDirectory()));
    connect(fileDialog, SIGNAL(rejected()), this, SLOT(_k_slotAbortDialog()));


    fileDialog->show();
    lineEdit->selectAll();
    lineEdit->setFocus();
}

void KNewFileMenu::createFile()
{
    if (d->m_popupFiles.isEmpty()) {
        return;
    }

    checkUpToDate();
    if (!d->m_firstFileEntry) {
        return;
    }

    d->executeRealFileOrDir(*d->m_firstFileEntry);
}

bool KNewFileMenu::isModal() const
{
    return d->m_modal;
}

QList<QUrl> KNewFileMenu::popupFiles() const
{
    return d->m_popupFiles;
}

void KNewFileMenu::setModal(bool modal)
{
    d->m_modal = modal;
}

void KNewFileMenu::setPopupFiles(const QList<QUrl> &files)
{
    d->m_popupFiles = files;
    if (files.isEmpty()) {
        d->m_newMenuGroup->setEnabled(false);
    } else {
        const QUrl &firstUrl = files.first();
        if (KProtocolManager::supportsWriting(firstUrl)) {
            d->m_newMenuGroup->setEnabled(true);
            if (d->m_newDirAction) {
                d->m_newDirAction->setEnabled(KProtocolManager::supportsMakeDir(firstUrl)); // e.g. trash:/
            }
        } else {
            d->m_newMenuGroup->setEnabled(true);
        }
    }
}

void KNewFileMenu::setParentWidget(QWidget *parentWidget)
{
    d->m_parentWidget = parentWidget;
}

void KNewFileMenu::setSupportedMimeTypes(const QStringList &mime)
{
    d->m_supportedMimeTypes = mime;
}

void KNewFileMenu::setViewShowsHiddenFiles(bool b)
{
    d->m_viewShowsHiddenFiles = b;
}

void KNewFileMenu::slotResult(KJob *job)
{
    if (job->error()) {
        static_cast<KIO::Job *>(job)->uiDelegate()->showErrorMessage();
    } else {
        // Was this a copy or a mkdir?
        if (job->property("newDirectoryURL").isValid()) {
            QUrl newDirectoryURL = job->property("newDirectoryURL").toUrl();
            emit directoryCreated(newDirectoryURL);
        } else {
            KIO::CopyJob *copyJob = ::qobject_cast<KIO::CopyJob *>(job);
            if (copyJob) {
                const QUrl destUrl = copyJob->destUrl();
                const QUrl localUrl = d->mostLocalUrl(destUrl);
                if (localUrl.isLocalFile()) {
                    // Normal (local) file. Need to "touch" it, kio_file copied the mtime.
                    (void) ::utime(QFile::encodeName(localUrl.toLocalFile()).constData(), nullptr);
                }
                emit fileCreated(destUrl);
            } else if (KIO::SimpleJob *simpleJob = ::qobject_cast<KIO::SimpleJob *>(job)) {
                // Called in the storedPut() case
                org::kde::KDirNotify::emitFilesAdded(simpleJob->url().adjusted(QUrl::RemoveFilename | QUrl::StripTrailingSlash));
                emit fileCreated(simpleJob->url());
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

#include "moc_knewfilemenu.cpp"

