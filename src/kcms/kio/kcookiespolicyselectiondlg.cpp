/*
    SPDX-FileCopyrightText: 2000 Dawit Alemayehu <adawit@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

// Own
#include "kcookiespolicyselectiondlg.h"

// Qt
#include <QPushButton>
#include <QDialogButtonBox>
#include <QWhatsThis>
#include <QValidator>

// KDE
#include <QLineEdit>
#include <QVBoxLayout>

class DomainNameValidator : public QValidator
{
    Q_OBJECT
public:
    DomainNameValidator (QObject* parent)
        :QValidator(parent)
    {
        setObjectName(QStringLiteral("domainValidator"));
    }

    State validate (QString& input, int&) const override
    {
        if (input.isEmpty() || (input == QLatin1Char('.'))) {
            return Intermediate;
        }

        const int length = input.length();

        for (int i = 0 ; i < length; i++) {
            if (!input[i].isLetterOrNumber() && input[i] != QLatin1Char('.') && input[i] != QLatin1Char('-')) {
                return Invalid;
            }
        }

        return Acceptable;
    }
};


KCookiesPolicySelectionDlg::KCookiesPolicySelectionDlg (QWidget* parent, Qt::WindowFlags flags)
    : QDialog (parent, flags)
      , mOldPolicy(-1)
      , mButtonBox(nullptr)
{
    QWidget *mainWidget = new QWidget(this);
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->addWidget(mainWidget);
    mUi.setupUi(mainWidget);
    mUi.leDomain->setValidator(new DomainNameValidator (mUi.leDomain));
    mUi.cbPolicy->setMinimumWidth(mUi.cbPolicy->fontMetrics().maxWidth() * 15);

    mButtonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    mainLayout->addWidget(mButtonBox);

    connect(mButtonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(mButtonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);

    mButtonBox->button(QDialogButtonBox::Ok)->setEnabled(false);
    connect(mUi.leDomain, &QLineEdit::textEdited,
            this, &KCookiesPolicySelectionDlg::slotTextChanged);
    connect(mUi.cbPolicy, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](const int index) {
                slotPolicyChanged(mUi.cbPolicy->itemText(index));
            });

    mUi.leDomain->setFocus();
}

void KCookiesPolicySelectionDlg::setEnableHostEdit (bool state, const QString& host)
{
    if (!host.isEmpty()) {
        mUi.leDomain->setText (host);
        mButtonBox->button(QDialogButtonBox::Ok)->setEnabled(state);
    }

    mUi.leDomain->setEnabled (state);
}

void KCookiesPolicySelectionDlg::setPolicy (int policy)
{
    if (policy > -1 && policy <= static_cast<int> (mUi.cbPolicy->count())) {
        const bool blocked = mUi.cbPolicy->blockSignals(true);
        mUi.cbPolicy->setCurrentIndex (policy - 1);
        mUi.cbPolicy->blockSignals(blocked);
        mOldPolicy = policy;
    }

    if (!mUi.leDomain->isEnabled())
        mUi.cbPolicy->setFocus();
}

int KCookiesPolicySelectionDlg::advice () const
{
    return mUi.cbPolicy->currentIndex() + 1;
}

QString KCookiesPolicySelectionDlg::domain () const
{
    return mUi.leDomain->text();
}

void KCookiesPolicySelectionDlg::slotTextChanged (const QString& text)
{
    mButtonBox->button(QDialogButtonBox::Ok)->setEnabled (text.length() > 1);
}

void KCookiesPolicySelectionDlg::slotPolicyChanged(const QString& policyText)
{
    const int policy = KCookieAdvice::strToAdvice(policyText);
    if (!mUi.leDomain->isEnabled()) {
        mButtonBox->button(QDialogButtonBox::Ok)->setEnabled(policy != mOldPolicy);
    } else {
        slotTextChanged(policyText);
    }
}

#include "kcookiespolicyselectiondlg.moc"

