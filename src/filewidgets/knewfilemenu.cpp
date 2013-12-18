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
#include "knameandurlinputdialog.h"

#include <QDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QVBoxLayout>
#include <QList>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QStandardPaths>

#include <qtemporaryfile.h>
#include <kactioncollection.h>
#include <kconfiggroup.h>
#include <QDebug>
#include <kdesktopfile.h>
#include <kdirwatch.h>
#include <kjobwidgets.h>
#include <klocalizedstring.h>
#include <kmessagebox.h>
#include <kprotocolinfo.h>
#include <kprotocolmanager.h>
#include <krun.h>
#include <kshell.h>
#include <kio/job.h>
#include <kio/copyjob.h>
#include <kio/jobuidelegate.h>
#include <kio/fileundomanager.h>
#include <kio/kurifilter.h>

#include <kpropertiesdialog.h>
#include <qmimedatabase.h>
#include <utime.h>

static QString expandTilde(const QString& name, bool isfile = false)
{
    if (!name.isEmpty() && (!isfile || name[0] == '\\'))
    {
        const QString expandedName = KShell::tildeExpand(name);
        // When a tilde mark cannot be properly expanded, the above call
        // returns an empty string...
        if (!expandedName.isEmpty())
            return expandedName;
    }

    return name;
}

// Singleton, with data shared by all KNewFileMenu instances
class KNewFileMenuSingleton
{
public:
    KNewFileMenuSingleton()
        : dirWatch(0),
	  filesParsed(false),
	  templatesList(0),
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

    KDirWatch * dirWatch;

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
    EntryList * templatesList;

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
        KNewFileMenuSingleton::Entry& templ = templIter.next();
        const QString filePath = templ.filePath;
        if (!filePath.isEmpty())
        {
            QString text;
            QString templatePath;
            // If a desktop file, then read the name from it.
            // Otherwise (or if no name in it?) use file name
            if (KDesktopFile::isDesktopFile(filePath)) {
                KDesktopFile desktopFile( filePath);
                if (desktopFile.noDisplay()) {
                    templIter.remove();
                    continue;
                }
                text = desktopFile.readName();
                templ.icon = desktopFile.readIcon();
                templ.comment = desktopFile.readComment();
                QString type = desktopFile.readType();
                if (type == "Link")
                {
                    templatePath = desktopFile.desktopGroup().readPathEntry("URL", QString());
                    if (templatePath[0] != '/' && !templatePath.startsWith("__"))
                    {
                        if (templatePath.startsWith("file:/"))
                            templatePath = QUrl(templatePath).toLocalFile();
                        else
                        {
                            // A relative path, then (that's the default in the files we ship)
                            QString linkDir = filePath.left(filePath.lastIndexOf('/') + 1 /*keep / */);
                            //qDebug() << "linkDir=" << linkDir;
                            templatePath = linkDir + templatePath;
                        }
                    }
                }
                if (templatePath.isEmpty())
                {
                    // No URL key, this is an old-style template
                    templ.entryType = KNewFileMenuSingleton::Template;
                    templ.templatePath = templ.filePath; // we'll copy the file
                } else {
                    templ.entryType = KNewFileMenuSingleton::LinkToTemplate;
                    templ.templatePath = templatePath;
                }

            }
            if (text.isEmpty())
            {
                text = QUrl(filePath).fileName();
                if (text.endsWith(".desktop"))
                    text.truncate(text.length() - 8);
            }
            templ.text = text;
            /*// qDebug() << "Updating entry with text=" << text
                          << "entryType=" << templ.entryType
                          << "templatePath=" << templ.templatePath;*/
        }
        else {
            templ.entryType = KNewFileMenuSingleton::Separator;
        }
    }
}

Q_GLOBAL_STATIC(KNewFileMenuSingleton, kNewMenuGlobals)

class KNewFileMenuCopyData
{
public:
    KNewFileMenuCopyData() { m_isSymlink = false;}
    ~KNewFileMenuCopyData() {}
    QString chosenFileName() const { return m_chosenFileName; }

    // If empty, no copy is performed.
    QString sourceFileToCopy() const { return m_src; }
    QString tempFileToDelete() const { return m_tempFileToDelete; }
    bool m_isSymlink;

