/*  This file is part of the KDE libraries

    Copyright (C) 1997 Torben Weis <weis@stud.uni-frankfurt.de>
    Copyright (C) 1999 Dirk Mueller <mueller@kde.org>
    Portions copyright (C) 1999 Preston Brown <pbrown@kde.org>
    Copyright (C) 2007 Pino Toscano <pino@kde.org>

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

#include "kopenwithdialog.h"
#include "kopenwithdialog_p.h"

#include <QDialogButtonBox>
#include <QtCore/QtAlgorithms>
#include <QtCore/QList>
#include <QLabel>
#include <QLayout>
#include <QCheckBox>
#include <QStyle>
#include <QStyleOptionButton>
#include <qstandardpaths.h>
#include <qmimedatabase.h>

#include <kurlauthorized.h>
#include <khistorycombobox.h>
#include <kdesktopfile.h>
#include <klineedit.h>
#include <klocalizedstring.h>
#include <kmessagebox.h>
#include <kshell.h>
#include <kio/desktopexecparser.h>
#include <kstringhandler.h>
#include <kurlcompletion.h>
#include <kurlrequester.h>
#include <kservicegroup.h>
#include <QDebug>

#include <assert.h>
#include <stdlib.h>
#include <kbuildsycocaprogressdialog.h>
#include <kconfiggroup.h>

inline void writeEntry( KConfigGroup& group, const char* key,
                        const KCompletion::CompletionMode& aValue,
                        KConfigBase::WriteConfigFlags flags = KConfigBase::Normal )
{
    group.writeEntry(key, int(aValue), flags);
}

namespace KDEPrivate {

class AppNode
{
public:
    AppNode()
        : isDir(false), parent(0), fetched(false)
    {
    }
    ~AppNode()
    {
        qDeleteAll(children);
    }

    QString icon;
    QString text;
    QString entryPath;
    QString exec;
    bool isDir;

    AppNode *parent;
    bool fetched;

    QList<AppNode*> children;
};

bool AppNodeLessThan(KDEPrivate::AppNode *n1, KDEPrivate::AppNode *n2)
{
    if (n1->isDir) {
        if (n2->isDir) {
            return n1->text.compare(n2->text, Qt::CaseInsensitive) < 0;
        } else {
            return true;
        }
    } else {
        if (n2->isDir) {
            return false;
        } else {
            return n1->text.compare(n2->text, Qt::CaseInsensitive) < 0;
        }
    }
    return true;
}

}


class KApplicationModelPrivate
{
public:
    KApplicationModelPrivate(KApplicationModel *qq)
        : q(qq), root(new KDEPrivate::AppNode())
    {
    }
    ~KApplicationModelPrivate()
    {
        delete root;
    }

    void fillNode(const QString &entryPath, KDEPrivate::AppNode *node);

    KApplicationModel *q;

    KDEPrivate::AppNode *root;
};

void KApplicationModelPrivate::fillNode(const QString &_entryPath, KDEPrivate::AppNode *node)
{
   KServiceGroup::Ptr root = KServiceGroup::group(_entryPath);
   if (!root || !root->isValid()) return;

   const KServiceGroup::List list = root->entries();

   for( KServiceGroup::List::ConstIterator it = list.begin();
       it != list.end(); ++it)
   {
      QString icon;
      QString text;
      QString entryPath;
      QString exec;
      bool isDir = false;
      const KSycocaEntry::Ptr p = (*it);
      if (p->isType(KST_KService))
      {
         const KService::Ptr service = KService::Ptr(p);

         if (service->noDisplay())
            continue;

         icon = service->icon();
         text = service->name();
         exec = service->exec();
         entryPath = service->entryPath();
      }
      else if (p->isType(KST_KServiceGroup))
      {
         const KServiceGroup::Ptr serviceGroup = KServiceGroup::Ptr(p);

         if (serviceGroup->noDisplay() || serviceGroup->childCount() == 0)
            continue;

         icon = serviceGroup->icon();
         text = serviceGroup->caption();
         entryPath = serviceGroup->entryPath();
         isDir = true;
      }
      else
      {
         qWarning() << "KServiceGroup: Unexpected object in list!";
         continue;
      }

      KDEPrivate::AppNode *newnode = new KDEPrivate::AppNode();
      newnode->icon = icon;
      newnode->text = text;
      newnode->entryPath = entryPath;
      newnode->exec = exec;
      newnode->isDir = isDir;
      newnode->parent = node;
      node->children.append(newnode);
   }
   qStableSort(node->children.begin(), node->children.end(), KDEPrivate::AppNodeLessThan);
}



KApplicationModel::KApplicationModel(QObject *parent)
    : QAbstractItemModel(parent), d(new KApplicationModelPrivate(this))
{
    d->fillNode(QString(), d->root);
}

KApplicationModel::~KApplicationModel()
{
    delete d;
}

bool KApplicationModel::canFetchMore(const QModelIndex &parent) const
{
    if (!parent.isValid())
        return false;

    KDEPrivate::AppNode *node = static_cast<KDEPrivate::AppNode*>(parent.internalPointer());
    return node->isDir && !node->fetched;
}

int KApplicationModel::columnCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent)
    return 1;
}

QVariant KApplicationModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid())
        return QVariant();

    KDEPrivate::AppNode *node = static_cast<KDEPrivate::AppNode*>(index.internalPointer());

    switch (role) {
    case Qt::DisplayRole:
        return node->text;
        break;
    case Qt::DecorationRole:
        if (!node->icon.isEmpty()) {
            return QIcon::fromTheme(node->icon);
        }
        break;
    default:
        ;
    }
    return QVariant();
}

void KApplicationModel::fetchMore(const QModelIndex &parent)
{
    if (!parent.isValid())
        return;

    KDEPrivate::AppNode *node = static_cast<KDEPrivate::AppNode*>(parent.internalPointer());
    if (!node->isDir)
        return;

    emit layoutAboutToBeChanged();
    d->fillNode(node->entryPath, node);
    node->fetched = true;
    emit layoutChanged();
}

bool KApplicationModel::hasChildren(const QModelIndex &parent) const
{
    if (!parent.isValid())
        return true;

    KDEPrivate::AppNode *node = static_cast<KDEPrivate::AppNode*>(parent.internalPointer());
    return node->isDir;
}

QVariant KApplicationModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation != Qt::Horizontal || section != 0)
        return QVariant();

    switch (role) {
    case Qt::DisplayRole:
        return i18n("Known Applications");
        break;
    default:
        return QVariant();
    }
}

QModelIndex KApplicationModel::index(int row, int column, const QModelIndex &parent) const
{
    if (row < 0 || column != 0)
        return QModelIndex();

    KDEPrivate::AppNode *node = d->root;
    if (parent.isValid())
        node = static_cast<KDEPrivate::AppNode*>(parent.internalPointer());

    if (row >= node->children.count())
        return QModelIndex();
    else
        return createIndex(row, 0, node->children.at(row));
}

QModelIndex KApplicationModel::parent(const QModelIndex &index) const
{
    if (!index.isValid())
        return QModelIndex();

    KDEPrivate::AppNode *node = static_cast<KDEPrivate::AppNode*>(index.internalPointer());
    if (node->parent->parent) {
        int id = node->parent->parent->children.indexOf(node->parent);

        if (id >= 0 && id < node->parent->parent->children.count())
           return createIndex(id, 0, node->parent);
        else
            return QModelIndex();
    }
    else
        return QModelIndex();
}

int KApplicationModel::rowCount(const QModelIndex &parent) const
{
    if (!parent.isValid())
        return d->root->children.count();

    KDEPrivate::AppNode *node = static_cast<KDEPrivate::AppNode*>(parent.internalPointer());
    return node->children.count();
}

QString KApplicationModel::entryPathFor(const QModelIndex &index) const
{
    if (!index.isValid())
        return QString();

    KDEPrivate::AppNode *node = static_cast<KDEPrivate::AppNode*>(index.internalPointer());
    return node->entryPath;
}

QString KApplicationModel::execFor(const QModelIndex &index) const
{
    if (!index.isValid())
        return QString();

    KDEPrivate::AppNode *node = static_cast<KDEPrivate::AppNode*>(index.internalPointer());
    return node->exec;
}

bool KApplicationModel::isDirectory(const QModelIndex &index) const
{
    if (!index.isValid())
        return false;

    KDEPrivate::AppNode *node = static_cast<KDEPrivate::AppNode*>(index.internalPointer());
    return node->isDir;
}

class KApplicationViewPrivate
{
public:
    KApplicationViewPrivate()
        : appModel(0)
    {
    }

    KApplicationModel *appModel;
};

KApplicationView::KApplicationView(QWidget *parent)
    : QTreeView(parent), d(new KApplicationViewPrivate)
{
}

KApplicationView::~KApplicationView()
{
    delete d;
}

void KApplicationView::setModel(QAbstractItemModel *model)
{
    if (d->appModel) {
        disconnect(selectionModel(), SIGNAL(selectionChanged(QItemSelection,QItemSelection)),
                this, SLOT(slotSelectionChanged(QItemSelection,QItemSelection)));
    }

    QTreeView::setModel(model);

    d->appModel = qobject_cast<KApplicationModel*>(model);
    if (d->appModel) {
        connect(selectionModel(), SIGNAL(selectionChanged(QItemSelection,QItemSelection)),
                this, SLOT(slotSelectionChanged(QItemSelection,QItemSelection)));
    }
}

bool KApplicationView::isDirSel() const
{
    if (d->appModel) {
        QModelIndex index = selectionModel()->currentIndex();
        return d->appModel->isDirectory(index);
    }
    return false;
}

void KApplicationView::currentChanged(const QModelIndex &current, const QModelIndex &previous)
{
    QTreeView::currentChanged(current, previous);

    if (d->appModel && !d->appModel->isDirectory(current)) {
        QString exec = d->appModel->execFor(current);
        if (!exec.isEmpty()) {
            emit highlighted(d->appModel->entryPathFor(current), exec);
        }
    }
}

void KApplicationView::slotSelectionChanged(const QItemSelection &selected, const QItemSelection &deselected)
{
    Q_UNUSED(deselected)

    const QModelIndexList indexes = selected.indexes();
    if (indexes.count() == 1 && !d->appModel->isDirectory(indexes.at(0))) {
        QString exec = d->appModel->execFor(indexes.at(0));
        if (!exec.isEmpty()) {
            emit this->selected(d->appModel->entryPathFor(indexes.at(0)), exec);
        }
    }
}



/***************************************************************
 *
 * KOpenWithDialog
 *
 ***************************************************************/
