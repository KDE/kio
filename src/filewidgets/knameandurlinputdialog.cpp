/*
    Copyright (c) 1998, 2008, 2009 David Faure <faure@kde.org>

    This library is free software; you can redistribute it and/or modify
    it under the terms of the GNU Lesser General Public License as published by
    the Free Software Foundation; either version 2 of the License or (at
    your option) version 3 or, at the discretion of KDE e.V. (which shall
    act as a proxy as in section 14 of the GPLv3), any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Lesser General Public License
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.
*/

#include "knameandurlinputdialog.h"

#include <klineedit.h>
#include <kurlrequester.h>
#include <kprotocolmanager.h>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLabel>
#include <QVBoxLayout>

class KNameAndUrlInputDialogPrivate
{
public:
    KNameAndUrlInputDialogPrivate(KNameAndUrlInputDialog* qq) : m_fileNameEdited(false), q(qq) {}

    void _k_slotNameTextChanged(const QString&);
    void _k_slotURLTextChanged(const QString&);

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

    KNameAndUrlInputDialog* q;
};

KNameAndUrlInputDialog::KNameAndUrlInputDialog(const QString& nameLabel, const QString& urlLabel, const QUrl& startDir, QWidget *parent)
    : QDialog(parent), d(new KNameAndUrlInputDialogPrivate(this))
{
    QVBoxLayout *topLayout = new QVBoxLayout;
    setLayout(topLayout);

    QFormLayout* formLayout = new QFormLayout;
    formLayout->setMargin(0);

    // First line: filename
    d->m_leName = new QLineEdit(this);
    d->m_leName->setMinimumWidth(d->m_leName->sizeHint().width() * 3);
    d->m_leName->setSelection(0, d->m_leName->text().length()); // autoselect
    connect(d->m_leName, SIGNAL(textChanged(QString)),
            SLOT(_k_slotNameTextChanged(QString)));
    formLayout->addRow(nameLabel, d->m_leName);

    // Second line: url
    d->m_urlRequester = new KUrlRequester(this);
    d->m_urlRequester->setStartDir(startDir);
    d->m_urlRequester->setMode(KFile::File | KFile::Directory);

    d->m_urlRequester->setMinimumWidth(d->m_urlRequester->sizeHint().width() * 3);
    connect(d->m_urlRequester->lineEdit(), SIGNAL(textChanged(QString)),
            SLOT(_k_slotURLTextChanged(QString)));
    formLayout->addRow(urlLabel, d->m_urlRequester);

    topLayout->addLayout(formLayout);

    d->m_buttonBox = new QDialogButtonBox(this);
    d->m_buttonBox->setStandardButtons(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    connect(d->m_buttonBox, SIGNAL(accepted()), this, SLOT(accept()));
    connect(d->m_buttonBox, SIGNAL(rejected()), this, SLOT(reject()));
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
    if (result() == QDialog::Accepted) {
        return d->m_urlRequester->url();
    }
    else
        return QUrl();
}

QString KNameAndUrlInputDialog::name() const
{
    if (result() == QDialog::Accepted)
        return d->m_leName->text();
    else
        return QString();
}

void KNameAndUrlInputDialogPrivate::_k_slotNameTextChanged(const QString&)
{
    m_fileNameEdited = true;
    m_buttonBox->button(QDialogButtonBox::Ok)->setEnabled(!m_leName->text().isEmpty() && !m_urlRequester->url().isEmpty());
}

void KNameAndUrlInputDialogPrivate::_k_slotURLTextChanged(const QString&)
{
    if (!m_fileNameEdited) {
        // use URL as default value for the filename
        // (we copy only its filename if protocol supports listing,
        // but for HTTP we don't want tons of index.html links)
        QUrl url(m_urlRequester->url());
        if (KProtocolManager::supportsListing(url) && !url.fileName().isEmpty())
            m_leName->setText(url.fileName());
        else
            m_leName->setText(url.toString());
        m_fileNameEdited = false; // slotNameTextChanged set it to true erroneously
    }
    m_buttonBox->button(QDialogButtonBox::Ok)->setEnabled(!m_leName->text().isEmpty() && !m_urlRequester->url().isEmpty());
}

void KNameAndUrlInputDialog::setSuggestedName(const QString& name)
{
    d->m_leName->setText(name);
    d->m_urlRequester->setFocus();
}

void KNameAndUrlInputDialog::setSuggestedUrl(const QUrl& url)
{
    d->m_urlRequester->setUrl(url);
}

#include "moc_knameandurlinputdialog.cpp"