    QString m_chosenFileName;
    QString m_src;
    QString m_tempFileToDelete;
    QString m_templatePath;
};

class KNewFileMenuPrivate
{
public:
    KNewFileMenuPrivate(KNewFileMenu* qq)
        : m_menuItemsVersion(0),
          m_modal(true),
          m_viewShowsHiddenFiles(false),
          q(qq)
    {}

    bool checkSourceExists(const QString& src);

    /**
      * Asks user whether to create a hidden directory with a dialog
      */
    void confirmCreatingHiddenDir(const QString& name);

    /**
      *	The strategy used for other desktop files than Type=Link. Example: Application, Device.
      */
    void executeOtherDesktopFile(const KNewFileMenuSingleton::Entry& entry);

    /**
      * The strategy used for "real files or directories" (the common case)
      */
    void executeRealFileOrDir(const KNewFileMenuSingleton::Entry& entry);

    /**
      * Actually performs file handling. Reads in m_copyData for needed data, that has been collected by execute*() before
      */
    void executeStrategy();

    /**
      *	The strategy used when creating a symlink
      */
    void executeSymLink(const KNewFileMenuSingleton::Entry& entry);

    /**
      * The strategy used for "url" desktop files
      */
    void executeUrlDesktopFile(const KNewFileMenuSingleton::Entry& entry);

    /**
     * Fills the menu from the templates list.
     */
    void fillMenu();

    /**
     * Tries to map a local URL for the given URL.
     */
    QUrl mostLocalUrl(const QUrl& url);

    /**
      * Just clears the string buffer d->m_text, but I need a slot for this to occur
      */
    void _k_slotAbortDialog();

    /**
     * Called when New->* is clicked
     */
    void _k_slotActionTriggered(QAction* action);

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
      * Callback in KNewFileMenu for the OtherDesktopFile Dialog. Handles dialog input and gives over
      * to executeStrategy()
      */
    void _k_slotOtherDesktopFile();

    /**
      * Callback in KNewFileMenu for the RealFile Dialog. Handles dialog input and gives over
      * to executeStrategy()
      */
    void _k_slotRealFileOrDir();

    /**
      * Dialogs use this slot to write the changed string into KNewFile menu when the user
      * changes touches them
      */
    void _k_slotTextChanged(const QString & text);

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


    KActionCollection * m_actionCollection;
    QDialog* m_fileDialog;

    KActionMenu *m_menuDev;
    int m_menuItemsVersion;
    bool m_modal;
    QAction* m_newDirAction;

    /**
     * The action group that our actions belong to
     */
    QActionGroup* m_newMenuGroup;
    QWidget *m_parentWidget;

    /**
     * When the user pressed the right mouse button over an URL a popup menu
     * is displayed. The URL belonging to this popup menu is stored here.
     */
    QList<QUrl> m_popupFiles;

    QStringList m_supportedMimeTypes;
    QString m_tempFileToDelete; // set when a tempfile was created for a Type=URL desktop file
    QString m_text;
    bool m_viewShowsHiddenFiles;

    KNewFileMenu* q;

    KNewFileMenuCopyData m_copyData;
};

bool KNewFileMenuPrivate::checkSourceExists(const QString& src)
{
    if (!QFile::exists(src)) {
        qWarning() << src << "doesn't exist" ;

        QDialog* dialog = new QDialog(m_parentWidget);
        dialog->setWindowTitle(i18n("Sorry"));
	dialog->setObjectName( "sorry" );
	dialog->setModal(q->isModal());
	dialog->setAttribute(Qt::WA_DeleteOnClose);

        QDialogButtonBox *buttonBox = new QDialogButtonBox(dialog);
        buttonBox->setStandardButtons(QDialogButtonBox::Ok);

        KMessageBox::createKMessageBox(dialog, buttonBox, QMessageBox::Warning,
	  i18n("<qt>The template file <b>%1</b> does not exist.</qt>", src),
	  QStringList(), QString(), 0, KMessageBox::NoExec,
	  QString());

	dialog->show();

        return false;
    }
    return true;
}