class KOpenWithDialogPrivate
{
public:
    KOpenWithDialogPrivate(KOpenWithDialog *qq)
        : q(qq), saveNewApps(false)
    {
    }

    KOpenWithDialog *q;

    /**
     * Determine mime type from URLs
     */
    void setMimeType(const QList<QUrl> &_urls);

    void addToMimeAppsList(const QString& serviceId);

    /**
     * Create a dialog that asks for a application to open a given
     * URL(s) with.
     *
     * @param text   appears as a label on top of the entry box.
     * @param value  is the initial value of the line
     */
    void init(const QString &text, const QString &value);

    /**
     * Called by checkAccept() in order to save the history of the combobox
     */
    void saveComboboxHistory();

    /**
     * Process the choices made by the user, and return true if everything is OK.
     * Called by KOpenWithDialog::accept(), i.e. when clicking on OK or typing Return.
     */
    bool checkAccept();

    // slots
    void _k_slotDbClick();
    void _k_slotFileSelected();

    bool saveNewApps;
    bool m_terminaldirty;
    KService::Ptr curService;
    KApplicationView *view;
    KUrlRequester *edit;
    QString m_command;
    QLabel *label;
    QString qMimeType;
    QCheckBox *terminal;
    QCheckBox *remember;
    QCheckBox *nocloseonexit;
    KService::Ptr m_pService;
    QDialogButtonBox *buttonBox;
};

