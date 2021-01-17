/*
    This file is part of the KDE libraries
    SPDX-FileCopyrightText: 1999, 2000, 2001 Carsten Pfeiffer <pfeiffer@kde.org>
    SPDX-FileCopyrightText: 2013 Teo Mrnjavac <teo@kde.org>

    SPDX-License-Identifier: LGPL-2.0-only
*/

#include "kurlrequester.h"
#include "kio_widgets_debug.h"
#include "../pathhelpers_p.h" // concatPaths(), isAbsoluteLocalPath()

#include <KComboBox>
#include <KDragWidgetDecorator>
#include <KLineEdit>
#include <KLocalizedString>
#include <kprotocolmanager.h>
#include <kurlcompletion.h>

#include <QAction>
#include <QApplication>
#include <QDrag>
#include <QEvent>
#include <QKeySequence>
#include <QHBoxLayout>
#include <QMimeData>
#include <QMenu>

class KUrlDragPushButton : public QPushButton
{
    Q_OBJECT
public:
    explicit KUrlDragPushButton(QWidget *parent)
        : QPushButton(parent)
    {
        new DragDecorator(this);
    }
    ~KUrlDragPushButton() override {}

    void setURL(const QUrl &url)
    {
        m_urls.clear();
        m_urls.append(url);
    }

private:
    class DragDecorator : public KDragWidgetDecoratorBase
    {
    public:
        explicit DragDecorator(KUrlDragPushButton *button)
            : KDragWidgetDecoratorBase(button), m_button(button) {}

    protected:
        QDrag *dragObject() override
        {
            if (m_button->m_urls.isEmpty()) {
                return nullptr;
            }

            QDrag *drag = new QDrag(m_button);
            QMimeData *mimeData = new QMimeData;
            mimeData->setUrls(m_button->m_urls);
            drag->setMimeData(mimeData);
            return drag;
        }

    private:
        KUrlDragPushButton *m_button;
    };

    QList<QUrl> m_urls;
};

class Q_DECL_HIDDEN KUrlRequester::KUrlRequesterPrivate
{
public:
    explicit KUrlRequesterPrivate(KUrlRequester *parent)
        : m_fileDialogModeWasDirAndFile(false),
          m_parent(parent),
          edit(nullptr),
          combo(nullptr),
          fileDialogMode(KFile::File | KFile::ExistingOnly | KFile::LocalOnly),
          fileDialogAcceptMode(QFileDialog::AcceptOpen)
    {
    }

    ~KUrlRequesterPrivate()
    {
        delete myCompletion;
        delete myFileDialog;
    }

    void init();

    void setText(const QString &text)
    {
        if (combo) {
            if (combo->isEditable()) {
                combo->setEditText(text);
            } else {
                int i = combo->findText(text);
                if (i == -1) {
                    combo->addItem(text);
                    combo->setCurrentIndex(combo->count() - 1);
                } else {
                    combo->setCurrentIndex(i);
                }
            }
        } else {
            edit->setText(text);
        }
    }

    void connectSignals(KUrlRequester *receiver)
    {
        if (combo) {
            connect(combo, &QComboBox::currentTextChanged,
                    receiver, &KUrlRequester::textChanged);
            connect(combo, &QComboBox::editTextChanged,
                    receiver, &KUrlRequester::textEdited);

            connect(combo, QOverload<>::of(&KComboBox::returnPressed),
                    receiver, QOverload<>::of(&KUrlRequester::returnPressed));
            connect(combo, QOverload<const QString&>::of(&KComboBox::returnPressed),
                    receiver, QOverload<const QString&>::of(&KUrlRequester::returnPressed));
        } else if (edit) {
            connect(edit, &QLineEdit::textChanged,
                    receiver, &KUrlRequester::textChanged);
            connect(edit, &QLineEdit::textEdited,
                    receiver, &KUrlRequester::textEdited);

            connect(edit, QOverload<>::of(&QLineEdit::returnPressed),
                    receiver, QOverload<>::of(&KUrlRequester::returnPressed));

            if (auto kline = qobject_cast<KLineEdit*>(edit)) {
                connect(kline, QOverload<const QString&>::of(&KLineEdit::returnPressed),
                        receiver, QOverload<const QString&>::of(&KUrlRequester::returnPressed));
            }
        }
    }

    void setCompletionObject(KCompletion *comp)
    {
        if (combo) {
            combo->setCompletionObject(comp);
        } else {
            edit->setCompletionObject(comp);
        }
    }