void KNewFileMenuPrivate::confirmCreatingHiddenDir(const QString& name)
{
    if(!KMessageBox::shouldBeShownContinue("confirm_create_hidden_dir")){
	_k_slotCreateHiddenDirectory();
	return;
    }

    KGuiItem continueGuiItem(KStandardGuiItem::cont());
    continueGuiItem.setText(i18nc("@action:button", "Create directory"));
    KGuiItem cancelGuiItem(KStandardGuiItem::cancel());
    cancelGuiItem.setText(i18nc("@action:button", "Enter a different name"));

    QDialog* confirmDialog = new QDialog(m_parentWidget);
    confirmDialog->setWindowTitle(i18n("Create hidden directory?"));
    confirmDialog->setModal(m_modal);
    confirmDialog->setAttribute(Qt::WA_DeleteOnClose);

    QDialogButtonBox *buttonBox = new QDialogButtonBox(confirmDialog);
    buttonBox->setStandardButtons(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    KGuiItem::assign(buttonBox->button(QDialogButtonBox::Ok), continueGuiItem);
    KGuiItem::assign(buttonBox->button(QDialogButtonBox::Cancel), cancelGuiItem);

    KMessageBox::createKMessageBox(confirmDialog, buttonBox, QMessageBox::Warning,
	  i18n("The name \"%1\" starts with a dot, so the directory will be hidden by default.", name),
	  QStringList(),
	  i18n("Do not ask again"),
	  0,
	  KMessageBox::NoExec,
	  QString());

    QObject::connect(confirmDialog, SIGNAL(accepted()), q, SLOT(_k_slotCreateHiddenDirectory()));
    QObject::connect(confirmDialog, SIGNAL(rejected()), q, SLOT(createDirectory()));

    m_fileDialog = confirmDialog;
    confirmDialog->show();

}

void KNewFileMenuPrivate::executeOtherDesktopFile(const KNewFileMenuSingleton::Entry& entry)
{
    if (!checkSourceExists(entry.templatePath)) {
        return;
    }

    QList<QUrl>::const_iterator it = m_popupFiles.constBegin();
    for (; it != m_popupFiles.constEnd(); ++it)
    {
        QString text = entry.text;
        text.remove("..."); // the ... is fine for the menu item but not for the default filename
        text = text.trimmed(); // In some languages, there is a space in front of "...", see bug 268895
        // KDE5 TODO: remove the "..." from link*.desktop files and use i18n("%1...") when making
        // the action.

        QUrl defaultFile(*it);
        defaultFile.setPath(defaultFile.path() + '/' + KIO::encodeFileName(text));
        if (defaultFile.isLocalFile() && QFile::exists(defaultFile.toLocalFile()))
            text = KIO::suggestName(*it, text);

        const QUrl templateUrl(entry.templatePath);

	QDialog* dlg = new KPropertiesDialog(templateUrl, *it, text, m_parentWidget);
	dlg->setModal(q->isModal());
	dlg->setAttribute(Qt::WA_DeleteOnClose);
        QObject::connect(dlg, SIGNAL(applied()), q, SLOT(_k_slotOtherDesktopFile()));
	dlg->show();
    }
    // We don't set m_src here -> there will be no copy, we are done.
}

void KNewFileMenuPrivate::executeRealFileOrDir(const KNewFileMenuSingleton::Entry& entry)
{
    // The template is not a desktop file
    // Show the small dialog for getting the destination filename
    QString text = entry.text;
    text.remove("..."); // the ... is fine for the menu item but not for the default filename
    text = text.trimmed(); // In some languages, there is a space in front of "...", see bug 268895
    m_copyData.m_src = entry.templatePath;

    QUrl defaultFile(m_popupFiles.first());
    defaultFile.setPath(defaultFile.path() + '/' + KIO::encodeFileName(text));
    if (defaultFile.isLocalFile() && QFile::exists(defaultFile.toLocalFile()))
        text = KIO::suggestName(m_popupFiles.first(), text);

    QDialog* fileDialog = new QDialog(m_parentWidget);
    fileDialog->setAttribute(Qt::WA_DeleteOnClose);
    fileDialog->setModal(q->isModal());

    QVBoxLayout *layout = new QVBoxLayout;
    QLabel *label = new QLabel(entry.comment, fileDialog);

    QLineEdit *lineEdit = new QLineEdit(fileDialog);
    lineEdit->setClearButtonEnabled(true);
    lineEdit->setText(text);

    _k_slotTextChanged(text);
    QObject::connect(lineEdit, SIGNAL(textChanged(QString)), q, SLOT(_k_slotTextChanged(QString)));

    QDialogButtonBox *buttonBox = new QDialogButtonBox(fileDialog);
    buttonBox->setStandardButtons(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    QObject::connect(buttonBox, SIGNAL(accepted()), fileDialog, SLOT(accept()));
    QObject::connect(buttonBox, SIGNAL(rejected()), fileDialog, SLOT(reject()));

    layout->addWidget(label);
    layout->addWidget(lineEdit);
    layout->addWidget(buttonBox);

    fileDialog->setLayout(layout);
    QObject::connect(fileDialog, SIGNAL(accepted()), q, SLOT(_k_slotRealFileOrDir()));
    QObject::connect(fileDialog, SIGNAL(rejected()), q, SLOT(_k_slotAbortDialog()));

    fileDialog->show();
    lineEdit->selectAll();
    lineEdit->setFocus();
}

void KNewFileMenuPrivate::executeSymLink(const KNewFileMenuSingleton::Entry& entry)
{
    KNameAndUrlInputDialog* dlg = new KNameAndUrlInputDialog(i18n("File name:"), entry.comment, m_popupFiles.first(), m_parentWidget);
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

    if (src.isEmpty())
        return;
    QUrl uSrc(src);
    if (uSrc.isLocalFile()) {
        // In case the templates/.source directory contains symlinks, resolve
        // them to the target files. Fixes bug #149628.
        KFileItem item(uSrc, QString(), KFileItem::Unknown);
        if (item.isLink())
            uSrc.setPath(item.linkDest());

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
                    if (!wantedMime.preferredSuffix().isEmpty())
                        chosenFileName += QLatin1Char('.') + wantedMime.preferredSuffix();
            }
        }
    }

    // The template is not a desktop file [or it's a URL one]
    // Copy it.
    QList<QUrl>::const_iterator it = m_popupFiles.constBegin();
    for (; it != m_popupFiles.constEnd(); ++it) {
        QUrl dest = *it;
        dest.setPath(dest.path() + '/' + KIO::encodeFileName(chosenFileName));

        QList<QUrl> lstSrc;
        lstSrc.append(uSrc);
        KIO::Job* kjob;
        if (m_copyData.m_isSymlink) {
            kjob = KIO::symlink(src, dest);
            // This doesn't work, FileUndoManager registers new links in copyingLinkDone,
            // which KIO::symlink obviously doesn't emit... Needs code in FileUndoManager.
            //KIO::FileUndoManager::self()->recordJob(KIO::FileUndoManager::Link, lstSrc, dest, kjob);
        } else {
            //qDebug() << "KIO::copyAs(" << uSrc.url() << "," << dest.url() << ")";
            KIO::CopyJob * job = KIO::copyAs(uSrc, dest);
            job->setDefaultPermissions(true);
            kjob = job;
            KIO::FileUndoManager::self()->recordJob(KIO::FileUndoManager::Copy, lstSrc, dest, job);
        }
        KJobWidgets::setWindow(kjob, m_parentWidget);
        QObject::connect(kjob, SIGNAL(result(KJob*)), q, SLOT(slotResult(KJob*)));
    }
}