KOpenWithDialog::KOpenWithDialog( const QList<QUrl>& _urls, QWidget* parent )
    : QDialog(parent), d(new KOpenWithDialogPrivate(this))
{
    setObjectName( QLatin1String( "openwith" ) );
    setModal( true );
    setWindowTitle( i18n( "Open With" ) );

    QString text;
    if( _urls.count() == 1 )
    {
        text = i18n("<qt>Select the program that should be used to open <b>%1</b>. "
                     "If the program is not listed, enter the name or click "
                     "the browse button.</qt>",  _urls.first().fileName() );
    }
    else
        // Should never happen ??
        text = i18n( "Choose the name of the program with which to open the selected files." );
    d->setMimeType(_urls);
    d->init(text, QString());
}

KOpenWithDialog::KOpenWithDialog( const QList<QUrl>& _urls, const QString&_text,
                            const QString& _value, QWidget *parent)
    : QDialog(parent), d(new KOpenWithDialogPrivate(this))
{
  setObjectName( QLatin1String( "openwith" ) );
  setModal( true );
  QString caption;
    if (!_urls.isEmpty() && !_urls.first().isEmpty())
        caption = KStringHandler::csqueeze(_urls.first().toDisplayString());
  if (_urls.count() > 1)
      caption += QString::fromLatin1("...");
  setWindowTitle(caption);
    d->setMimeType(_urls);
    d->init(_text, _value);
}

