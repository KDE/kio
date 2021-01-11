/*
    SPDX-FileCopyrightText: 1998, 2008, 2009 David Faure <faure@kde.org>

    SPDX-License-Identifier: LGPL-2.0-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/

#include "knameandurlinputdialog.h"

#include <KLineEdit>
#include <kurlrequester.h>
#include <kprotocolmanager.h>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QVBoxLayout>

class KNameAndUrlInputDialogPrivate
{
public:
    explicit KNameAndUrlInputDialogPrivate(KNameAndUrlInputDialog *qq)
        : m_fileNameEdited(false)
        , q(qq)
    {}

    void _k_slotNameTextChanged(const QString &);
    void _k_slotURLTextChanged(const QString &);

    /**
     * The line edit widget for the fileName
     */
    QLineEdit *m_leName;
    /**
     * The URL requester for the URL :)
     */
    KUrlRequester *m_urlRequester;
    /**
     * True if the filename was manually edited.
     */
    bool m_fileNameEdited;

    QDialogButtonBox *m_buttonBox;

    KNameAndUrlInputDialog * const q;
};

KNameAndUrlInputDialog::KNameAndUrlInputDialog(const QString &nameLabel, const QString &urlLabel, const QUrl &startDir, QWidget *parent)
    : QDialog(parent), d(new KNameAndUrlInputDialogPrivate(this))
{
    QVBoxLayout *topLayout = new QVBoxLayout(this);

    QFormLayout *formLayout = new QFormLayout;
    formLayout->setContentsMargins(0, 0, 0, 0);

    // First line: filename
    d->m_leName = new QLineEdit(this);
    d->m_leName->setMinimumWidth(d->m_leName->sizeHint().width() * 3);
    d->m_leName->setSelection(0, d->m_leName->text().length()); // autoselect
    connect(d->m_leName, &QLineEdit::textChanged,
            this, [this](const QString &text) { d->_k_slotNameTextChanged(text); });
    formLayout->addRow(nameLabel, d->m_leName);

    // Second line: url
    d->m_urlRequester = new KUrlRequester(this);
    d->m_urlRequester->setStartDir(startDir);
    d->m_urlRequester->setMode(KFile::File | KFile::Directory);

    d->m_urlRequester->setMinimumWidth(d->m_urlRequester->sizeHint().width() * 3);
    connect(d->m_urlRequester->lineEdit(), &QLineEdit::textChanged,
            this, [this](const QString &text) { d->_k_slotURLTextChanged(text); });
    formLayout->addRow(urlLabel, d->m_urlRequester);

    topLayout->addLayout(formLayout);

    d->m_buttonBox = new QDialogButtonBox(this);
    d->m_buttonBox->setStandardButtons(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    connect(d->m_buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(d->m_buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
    topLayout->addWidget(d->m_buttonBox);

    d->m_fileNameEdited = false;
    d->m_buttonBox->button(QDialogButtonBox::Ok)->setEnabled(!d->m_leName->text().isEmpty() && !d->m_urlRequester->url().isEmpty());
    d->m_leName->setFocus();
}

KNameAndUrlInputDialog::~KNameAndUrlInputDialog()
{
    delete d;
}

QUrl KNameAndUrlInputDialog::url() const
{
    return d->m_urlRequester->url();
}

QString KNameAndUrlInputDialog::urlText() const
{
    return d->m_urlRequester->text();
}

QString KNameAndUrlInputDialog::name() const
{
    return d->m_leName->text();
}

void KNameAndUrlInputDialogPrivate::_k_slotNameTextChanged(const QString &)
{
    m_fileNameEdited = true;
    m_buttonBox->button(QDialogButtonBox::Ok)->setEnabled(!m_leName->text().isEmpty() && !m_urlRequester->url().isEmpty());
}

void KNameAndUrlInputDialogPrivate::_k_slotURLTextChanged(const QString &)
{
    if (!m_fileNameEdited) {
        // use URL as default value for the filename
        // (we copy only its filename if protocol supports listing,
        // but for HTTP we don't want tons of index.html links)
        QUrl url(m_urlRequester->url());
        if (KProtocolManager::supportsListing(url) && !url.fileName().isEmpty()) {
            m_leName->setText(url.fileName());
        } else {
            m_leName->setText(url.toString());
        }
        m_fileNameEdited = false; // slotNameTextChanged set it to true erroneously
    }
    m_buttonBox->button(QDialogButtonBox::Ok)->setEnabled(!m_leName->text().isEmpty() && !m_urlRequester->url().isEmpty());
}

void KNameAndUrlInputDialog::setSuggestedName(const QString &name)
{
    d->m_leName->setText(name);
    d->m_urlRequester->setFocus();
}

void KNameAndUrlInputDialog::setSuggestedUrl(const QUrl &url)
{
    d->m_urlRequester->setUrl(url);
}

#include "moc_knameandurlinputdialog.cpp"