void KNewFileMenuPrivate::executeUrlDesktopFile(const KNewFileMenuSingleton::Entry& entry)
{
    KNameAndUrlInputDialog* dlg = new KNameAndUrlInputDialog(i18n("File name:"), entry.comment, m_popupFiles.first(), m_parentWidget);
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
    QMenu* menu = q->menu();
    menu->clear();
    m_menuDev->menu()->clear();
    m_newDirAction = 0;

    QSet<QString> seenTexts;
    // these shall be put at special positions
    QAction* linkURL = 0;
    QAction* linkApp = 0;
    QAction* linkPath = 0;

    KNewFileMenuSingleton* s = kNewMenuGlobals();
    int i = 1;
    KNewFileMenuSingleton::EntryList::iterator templ = s->templatesList->begin();
    const KNewFileMenuSingleton::EntryList::iterator templ_end = s->templatesList->end();
    for (; templ != templ_end; ++templ, ++i)
    {
        KNewFileMenuSingleton::Entry& entry = *templ;
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
                if (templatePath.endsWith("emptydir")) {
                    QAction * act = new QAction(q);
                    m_newDirAction = act;
                    act->setIcon(QIcon::fromTheme(entry.icon));
                    act->setText(i18nc("@item:inmenu Create New", "%1", entry.text));
                    act->setActionGroup(m_newMenuGroup);
                    menu->addAction(act);

                    QAction *sep = new QAction(q);
                    sep->setSeparator(true);
                    menu->addAction(sep);
                } else {

                    if (!m_supportedMimeTypes.isEmpty()) {
                        bool keep = false;

                        // We need to do mimetype filtering, for real files.
                        const bool createSymlink = entry.templatePath == "__CREATE_SYMLINK__";
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
                            Q_FOREACH(const QString& supportedMime, m_supportedMimeTypes) {
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

                    QAction * act = new QAction(q);
                    act->setData(i);
                    act->setIcon(QIcon::fromTheme(entry.icon));
                    act->setText(i18nc("@item:inmenu Create New", "%1", entry.text));
                    act->setActionGroup(m_newMenuGroup);

                    //qDebug() << templatePath << entry.filePath;

                    if (templatePath.endsWith("/URL.desktop")) {
                        linkURL = act;
                    } else if (templatePath.endsWith("/Program.desktop")) {
                        linkApp = act;
                    } else if (entry.filePath.endsWith("/linkPath.desktop")) {
                        linkPath = act;
                    } else if (KDesktopFile::isDesktopFile(templatePath)) {
                        KDesktopFile df(templatePath);
                        if (df.readType() == "FSDevice")
                            m_menuDev->menu()->addAction(act);
                        else
                            menu->addAction(act);
                    }
                    else
                    {
                        menu->addAction(act);
                    }
                }
            }
        } else { // Separate system from personal templates
            Q_ASSERT(entry.entryType != 0);

            QAction *sep = new QAction(q);
            sep->setSeparator(true);
            menu->addAction(sep);
        }
    }

    if (m_supportedMimeTypes.isEmpty()) {
        QAction *sep = new QAction(q);
        sep->setSeparator(true);
        menu->addAction(sep);
        if (linkURL) menu->addAction(linkURL);
        if (linkPath) menu->addAction(linkPath);
        if (linkApp) menu->addAction(linkApp);
        Q_ASSERT(m_menuDev);
        menu->addAction(m_menuDev);
    }
}

