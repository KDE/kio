/* This file is part of the KDE libraries
    Copyright (C) 1999,2000,2001 Carsten Pfeiffer <pfeiffer@kde.org>
    Copyright (C) 2013           Teo Mrnjavac <teo@kde.org>

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License version 2, as published by the Free Software Foundation.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.
*/

#include "kurlrequester.h"
#include "kio_widgets_debug.h"

#include <kcombobox.h>
#include <kdragwidgetdecorator.h>
#include <klineedit.h>
#include <klocalizedstring.h>
#include <kprotocolmanager.h>
#include <kurlcompletion.h>

#include <QAction>
#include <QApplication>
#include <QDebug>
#include <QDrag>
#include <QEvent>
#include <QKeySequence>
#include <QHBoxLayout>
#include <QMimeData>
#include <QtGlobal>

class KUrlDragPushButton : public QPushButton
{
    Q_OBJECT
public:
    KUrlDragPushButton(QWidget *parent)
        : QPushButton(parent)
    {
        new DragDecorator(this);
    }
    ~KUrlDragPushButton() {}

    void setURL(const QUrl &url)
    {
        m_urls.clear();
        m_urls.append(url);
    }

private:
    class DragDecorator : public KDragWidgetDecoratorBase
    {
    public:
        DragDecorator(KUrlDragPushButton *button)
            : KDragWidgetDecoratorBase(button), m_button(button) {}

    protected:
        QDrag *dragObject() Q_DECL_OVERRIDE
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

class KUrlRequester::KUrlRequesterPrivate
{
public:
    KUrlRequesterPrivate(KUrlRequester *parent)
        : m_parent(parent),
          edit(nullptr),
          combo(nullptr),
          fileDialogMode(KFile::File | KFile::ExistingOnly | KFile::LocalOnly)
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
        QLineEdit *sender;
        if (combo) {
            sender = combo->lineEdit();
        } else {
            sender = edit;
        }
        if (sender) {
            connect(sender, &QLineEdit::textChanged,
                    receiver, &KUrlRequester::textChanged);
            connect(sender, &QLineEdit::textEdited,
                    receiver, &KUrlRequester::textEdited);

            connect(sender, SIGNAL(returnPressed()),
                    receiver, SIGNAL(returnPressed()));
            connect(sender, SIGNAL(returnPressed(QString)),
                    receiver, SIGNAL(returnPressed(QString)));
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

        if (QDir::isAbsolutePath(enteredPath)) {
            return QUrl::fromLocalFile(enteredPath);
        }

        const QUrl enteredUrl = QUrl(enteredPath); // absolute or relative
        if (enteredUrl.isRelative() && !txt.isEmpty()) {
            QUrl finalUrl(m_startDir);
            finalUrl.setPath(finalUrl.path() + '/' + enteredPath);
            return finalUrl;
        } else {
            return enteredUrl;
        }
    }