    void updateCompletionStartDir(const QUrl &newStartDir)
    {
        myCompletion->setDir(newStartDir);
    }


    QString text() const
    {
        return combo ? combo->currentText() : edit->text();
    }

    /**
     * replaces ~user or $FOO, if necessary
     * if text() is a relative path, make it absolute using startDir()
     */
    QUrl url() const
    {
        const QString txt = text();
        KUrlCompletion *comp;
        if (combo) {
            comp = qobject_cast<KUrlCompletion *>(combo->completionObject());
        } else {
            comp = qobject_cast<KUrlCompletion *>(edit->completionObject());
        }

        QString enteredPath;
        if (comp)
            enteredPath = comp->replacedPath(txt);
        else
            enteredPath = txt;

        if (isAbsoluteLocalPath(enteredPath)) {
            return QUrl::fromLocalFile(enteredPath);
        }

        const QUrl enteredUrl = QUrl(enteredPath); // absolute or relative
        if (enteredUrl.isRelative() && !txt.isEmpty()) {
            QUrl finalUrl(m_startDir);
            finalUrl.setPath(concatPaths(finalUrl.path(), enteredPath));
            return finalUrl;
        } else {
            return enteredUrl;
        }
    }

    static void applyFileMode(QFileDialog *dlg, KFile::Modes m, QFileDialog::AcceptMode acceptMode)
    {
        QFileDialog::FileMode fileMode;
        bool dirsOnly = false;
        if (m & KFile::Directory) {
            fileMode = QFileDialog::Directory;
            if ((m & KFile::File) == 0 &&
                    (m & KFile::Files) == 0) {
                dirsOnly = true;
            }
        } else if (m & KFile::Files &&
                   m & KFile::ExistingOnly) {
            fileMode = QFileDialog::ExistingFiles;
        } else if (m & KFile::File &&
                   m & KFile::ExistingOnly) {
            fileMode = QFileDialog::ExistingFile;
        } else {
            fileMode = QFileDialog::AnyFile;
        }

        dlg->setFileMode(fileMode);
        dlg->setAcceptMode(acceptMode);
        dlg->setOption(QFileDialog::ShowDirsOnly, dirsOnly);
    }

    // Converts from "*.foo *.bar|Comment" to "Comment (*.foo *.bar)"
    QStringList kToQFilters(const QString &filters) const
    {
        QStringList qFilters = filters.split(QLatin1Char('\n'), Qt::SkipEmptyParts);

        for (QString &qFilter : qFilters) {
            int sep = qFilter.indexOf(QLatin1Char('|'));
            const QStringRef globs = qFilter.leftRef(sep);
            const QStringRef desc  = qFilter.midRef(sep + 1);
            qFilter = desc + QLatin1String(" (") + globs + QLatin1Char(')');
        }

        return qFilters;
    }

    QUrl getDirFromFileDialog(const QUrl &openUrl) const
    {
        return QFileDialog::getExistingDirectoryUrl(m_parent, QString(), openUrl, QFileDialog::ShowDirsOnly);
    }

    void createFileDialog()
    {
        //Creates the fileDialog if it doesn't exist yet
        QFileDialog *dlg = m_parent->fileDialog();

        if (!url().isEmpty() && !url().isRelative()) {
            QUrl u(url());
            // If we won't be able to list it (e.g. http), then don't try :)
            if (KProtocolManager::supportsListing(u)) {
                dlg->selectUrl(u);
            }
        } else {
            dlg->setDirectoryUrl(m_startDir);
        }

        dlg->setAcceptMode(fileDialogAcceptMode);

        //Update the file dialog window modality
        if (dlg->windowModality() != fileDialogModality) {
            dlg->setWindowModality(fileDialogModality);
        }

        if (fileDialogModality == Qt::NonModal) {
            dlg->show();
        } else {
            dlg->exec();
        }
    }

    // slots
    void _k_slotUpdateUrl();
    void _k_slotOpenDialog();
    void _k_slotFileDialogAccepted();

    QUrl m_startDir;
    bool m_startDirCustomized;
    bool m_fileDialogModeWasDirAndFile;
    KUrlRequester * const m_parent; // TODO: rename to 'q'
    KLineEdit *edit;
    KComboBox *combo;
    KFile::Modes fileDialogMode;
    QFileDialog::AcceptMode fileDialogAcceptMode;
    QString fileDialogFilter;
    QStringList mimeTypeFilters;
    KEditListWidget::CustomEditor editor;
    KUrlDragPushButton *myButton;
    QFileDialog *myFileDialog;
    KUrlCompletion *myCompletion;
    Qt::WindowModality fileDialogModality;
};

