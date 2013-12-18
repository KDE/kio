// -*- c++ -*-
/* This file is part of the KDE libraries
    Copyright (C) 2003 Joseph Wenninger <jowenn@kde.org>
                  2003 Andras Mantia <amantia@freemail.hu>
                  2013 Teo Mrnjavac <teo@kde.org>

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

#include "kencodingfiledialog.h"

#include "kfilewidget.h"

#include <defaults-kfile.h>

#include <kfilewidget.h>
#include <kcombobox.h>
#include <kconfiggroup.h>
#include <klocalizedstring.h>
#include <kcharsets.h>
#include <krecentdocument.h>
#include <ksharedconfig.h>
#include <kwindowconfig.h>

#include <QBoxLayout>
#include <QDialogButtonBox>
#include <QPushButton>
#include <QTextCodec>

struct KEncodingFileDialogPrivate
{
    KEncodingFileDialogPrivate()
        : cfgGroup(KSharedConfig::openConfig(), ConfigGroup)
    {}

    KComboBox *encoding;
    KFileWidget *w;
    KConfigGroup cfgGroup;
};


KEncodingFileDialog::KEncodingFileDialog(const QUrl &startDir,
                                         const QString &encoding,
                                         const QString &filter,
                                         const QString &caption,
                                         QFileDialog::AcceptMode type,
                                         QWidget *parent)
    : QDialog(parent, Qt::Dialog)
    , d(new KEncodingFileDialogPrivate)
{
    d->w = new KFileWidget(startDir, this);
    d->w->setFilter(filter);
    if (type == QFileDialog::AcceptOpen) {
        d->w->setOperationMode(KFileWidget::Opening);
    } else {
        d->w->setOperationMode(KFileWidget::Saving);
    }

    setWindowTitle(caption);
    //ops->clearHistory();

    KWindowConfig::restoreWindowSize(windowHandle(), d->cfgGroup);

    QBoxLayout *mainLayout = new QVBoxLayout;
    setLayout(mainLayout);
    mainLayout->addWidget(d->w);

    d->w->okButton()->show();
    connect(d->w->okButton(), SIGNAL(clicked()), SLOT(slotOk()));
    d->w->cancelButton()->show();
    connect(d->w->cancelButton(), SIGNAL(clicked()), SLOT(slotCancel()));
    connect(d->w, SIGNAL(accepted()), SLOT(accept()));

    d->encoding = new KComboBox(this);
    d->w->setCustomWidget(i18n("Encoding:"), d->encoding);

    d->encoding->clear();
    QString sEncoding = encoding;
    QString systemEncoding = QTextCodec::codecForLocale()->name();
    if (sEncoding.isEmpty() || sEncoding == "System")
        sEncoding = systemEncoding;

    const QStringList encodings (KCharsets::charsets()->availableEncodingNames());
    int insert = 0, system = 0;
    bool foundRequested=false;
    foreach (const QString& encoding, encodings)
    {
        bool found = false;
        QTextCodec *codecForEnc = KCharsets::charsets()->codecForName(encoding, found);

        if (found)
        {
            d->encoding->addItem (encoding);
            if ( (codecForEnc->name() == sEncoding) || (encoding == sEncoding) )
            {
                d->encoding->setCurrentIndex(insert);
                foundRequested = true;
            }

            if ( (codecForEnc->name() == systemEncoding) || (encoding == systemEncoding) )
                system = insert;
            insert++;
        }
    }

    if ( !foundRequested )
        d->encoding->setCurrentIndex(system);
}

KEncodingFileDialog::~KEncodingFileDialog()
{
    delete d;
}


QString KEncodingFileDialog::selectedEncoding() const
{
    if (d->encoding)
        return d->encoding->currentText();
    else
        return QString();
}


KEncodingFileDialog::Result KEncodingFileDialog::getOpenFileNameAndEncoding(const QString &encoding,
                                                                            const QUrl &startDir,
                                                                            const QString &filter,
                                                                            QWidget *parent,
                                                                            const QString &caption)
{
    KEncodingFileDialog dlg(startDir, encoding, filter,
                            caption.isNull() ? i18n("Open") : caption,
                            QFileDialog::AcceptOpen, parent);

    dlg.d->w->setMode( KFile::File | KFile::LocalOnly );
    dlg.exec();

    Result res;
    res.fileNames << dlg.d->w->selectedFile();
    res.encoding = dlg.selectedEncoding();
    return res;
}


KEncodingFileDialog::Result KEncodingFileDialog::getOpenFileNamesAndEncoding(const QString &encoding,
                                                                             const QUrl &startDir,
                                                                             const QString &filter,
                                                                             QWidget *parent,
                                                                             const QString &caption)
{
    KEncodingFileDialog dlg(startDir, encoding, filter,
                            caption.isNull() ? i18n("Open") : caption,
                            QFileDialog::AcceptOpen, parent);
    dlg.d->w->setMode(KFile::Files | KFile::LocalOnly);
    dlg.exec();

    Result res;
    res.fileNames = dlg.d->w->selectedFiles();
    res.encoding = dlg.selectedEncoding();
    return res;
}

KEncodingFileDialog::Result KEncodingFileDialog::getOpenUrlAndEncoding(const QString &encoding,
                                                                       const QUrl &startDir,
                                                                       const QString &filter,
                                                                       QWidget *parent,
                                                                       const QString &caption)
{
    KEncodingFileDialog dlg(startDir, encoding, filter,
                            caption.isNull() ? i18n("Open") : caption,
                            QFileDialog::AcceptOpen, parent);

    dlg.d->w->setMode( KFile::File );
    dlg.exec();

    Result res;
    res.URLs << dlg.d->w->selectedUrl();
    res.encoding = dlg.selectedEncoding();
    return res;
}

KEncodingFileDialog::Result KEncodingFileDialog::getOpenUrlsAndEncoding(const QString &encoding,
                                                                        const QUrl &startDir,
                                                                        const QString &filter,
                                                                        QWidget *parent,
                                                                        const QString &caption)
{
    KEncodingFileDialog dlg(startDir, encoding, filter,
                            caption.isNull() ? i18n("Open") : caption,
                            QFileDialog::AcceptOpen, parent);

    dlg.d->w->setMode(KFile::Files);
    dlg.exec();

    Result res;
    res.URLs = dlg.d->w->selectedUrls();
    res.encoding = dlg.selectedEncoding();
    return res;
}


KEncodingFileDialog::Result KEncodingFileDialog::getSaveFileNameAndEncoding(const QString &encoding,
                                                                            const QUrl &dir,
                                                                            const QString &filter,
                                                                            QWidget *parent,
                                                                            const QString &caption)
{
    KEncodingFileDialog dlg(dir, encoding, filter,
                            caption.isNull() ? i18n("Save As") : caption,
                            QFileDialog::AcceptSave, parent);
    dlg.d->w->setMode(KFile::File);
    dlg.exec();

    QString filename = dlg.d->w->selectedFile();
    if (!filename.isEmpty())
        KRecentDocument::add(QUrl::fromLocalFile(filename));

    Result res;
    res.fileNames << filename;
    res.encoding = dlg.selectedEncoding();
    return res;
}


KEncodingFileDialog::Result KEncodingFileDialog::getSaveUrlAndEncoding(const QString &encoding,
                                                                       const QUrl &dir,
                                                                       const QString &filter,
                                                                       QWidget *parent,
                                                                       const QString &caption)
{
    KEncodingFileDialog dlg(dir, encoding, filter,
                            caption.isNull() ? i18n("Save As") : caption,
                            QFileDialog::AcceptSave, parent);
    dlg.d->w->setMode(KFile::File);

    Result res;
    if (dlg.exec() == QDialog::Accepted) {
        QUrl url = dlg.d->w->selectedUrl();
        if (url.isValid())
            KRecentDocument::add( url );
        res.URLs << url;
        res.encoding = dlg.selectedEncoding();
    }
    return res;
}

QSize KEncodingFileDialog::sizeHint() const
{
    return d->w->dialogSizeHint();
}

void KEncodingFileDialog::hideEvent(QHideEvent *e)
{
    KWindowConfig::saveWindowSize(windowHandle(), d->cfgGroup, KConfigBase::Persistent);

    QDialog::hideEvent(e);
}

void KEncodingFileDialog::accept()
{
    d->w->accept();
    QDialog::accept();
}

void KEncodingFileDialog::slotOk()
{
    d->w->slotOk();
}

void KEncodingFileDialog::slotCancel()
{
    d->w->slotCancel();
    reject();
}

#include "moc_kencodingfiledialog.cpp"
