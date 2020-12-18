/*
    SPDX-FileCopyrightText: 2001 Dawit Alemayehu <adawit@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

// Own
#include "useragentselectordlg.h"

// Local
#include "useragentinfo.h"

// Qt
#include <QComboBox>
#include <QDialogButtonBox>
#include <QLineEdit>
#include <QPushButton>
#include <QValidator>
#include <QVBoxLayout>
#include <QWidget>

// KDE
#include <KUrlLabel>

class UserAgentSiteNameValidator : public QValidator
{
    Q_OBJECT
public:
    UserAgentSiteNameValidator (QObject* parent)
        : QValidator (parent)
    {
        setObjectName (QStringLiteral ("UserAgentSiteNameValidator"));
    }

    State validate (QString& input, int&) const override
    {
        if (input.isEmpty())
            return Intermediate;

        if (input.startsWith(QLatin1Char('.')))
            return Invalid;

        const int length = input.length();

        for (int i = 0 ; i < length; i++) {
            if (!input[i].isLetterOrNumber() && input[i] != QLatin1Char('.') && input[i] != QLatin1Char('-'))
                return Invalid;
        }

        return Acceptable;
    }
};


UserAgentSelectorDlg::UserAgentSelectorDlg (UserAgentInfo* info, QWidget* parent, Qt::WindowFlags f)
    : QDialog (parent, f),
      mUserAgentInfo (info),
      mButtonBox(nullptr)
{
    QWidget *mainWidget = new QWidget(this);
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->addWidget(mainWidget);
    mUi.setupUi (mainWidget);

    mButtonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    mainLayout->addWidget(mButtonBox);

    connect(mButtonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(mButtonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);

    if (!mUserAgentInfo) {
        setEnabled (false);
        return;
    }

    mUi.aliasComboBox->clear();
    mUi.aliasComboBox->addItems (mUserAgentInfo->userAgentAliasList());
    mUi.aliasComboBox->insertItem (0, QString());
    mUi.aliasComboBox->model()->sort (0);
    mUi.aliasComboBox->setCurrentIndex (0);

    UserAgentSiteNameValidator* validator = new UserAgentSiteNameValidator (this);
    mUi.siteLineEdit->setValidator (validator);
    mUi.siteLineEdit->setFocus();

    connect (mUi.siteLineEdit, &QLineEdit::textEdited,
             this, &UserAgentSelectorDlg::onHostNameChanged);
    connect (mUi.aliasComboBox, &QComboBox::textActivated,
             this, &UserAgentSelectorDlg::onAliasChanged);

    mButtonBox->button(QDialogButtonBox::Ok)->setEnabled (false);
}

UserAgentSelectorDlg::~UserAgentSelectorDlg()
{
}

void UserAgentSelectorDlg::onAliasChanged (const QString& text)
{
    if (text.isEmpty())
        mUi.identityLineEdit->setText (QString());
    else
        mUi.identityLineEdit->setText (mUserAgentInfo->agentStr (text));

    const bool enable = (!mUi.siteLineEdit->text().isEmpty() && !text.isEmpty());
    mButtonBox->button(QDialogButtonBox::Ok)->setEnabled (enable);
}

void UserAgentSelectorDlg::onHostNameChanged (const QString& text)
{
    const bool enable = (!text.isEmpty() && !mUi.aliasComboBox->currentText().isEmpty());
    mButtonBox->button(QDialogButtonBox::Ok)->setEnabled (enable);
}

void UserAgentSelectorDlg::setSiteName (const QString& text)
{
    mUi.siteLineEdit->setText (text);
}

void UserAgentSelectorDlg::setIdentity (const QString& text)
{
    const int id = mUi.aliasComboBox->findText (text);
    if (id != -1)
        mUi.aliasComboBox->setCurrentIndex (id);

    mUi.identityLineEdit->setText (mUserAgentInfo->agentStr (mUi.aliasComboBox->currentText()));
    if (!mUi.siteLineEdit->isEnabled())
        mUi.aliasComboBox->setFocus();
}

QString UserAgentSelectorDlg::siteName()
{
    return mUi.siteLineEdit->text().toLower();
}

QString UserAgentSelectorDlg::identity()
{
    return mUi.aliasComboBox->currentText();
}

QString UserAgentSelectorDlg::alias()
{
    return mUi.identityLineEdit->text();
}

#include "useragentselectordlg.moc"
