/* This file is part of the KDE libraries
    Copyright (C) 2000 Wilco Greven <greven@kde.org>

    library is free software; you can redistribute it and/or
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

#include "kurlrequesterdialog.h"

#include <QDialogButtonBox>
#include <QLabel>
#include <QLayout>
#include <QFileDialog>

#include <klineedit.h>
#include <klocalizedstring.h>
#include <krecentdocument.h>
#include <kstandardguiitem.h>
#include <kurlrequester.h>

class KUrlRequesterDialogPrivate
{
public:
    KUrlRequesterDialogPrivate(KUrlRequesterDialog *qq)
        : q(qq)
    {
    }

    KUrlRequesterDialog *q;

    void initDialog(const QString &text, const QUrl &url);

    // slots
    void _k_slotClear();
    void _k_slotTextChanged(const QString &);

    KUrlRequester *urlRequester;
    QDialogButtonBox *buttonBox;
    QPushButton *clearButton;
};


KUrlRequesterDialog::KUrlRequesterDialog( const QUrl& urlName, QWidget *parent)
    : QDialog(parent), d(new KUrlRequesterDialogPrivate(this))
{
    d->initDialog(i18n("Location:"), urlName);
}

KUrlRequesterDialog::KUrlRequesterDialog( const QUrl& urlName, const QString& _text, QWidget *parent)
    : QDialog(parent), d(new KUrlRequesterDialogPrivate(this))
{
    d->initDialog(_text, urlName);
}

KUrlRequesterDialog::~KUrlRequesterDialog()
{
    delete d;
}

void KUrlRequesterDialogPrivate::initDialog(const QString &text,const QUrl &urlName)
{
    QVBoxLayout *topLayout = new QVBoxLayout;
    q->setLayout(topLayout);

    QLabel *label = new QLabel(text, q);
    topLayout->addWidget( label );

    urlRequester = new KUrlRequester(urlName, q);
    urlRequester->setMinimumWidth(urlRequester->sizeHint().width() * 3);
    topLayout->addWidget(urlRequester);
    urlRequester->setFocus();
    QObject::connect(urlRequester->lineEdit(), SIGNAL(textChanged(QString)),
                     q, SLOT(_k_slotTextChanged(QString)));
    /*
    KFile::Mode mode = static_cast<KFile::Mode>( KFile::File |
            KFile::ExistingOnly );
	urlRequester_->setMode( mode );
    */

    clearButton = new QPushButton;
    KGuiItem::assign(clearButton, KStandardGuiItem::clear());
    QObject::connect(clearButton, SIGNAL(clicked()), q, SLOT(_k_slotClear()));

    buttonBox = new QDialogButtonBox(q);
    buttonBox->addButton(clearButton, QDialogButtonBox::ActionRole);
    buttonBox->setStandardButtons(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    QObject::connect(buttonBox, SIGNAL(accepted()), q, SLOT(accept()));
    QObject::connect(buttonBox, SIGNAL(rejected()), q, SLOT(reject()));
    topLayout->addWidget(buttonBox);

    _k_slotTextChanged(urlName.toString());
}

void KUrlRequesterDialogPrivate::_k_slotTextChanged(const QString & text)
{
    bool state = !text.trimmed().isEmpty();
    buttonBox->button(QDialogButtonBox::Ok)->setEnabled(state);
    clearButton->setEnabled(state);
}

void KUrlRequesterDialogPrivate::_k_slotClear()
{
    urlRequester->clear();
}

QUrl KUrlRequesterDialog::selectedUrl() const
{
    if ( result() == QDialog::Accepted )
        return d->urlRequester->url();
    else
        return QUrl();
}


QUrl KUrlRequesterDialog::getUrl(const QUrl& dir, QWidget *parent,
                                 const QString& caption)
{
    KUrlRequesterDialog dlg(dir, parent);

    dlg.setWindowTitle(caption.isEmpty() ? i18n("Open") : caption);

    dlg.exec();

    const QUrl& url = dlg.selectedUrl();
    if (url.isValid())
        KRecentDocument::add(url);

    return url;
}

#ifndef KDE_NO_DEPRECATED
QFileDialog * KUrlRequesterDialog::fileDialog()
{
    return d->urlRequester->fileDialog();
}
#endif

KUrlRequester * KUrlRequesterDialog::urlRequester()
{
    return d->urlRequester;
}

#include "moc_kurlrequesterdialog.cpp"

// vim:ts=4:sw=4:tw=78