KUrlRequester::KUrlRequester(QWidget *editWidget, QWidget *parent)
    : QWidget(parent), d(new KUrlRequesterPrivate(this))
{
    // must have this as parent
    editWidget->setParent(this);
    d->combo = qobject_cast<KComboBox *>(editWidget);
    d->edit = qobject_cast<KLineEdit *>(editWidget);
    if (d->edit) {
        d->edit->setClearButtonEnabled(true);
    }

    d->init();
}

KUrlRequester::KUrlRequester(QWidget *parent)
    : QWidget(parent), d(new KUrlRequesterPrivate(this))
{
    d->init();
}

KUrlRequester::KUrlRequester(const QUrl &url, QWidget *parent)
    : QWidget(parent), d(new KUrlRequesterPrivate(this))
{
    d->init();
    setUrl(url);
}

KUrlRequester::~KUrlRequester()
{
    delete d;
}

void KUrlRequester::KUrlRequesterPrivate::init()
{
    myFileDialog = nullptr;
    fileDialogModality = Qt::ApplicationModal;

    if (!combo && !edit) {
        edit = new KLineEdit(m_parent);
        edit->setClearButtonEnabled(true);
    }

    QWidget *widget = combo ? static_cast<QWidget *>(combo) : static_cast<QWidget *>(edit);

    QHBoxLayout *topLayout = new QHBoxLayout(m_parent);
    topLayout->setContentsMargins(0, 0, 0, 0);
    topLayout->setSpacing(-1); // use default spacing
    topLayout->addWidget(widget);

    myButton = new KUrlDragPushButton(m_parent);
    myButton->setIcon(QIcon::fromTheme(QStringLiteral("document-open")));
    int buttonSize = myButton->sizeHint().expandedTo(widget->sizeHint()).height();
    myButton->setFixedSize(buttonSize, buttonSize);
    myButton->setToolTip(i18n("Open file dialog"));

    connect(myButton, &KUrlDragPushButton::pressed, m_parent, [this]() { _k_slotUpdateUrl(); });

    widget->installEventFilter(m_parent);
    m_parent->setFocusProxy(widget);
    m_parent->setFocusPolicy(Qt::StrongFocus);
    topLayout->addWidget(myButton);

    connectSignals(m_parent);
    connect(myButton, &KUrlDragPushButton::clicked, m_parent, [this]() { _k_slotOpenDialog(); });

    m_startDir = QUrl::fromLocalFile(QDir::currentPath());
    m_startDirCustomized = false;

    myCompletion = new KUrlCompletion();
    updateCompletionStartDir(m_startDir);

    setCompletionObject(myCompletion);

    QAction *openAction = new QAction(m_parent);
    openAction->setShortcut(QKeySequence::Open);
    m_parent->connect(openAction, &QAction::triggered, m_parent, [this]() { _k_slotOpenDialog(); });
}

void KUrlRequester::setUrl(const QUrl &url)
{
    d->setText(url.toDisplayString(QUrl::PreferLocalFile));
}

#if KIOWIDGETS_BUILD_DEPRECATED_SINCE(4, 3)
void KUrlRequester::setPath(const QString &path)
{
    d->setText(path);
}
#endif

void KUrlRequester::setText(const QString &text)
{
    d->setText(text);
}

void KUrlRequester::setStartDir(const QUrl &startDir)
{
    d->m_startDir = startDir;
    d->m_startDirCustomized = true;
    d->updateCompletionStartDir(startDir);
}

void KUrlRequester::changeEvent(QEvent *e)
{
    if (e->type() == QEvent::WindowTitleChange) {
        if (d->myFileDialog) {
            d->myFileDialog->setWindowTitle(windowTitle());
        }
    }
    QWidget::changeEvent(e);
}

QUrl KUrlRequester::url() const
{
    return d->url();
}

QUrl KUrlRequester::startDir() const
{
    return d->m_startDir;
}

QString KUrlRequester::text() const
{
    return d->text();
}