QUrl KNewFileMenuPrivate::mostLocalUrl(const QUrl& url)
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

void KNewFileMenuPrivate::_k_slotActionTriggered(QAction* action)
{
    q->trigger(); // was for kdesktop's slotNewMenuActivated() in kde3 times. Can't hurt to keep it...

    if (action == m_newDirAction) {
        q->createDirectory();
        return;
    }
    const int id = action->data().toInt();
    Q_ASSERT(id > 0);

    KNewFileMenuSingleton* s = kNewMenuGlobals();
    const KNewFileMenuSingleton::Entry entry = s->templatesList->at(id - 1);

    const bool createSymlink = entry.templatePath == "__CREATE_SYMLINK__";

    m_copyData = KNewFileMenuCopyData();

    if (createSymlink) {
        m_copyData.m_isSymlink = true;
	executeSymLink(entry);
    }
    else if (KDesktopFile::isDesktopFile(entry.templatePath)) {
        KDesktopFile df(entry.templatePath);
        if (df.readType() == "Link") {
	    executeUrlDesktopFile(entry);
        } else { // any other desktop file (Device, App, etc.)
	    executeOtherDesktopFile(entry);
        }
    }
    else {
	executeRealFileOrDir(entry);
    }

}

void KNewFileMenuPrivate::_k_slotCreateDirectory(bool writeHiddenDir)
{
    QUrl url;
    QUrl baseUrl = m_popupFiles.first();
    bool askAgain = false;

    QString name = expandTilde(m_text);

    if (!name.isEmpty()) {
      if (QDir::isAbsolutePath(name)) {
        url = QUrl::fromLocalFile(name);
      } else {
        if (!m_viewShowsHiddenFiles && name.startsWith('.')) {
          if (!writeHiddenDir) {
            confirmCreatingHiddenDir(name);
            return;
          }
        }
        name = KIO::encodeFileName( name );
        url = baseUrl;
        url.setPath(url.path() + '/' + name);
      }
    }

    if (!askAgain) {
      KIO::SimpleJob * job = KIO::mkdir(url);
      job->setProperty("isMkdirJob", true); // KDE5: cast to MkdirJob in slotResult instead
      KJobWidgets::setWindow(job, m_parentWidget);
      job->ui()->setAutoErrorHandlingEnabled(true);
      KIO::FileUndoManager::self()->recordJob( KIO::FileUndoManager::Mkdir, QList<QUrl>(), url, job );

      if (job) {
        // We want the error handling to be done by slotResult so that subclasses can reimplement it
        job->ui()->setAutoErrorHandlingEnabled(false);
        QObject::connect(job, SIGNAL(result(KJob*)), q, SLOT(slotResult(KJob*)));
      }
    }
    else {
      q->createDirectory(); // ask again for the name
    }
    _k_slotAbortDialog();
}

