/*
    This file is part of the KDE libraries
    SPDX-FileCopyrightText: 2001, 2002, 2003 Carsten Pfeiffer <pfeiffer@kde.org>
    SPDX-FileCopyrightText: 2007 Kevin Ottens <ervin@kde.org>

    SPDX-License-Identifier: LGPL-2.0-only
*/

#include "kfileplaceeditdialog.h"

#include <KAboutData>
#include <KConfig>
#include <KIconButton>
#include <KLineEdit>
#include <KLocalizedString>
#include <kio/global.h>
#include <kprotocolinfo.h>
#include <kurlrequester.h>

#include <QDialogButtonBox>
#include <QCheckBox>
#include <qdrawutil.h>
#include <QFormLayout>
#include <QApplication>

#include <qplatformdefs.h>
#include <KConfigGroup>

bool KFilePlaceEditDialog::getInformation(bool allowGlobal, QUrl &url,
        QString &label, QString &icon,
        bool isAddingNewPlace,
        bool &appLocal, int iconSize,
        QWidget *parent)
{
    KFilePlaceEditDialog *dialog = new KFilePlaceEditDialog(allowGlobal, url,
            label, icon,
            isAddingNewPlace,
            appLocal,
            iconSize,
            parent);
    if (dialog->exec() == QDialog::Accepted) {
        // set the return parameters
        url         = dialog->url();
        label       = dialog->label();
        if (dialog->isIconEditable()) {
            icon = dialog->icon();
        }
        appLocal    = dialog->applicationLocal();

        delete dialog;
        return true;
    }

    delete dialog;
    return false;
}

KFilePlaceEditDialog::KFilePlaceEditDialog(bool allowGlobal, const QUrl &url,
        const QString &label,
        const QString &icon,
        bool isAddingNewPlace,
        bool appLocal, int iconSize,
        QWidget *parent)
    : QDialog(parent), m_iconButton(nullptr)
{
    if (isAddingNewPlace) {
        setWindowTitle(i18n("Add Places Entry"));
    } else {
        setWindowTitle(i18n("Edit Places Entry"));
    }
    setModal(true);

    QVBoxLayout *box = new QVBoxLayout(this);

    QFormLayout *layout = new QFormLayout();
    box->addLayout(layout);

    QString whatsThisText = i18n("<qt>This is the text that will appear in the Places panel.<br /><br />"
                                 "The label should consist of one or two words "
                                 "that will help you remember what this entry refers to. "
                                 "If you do not enter a label, it will be derived from "
                                 "the location's URL.</qt>");
    m_labelEdit = new QLineEdit(this);
    layout->addRow(i18n("L&abel:"), m_labelEdit);
    m_labelEdit->setText(label);
    m_labelEdit->setPlaceholderText(i18n("Enter descriptive label here"));
    m_labelEdit->setWhatsThis(whatsThisText);
    layout->labelForField(m_labelEdit)->setWhatsThis(whatsThisText);

    whatsThisText = i18n("<qt>This is the location associated with the entry. Any valid URL may be used. For example:<br /><br />"
                         "%1<br />http://www.kde.org<br />ftp://ftp.kde.org/pub/kde/stable<br /><br />"
                         "By clicking on the button next to the text edit box you can browse to an "
                         "appropriate URL.</qt>", QDir::homePath());
    m_urlEdit = new KUrlRequester(url, this);
    m_urlEdit->setMode(KFile::Directory);
    layout->addRow(i18n("&Location:"), m_urlEdit);
    m_urlEdit->setWhatsThis(whatsThisText);
    layout->labelForField(m_urlEdit)->setWhatsThis(whatsThisText);
    // Room for at least 40 chars (average char width is half of height)
    m_urlEdit->setMinimumWidth(m_urlEdit->fontMetrics().height() * (40 / 2));

    if (isIconEditable()) {
        whatsThisText = i18n("<qt>This is the icon that will appear in the Places panel.<br /><br />"
                             "Click on the button to select a different icon.</qt>");
        m_iconButton = new KIconButton(this);
        layout->addRow(i18n("Choose an &icon:"), m_iconButton);
        m_iconButton->setObjectName(QStringLiteral("icon button"));
        m_iconButton->setIconSize(iconSize);
        m_iconButton->setIconType(KIconLoader::NoGroup, KIconLoader::Place);
        if (icon.isEmpty()) {
            m_iconButton->setIcon(KIO::iconNameForUrl(url));
        } else {
            m_iconButton->setIcon(icon);
        }
        m_iconButton->setWhatsThis(whatsThisText);
        layout->labelForField(m_iconButton)->setWhatsThis(whatsThisText);
    }

    if (allowGlobal) {
        QString appName;
        appName = QGuiApplication::applicationDisplayName();
        if (appName.isEmpty()) {
            appName = QCoreApplication::applicationName();
        }
        m_appLocal = new QCheckBox(i18n("&Only show when using this application (%1)", appName), this);
        m_appLocal->setChecked(appLocal);
        m_appLocal->setWhatsThis(i18n("<qt>Select this setting if you want this "
                                      "entry to show only when using the current application (%1).<br /><br />"
                                      "If this setting is not selected, the entry will be available in all "
                                      "applications.</qt>",
                                      appName));
        box->addWidget(m_appLocal);
    } else {
        m_appLocal = nullptr;
    }
    connect(m_urlEdit->lineEdit(), &QLineEdit::textChanged, this, &KFilePlaceEditDialog::urlChanged);
    if (!label.isEmpty()) {
        // editing existing entry
        m_labelEdit->setFocus();
    } else {
        // new entry
        m_urlEdit->setFocus();
    }

    m_buttonBox = new QDialogButtonBox(this);
    m_buttonBox->setStandardButtons(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    connect(m_buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(m_buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
    box->addWidget(m_buttonBox);
}

KFilePlaceEditDialog::~KFilePlaceEditDialog()
{
}

void KFilePlaceEditDialog::urlChanged(const QString &text)
{
    m_buttonBox->button(QDialogButtonBox::Ok)->setEnabled(!text.isEmpty());
}

QUrl KFilePlaceEditDialog::url() const
{
    return m_urlEdit->url();
}

QString KFilePlaceEditDialog::label() const
{
    if (!m_labelEdit->text().isEmpty()) {
        return m_labelEdit->text();
    }

    // derive descriptive label from the URL
    QUrl url = m_urlEdit->url();
    if (!url.fileName().isEmpty()) {
        return url.fileName();
    }
    if (!url.host().isEmpty()) {
        return url.host();
    }
    return url.scheme();
}

QString KFilePlaceEditDialog::icon() const
{
    if (!isIconEditable()) {
        return QString();
    }

    return m_iconButton->icon();
}

bool KFilePlaceEditDialog::applicationLocal() const
{
    if (!m_appLocal) {
        return true;
    }

    return m_appLocal->isChecked();
}

bool KFilePlaceEditDialog::isIconEditable() const
{
    return url().scheme() != QLatin1String("trash");
}