void KUrlRequester::KUrlRequesterPrivate::_k_slotOpenDialog()
{
    if (myFileDialog)
        if (myFileDialog->isVisible()) {
            //The file dialog is already being shown, raise it and exit
            myFileDialog->raise();
            myFileDialog->activateWindow();
            return;
        }

    if (!m_fileDialogModeWasDirAndFile &&
            (((fileDialogMode & KFile::Directory) && !(fileDialogMode & KFile::File)) ||
             /* catch possible fileDialog()->setMode( KFile::Directory ) changes */
             (myFileDialog && (myFileDialog->fileMode() == QFileDialog::Directory &&
                               myFileDialog->testOption(QFileDialog::ShowDirsOnly))))) {
        const QUrl openUrl = (!m_parent->url().isEmpty() && !m_parent->url().isRelative())
                             ? m_parent->url() : m_startDir;

        /* FIXME We need a new abstract interface for using KDirSelectDialog in a non-modal way */

        QUrl newUrl;
        if (fileDialogMode & KFile::LocalOnly) {
            newUrl = QFileDialog::getExistingDirectoryUrl(m_parent, QString(), openUrl, QFileDialog::ShowDirsOnly, QStringList() << QStringLiteral("file"));
        } else {
            newUrl = getDirFromFileDialog(openUrl);
        }

        if (newUrl.isValid()) {
            m_parent->setUrl(newUrl);
            Q_EMIT m_parent->urlSelected(url());
        }
    } else {
        Q_EMIT m_parent->openFileDialog(m_parent);

        if (((fileDialogMode & KFile::Directory) && (fileDialogMode & KFile::File)) || m_fileDialogModeWasDirAndFile) {
            QMenu *dirOrFileMenu = new QMenu();
            QAction *fileAction = new QAction(QIcon::fromTheme(QStringLiteral("document-new")), i18n("File"));
            QAction *dirAction = new QAction(QIcon::fromTheme(QStringLiteral("folder-new")), i18n("Directory"));
            dirOrFileMenu->addAction(fileAction);
            dirOrFileMenu->addAction(dirAction);

            connect(fileAction, &QAction::triggered, [this]() {
                fileDialogMode = KFile::File;
                applyFileMode(m_parent->fileDialog(), fileDialogMode, fileDialogAcceptMode);
                m_fileDialogModeWasDirAndFile = true;
                createFileDialog();
            });

            connect(dirAction, &QAction::triggered, [this]() {
                fileDialogMode = KFile::Directory;
                applyFileMode(m_parent->fileDialog(), fileDialogMode, fileDialogAcceptMode);
                m_fileDialogModeWasDirAndFile = true;
                createFileDialog();
            });

            dirOrFileMenu->exec(m_parent->mapToGlobal(QPoint(m_parent->width(), m_parent->height())));

            return;
        }

        createFileDialog();
    }
}

void KUrlRequester::KUrlRequesterPrivate::_k_slotFileDialogAccepted()
{
    if (!myFileDialog) {
        return;
    }

    const QUrl newUrl = myFileDialog->selectedUrls().constFirst();
    if (newUrl.isValid()) {
        m_parent->setUrl(newUrl);
        Q_EMIT m_parent->urlSelected(url());
        // remember url as defaultStartDir and update startdir for autocompletion
        if (newUrl.isLocalFile() && !m_startDirCustomized) {
            m_startDir = newUrl.adjusted(QUrl::RemoveFilename);
            updateCompletionStartDir(m_startDir);
        }
    }
}

void KUrlRequester::setMode(KFile::Modes mode)
{
    Q_ASSERT((mode & KFile::Files) == 0);
    d->fileDialogMode = mode;
    if ((mode & KFile::Directory) && !(mode & KFile::File)) {
        d->myCompletion->setMode(KUrlCompletion::DirCompletion);
    }

    if (d->myFileDialog) {
        d->applyFileMode(d->myFileDialog, mode, d->fileDialogAcceptMode);
    }
}

KFile::Modes KUrlRequester::mode() const
{
    return d->fileDialogMode;
}

void KUrlRequester::setAcceptMode(QFileDialog::AcceptMode mode)
{
    d->fileDialogAcceptMode = mode;

    if (d->myFileDialog) {
        d->applyFileMode(d->myFileDialog, d->fileDialogMode, mode);
    }
}

QFileDialog::AcceptMode KUrlRequester::acceptMode() const
{
    return d->fileDialogAcceptMode;
}

void KUrlRequester::setFilter(const QString &filter)
{
    d->fileDialogFilter = filter;

    if (d->myFileDialog) {
        d->myFileDialog->setNameFilters(d->kToQFilters(d->fileDialogFilter));
    }
}

QString KUrlRequester::filter() const
{
    return d->fileDialogFilter;
}

void KUrlRequester::setMimeTypeFilters(const QStringList &mimeTypes)
{
    d->mimeTypeFilters = mimeTypes;

    if (d->myFileDialog) {
        d->myFileDialog->setMimeTypeFilters(d->mimeTypeFilters);
    }
    d->myCompletion->setMimeTypeFilters(d->mimeTypeFilters);
}