void KNewFileMenuPrivate::_k_slotCreateHiddenDirectory()
{
    _k_slotCreateDirectory(true);
}

void KNewFileMenuPrivate::_k_slotFillTemplates()
{
    KNewFileMenuSingleton* s = kNewMenuGlobals();
    //qDebug();
    // Ensure any changes in the templates dir will call this
    if (! s->dirWatch) {
        s->dirWatch = new KDirWatch;
        const QStringList dirs = QStandardPaths::locateAll(QStandardPaths::GenericDataLocation, "templates", QStandardPaths::LocateDirectory);
        for (QStringList::const_iterator it = dirs.constBegin() ; it != dirs.constEnd() ; ++it) {
            //qDebug() << "Templates resource dir:" << *it;
            s->dirWatch->addDir(*it);
        }
        QObject::connect(s->dirWatch, SIGNAL(dirty(QString)),
                         q, SLOT(_k_slotFillTemplates()));
        QObject::connect(s->dirWatch, SIGNAL(created(QString)),
                         q, SLOT(_k_slotFillTemplates()));
        QObject::connect(s->dirWatch, SIGNAL(deleted(QString)),
                         q, SLOT(_k_slotFillTemplates()));
        // Ok, this doesn't cope with new dirs in KDEDIRS, but that's another story
    }
    ++s->templatesVersion;
    s->filesParsed = false;

    s->templatesList->clear();

    // Look into "templates" dirs.
    const QStringList files = QStandardPaths::locateAll(QStandardPaths::GenericDataLocation, "templates", QStandardPaths::LocateDirectory);
    QMap<QString, KNewFileMenuSingleton::Entry> slist; // used for sorting
    Q_FOREACH(const QString& file, files) {
        //qDebug() << file;
        if (file[0] != '.') {
            KNewFileMenuSingleton::Entry e;
            e.filePath = file;
            e.entryType = KNewFileMenuSingleton::Unknown; // not parsed yet

            // Put Directory first in the list (a bit hacky),
            // and TextFile before others because it's the most used one.
            // This also sorts by user-visible name.
            // The rest of the re-ordering is done in fillMenu.
            const KDesktopFile config(file);
            QString key = config.desktopGroup().readEntry("Name");
            if (file.endsWith("Directory.desktop")) {
                key.prepend('0');
            } else if (file.endsWith("TextFile.desktop")) {
                key.prepend('1');
            } else {
                key.prepend('2');
            }
            slist.insert(key, e);
        }
    }
    (*s->templatesList) += slist.values();
}

void KNewFileMenuPrivate::_k_slotOtherDesktopFile()
{
    executeStrategy();
}

void KNewFileMenuPrivate::_k_slotRealFileOrDir()
{
    m_copyData.m_chosenFileName = m_text;
    _k_slotAbortDialog();
    executeStrategy();
}