    void applyFileMode(QFileDialog *dlg, KFile::Modes m)
    {
        QFileDialog::FileMode fileMode;
        if (m & KFile::Directory) {
            fileMode = QFileDialog::Directory;
            if ((m & KFile::File) == 0 &&
                    (m & KFile::Files) == 0) {
                dlg->setOption(QFileDialog::ShowDirsOnly, true);
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
    }

    // Converts from "*.foo *.bar|Comment" to "Comment (*.foo *.bar)"
    QStringList kToQFilters(const QString &filters) const
    {
        QStringList qFilters = filters.split('\n', QString::SkipEmptyParts);

        for (QStringList::iterator it = qFilters.begin(); it != qFilters.end(); ++it) {
            int sep = it->indexOf('|');
            QString globs = it->left(sep);
            QString desc  = it->mid(sep + 1);
            *it = QStringLiteral("%1 (%2)").arg(desc, globs);
        }

        return qFilters;
    }

    QUrl getDirFromFileDialog(const QUrl &openUrl) const
    {
        return QFileDialog::getExistingDirectoryUrl(m_parent, QString(), openUrl, QFileDialog::ShowDirsOnly);
    }

    // slots
    void _k_slotUpdateUrl();
    void _k_slotOpenDialog();
    void _k_slotFileDialogAccepted();

    QUrl m_startDir;
    bool m_startDirCustomized;
    KUrlRequester *m_parent; // TODO: rename to 'q'
    KLineEdit *edit;
    KComboBox *combo;
    KFile::Modes fileDialogMode;
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
        d->edit->setClearButtonShown(true);
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
        edit->setClearButtonShown(true);
    }

    QWidget *widget = combo ? static_cast<QWidget *>(combo) : static_cast<QWidget *>(edit);

    QHBoxLayout *topLayout = new QHBoxLayout(m_parent);
    topLayout->setMargin(0);
    topLayout->setSpacing(-1); // use default spacing
    topLayout->addWidget(widget);

    myButton = new KUrlDragPushButton(m_parent);
    myButton->setIcon(QIcon::fromTheme(QStringLiteral("document-open")));
    int buttonSize = myButton->sizeHint().expandedTo(widget->sizeHint()).height();
    myButton->setFixedSize(buttonSize, buttonSize);
    myButton->setToolTip(i18n("Open file dialog"));

    connect(myButton, SIGNAL(pressed()), m_parent, SLOT(_k_slotUpdateUrl()));

    widget->installEventFilter(m_parent);
    m_parent->setFocusProxy(widget);
    m_parent->setFocusPolicy(Qt::StrongFocus);
    topLayout->addWidget(myButton);

    connectSignals(m_parent);
    connect(myButton, SIGNAL(clicked()), m_parent, SLOT(_k_slotOpenDialog()));

    m_startDir = QUrl::fromLocalFile(QDir::currentPath());
    m_startDirCustomized = false;

    myCompletion = new KUrlCompletion();
    updateCompletionStartDir(m_startDir);

    setCompletionObject(myCompletion);

    QAction *openAction = new QAction(m_parent);
    openAction->setShortcut(QKeySequence::Open);
    m_parent->connect(openAction, SIGNAL(triggered(bool)), SLOT(_k_slotOpenDialog()));
}

void KUrlRequester::setUrl(const QUrl &url)
{
    d->setText(url.toDisplayString(QUrl::PreferLocalFile));
}

#ifndef KIOWIDGETS_NO_DEPRECATED
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

    if (((fileDialogMode & KFile::Directory) && !(fileDialogMode & KFile::File)) ||
            /* catch possible fileDialog()->setMode( KFile::Directory ) changes */
            (myFileDialog && (myFileDialog->fileMode() == QFileDialog::Directory &&
                              myFileDialog->testOption(QFileDialog::ShowDirsOnly)))) {
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
            emit m_parent->urlSelected(url());
        }
    } else {
        emit m_parent->openFileDialog(m_parent);

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
}

void KUrlRequester::KUrlRequesterPrivate::_k_slotFileDialogAccepted()
{
    if (!myFileDialog) {
        return;
    }

    QUrl newUrl = myFileDialog->selectedUrls().first();
    if (newUrl.isValid()) {
        m_parent->setUrl(newUrl);
        emit m_parent->urlSelected(url());
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
        d->applyFileMode(d->myFileDialog, mode);
    }
}

KFile::Modes KUrlRequester::mode() const
{
    return d->fileDialogMode;
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
}

QStringList KUrlRequester::mimeTypeFilters() const
{
    return d->mimeTypeFilters;
}

#ifndef KIOWIDGETS_NO_DEPRECATED
QFileDialog *KUrlRequester::fileDialog() const
{
    if (!d->myFileDialog) {
        d->myFileDialog = new QFileDialog(window(), windowTitle());
        if (!d->mimeTypeFilters.isEmpty()) {
            d->myFileDialog->setMimeTypeFilters(d->mimeTypeFilters);
        } else {
            d->myFileDialog->setNameFilters(d->kToQFilters(d->fileDialogFilter));
        }

        d->applyFileMode(d->myFileDialog, d->fileDialogMode);

        d->myFileDialog->setWindowModality(d->fileDialogModality);
        connect(d->myFileDialog, SIGNAL(accepted()), SLOT(_k_slotFileDialogAccepted()));
    }

    return d->myFileDialog;
}
#endif

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
        u = QUrl::fromLocalFile(QDir::currentPath() + '/').resolved(visibleUrl);
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

#ifndef KIOWIDGETS_NO_DEPRECATED
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

#ifndef KIOWIDGETS_NO_DEPRECATED
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
