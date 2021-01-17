/*
    This file is part of the KDE libraries
    SPDX-FileCopyrightText: 1997 Torben Weis <weis@stud.uni-frankfurt.de>
    SPDX-FileCopyrightText: 1999 Dirk Mueller <mueller@kde.org>
    Portions SPDX-FileCopyrightText: 1999 Preston Brown <pbrown@kde.org>
    SPDX-FileCopyrightText: 2007 Pino Toscano <pino@kde.org>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#include "kopenwithdialog.h"
#include "kopenwithdialog_p.h"
#include "kio_widgets_debug.h"

#include <QApplication>
#include <QDesktopWidget>
#include <QDialogButtonBox>
#include <QtAlgorithms>
#include <QList>
#include <QKeyEvent>
#include <QLabel>
#include <QLayout>
#include <QCheckBox>
#include <QStyle>
#include <QStyleOptionButton>
#include <QStandardPaths>
#include <QMimeDatabase>
#include <QScreen>

#include <kurlauthorized.h>
#include <KHistoryComboBox>
#include <KDesktopFile>
#include <KLineEdit>
#include <KSharedConfig>
#include <KLocalizedString>
#include <KMessageBox>
#include <KShell>
#include <kio/desktopexecparser.h>
#include <KStringHandler>
#include <kurlcompletion.h>
#include <kurlrequester.h>
#include <KServiceGroup>
#include <KCollapsibleGroupBox>
#include <QDebug>

#include <assert.h>
#include <stdlib.h>
#include <kbuildsycocaprogressdialog.h>
#include <KConfigGroup>

inline void writeEntry(KConfigGroup &group, const char *key,
                       const KCompletion::CompletionMode &aValue,
                       KConfigBase::WriteConfigFlags flags = KConfigBase::Normal)
{
    group.writeEntry(key, int(aValue), flags);
}

namespace KDEPrivate
{

class AppNode
{
public:
    AppNode()
        : isDir(false), parent(nullptr), fetched(false)
    {
    }
    ~AppNode()
    {
        qDeleteAll(children);
    }
    AppNode(const AppNode &) = delete;
    AppNode &operator=(const AppNode &) = delete;

    QString icon;
    QString text;
    QString tooltip;
    QString entryPath;
    QString exec;
    bool isDir;

    AppNode *parent;
    bool fetched;

    QList<AppNode *> children;
};

static bool AppNodeLessThan(KDEPrivate::AppNode *n1, KDEPrivate::AppNode *n2)
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
}

}

class KApplicationModelPrivate
{
public:
    explicit KApplicationModelPrivate(KApplicationModel *qq)
        : q(qq), root(new KDEPrivate::AppNode())
    {
    }
    ~KApplicationModelPrivate()
    {
        delete root;
    }

    void fillNode(const QString &entryPath, KDEPrivate::AppNode *node);

    KApplicationModel * const q;

    KDEPrivate::AppNode *root;
};

void KApplicationModelPrivate::fillNode(const QString &_entryPath, KDEPrivate::AppNode *node)
{
    KServiceGroup::Ptr root = KServiceGroup::group(_entryPath);
    if (!root || !root->isValid()) {
        return;
    }

    const KServiceGroup::List list = root->entries();

    for (const KSycocaEntry::Ptr &p : list) {
        QString icon;
        QString text;
        QString tooltip;
        QString entryPath;
        QString exec;
        bool isDir = false;
        if (p->isType(KST_KService)) {
            const KService::Ptr service(static_cast<KService*>(p.data()));

            if (service->noDisplay()) {
                continue;
            }

            icon = service->icon();
            text = service->name();

            // no point adding a tooltip that only repeats service->name()
            const QString generic = service->genericName();
            tooltip = generic != text ? generic : QString();

            exec = service->exec();
            entryPath = service->entryPath();
        } else if (p->isType(KST_KServiceGroup)) {
            const KServiceGroup::Ptr serviceGroup(static_cast<KServiceGroup*>(p.data()));

            if (serviceGroup->noDisplay() || serviceGroup->childCount() == 0) {
                continue;
            }

            icon = serviceGroup->icon();
            text = serviceGroup->caption();
            entryPath = serviceGroup->entryPath();
            isDir = true;
        } else {
            qCWarning(KIO_WIDGETS) << "KServiceGroup: Unexpected object in list!";
            continue;
        }

        KDEPrivate::AppNode *newnode = new KDEPrivate::AppNode();
        newnode->icon = icon;
        newnode->text = text;
        newnode->tooltip = tooltip;
        newnode->entryPath = entryPath;
        newnode->exec = exec;
        newnode->isDir = isDir;
        newnode->parent = node;
        node->children.append(newnode);
    }
    std::stable_sort(node->children.begin(), node->children.end(), KDEPrivate::AppNodeLessThan);
}

KApplicationModel::KApplicationModel(QObject *parent)
    : QAbstractItemModel(parent), d(new KApplicationModelPrivate(this))
{
    d->fillNode(QString(), d->root);
    const int nRows = rowCount();
    for (int i = 0; i < nRows; i++) {
        fetchAll(index(i, 0));
    }
}

KApplicationModel::~KApplicationModel()
{
    delete d;
}

bool KApplicationModel::canFetchMore(const QModelIndex &parent) const
{
    if (!parent.isValid()) {
        return false;
    }

    KDEPrivate::AppNode *node = static_cast<KDEPrivate::AppNode *>(parent.internalPointer());
    return node->isDir && !node->fetched;
}

int KApplicationModel::columnCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent)
    return 1;
}

QVariant KApplicationModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid()) {
        return QVariant();
    }

    KDEPrivate::AppNode *node = static_cast<KDEPrivate::AppNode *>(index.internalPointer());

    switch (role) {
    case Qt::DisplayRole:
        return node->text;
    case Qt::DecorationRole:
        if (!node->icon.isEmpty()) {
            return QIcon::fromTheme(node->icon);
        }
        break;
    case Qt::ToolTipRole:
        if (!node->tooltip.isEmpty()) {
            return node->tooltip;
        }
        break;
    default:
        ;
    }
    return QVariant();
}

void KApplicationModel::fetchMore(const QModelIndex &parent)
{
    if (!parent.isValid()) {
        return;
    }

    KDEPrivate::AppNode *node = static_cast<KDEPrivate::AppNode *>(parent.internalPointer());
    if (!node->isDir) {
        return;
    }

    Q_EMIT layoutAboutToBeChanged();
    d->fillNode(node->entryPath, node);
    node->fetched = true;
    Q_EMIT layoutChanged();
}

void KApplicationModel::fetchAll(const QModelIndex &parent)
{
    if (!parent.isValid() || !canFetchMore(parent)) {
        return;
    }

    fetchMore(parent);

    int childCount = rowCount(parent);
    for (int i = 0; i < childCount; i++) {
        const QModelIndex &child = index(i, 0, parent);
        // Recursively call the function for each child node.
        fetchAll(child);
    }
}

bool KApplicationModel::hasChildren(const QModelIndex &parent) const
{
    if (!parent.isValid()) {
        return true;
    }

    KDEPrivate::AppNode *node = static_cast<KDEPrivate::AppNode *>(parent.internalPointer());
    return node->isDir;
}

QVariant KApplicationModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation != Qt::Horizontal || section != 0) {
        return QVariant();
    }

    switch (role) {
    case Qt::DisplayRole:
        return i18n("Known Applications");
    default:
        return QVariant();
    }
}

QModelIndex KApplicationModel::index(int row, int column, const QModelIndex &parent) const
{
    if (row < 0 || column != 0) {
        return QModelIndex();
    }

    KDEPrivate::AppNode *node = d->root;
    if (parent.isValid()) {
        node = static_cast<KDEPrivate::AppNode *>(parent.internalPointer());
    }

    if (row >= node->children.count()) {
        return QModelIndex();
    } else {
        return createIndex(row, 0, node->children.at(row));
    }
}

QModelIndex KApplicationModel::parent(const QModelIndex &index) const
{
    if (!index.isValid()) {
        return QModelIndex();
    }

    KDEPrivate::AppNode *node = static_cast<KDEPrivate::AppNode *>(index.internalPointer());
    if (node->parent->parent) {
        int id = node->parent->parent->children.indexOf(node->parent);

        if (id >= 0 && id < node->parent->parent->children.count()) {
            return createIndex(id, 0, node->parent);
        } else {
            return QModelIndex();
        }
    } else {
        return QModelIndex();
    }
}

int KApplicationModel::rowCount(const QModelIndex &parent) const
{
    if (!parent.isValid()) {
        return d->root->children.count();
    }

    KDEPrivate::AppNode *node = static_cast<KDEPrivate::AppNode *>(parent.internalPointer());
    return node->children.count();
}

QString KApplicationModel::entryPathFor(const QModelIndex &index) const
{
    if (!index.isValid()) {
        return QString();
    }

    KDEPrivate::AppNode *node = static_cast<KDEPrivate::AppNode *>(index.internalPointer());
    return node->entryPath;
}

QString KApplicationModel::execFor(const QModelIndex &index) const
{
    if (!index.isValid()) {
        return QString();
    }

    KDEPrivate::AppNode *node = static_cast<KDEPrivate::AppNode *>(index.internalPointer());
    return node->exec;
}

bool KApplicationModel::isDirectory(const QModelIndex &index) const
{
    if (!index.isValid()) {
        return false;
    }

    KDEPrivate::AppNode *node = static_cast<KDEPrivate::AppNode *>(index.internalPointer());
    return node->isDir;
}


QTreeViewProxyFilter::QTreeViewProxyFilter(QObject *parent)
    : QSortFilterProxyModel(parent)
{
}

bool QTreeViewProxyFilter::filterAcceptsRow(int sourceRow, const QModelIndex &parent) const
{
    QModelIndex index = sourceModel()->index(sourceRow, 0, parent);

    if (!index.isValid()) {
        return false;
    }

    // Match the regexp only on leaf nodes
    if (!sourceModel()->hasChildren(index) && index.data().toString().contains(filterRegExp())) {
        return true;
    }

    return false;
}

class KApplicationViewPrivate
{
public:
    KApplicationViewPrivate()
        : appModel(nullptr),
          m_proxyModel(nullptr)
    {
    }

    KApplicationModel *appModel;
    QSortFilterProxyModel *m_proxyModel;
};

KApplicationView::KApplicationView(QWidget *parent)
    : QTreeView(parent), d(new KApplicationViewPrivate)
{
    setHeaderHidden(true);
}

KApplicationView::~KApplicationView()
{
    delete d;
}

void KApplicationView::setModels(KApplicationModel *model, QSortFilterProxyModel *proxyModel)
{
    if (d->appModel) {
        disconnect(selectionModel(), &QItemSelectionModel::selectionChanged,
                   this, &KApplicationView::slotSelectionChanged);
    }

    QTreeView::setModel(proxyModel); // Here we set the proxy model
    d->m_proxyModel = proxyModel; // Also store it in a member property to avoid many casts later

    d->appModel = model;
    if (d->appModel) {
        connect(selectionModel(), &QItemSelectionModel::selectionChanged,
                this, &KApplicationView::slotSelectionChanged);
    }
}

QSortFilterProxyModel* KApplicationView::proxyModel()
{
    return d->m_proxyModel;
}

bool KApplicationView::isDirSel() const
{
    if (d->appModel) {
        QModelIndex index = selectionModel()->currentIndex();
        index = d->m_proxyModel->mapToSource(index);
        return d->appModel->isDirectory(index);
    }
    return false;
}

void KApplicationView::currentChanged(const QModelIndex &current, const QModelIndex &previous)
{
    QTreeView::currentChanged(current, previous);

    if (d->appModel) {
        QModelIndex sourceCurrent = d->m_proxyModel->mapToSource(current);
        if(!d->appModel->isDirectory(sourceCurrent)) {
            QString exec = d->appModel->execFor(sourceCurrent);
            if (!exec.isEmpty()) {
                Q_EMIT highlighted(d->appModel->entryPathFor(sourceCurrent), exec);
            }
        }
    }
}

void KApplicationView::slotSelectionChanged(const QItemSelection &selected, const QItemSelection &deselected)
{
    Q_UNUSED(deselected)

    QItemSelection sourceSelected = d->m_proxyModel->mapSelectionToSource(selected);

    const QModelIndexList indexes = sourceSelected.indexes();
    if (indexes.count() == 1) {
        QString exec = d->appModel->execFor(indexes.at(0));
        Q_EMIT this->selected(d->appModel->entryPathFor(indexes.at(0)), exec);
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
    explicit KOpenWithDialogPrivate(KOpenWithDialog *qq)
        : q(qq), saveNewApps(false)
    {
    }

    KOpenWithDialog * const q;

    /**
     * Determine MIME type from URLs
     */
    void setMimeTypeFromUrls(const QList<QUrl> &_urls);

    void setMimeType(const QString &mimeType);

    void addToMimeAppsList(const QString &serviceId);

    /**
     * Creates a dialog that lets the user select an application for opening one or more URLs.
     *
     * @param text   appears as a label on top of the entry box
     * @param value  is the initial value in the entry box
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
    QString qMimeTypeComment;
    KCollapsibleGroupBox *dialogExtension;
    QCheckBox *terminal;
    QCheckBox *remember;
    QCheckBox *nocloseonexit;
    KService::Ptr m_pService;
    QDialogButtonBox *buttonBox;
};

KOpenWithDialog::KOpenWithDialog(const QList<QUrl> &_urls, QWidget *parent)
    : QDialog(parent), d(new KOpenWithDialogPrivate(this))
{
    setObjectName(QStringLiteral("openwith"));
    setModal(true);
    setWindowTitle(i18n("Open With"));

    QString text;
    if (_urls.count() == 1) {
        text = i18n("<qt>Select the program that should be used to open <b>%1</b>. "
                    "If the program is not listed, enter the name or click "
                    "the browse button.</qt>",  _urls.first().fileName().toHtmlEscaped());
    } else
        // Should never happen ??
    {
        text = i18n("Choose the name of the program with which to open the selected files.");
    }
    d->setMimeTypeFromUrls(_urls);
    d->init(text, QString());
}

KOpenWithDialog::KOpenWithDialog(const QList<QUrl> &_urls, const QString &_text,
                                 const QString &_value, QWidget *parent)
    : KOpenWithDialog(_urls, QString(), _text, _value, parent)
{
}

KOpenWithDialog::KOpenWithDialog(const QList<QUrl> &_urls, const QString &mimeType,
                                 const QString &_text, const QString &_value,
                                 QWidget *parent)
    : QDialog(parent), d(new KOpenWithDialogPrivate(this))
{
    setObjectName(QStringLiteral("openwith"));
    setModal(true);
    QString text = _text;
    if (text.isEmpty() && !_urls.isEmpty()) {
        if (_urls.count() == 1) {
            const QString fileName = KStringHandler::csqueeze(_urls.first().fileName());
            text = i18n("<qt>Select the program you want to use to open the file<br/>%1</qt>", fileName.toHtmlEscaped());
        } else {
            text = i18np("<qt>Select the program you want to use to open the file.</qt>",
                         "<qt>Select the program you want to use to open the %1 files.</qt>", _urls.count());
        }
    }
    setWindowTitle(i18n("Choose Application"));
    if (mimeType.isEmpty()) {
        d->setMimeTypeFromUrls(_urls);
    } else {
        d->setMimeType(mimeType);
    }
    d->init(text, _value);
}

KOpenWithDialog::KOpenWithDialog(const QString &mimeType, const QString &value,
                                 QWidget *parent)
    : QDialog(parent), d(new KOpenWithDialogPrivate(this))
{
    setObjectName(QStringLiteral("openwith"));
    setModal(true);
    setWindowTitle(i18n("Choose Application for %1", mimeType));
    QString text = i18n("<qt>Select the program for the file type: <b>%1</b>. "
                        "If the program is not listed, enter the name or click "
                        "the browse button.</qt>", mimeType);
    d->setMimeType(mimeType);
    d->init(text, value);
}

KOpenWithDialog::KOpenWithDialog(QWidget *parent)
    : QDialog(parent), d(new KOpenWithDialogPrivate(this))
{
    setObjectName(QStringLiteral("openwith"));
    setModal(true);
    setWindowTitle(i18n("Choose Application"));
    QString text = i18n("<qt>Select a program. "
                        "If the program is not listed, enter the name or click "
                        "the browse button.</qt>");
    d->qMimeType.clear();
    d->init(text, QString());
}

void KOpenWithDialogPrivate::setMimeTypeFromUrls(const QList<QUrl> &_urls)
{
    if (_urls.count() == 1) {
        QMimeDatabase db;
        QMimeType mime = db.mimeTypeForUrl(_urls.first());
        qMimeType = mime.name();
        if (mime.isDefault()) {
            qMimeType.clear();
        } else {
            qMimeTypeComment = mime.comment();
        }
    } else {
        qMimeType.clear();
    }
}

void KOpenWithDialogPrivate::setMimeType(const QString &mimeType)
{
    qMimeType = mimeType;
    QMimeDatabase db;
    qMimeTypeComment = db.mimeTypeForName(mimeType).comment();
}

void KOpenWithDialogPrivate::init(const QString &_text, const QString &_value)
{
    bool bReadOnly = !KAuthorized::authorize(QStringLiteral("shell_access"));
    m_terminaldirty = false;
    view = nullptr;
    m_pService = nullptr;
    curService = nullptr;

    QBoxLayout *topLayout = new QVBoxLayout(q);
    label = new QLabel(_text, q);
    label->setWordWrap(true);
    topLayout->addWidget(label);

    if (!bReadOnly) {
        // init the history combo and insert it into the URL-Requester
        KHistoryComboBox *combo = new KHistoryComboBox();
        combo->setToolTip(i18n("Type to filter the applications below, or specify the name of a command.\nPress down arrow to navigate the results."));
        KLineEdit *lineEdit = new KLineEdit(q);
        lineEdit->setClearButtonEnabled(true);
        combo->setLineEdit(lineEdit);
        combo->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
        combo->setDuplicatesEnabled(false);
        KConfigGroup cg(KSharedConfig::openConfig(), QStringLiteral("Open-with settings"));
        int max = cg.readEntry("Maximum history", 15);
        combo->setMaxCount(max);
        int mode = cg.readEntry("CompletionMode", int(KCompletion::CompletionNone));
        combo->setCompletionMode(static_cast<KCompletion::CompletionMode>(mode));
        const QStringList list = cg.readEntry("History", QStringList());
        combo->setHistoryItems(list, true);
        edit = new KUrlRequester(combo, q);
        edit->installEventFilter(q);
    } else {
        edit = new KUrlRequester(q);
        edit->lineEdit()->setReadOnly(true);
        edit->button()->hide();
    }

    edit->setText(_value);
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

    if (edit->comboBox()) {
        KUrlCompletion *comp = new KUrlCompletion(KUrlCompletion::ExeCompletion);
        edit->comboBox()->setCompletionObject(comp);
        edit->comboBox()->setAutoDeleteCompletionObject(true);
    }

    QObject::connect(edit, &KUrlRequester::textChanged, q, &KOpenWithDialog::slotTextChanged);
    QObject::connect(edit, &KUrlRequester::urlSelected, q, [this]() { _k_slotFileSelected(); });

    view = new KApplicationView(q);
    QTreeViewProxyFilter *proxyModel = new QTreeViewProxyFilter(view);
    KApplicationModel *appModel = new KApplicationModel(proxyModel);
    proxyModel->setSourceModel(appModel);
    proxyModel->setFilterKeyColumn(0);
    proxyModel->setFilterCaseSensitivity(Qt::CaseInsensitive);
    proxyModel->setRecursiveFilteringEnabled(true);
    view->setModels(appModel, proxyModel);
    topLayout->addWidget(view);
    topLayout->setStretchFactor(view, 1);

    QObject::connect(view, &KApplicationView::selected, q, &KOpenWithDialog::slotSelected);
    QObject::connect(view, &KApplicationView::highlighted, q, &KOpenWithDialog::slotHighlighted);
    QObject::connect(view, &KApplicationView::doubleClicked, q, [this]() { _k_slotDbClick(); });

    if (!qMimeType.isNull()) {
        remember = new QCheckBox(i18n("&Remember application association for all files of type\n\"%1\" (%2)", qMimeTypeComment, qMimeType));
        //    remember->setChecked(true);
        topLayout->addWidget(remember);
    } else {
        remember = nullptr;
    }

    //Advanced options
    dialogExtension = new KCollapsibleGroupBox(q);
    dialogExtension->setTitle(i18n("Terminal options"));

    QVBoxLayout *dialogExtensionLayout = new QVBoxLayout(dialogExtension);
    dialogExtensionLayout->setContentsMargins(0, 0, 0, 0);

    terminal = new QCheckBox(i18n("Run in &terminal"), q);
    if (bReadOnly) {
        terminal->hide();
    }
    QObject::connect(terminal, &QAbstractButton::toggled, q, &KOpenWithDialog::slotTerminalToggled);

    dialogExtensionLayout->addWidget(terminal);

    QStyleOptionButton checkBoxOption;
    checkBoxOption.initFrom(terminal);
    int checkBoxIndentation = terminal->style()->pixelMetric(QStyle::PM_IndicatorWidth, &checkBoxOption, terminal);
    checkBoxIndentation += terminal->style()->pixelMetric(QStyle::PM_CheckBoxLabelSpacing, &checkBoxOption, terminal);

    QBoxLayout *nocloseonexitLayout = new QHBoxLayout();
    nocloseonexitLayout->setContentsMargins(0, 0, 0, 0);
    QSpacerItem *spacer = new QSpacerItem(checkBoxIndentation, 0, QSizePolicy::Fixed, QSizePolicy::Minimum);
    nocloseonexitLayout->addItem(spacer);

    nocloseonexit = new QCheckBox(i18n("&Do not close when command exits"), q);
    nocloseonexit->setChecked(false);
    nocloseonexit->setDisabled(true);

    // check to see if we use konsole if not disable the nocloseonexit
    // because we don't know how to do this on other terminal applications
    KConfigGroup confGroup(KSharedConfig::openConfig(), QStringLiteral("General"));
    QString preferredTerminal = confGroup.readPathEntry("TerminalApplication", QStringLiteral("konsole"));

    if (bReadOnly || preferredTerminal != QLatin1String("konsole")) {
        nocloseonexit->hide();
    }

    nocloseonexitLayout->addWidget(nocloseonexit);
    dialogExtensionLayout->addLayout(nocloseonexitLayout);

    topLayout->addWidget(dialogExtension);

    buttonBox = new QDialogButtonBox(q);
    buttonBox->setStandardButtons(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    q->connect(buttonBox, &QDialogButtonBox::accepted, q, &QDialog::accept);
    q->connect(buttonBox, &QDialogButtonBox::rejected, q, &QDialog::reject);
    topLayout->addWidget(buttonBox);

    q->setMinimumSize(q->minimumSizeHint());
    //edit->setText( _value );
    // The resize is what caused "can't click on items before clicking on Name header" in previous versions.
    // Probably due to the resizeEvent handler using width().
    q->resize( q->minimumWidth(), 0.6 * q->screen()->availableGeometry().height());
    edit->setFocus();
    q->slotTextChanged();
}

// ----------------------------------------------------------------------

KOpenWithDialog::~KOpenWithDialog()
{
    delete d;
}

// ----------------------------------------------------------------------

void KOpenWithDialog::slotSelected(const QString & /*_name*/, const QString &_exec)
{
    d->buttonBox->button(QDialogButtonBox::Ok)->setEnabled(!_exec.isEmpty());
}

