// -*- c++ -*-
/*
    This file is part of the KDE libraries
    SPDX-FileCopyrightText: 2003 Joseph Wenninger <jowenn@kde.org>
    SPDX-FileCopyrightText: 2003 Andras Mantia <amantia@freemail.hu>
    SPDX-FileCopyrightText: 2013 Teo Mrnjavac <teo@kde.org>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#include "kencodingfiledialog.h"

#include "kfilewidget.h"

#include <defaults-kfile.h>

#include <KConfigGroup>
#include <KLocalizedString>
#include <KSharedConfig>
#include <KWindowConfig>
#include <krecentdocument.h>

#include <QBoxLayout>
#include <QComboBox>
#include <QPushButton>
#include <QStringDecoder>
#if QT_VERSION < QT_VERSION_CHECK(6, 7, 0)
#include <QTextCodec>
#endif

struct KEncodingFileDialogPrivate {
    KEncodingFileDialogPrivate()
        : cfgGroup(KSharedConfig::openConfig(), ConfigGroup)
    {
    }

    QComboBox *encoding;
    KFileWidget *w;
    KConfigGroup cfgGroup;
};

KEncodingFileDialog::KEncodingFileDialog(const QUrl &startDir,
                                         const QString &encoding,
                                         const QString &filter,
                                         const QString &title,
                                         QFileDialog::AcceptMode type,
                                         QWidget *parent)
    : QDialog(parent, Qt::Dialog)
    , d(new KEncodingFileDialogPrivate)
{
    d->w = new KFileWidget(startDir, this);
    d->w->setFilters(KFileFilter::fromFilterString(filter));
    if (type == QFileDialog::AcceptOpen) {
        d->w->setOperationMode(KFileWidget::Opening);
    } else {
        d->w->setOperationMode(KFileWidget::Saving);
    }

    setWindowTitle(title);
    // ops->clearHistory();

    KWindowConfig::restoreWindowSize(windowHandle(), d->cfgGroup);

    QBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->addWidget(d->w);

    d->w->okButton()->show();
    connect(d->w->okButton(), &QAbstractButton::clicked, this, &KEncodingFileDialog::slotOk);
    d->w->cancelButton()->show();
    connect(d->w->cancelButton(), &QAbstractButton::clicked, this, &KEncodingFileDialog::slotCancel);
    connect(d->w, &KFileWidget::accepted, this, &KEncodingFileDialog::accept);

    d->encoding = new QComboBox(this);
    d->w->setCustomWidget(i18n("Encoding:"), d->encoding);

    d->encoding->clear();
    QByteArray sEncoding = encoding.toUtf8();
#if QT_VERSION < QT_VERSION_CHECK(6, 7, 0)
    const auto systemEncoding = QTextCodec::codecForLocale()->name();
#else
    const auto systemEncoding = QStringDecoder(QStringDecoder::System).name();
#endif
    if (sEncoding.isEmpty() || sEncoding == "System") {
        sEncoding = systemEncoding;
    }

#if QT_VERSION < QT_VERSION_CHECK(6, 7, 0)
    auto encodings = QTextCodec::availableCodecs();
#else
    auto encodings = QStringDecoder::availableCodecs();
#endif
    std::sort(encodings.begin(), encodings.end(), [](auto &a, auto &b) {
        return (a.compare(b, Qt::CaseInsensitive) < 0);
    });

    int insert = 0;
    int system = 0;
    bool foundRequested = false;
    for (const auto &encoding : encodings) {
#if QT_VERSION < QT_VERSION_CHECK(6, 7, 0)
        QTextCodec *codecForEnc = QTextCodec::codecForName(encoding);

        if (codecForEnc) {
            d->encoding->addItem(QString::fromUtf8(encoding));
            auto codecName = codecForEnc->name();
            if ((codecName == sEncoding) || (encoding == sEncoding)) {
                d->encoding->setCurrentIndex(insert);
                foundRequested = true;
            }

            if ((codecName == systemEncoding) || (encoding == systemEncoding)) {
                system = insert;
            }
            insert++;
        }
#else
        d->encoding->addItem(encoding);
        const auto codecName = QStringDecoder(encoding.toUtf8().constData()).name();
        if ((codecName == sEncoding) || (encoding.toUtf8() == sEncoding)) {
            d->encoding->setCurrentIndex(insert);
            foundRequested = true;
        }

        if ((codecName == systemEncoding) || (encoding.toUtf8() == systemEncoding)) {
            system = insert;
        }
        insert++;
#endif
    }

    if (!foundRequested) {
        d->encoding->setCurrentIndex(system);
    }
}

KEncodingFileDialog::~KEncodingFileDialog() = default;

QString KEncodingFileDialog::selectedEncoding() const
{
    if (d->encoding) {
        return d->encoding->currentText();
    } else {
        return QString();
    }
}

KEncodingFileDialog::Result
KEncodingFileDialog::getOpenFileNameAndEncoding(const QString &encoding, const QUrl &startDir, const QString &filter, QWidget *parent, const QString &title)
{
    KEncodingFileDialog dlg(startDir, encoding, filter, title.isNull() ? i18n("Open") : title, QFileDialog::AcceptOpen, parent);

    dlg.d->w->setMode(KFile::File | KFile::LocalOnly);
    dlg.exec();

    Result res;
    res.fileNames << dlg.d->w->selectedFile();
    res.encoding = dlg.selectedEncoding();
    return res;
}

KEncodingFileDialog::Result
KEncodingFileDialog::getOpenFileNamesAndEncoding(const QString &encoding, const QUrl &startDir, const QString &filter, QWidget *parent, const QString &title)
{
    KEncodingFileDialog dlg(startDir, encoding, filter, title.isNull() ? i18n("Open") : title, QFileDialog::AcceptOpen, parent);
    dlg.d->w->setMode(KFile::Files | KFile::LocalOnly);
    dlg.exec();

    Result res;
    res.fileNames = dlg.d->w->selectedFiles();
    res.encoding = dlg.selectedEncoding();
    return res;
}

KEncodingFileDialog::Result
KEncodingFileDialog::getOpenUrlAndEncoding(const QString &encoding, const QUrl &startDir, const QString &filter, QWidget *parent, const QString &title)
{
    KEncodingFileDialog dlg(startDir, encoding, filter, title.isNull() ? i18n("Open") : title, QFileDialog::AcceptOpen, parent);

    dlg.d->w->setMode(KFile::File);
    dlg.exec();

    Result res;
    res.URLs << dlg.d->w->selectedUrl();
    res.encoding = dlg.selectedEncoding();
    return res;
}

KEncodingFileDialog::Result
KEncodingFileDialog::getOpenUrlsAndEncoding(const QString &encoding, const QUrl &startDir, const QString &filter, QWidget *parent, const QString &title)
{
    KEncodingFileDialog dlg(startDir, encoding, filter, title.isNull() ? i18n("Open") : title, QFileDialog::AcceptOpen, parent);

    dlg.d->w->setMode(KFile::Files);
    dlg.exec();

    Result res;
    res.URLs = dlg.d->w->selectedUrls();
    res.encoding = dlg.selectedEncoding();
    return res;
}

KEncodingFileDialog::Result
KEncodingFileDialog::getSaveFileNameAndEncoding(const QString &encoding, const QUrl &dir, const QString &filter, QWidget *parent, const QString &title)
{
    KEncodingFileDialog dlg(dir, encoding, filter, title.isNull() ? i18n("Save As") : title, QFileDialog::AcceptSave, parent);
    dlg.d->w->setMode(KFile::File);
    dlg.exec();

    QString filename = dlg.d->w->selectedFile();
    if (!filename.isEmpty()) {
        KRecentDocument::add(QUrl::fromLocalFile(filename));
    }

    Result res;
    res.fileNames << filename;
    res.encoding = dlg.selectedEncoding();
    return res;
}

KEncodingFileDialog::Result
KEncodingFileDialog::getSaveUrlAndEncoding(const QString &encoding, const QUrl &dir, const QString &filter, QWidget *parent, const QString &title)
{
    KEncodingFileDialog dlg(dir, encoding, filter, title.isNull() ? i18n("Save As") : title, QFileDialog::AcceptSave, parent);
    dlg.d->w->setMode(KFile::File);

    Result res;
    if (dlg.exec() == QDialog::Accepted) {
        QUrl url = dlg.d->w->selectedUrl();
        if (url.isValid()) {
            KRecentDocument::add(url);
        }
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
