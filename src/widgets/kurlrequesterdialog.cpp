/*
    This file is part of the KDE libraries
    SPDX-FileCopyrightText: 2000 Wilco Greven <greven@kde.org>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#include "kurlrequesterdialog.h"

#include <QDialogButtonBox>
#include <QLabel>
#include <QFileDialog>
#include <QVBoxLayout>

#include <KLineEdit>
#include <KLocalizedString>
#include <krecentdocument.h>
#include <KStandardGuiItem>
#include <kurlrequester.h>

class KUrlRequesterDialogPrivate
{
public:
    explicit KUrlRequesterDialogPrivate(KUrlRequesterDialog *qq)
        : q(qq)
    {
    }

    KUrlRequesterDialog * const q;

    void initDialog(const QString &text, const QUrl &url);

    // slots
    void _k_slotTextChanged(const QString &);

    KUrlRequester *urlRequester;
    QDialogButtonBox *buttonBox;
};

KUrlRequesterDialog::KUrlRequesterDialog(const QUrl &urlName, QWidget *parent)
    : QDialog(parent), d(new KUrlRequesterDialogPrivate(this))
{
    d->initDialog(i18n("Location:"), urlName);
}

KUrlRequesterDialog::KUrlRequesterDialog(const QUrl &urlName, const QString &_text, QWidget *parent)
    : QDialog(parent), d(new KUrlRequesterDialogPrivate(this))
{
    d->initDialog(_text, urlName);
}

KUrlRequesterDialog::~KUrlRequesterDialog()
{
    delete d;
}

void KUrlRequesterDialogPrivate::initDialog(const QString &text, const QUrl &urlName)
{
    QVBoxLayout *topLayout = new QVBoxLayout(q);

    QLabel *label = new QLabel(text, q);
    topLayout->addWidget(label);

    urlRequester = new KUrlRequester(urlName, q);
    urlRequester->setMinimumWidth(urlRequester->sizeHint().width() * 3);
    topLayout->addWidget(urlRequester);
    urlRequester->setFocus();
    QObject::connect(urlRequester->lineEdit(), &KLineEdit::textChanged,
                     q, [this](const QString &text) { _k_slotTextChanged(text); });
    /*
    KFile::Mode mode = static_cast<KFile::Mode>( KFile::File |
            KFile::ExistingOnly );
    urlRequester_->setMode( mode );
    */

    buttonBox = new QDialogButtonBox(q);
    buttonBox->setStandardButtons(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    QObject::connect(buttonBox, &QDialogButtonBox::accepted, q, &QDialog::accept);
    QObject::connect(buttonBox, &QDialogButtonBox::rejected, q, &QDialog::reject);
    topLayout->addWidget(buttonBox);

    _k_slotTextChanged(urlName.toString());
}

void KUrlRequesterDialogPrivate::_k_slotTextChanged(const QString &text)
{
    bool state = !text.trimmed().isEmpty();
    buttonBox->button(QDialogButtonBox::Ok)->setEnabled(state);
}

QUrl KUrlRequesterDialog::selectedUrl() const
{
    if (result() == QDialog::Accepted) {
        return d->urlRequester->url();
    } else {
        return QUrl();
    }
}

QUrl KUrlRequesterDialog::getUrl(const QUrl &dir, QWidget *parent,
                                 const QString &caption)
{
    KUrlRequesterDialog dlg(dir, parent);

    dlg.setWindowTitle(caption.isEmpty() ? i18n("Open") : caption);

    dlg.exec();

    const QUrl &url = dlg.selectedUrl();
    if (url.isValid()) {
        KRecentDocument::add(url);
    }

    return url;
}

#if KIOWIDGETS_BUILD_DEPRECATED_SINCE(5, 0)
QFileDialog *KUrlRequesterDialog::fileDialog()
{
    return d->urlRequester->fileDialog();
}
#endif

KUrlRequester *KUrlRequesterDialog::urlRequester()
{
    return d->urlRequester;
}

#include "moc_kurlrequesterdialog.cpp"