// ----------------------------------------------------------------------

void KOpenWithDialog::slotHighlighted(const QString &entryPath, const QString &)
{
    d->curService = KService::serviceByDesktopPath(entryPath);
    if (d->curService && !d->m_terminaldirty) {
        // ### indicate that default value was restored
        d->terminal->setChecked(d->curService->terminal());
        QString terminalOptions = d->curService->terminalOptions();
        d->nocloseonexit->setChecked((terminalOptions.contains(QLatin1String("--noclose"))));
        d->m_terminaldirty = false; // slotTerminalToggled changed it
    }
}

// ----------------------------------------------------------------------

void KOpenWithDialog::slotTextChanged()
{
    // Forget about the service only when the selection is empty
    // otherwise changing text but hitting the same result clears curService
    bool selectionEmpty = !d->view->currentIndex().isValid();
    if (d->curService && selectionEmpty) {
        d->curService = nullptr;
    }
    d->buttonBox->button(QDialogButtonBox::Ok)->setEnabled(!d->edit->text().isEmpty() || d->curService);

    //Update the filter regexp with the new text in the lineedit
    d->view->proxyModel()->setFilterFixedString(d->edit->text());

    //Expand all the nodes when the search string is 3 characters long
    //If the search string doesn't match anything there will be no nodes to expand
    if (d->edit->text().size() > 2) {
        d->view->expandAll();
        QAbstractItemModel *model = d->view->model();
        if (model->rowCount() == 1) { // Automatically select the result (first leaf node) if the
                                      // filter has only one match
            QModelIndex leafNodeIdx = model->index(0, 0);
            while (model->hasChildren(leafNodeIdx)) {
                leafNodeIdx = model->index(0, 0, leafNodeIdx);
            }
            d->view->setCurrentIndex(leafNodeIdx);
        }
    } else {
        d->view->collapseAll();
        d->view->setCurrentIndex(d->view->rootIndex()); // Unset and deselect all the elements
        d->curService = nullptr;
    }
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

static QString simplifiedExecLineFromService(const QString &fullExec)
{
    QString exec = fullExec;
    exec.remove(QStringLiteral("%u"), Qt::CaseInsensitive);
    exec.remove(QStringLiteral("%f"), Qt::CaseInsensitive);
    exec.remove(QStringLiteral("-caption %c"));
    exec.remove(QStringLiteral("-caption \"%c\""));
    exec.remove(QStringLiteral("%i"));
    exec.remove(QStringLiteral("%m"));
    return exec.simplified();
}

void KOpenWithDialogPrivate::addToMimeAppsList(const QString &serviceId /*menu id or storage id*/)
{
    KSharedConfig::Ptr profile = KSharedConfig::openConfig(QStringLiteral("mimeapps.list"), KConfig::NoGlobals, QStandardPaths::GenericConfigLocation);

    // Save the default application according to mime-apps-spec 1.0
    KConfigGroup defaultApp(profile, "Default Applications");
    defaultApp.writeXdgListEntry(qMimeType, QStringList(serviceId));

    KConfigGroup addedApps(profile, "Added Associations");
    QStringList apps = addedApps.readXdgListEntry(qMimeType);
    apps.removeAll(serviceId);
    apps.prepend(serviceId); // make it the preferred app
    addedApps.writeXdgListEntry(qMimeType, apps);

    profile->sync();

    // Also make sure the "auto embed" setting for this MIME type is off
    KSharedConfig::Ptr fileTypesConfig = KSharedConfig::openConfig(QStringLiteral("filetypesrc"), KConfig::NoGlobals);
    fileTypesConfig->group("EmbedSettings").writeEntry(QStringLiteral("embed-") + qMimeType, false);
    fileTypesConfig->sync();

    // qDebug() << "rebuilding ksycoca...";

    // kbuildsycoca is the one reading mimeapps.list, so we need to run it now
    KBuildSycocaProgressDialog::rebuildKSycoca(q);

    // could be nullptr if the user canceled the dialog...
    m_pService = KService::serviceByStorageId(serviceId);
}

bool KOpenWithDialogPrivate::checkAccept()
{
    const QString typedExec(edit->text());
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
            KService::Ptr serv = KService::serviceByDesktopName(serviceName);
            ok = !serv; // ok if no such service yet
            // also ok if we find the exact same service (well, "kwrite" == "kwrite %U")
            if (serv && !serv->noDisplay() /* #297720 */) {
                if (serv->isApplication()) {
                    /*// qDebug() << "typedExec=" << typedExec
                      << "serv->exec=" << serv->exec()
                      << "simplifiedExecLineFromService=" << simplifiedExecLineFromService(fullExec);*/
                    serviceExec = simplifiedExecLineFromService(serv->exec());
                    if (typedExec == serviceExec) {
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
                serviceName = initialServiceName + QLatin1Char('-') + QString::number(i);
            }
        } while (!ok);
    }
    if (m_pService) {
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
        KConfigGroup confGroup(KSharedConfig::openConfig(), QStringLiteral("General"));
        preferredTerminal = confGroup.readPathEntry("TerminalApplication", QStringLiteral("konsole"));
        m_command = preferredTerminal;
        // only add --noclose when we are sure it is konsole we're using
        if (preferredTerminal == QLatin1String("konsole") && nocloseonexit->isChecked()) {
            m_command += QStringLiteral(" --noclose");
        }
        m_command += QLatin1String(" -e ") + edit->text();
        // qDebug() << "Setting m_command to" << m_command;
    }
    if (m_pService && terminal->isChecked() != m_pService->terminal()) {
        m_pService = nullptr;    // It's not exactly this service we're running
    }

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
            if (configPath.isEmpty()) {
                m_pService = new KService(initialServiceName, fullExec, QString());
            } else {
                if (!typedExec.contains(QLatin1String("%u"), Qt::CaseInsensitive) &&
                        !typedExec.contains(QLatin1String("%f"), Qt::CaseInsensitive)) {
                    int index = serviceExec.indexOf(QLatin1String("%u"), 0, Qt::CaseInsensitive);
                    if (index == -1) {
                        index = serviceExec.indexOf(QLatin1String("%f"), 0, Qt::CaseInsensitive);
                    }
                    if (index > -1) {
                        fullExec += QLatin1Char(' ') + serviceExec.midRef(index, 2);
                    }
                }
                // qDebug() << "Creating service with Exec=" << fullExec;
                m_pService = new KService(configPath);
                m_pService->setExec(fullExec);
            }
            if (terminal->isChecked()) {
                m_pService->setTerminal(true);
                // only add --noclose when we are sure it is konsole we're using
                if (preferredTerminal == QLatin1String("konsole") && nocloseonexit->isChecked()) {
                    m_pService->setTerminalOptions(QStringLiteral("--noclose"));
                }
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
                if (preferredTerminal == QLatin1String("konsole") && nocloseonexit->isChecked()) {
                    cg.writeEntry("TerminalOptions", "--noclose");
                }
            }
            if (!qMimeType.isEmpty()) {
                cg.writeXdgListEntry("MimeType", QStringList() << qMimeType);
            }
            cg.sync();

            if (!qMimeType.isEmpty()) {
                addToMimeAppsList(menuId);
            }
            m_pService = new KService(newPath);
        }
    }

    saveComboboxHistory();
    return true;
}

