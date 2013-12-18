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

class KUrlDragPushButton : public QPushButton
{
public:
    KUrlDragPushButton( QWidget *parent)
        : QPushButton(parent)
    {
        new DragDecorator(this);
    }
    ~KUrlDragPushButton() {}

    void setURL(const QUrl& url)
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
        virtual QDrag *dragObject()
        {
            if (m_button->m_urls.isEmpty())
                return 0;

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
          edit(0),
          combo(0),
          fileDialogMode(KFile::File | KFile::ExistingOnly | KFile::LocalOnly)
    {
    }

    ~KUrlRequesterPrivate()
    {
        delete myCompletion;
        delete myFileDialog;
    }

    void init();

    void setText( const QString& text ) {
        if ( combo )
        {
            if (combo->isEditable())
            {
               combo->setEditText( text );
            }
            else
            {
               int i = combo->findText( text );
               if ( i == -1 )
               {
                  combo->addItem( text );
                  combo->setCurrentIndex( combo->count()-1 );
               }
               else
               {
                  combo->setCurrentIndex( i );
               }
            }
        }
        else
        {
            edit->setText( text );
        }
    }

    void connectSignals( QObject *receiver )
    {
        QObject *sender;
        if ( combo )
            sender = combo;
        else
            sender = edit;

        if (combo )
            connect( sender, SIGNAL(editTextChanged(QString)),
                     receiver, SIGNAL(textChanged(QString)));
        else
            connect( sender, SIGNAL(textChanged(QString)),
                     receiver, SIGNAL(textChanged(QString)));

        connect( sender, SIGNAL(returnPressed()),
                 receiver, SIGNAL(returnPressed()));
        connect( sender, SIGNAL(returnPressed(QString)),
                 receiver, SIGNAL(returnPressed(QString)));
    }

    void setCompletionObject( KCompletion *comp )
    {
        if ( combo )
            combo->setCompletionObject( comp );
        else
            edit->setCompletionObject( comp );
    }

    QString text() const {
        return combo ? combo->currentText() : edit->text();
    }

    /**
     * replaces ~user or $FOO, if necessary
     */
    QUrl url() const {
        const QString txt = text();
        KUrlCompletion *comp;
        if ( combo )
            comp = qobject_cast<KUrlCompletion*>(combo->completionObject());
        else
            comp = qobject_cast<KUrlCompletion*>(edit->completionObject());

        if ( comp )
            return QUrl::fromUserInput(comp->replacedPath(txt));
        else
            return QUrl::fromUserInput(txt);
    }

    void applyFileMode(QFileDialog *dlg, KFile::Modes m) {
        QFileDialog::FileMode fileMode;
        if ( m & KFile::Directory ) {
            fileMode = QFileDialog::Directory;
            if ((m & KFile::File) == 0 &&
                (m & KFile::Files) == 0) {
                dlg->setOption(QFileDialog::ShowDirsOnly, true);
            }
        }
        else if (m & KFile::Files &&
                 m & KFile::ExistingOnly) {
            fileMode = QFileDialog::ExistingFiles;
        }
        else if (m & KFile::File &&
                 m & KFile::ExistingOnly) {
            fileMode = QFileDialog::ExistingFile;
        }
        else {
            fileMode = QFileDialog::AnyFile;
        }

        dlg->setFileMode(fileMode);
    }

    // Converts from "*.foo *.bar|Comment" to "Comment (*.foo *.bar)"
    QStringList kToQFilters(const QString &filters) const {
        QStringList qFilters = filters.split('\n', QString::SkipEmptyParts);

        for (QStringList::iterator it = qFilters.begin(); it != qFilters.end(); ++it)
        {
            int sep = it->indexOf('|');
            QString globs = it->left(sep);
            QString desc  = it->mid(sep+1);
            *it = QString("%1 (%2)").arg(desc).arg(globs);
        }

        return qFilters;
    }

    // slots
    void _k_slotUpdateUrl();
    void _k_slotOpenDialog();
    void _k_slotFileDialogFinished();

    QUrl m_startDir;
    KUrlRequester *m_parent; // TODO: rename to 'q'
    KLineEdit *edit;
    KComboBox *combo;
    KFile::Modes fileDialogMode;
    QString fileDialogFilter;
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
    editWidget->setParent( this );
    d->combo = qobject_cast<KComboBox*>( editWidget );
    d->edit = qobject_cast<KLineEdit*>( editWidget );
    if ( d->edit ) {
        d->edit->setClearButtonShown( true );
    }

    d->init();
}


KUrlRequester::KUrlRequester( QWidget *parent)
  : QWidget(parent), d(new KUrlRequesterPrivate(this))
{
    d->init();
}


KUrlRequester::KUrlRequester( const QUrl& url, QWidget *parent)
  : QWidget(parent), d(new KUrlRequesterPrivate(this))
{
    d->init();
    setUrl( url );
}

KUrlRequester::~KUrlRequester()
{
    delete d;
}


void KUrlRequester::KUrlRequesterPrivate::init()
{
    myFileDialog = 0L;
    fileDialogModality = Qt::ApplicationModal;

    if ( !combo && !edit ) {
        edit = new KLineEdit( m_parent );
        edit->setClearButtonShown( true );
    }

    QWidget *widget = combo ? (QWidget*) combo : (QWidget*) edit;

    QHBoxLayout* topLayout = new QHBoxLayout(m_parent);
    topLayout->setMargin(0);
    topLayout->setSpacing(-1); // use default spacing
    topLayout->addWidget(widget);

    myButton = new KUrlDragPushButton(m_parent);
    myButton->setIcon(QIcon::fromTheme("document-open"));
    int buttonSize = myButton->sizeHint().expandedTo(widget->sizeHint()).height();
    myButton->setFixedSize(buttonSize, buttonSize);
    myButton->setToolTip(i18n("Open file dialog"));

    m_parent->connect(myButton, SIGNAL(pressed()), SLOT(_k_slotUpdateUrl()));

    widget->installEventFilter( m_parent );
    m_parent->setFocusProxy( widget );
    m_parent->setFocusPolicy(Qt::StrongFocus);
    topLayout->addWidget(myButton);

    connectSignals( m_parent );
    m_parent->connect(myButton, SIGNAL(clicked()), m_parent, SLOT(_k_slotOpenDialog()));

    myCompletion = new KUrlCompletion();
    setCompletionObject( myCompletion );

    QAction* openAction = new QAction(m_parent);
    openAction->setShortcut(QKeySequence::Open);
    m_parent->connect(openAction, SIGNAL(triggered(bool)), SLOT(_k_slotOpenDialog()));
}

void KUrlRequester::setUrl(const QUrl& url)
{
    d->setText(url.toDisplayString(QUrl::PreferLocalFile));
}

#ifndef KDE_NO_DEPRECATED
void KUrlRequester::setPath( const QString& path )
{
    d->setText( path );
}
#endif

void KUrlRequester::setText(const QString& text)
{
    d->setText(text);
}

void KUrlRequester::setStartDir(const QUrl& startDir)
{
    d->m_startDir = startDir;
    d->myCompletion->setDir(startDir);
}

void KUrlRequester::changeEvent(QEvent *e)
{
   if (e->type()==QEvent::WindowTitleChange) {
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
    if ( myFileDialog )
        if ( myFileDialog->isVisible() )
        {
            //The file dialog is already being shown, raise it and exit
            myFileDialog->raise();
            myFileDialog->activateWindow();
            return;
        }

    if ( ((fileDialogMode & KFile::Directory) && !(fileDialogMode & KFile::File)) ||
         /* catch possible fileDialog()->setMode( KFile::Directory ) changes */
         (myFileDialog && (myFileDialog->fileMode() == QFileDialog::Directory &&
                           myFileDialog->testOption(QFileDialog::ShowDirsOnly))))
    {
        const QUrl openUrl = (!m_parent->url().isEmpty() && !m_parent->url().isRelative() )
          ? m_parent->url() : m_startDir;

        /* FIXME We need a new abstract interface for using KDirSelectDialog in a non-modal way */

        QUrl newUrl;
        if (fileDialogMode & KFile::LocalOnly) {
            newUrl = QUrl::fromLocalFile(QFileDialog::getExistingDirectory(m_parent, QString(), openUrl.toString(), QFileDialog::ShowDirsOnly));
        }
        else {
            newUrl = QFileDialog::getExistingDirectoryUrl(m_parent, QString(), openUrl, QFileDialog::ShowDirsOnly);
        }

        if ( newUrl.isValid() )
        {
            m_parent->setUrl( newUrl );
            emit m_parent->urlSelected( url() );
        }
    }
    else
    {
        emit m_parent->openFileDialog( m_parent );

        //Creates the fileDialog if it doesn't exist yet
        QFileDialog *dlg = m_parent->fileDialog();

        if ( !url().isEmpty() && !url().isRelative() ) {
            QUrl u(url());
            // If we won't be able to list it (e.g. http), then don't try :)
            if (KProtocolManager::supportsListing(u))
                dlg->selectUrl(u);
        } else {
            dlg->setDirectoryUrl(m_startDir);
        }

        //Update the file dialog window modality
        if ( dlg->windowModality() != fileDialogModality )
            dlg->setWindowModality(fileDialogModality);

        if ( fileDialogModality == Qt::NonModal )
        {
            dlg->show();
        } else {
            dlg->exec();
        }
    }
}

void KUrlRequester::KUrlRequesterPrivate::_k_slotFileDialogFinished()
{
    if ( !myFileDialog )
        return;

    if ( myFileDialog->result() == QDialog::Accepted )
    {
        QUrl newUrl = myFileDialog->selectedUrls().first();
        if ( newUrl.isValid() )
        {
            m_parent->setUrl( newUrl );
            emit m_parent->urlSelected( url() );
        }
    }
}

void KUrlRequester::setMode( KFile::Modes mode)
{
    Q_ASSERT( (mode & KFile::Files) == 0 );
    d->fileDialogMode = mode;
    if ( (mode & KFile::Directory) && !(mode & KFile::File) )
        d->myCompletion->setMode( KUrlCompletion::DirCompletion );

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

QString KUrlRequester::filter( ) const
{
    return d->fileDialogFilter;
}

#ifndef KDE_NO_DEPRECATED
QFileDialog* KUrlRequester::fileDialog() const
{
    if (!d->myFileDialog) {
        QWidget *p = parentWidget();
        d->myFileDialog = new QFileDialog(p, windowTitle());
        d->myFileDialog->setNameFilters(d->kToQFilters(d->fileDialogFilter));

        d->applyFileMode(d->myFileDialog, d->fileDialogMode);

        d->myFileDialog->setWindowModality(d->fileDialogModality);
        connect(d->myFileDialog, SIGNAL(finished()), SLOT(_k_slotFileDialogFinished()));
    }

    return d->myFileDialog;
}
#endif

void KUrlRequester::clear()
{
    d->setText( QString() );
}

KLineEdit * KUrlRequester::lineEdit() const
{
    return d->edit;
}

KComboBox * KUrlRequester::comboBox() const
{
    return d->combo;
}

void KUrlRequester::KUrlRequesterPrivate::_k_slotUpdateUrl()
{
    const QUrl visibleUrl = url();
    QUrl u = visibleUrl;
    if (visibleUrl.isRelative())
        u = QUrl::fromLocalFile(QDir::currentPath() + '/').resolved(visibleUrl);
    myButton->setURL(u);
}

bool KUrlRequester::eventFilter( QObject *obj, QEvent *ev )
{
    if ( ( d->edit == obj ) || ( d->combo == obj ) )
    {
        if (( ev->type() == QEvent::FocusIn ) || ( ev->type() == QEvent::FocusOut ))
            // Forward focusin/focusout events to the urlrequester; needed by file form element in khtml
            QApplication::sendEvent( this, ev );
    }
    return QWidget::eventFilter( obj, ev );
}

QPushButton *KUrlRequester::button() const
{
    return d->myButton;
}

KUrlCompletion *KUrlRequester::completionObject() const
{
    return d->myCompletion;
}

#ifndef KDE_NO_DEPRECATED
void KUrlRequester::setClickMessage(const QString& msg)
{
    setPlaceholderText(msg);
}
#endif

void KUrlRequester::setPlaceholderText(const QString& msg)
{
    if(d->edit)
        d->edit->setPlaceholderText(msg);
}

#ifndef KDE_NO_DEPRECATED
QString KUrlRequester::clickMessage() const
{
    return placeholderText();
}
#endif

QString KUrlRequester::placeholderText() const
{
    if(d->edit)
        return d->edit->placeholderText();
    else
        return QString();
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
    setSizePolicy(QSizePolicy( QSizePolicy::Preferred,
                               QSizePolicy::Fixed));

    KLineEdit *edit = d->edit;
    if ( !edit && d->combo )
        edit = qobject_cast<KLineEdit*>( d->combo->lineEdit() );

#ifndef NDEBUG
    if ( !edit ) {
        qWarning() << "KUrlRequester's lineedit is not a KLineEdit!??\n";
    }
#endif

    d->editor.setRepresentationWidget(this);
    d->editor.setLineEdit(edit);
    return d->editor;
}

KUrlComboRequester::KUrlComboRequester( QWidget *parent)
  : KUrlRequester( new KComboBox(false), parent), d(0)
{
}

#include "moc_kurlrequester.cpp"