KOpenWithDialog::KOpenWithDialog( const QString &mimeType, const QString& value,
                            QWidget *parent)
    : QDialog(parent), d(new KOpenWithDialogPrivate(this))
{
  setObjectName( QLatin1String( "openwith" ) );
  setModal( true );
  setWindowTitle(i18n("Choose Application for %1", mimeType));
  QString text = i18n("<qt>Select the program for the file type: <b>%1</b>. "
                      "If the program is not listed, enter the name or click "
                      "the browse button.</qt>", mimeType);
    d->qMimeType = mimeType;
    d->init(text, value);
    if (d->remember) {
        d->remember->hide();
    }
}

KOpenWithDialog::KOpenWithDialog( QWidget *parent)
    : QDialog(parent), d(new KOpenWithDialogPrivate(this))
{
  setObjectName( QLatin1String( "openwith" ) );
  setModal( true );
  setWindowTitle(i18n("Choose Application"));
  QString text = i18n("<qt>Select a program. "
                      "If the program is not listed, enter the name or click "
                      "the browse button.</qt>");
    d->qMimeType.clear();
    d->init(text, QString());
}

void KOpenWithDialogPrivate::setMimeType(const QList<QUrl> &_urls)
{
    if (_urls.count() == 1) {
        QMimeDatabase db;
        QMimeType mime = db.mimeTypeForUrl(_urls.first());
        qMimeType = mime.name();
        if (mime.isDefault())
            qMimeType.clear();
    } else {
        qMimeType.clear();
    }
}