QStringList KUrlRequester::mimeTypeFilters() const
{
    return d->mimeTypeFilters;
}

QFileDialog *KUrlRequester::fileDialog() const
{
    if (d->myFileDialog &&
            (   (d->myFileDialog->fileMode() == QFileDialog::Directory && !(d->fileDialogMode & KFile::Directory))
             || (d->myFileDialog->fileMode() != QFileDialog::Directory &&  (d->fileDialogMode & KFile::Directory)))) {
        delete d->myFileDialog;
        d->myFileDialog = nullptr;
    }

    if (!d->myFileDialog) {
        d->myFileDialog = new QFileDialog(window(), windowTitle());
        if (!d->mimeTypeFilters.isEmpty()) {
            d->myFileDialog->setMimeTypeFilters(d->mimeTypeFilters);
        } else {
            d->myFileDialog->setNameFilters(d->kToQFilters(d->fileDialogFilter));
        }

        d->applyFileMode(d->myFileDialog, d->fileDialogMode, d->fileDialogAcceptMode);

        d->myFileDialog->setWindowModality(d->fileDialogModality);
        connect(d->myFileDialog, &QFileDialog::accepted, this, [this]() { d->_k_slotFileDialogAccepted(); });
    }

    return d->myFileDialog;
}

void KUrlRequester::clear()
{
    d->setText(QString());
}

KLineEdit *KUrlRequester::lineEdit() const
{
    return d->edit;
}

KComboBox *KUrlRequester::comboBox() const
{
    return d->combo;
}

void KUrlRequester::KUrlRequesterPrivate::_k_slotUpdateUrl()
{
    const QUrl visibleUrl = url();
    QUrl u = visibleUrl;
    if (visibleUrl.isRelative()) {
        u = QUrl::fromLocalFile(QDir::currentPath() + QLatin1Char('/')).resolved(visibleUrl);
    }
    myButton->setURL(u);
}

bool KUrlRequester::eventFilter(QObject *obj, QEvent *ev)
{
    if ((d->edit == obj) || (d->combo == obj)) {
        if ((ev->type() == QEvent::FocusIn) || (ev->type() == QEvent::FocusOut))
            // Forward focusin/focusout events to the urlrequester; needed by file form element in khtml
        {
            QApplication::sendEvent(this, ev);
        }
    }
    return QWidget::eventFilter(obj, ev);
}

QPushButton *KUrlRequester::button() const
{
    return d->myButton;
}

KUrlCompletion *KUrlRequester::completionObject() const
{
    return d->myCompletion;
}

#if KIOWIDGETS_BUILD_DEPRECATED_SINCE(5, 0)
void KUrlRequester::setClickMessage(const QString &msg)
{
    setPlaceholderText(msg);
}
#endif

void KUrlRequester::setPlaceholderText(const QString &msg)
{
    if (d->edit) {
        d->edit->setPlaceholderText(msg);
    }
}

#if KIOWIDGETS_BUILD_DEPRECATED_SINCE(5, 0)
QString KUrlRequester::clickMessage() const
{
    return placeholderText();
}
#endif

QString KUrlRequester::placeholderText() const
{
    if (d->edit) {
        return d->edit->placeholderText();
    } else {
        return QString();
    }
}

Qt::WindowModality KUrlRequester::fileDialogModality() const
{
    return d->fileDialogModality;
}

void KUrlRequester::setFileDialogModality(Qt::WindowModality modality)
{
    d->fileDialogModality = modality;
}

const KEditListWidget::CustomEditor &KUrlRequester::customEditor()
{
    setSizePolicy(QSizePolicy(QSizePolicy::Preferred,
                              QSizePolicy::Fixed));

    KLineEdit *edit = d->edit;
    if (!edit && d->combo) {
        edit = qobject_cast<KLineEdit *>(d->combo->lineEdit());
    }

#ifndef NDEBUG
    if (!edit) {
        qCWarning(KIO_WIDGETS) << "KUrlRequester's lineedit is not a KLineEdit!??\n";
    }
#endif

    d->editor.setRepresentationWidget(this);
    d->editor.setLineEdit(edit);
    return d->editor;
}

KUrlComboRequester::KUrlComboRequester(QWidget *parent)
    : KUrlRequester(new KComboBox(false), parent), d(nullptr)
{
}

#include "moc_kurlrequester.cpp"
#include "kurlrequester.moc"