void KNewFileMenuPrivate::_k_slotSymLink()
{
    KNameAndUrlInputDialog* dlg = static_cast<KNameAndUrlInputDialog*>(m_fileDialog);

    m_copyData.m_chosenFileName = dlg->name(); // no path
    QUrl linkUrl = dlg->url(); // the url to put in the file

    if (m_copyData.m_chosenFileName.isEmpty() || linkUrl.isEmpty())
        return;

    if (linkUrl.isRelative())
        m_copyData.m_src = linkUrl.url();
    else if (linkUrl.isLocalFile())
        m_copyData.m_src = linkUrl.toLocalFile();
    else {
        QDialog* dialog = new QDialog(m_parentWidget);
        dialog->setWindowTitle(i18n("Sorry"));
	dialog->setObjectName( "sorry" );
	dialog->setModal(m_modal);
	dialog->setAttribute(Qt::WA_DeleteOnClose);

        QDialogButtonBox *buttonBox = new QDialogButtonBox(dialog);
        buttonBox->setStandardButtons(QDialogButtonBox::Ok);

	m_fileDialog = dialog;

        KMessageBox::createKMessageBox(dialog, buttonBox, QMessageBox::Warning,
	  i18n("Basic links can only point to local files or directories.\nPlease use \"Link to Location\" for remote URLs."),
	  QStringList(), QString(), 0, KMessageBox::NoExec,
	  QString());

	dialog->show();
	return;
    }
    executeStrategy();
}

void KNewFileMenuPrivate::_k_slotTextChanged(const QString & text)
{
    m_text = text;
}