void KOpenWithDialogPrivate::init(const QString &_text, const QString &_value)
{
  bool bReadOnly = !KAuthorized::authorize("shell_access");
  m_terminaldirty = false;
    view = 0;
    m_pService = 0;
    curService = 0;

  QBoxLayout *topLayout = new QVBoxLayout;
  q->setLayout(topLayout);
    label = new QLabel(_text, q);
  label->setWordWrap(true);
  topLayout->addWidget(label);

  if (!bReadOnly)
  {
    // init the history combo and insert it into the URL-Requester
    KHistoryComboBox *combo = new KHistoryComboBox();
    KLineEdit *lineEdit = new KLineEdit(q);
    lineEdit->setClearButtonShown(true);
    combo->setLineEdit(lineEdit);
    combo->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
    combo->setDuplicatesEnabled( false );
    KConfigGroup cg( KSharedConfig::openConfig(), QString::fromLatin1("Open-with settings") );
    int max = cg.readEntry( "Maximum history", 15 );
    combo->setMaxCount( max );
    int mode = cg.readEntry( "CompletionMode", int(KCompletion::CompletionPopup));
    combo->setCompletionMode((KCompletion::CompletionMode)mode);
    const QStringList list = cg.readEntry( "History", QStringList() );
    combo->setHistoryItems( list, true );
    edit = new KUrlRequester(combo, q);
  }
  else
  {
    edit = new KUrlRequester(q);
    edit->lineEdit()->setReadOnly(true);
    edit->button()->hide();
  }

  edit->setText( _value );
  edit->setWhatsThis(i18n(
    "Following the command, you can have several place holders which will be replaced "
    "with the actual values when the actual program is run:\n"
    "%f - a single file name\n"
    "%F - a list of files; use for applications that can open several local files at once\n"
    "%u - a single URL\n"
    "%U - a list of URLs\n"
    "%d - the directory of the file to open\n"
    "%D - a list of directories\n"
    "%i - the icon\n"
    "%m - the mini-icon\n"
    "%c - the comment"));

  topLayout->addWidget(edit);

  if ( edit->comboBox() ) {
    KUrlCompletion *comp = new KUrlCompletion( KUrlCompletion::ExeCompletion );
    edit->comboBox()->setCompletionObject( comp );
    edit->comboBox()->setAutoDeleteCompletionObject( true );
  }

    QObject::connect(edit, SIGNAL(textChanged(QString)), q, SLOT(slotTextChanged()));
    QObject::connect(edit, SIGNAL(urlSelected(QUrl)), q, SLOT(_k_slotFileSelected()));

    view = new KApplicationView(q);
    view->setModel(new KApplicationModel(view));
    topLayout->addWidget(view);
    topLayout->setStretchFactor(view, 1);

    QObject::connect(view, SIGNAL(selected(QString,QString)),
                     q, SLOT(slotSelected(QString,QString)));
    QObject::connect(view, SIGNAL(highlighted(QString,QString)),
                     q, SLOT(slotHighlighted(QString,QString)));
    QObject::connect(view, SIGNAL(doubleClicked(QModelIndex)),
                     q, SLOT(_k_slotDbClick()));

  terminal = new QCheckBox(i18n("Run in &terminal"), q);
  if (bReadOnly)
     terminal->hide();
    QObject::connect(terminal, SIGNAL(toggled(bool)), q, SLOT(slotTerminalToggled(bool)));

  topLayout->addWidget(terminal);

  QStyleOptionButton checkBoxOption;
  checkBoxOption.initFrom(terminal);
  int checkBoxIndentation = terminal->style()->pixelMetric( QStyle::PM_IndicatorWidth, &checkBoxOption, terminal );
  checkBoxIndentation += terminal->style()->pixelMetric( QStyle::PM_CheckBoxLabelSpacing, &checkBoxOption, terminal );

  QBoxLayout* nocloseonexitLayout = new QHBoxLayout();
  nocloseonexitLayout->setMargin( 0 );
  QSpacerItem* spacer = new QSpacerItem( checkBoxIndentation, 0, QSizePolicy::Fixed, QSizePolicy::Minimum );
  nocloseonexitLayout->addItem( spacer );

  nocloseonexit = new QCheckBox(i18n("&Do not close when command exits"), q);
  nocloseonexit->setChecked( false );
  nocloseonexit->setDisabled( true );

  // check to see if we use konsole if not disable the nocloseonexit
  // because we don't know how to do this on other terminal applications
  KConfigGroup confGroup( KSharedConfig::openConfig(), QString::fromLatin1("General") );
  QString preferredTerminal = confGroup.readPathEntry("TerminalApplication", QString::fromLatin1("konsole"));

  if (bReadOnly || preferredTerminal != "konsole")
     nocloseonexit->hide();

  nocloseonexitLayout->addWidget( nocloseonexit );
  topLayout->addLayout( nocloseonexitLayout );

  if (!qMimeType.isNull())
  {
    remember = new QCheckBox(i18n("&Remember application association for this type of file"), q);
    //    remember->setChecked(true);
    topLayout->addWidget(remember);
  }
  else
    remember = 0L;

  buttonBox = new QDialogButtonBox(q);
  buttonBox->setStandardButtons(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
  q->connect(buttonBox, SIGNAL(accepted()), q, SLOT(accept()));
  q->connect(buttonBox, SIGNAL(rejected()), q, SLOT(reject()));
  topLayout->addWidget(buttonBox);

  q->setMinimumSize(q->minimumSizeHint());
  //edit->setText( _value );
  // This is what caused "can't click on items before clicking on Name header".
  // Probably due to the resizeEvent handler using width().
  //resize( minimumWidth(), sizeHint().height() );
  edit->setFocus();
    q->slotTextChanged();
}


// ----------------------------------------------------------------------

KOpenWithDialog::~KOpenWithDialog()
{
    delete d;
}


// ----------------------------------------------------------------------

void KOpenWithDialog::slotSelected( const QString& /*_name*/, const QString& _exec )
{
    KService::Ptr pService = d->curService;
    d->edit->setText(_exec); // calls slotTextChanged :(
    d->curService = pService;
}


// ----------------------------------------------------------------------

void KOpenWithDialog::slotHighlighted(const QString& entryPath, const QString&)
{
    d->curService = KService::serviceByDesktopPath(entryPath);
    if (d->curService && !d->m_terminaldirty)
    {
        // ### indicate that default value was restored
        d->terminal->setChecked(d->curService->terminal());
        QString terminalOptions = d->curService->terminalOptions();
        d->nocloseonexit->setChecked((terminalOptions.contains(QLatin1String("--noclose")) > 0));
        d->m_terminaldirty = false; // slotTerminalToggled changed it
    }
}

// ----------------------------------------------------------------------

void KOpenWithDialog::slotTextChanged()
{
    // Forget about the service
    d->curService = 0L;
    d->buttonBox->button(QDialogButtonBox::Ok)->setEnabled(!d->edit->text().isEmpty());
}

// ----------------------------------------------------------------------

void KOpenWithDialog::slotTerminalToggled(bool)
{
    // ### indicate that default value was overridden
    d->m_terminaldirty = true;
    d->nocloseonexit->setDisabled(!d->terminal->isChecked());
}

// ----------------------------------------------------------------------

void KOpenWithDialogPrivate::_k_slotDbClick()
{
    // check if a directory is selected
    if (view->isDirSel()) {
        return;
    }
    q->accept();
}

void KOpenWithDialogPrivate::_k_slotFileSelected()
{
    // quote the path to avoid unescaped whitespace, backslashes, etc.
    edit->setText(KShell::quoteArg(edit->text()));
}

void KOpenWithDialog::setSaveNewApplications(bool b)
{
  d->saveNewApps = b;
}

static QString simplifiedExecLineFromService(const QString& fullExec)
{
    QString exec = fullExec;
    exec.remove("%u", Qt::CaseInsensitive);
    exec.remove("%f", Qt::CaseInsensitive);
    exec.remove("-caption %c");
    exec.remove("-caption \"%c\"");
    exec.remove("%i");
    exec.remove("%m");
    return exec.simplified();
}

void KOpenWithDialogPrivate::addToMimeAppsList(const QString& serviceId /*menu id or storage id*/)
{
    KSharedConfig::Ptr profile = KSharedConfig::openConfig("mimeapps.list", KConfig::NoGlobals, QStandardPaths::ApplicationsLocation);
    KConfigGroup addedApps(profile, "Added Associations");
    QStringList apps = addedApps.readXdgListEntry(qMimeType);
    apps.removeAll(serviceId);
    apps.prepend(serviceId); // make it the preferred app
    addedApps.writeXdgListEntry(qMimeType, apps);
    addedApps.sync();

    // Also make sure the "auto embed" setting for this mimetype is off
    KSharedConfig::Ptr fileTypesConfig = KSharedConfig::openConfig("filetypesrc", KConfig::NoGlobals);
    fileTypesConfig->group("EmbedSettings").writeEntry(QString("embed-")+qMimeType, false);
    fileTypesConfig->sync();

    // qDebug() << "rebuilding ksycoca...";

    // kbuildsycoca is the one reading mimeapps.list, so we need to run it now
    KBuildSycocaProgressDialog::rebuildKSycoca(q);

    m_pService = KService::serviceByStorageId(serviceId);
    Q_ASSERT( m_pService );
}

bool KOpenWithDialogPrivate::checkAccept()
{
    const QString typedExec(edit->text());
    if (typedExec.isEmpty())
        return false;
    QString fullExec(typedExec);

    QString serviceName;
    QString initialServiceName;
    QString preferredTerminal;
    QString configPath;
    QString serviceExec;
    m_pService = curService;
    if (!m_pService) {
        // No service selected - check the command line

        // Find out the name of the service from the command line, removing args and paths
        serviceName = KIO::DesktopExecParser::executableName(typedExec);
        if (serviceName.isEmpty()) {
            KMessageBox::error(q, i18n("Could not extract executable name from '%1', please type a valid program name.", serviceName));
            return false;
        }
        initialServiceName = serviceName;
        // Also remember the executableName with a path, if any, for the
        // check that the executable exists.
        // qDebug() << "initialServiceName=" << initialServiceName;
        int i = 1; // We have app, app-2, app-3... Looks better for the user.
        bool ok = false;
        // Check if there's already a service by that name, with the same Exec line
        do {
            // qDebug() << "looking for service" << serviceName;
            KService::Ptr serv = KService::serviceByDesktopName( serviceName );
            ok = !serv; // ok if no such service yet
            // also ok if we find the exact same service (well, "kwrite" == "kwrite %U")
            if (serv && !serv->noDisplay() /* #297720 */) {
                if (serv->isApplication()) {
                    /*// qDebug() << "typedExec=" << typedExec
                      << "serv->exec=" << serv->exec()
                      << "simplifiedExecLineFromService=" << simplifiedExecLineFromService(fullExec);*/
                    serviceExec = simplifiedExecLineFromService(serv->exec());
                    if (typedExec == serviceExec){
                        ok = true;
                        m_pService = serv;
                        // qDebug() << "OK, found identical service: " << serv->entryPath();
                    } else {
                        // qDebug() << "Exec line differs, service says:" << serviceExec;
                        configPath = serv->entryPath();
                        serviceExec = serv->exec();
                    }
                } else {
                    // qDebug() << "Found, but not an application:" << serv->entryPath();
                }
            }
            if (!ok) { // service was found, but it was different -> keep looking
                ++i;
                serviceName = initialServiceName + '-' + QString::number(i);
            }
        } while (!ok);
    }
    if ( m_pService ) {
        // Existing service selected
        serviceName = m_pService->name();
        initialServiceName = serviceName;
        fullExec = m_pService->exec();
    } else {
        const QString binaryName = KIO::DesktopExecParser::executablePath(typedExec);
        // qDebug() << "binaryName=" << binaryName;
        // Ensure that the typed binary name actually exists (#81190)
        if (QStandardPaths::findExecutable(binaryName).isEmpty()) {
            KMessageBox::error(q, i18n("'%1' not found, please type a valid program name.", binaryName));
            return false;
        }
    }

    if (terminal->isChecked()) {
        KConfigGroup confGroup( KSharedConfig::openConfig(), QString::fromLatin1("General") );
        preferredTerminal = confGroup.readPathEntry("TerminalApplication", QString::fromLatin1("konsole"));
        m_command = preferredTerminal;
        // only add --noclose when we are sure it is konsole we're using
        if (preferredTerminal == "konsole" && nocloseonexit->isChecked())
            m_command += QString::fromLatin1(" --noclose");
        m_command += QString::fromLatin1(" -e ");
        m_command += edit->text();
        // qDebug() << "Setting m_command to" << m_command;
    }
    if ( m_pService && terminal->isChecked() != m_pService->terminal() )
        m_pService = 0; // It's not exactly this service we're running


    const bool bRemember = remember && remember->isChecked();
    // qDebug() << "bRemember=" << bRemember << "service found=" << m_pService;
    if (m_pService) {
        if (bRemember) {
            // Associate this app with qMimeType in mimeapps.list
            Q_ASSERT(!qMimeType.isEmpty()); // we don't show the remember checkbox otherwise
            addToMimeAppsList(m_pService->storageId());
        }
    } else {
        const bool createDesktopFile = bRemember || saveNewApps;
        if (!createDesktopFile) {
            // Create temp service
            if (configPath.isEmpty())
                m_pService = new KService(initialServiceName, fullExec, QString());
            else {
                if (!typedExec.contains(QLatin1String("%u"), Qt::CaseInsensitive) &&
                    !typedExec.contains(QLatin1String("%f"), Qt::CaseInsensitive)) {
                    int index = serviceExec.indexOf(QLatin1String("%u"), 0, Qt::CaseInsensitive);
                    if (index == -1) {
                      index = serviceExec.indexOf(QLatin1String("%f"), 0, Qt::CaseInsensitive);
                    }
                    if (index > -1) {
                      fullExec += QLatin1Char(' ');
                      fullExec += serviceExec.mid(index, 2);
                    }
                }
                // qDebug() << "Creating service with Exec=" << fullExec;
                m_pService = new KService(configPath);
                m_pService->setExec(fullExec);
            }
            if (terminal->isChecked()) {
                m_pService->setTerminal(true);
                // only add --noclose when we are sure it is konsole we're using
                if (preferredTerminal == "konsole" && nocloseonexit->isChecked())
                    m_pService->setTerminalOptions("--noclose");
            }
        } else {
            // If we got here, we can't seem to find a service for what they wanted. Create one.

            QString menuId;
#ifdef Q_OS_WIN32
            // on windows, do not use the complete path, but only the default name.
            serviceName = QFileInfo(serviceName).fileName();
#endif
            QString newPath = KService::newServicePath(false /* ignored argument */, serviceName, &menuId);
            // qDebug() << "Creating new service" << serviceName << "(" << newPath << ")" << "menuId=" << menuId;

            KDesktopFile desktopFile(newPath);
            KConfigGroup cg = desktopFile.desktopGroup();
            cg.writeEntry("Type", "Application");
            cg.writeEntry("Name", initialServiceName);
            cg.writeEntry("Exec", fullExec);
            cg.writeEntry("NoDisplay", true); // don't make it appear in the K menu
            if (terminal->isChecked()) {
                cg.writeEntry("Terminal", true);
                // only add --noclose when we are sure it is konsole we're using
                if (preferredTerminal == "konsole" && nocloseonexit->isChecked())
                    cg.writeEntry("TerminalOptions", "--noclose");
            }
            cg.writeXdgListEntry("MimeType", QStringList() << qMimeType);
            cg.sync();

            addToMimeAppsList(menuId);
        }
    }

    saveComboboxHistory();
    return true;
}

void KOpenWithDialog::accept()
{
    if (d->checkAccept())
        QDialog::accept();
}

QString KOpenWithDialog::text() const
{
    if (!d->m_command.isEmpty())
        return d->m_command;
    else
        return d->edit->text();
}

void KOpenWithDialog::hideNoCloseOnExit()
{
    // uncheck the checkbox because the value could be used when "Run in Terminal" is selected
    d->nocloseonexit->setChecked(false);
    d->nocloseonexit->hide();
}

void KOpenWithDialog::hideRunInTerminal()
{
    d->terminal->hide();
    hideNoCloseOnExit();
}

KService::Ptr KOpenWithDialog::service() const
{
    return d->m_pService;
}

void KOpenWithDialogPrivate::saveComboboxHistory()
{
    KHistoryComboBox *combo = static_cast<KHistoryComboBox*>(edit->comboBox());
    if (combo) {
        combo->addToHistory(edit->text());

        KConfigGroup cg( KSharedConfig::openConfig(), QString::fromLatin1("Open-with settings") );
        cg.writeEntry( "History", combo->historyItems() );
        writeEntry( cg, "CompletionMode", combo->completionMode() );
        // don't store the completion-list, as it contains all of KUrlCompletion's
        // executables
        cg.sync();
    }
}

#include "moc_kopenwithdialog.cpp"
#include "moc_kopenwithdialog_p.cpp"