bool KOpenWithDialog::eventFilter(QObject *object, QEvent *event)
{
    // Detect DownArrow to navigate the results in the QTreeView
    if (object == d->edit && event->type() == QEvent::ShortcutOverride) {
        QKeyEvent *keyEvent = static_cast<QKeyEvent *>(event);
        if (keyEvent->key() == Qt::Key_Down) {
            KHistoryComboBox *combo = static_cast<KHistoryComboBox *>(d->edit->comboBox());
            // FIXME: Disable arrow down in CompletionPopup and CompletionPopupAuto only when the dropdown list is shown.
            // When popup completion mode is used the down arrow is used to navigate the dropdown list of results
            if (combo->completionMode() != KCompletion::CompletionPopup && combo->completionMode() != KCompletion::CompletionPopupAuto) {
                QModelIndex leafNodeIdx = d->view->model()->index(0, 0);
                // Check if we have at least one result or the focus is passed to the empty QTreeView
                if (d->view->model()->hasChildren(leafNodeIdx)) {
                    d->view->setFocus(Qt::OtherFocusReason);
                    QApplication::sendEvent(d->view, keyEvent);
                    return true;
                }
            }
        }
    }
    return QDialog::eventFilter(object, event);
}

void KOpenWithDialog::accept()
{
    if (d->checkAccept()) {
        QDialog::accept();
    }
}

QString KOpenWithDialog::text() const
{
    if (!d->m_command.isEmpty()) {
        return d->m_command;
    } else {
        return d->edit->text();
    }
}

void KOpenWithDialog::hideNoCloseOnExit()
{
    // uncheck the checkbox because the value could be used when "Run in Terminal" is selected
    d->nocloseonexit->setChecked(false);
    d->nocloseonexit->hide();

    d->dialogExtension->setVisible(d->nocloseonexit->isVisible() || d->terminal->isVisible());
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
    KHistoryComboBox *combo = static_cast<KHistoryComboBox *>(edit->comboBox());
    if (combo) {
        combo->addToHistory(edit->text());

        KConfigGroup cg(KSharedConfig::openConfig(), QStringLiteral("Open-with settings"));
        cg.writeEntry("History", combo->historyItems());
        writeEntry(cg, "CompletionMode", combo->completionMode());
        // don't store the completion-list, as it contains all of KUrlCompletion's
        // executables
        cg.sync();
    }
}

#include "moc_kopenwithdialog.cpp"
#include "moc_kopenwithdialog_p.cpp"