void KNewFileMenuPrivate::_k_slotUrlDesktopFile()
{
    KNameAndUrlInputDialog* dlg = static_cast<KNameAndUrlInputDialog*>(m_fileDialog);

    m_copyData.m_chosenFileName = dlg->name(); // no path
    QUrl linkUrl = dlg->url();

    // Filter user input so that short uri entries, e.g. www.kde.org, are
    // handled properly. This not only makes the icon detection below work
    // properly, but opening the URL link where the short uri will not be
    // sent to the application (opening such link Konqueror fails).
    KUriFilterData uriData;
    uriData.setData(linkUrl); // the url to put in the file
    uriData.setCheckForExecutables(false);

    if (KUriFilter::self()->filterUri(uriData, QStringList() << QLatin1String("kshorturifilter"))) {
        linkUrl = uriData.uri();
    }

    if (m_copyData.m_chosenFileName.isEmpty() || linkUrl.isEmpty())
        return;

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

KNewFileMenu::KNewFileMenu(KActionCollection* collection, const QString& name, QObject* parent)
    : KActionMenu(QIcon::fromTheme("document-new"), i18n("Create New"), parent),
      d(new KNewFileMenuPrivate(this))
{
    // Don't fill the menu yet
    // We'll do that in checkUpToDate (should be connected to aboutToShow)
    d->m_newMenuGroup = new QActionGroup(this);
    connect(d->m_newMenuGroup, SIGNAL(triggered(QAction*)), this, SLOT(_k_slotActionTriggered(QAction*)));
    d->m_actionCollection = collection;
    d->m_parentWidget = qobject_cast<QWidget*>(parent);
    d->m_newDirAction = 0;

    d->m_actionCollection->addAction(name, this);

    d->m_menuDev = new KActionMenu(QIcon::fromTheme("drive-removable-media"), i18n("Link to Device"), this);
}

KNewFileMenu::~KNewFileMenu()
{
    //qDebug() << this;
    delete d;
}

void KNewFileMenu::checkUpToDate()
{
    KNewFileMenuSingleton* s = kNewMenuGlobals();
    //qDebug() << this << "m_menuItemsVersion=" << d->m_menuItemsVersion
    //              << "s->templatesVersion=" << s->templatesVersion;
    if (d->m_menuItemsVersion < s->templatesVersion || s->templatesVersion == 0) {
        //qDebug() << "recreating actions";
        // We need to clean up the action collection
        // We look for our actions using the group
        foreach (QAction* action, d->m_newMenuGroup->actions())
            delete action;

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
    if (d->m_popupFiles.isEmpty())
	return;

    QUrl baseUrl = d->m_popupFiles.first();
    QString name = d->m_text.isEmpty()? i18nc("Default name for a new folder", "New Folder") :
      d->m_text;

    if (baseUrl.isLocalFile() && QFileInfo(baseUrl.toLocalFile() + '/' + name).exists())
	name = KIO::suggestName(baseUrl, name);

    QDialog* fileDialog = new QDialog(d->m_parentWidget);
    fileDialog->setModal(isModal());
    fileDialog->setAttribute(Qt::WA_DeleteOnClose);
    fileDialog->setWindowTitle(i18nc("@title:window", "New Folder"));

    QVBoxLayout *layout = new QVBoxLayout;
    QLabel *label = new QLabel(i18n("Create new folder in:\n%1", baseUrl.toDisplayString(QUrl::PreferLocalFile)), fileDialog);

    QLineEdit *lineEdit = new QLineEdit(fileDialog);
    lineEdit->setClearButtonEnabled(true);
    lineEdit->setText(name);

    d->_k_slotTextChanged(name); // have to save string in d->m_text in case user does not touch dialog
    connect(lineEdit, SIGNAL(textChanged(QString)), this, SLOT(_k_slotTextChanged(QString)));

    QDialogButtonBox *buttonBox = new QDialogButtonBox(fileDialog);
    buttonBox->setStandardButtons(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    QObject::connect(buttonBox, SIGNAL(accepted()), fileDialog, SLOT(accept()));
    QObject::connect(buttonBox, SIGNAL(rejected()), fileDialog, SLOT(reject()));

    layout->addWidget(label);
    layout->addWidget(lineEdit);
    layout->addWidget(buttonBox);

    fileDialog->setLayout(layout);
    connect(fileDialog, SIGNAL(accepted()), this, SLOT(_k_slotCreateDirectory()));
    connect(fileDialog, SIGNAL(rejected()), this, SLOT(_k_slotAbortDialog()));

    d->m_fileDialog = fileDialog;

    fileDialog->show();
    lineEdit->selectAll();
    lineEdit->setFocus();
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

void KNewFileMenu::setPopupFiles(const QList<QUrl>& files)
{
    d->m_popupFiles = files;
    if (files.isEmpty()) {
        d->m_newMenuGroup->setEnabled(false);
    } else {
        QUrl firstUrl = files.first();
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


void KNewFileMenu::setParentWidget(QWidget* parentWidget)
{
    d->m_parentWidget = parentWidget;
}

void KNewFileMenu::setSupportedMimeTypes(const QStringList& mime)
{
    d->m_supportedMimeTypes = mime;
}

void KNewFileMenu::setViewShowsHiddenFiles(bool b)
{
    d->m_viewShowsHiddenFiles = b;
}

void KNewFileMenu::slotResult(KJob * job)
{
    if (job->error()) {
        static_cast<KIO::Job*>(job)->ui()->showErrorMessage();
    } else {
        // Was this a copy or a mkdir?
        KIO::CopyJob* copyJob = ::qobject_cast<KIO::CopyJob*>(job);
        if (copyJob) {
            const QUrl destUrl = copyJob->destUrl();
            const QUrl localUrl = d->mostLocalUrl(destUrl);
            if (localUrl.isLocalFile()) {
                // Normal (local) file. Need to "touch" it, kio_file copied the mtime.
                (void) ::utime(QFile::encodeName(localUrl.toLocalFile()).constData(), 0);
            }
            emit fileCreated(destUrl);
        } else if (KIO::SimpleJob* simpleJob = ::qobject_cast<KIO::SimpleJob*>(job)) {
            // Can be mkdir or symlink
            if (simpleJob->property("isMkdirJob").toBool() == true) {
                // qDebug() << "Emit directoryCreated" << simpleJob->url();
                emit directoryCreated(simpleJob->url());
            } else {
                emit fileCreated(simpleJob->url());
            }
        }
    }
    if (!d->m_tempFileToDelete.isEmpty())
        QFile::remove(d->m_tempFileToDelete);
}


QStringList KNewFileMenu::supportedMimeTypes() const
{
    return d->m_supportedMimeTypes;
}


#include "moc_knewfilemenu.cpp"

