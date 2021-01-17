/*
    This file is part of the KDE project
    SPDX-FileCopyrightText: 2000 Alexander Neundorf <neundorf@kde.org>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

// Own
#include "smbrodlg.h"

// Qt
#include <QLabel>
#include <QGridLayout>

// KDE
#include <KConfig>
#include <KConfigGroup>
#include <KLocalizedString>
#include <KPluginFactory>

K_PLUGIN_FACTORY_DECLARATION(KioConfigFactory)

SMBRoOptions::SMBRoOptions(QWidget *parent, const QVariantList &)
  : KCModule(parent)
{
   QGridLayout *layout = new QGridLayout(this );
   QLabel *label = new QLabel(i18n("These settings apply to network browsing only."),this);
   layout->addWidget(label,0,0, 1, 2 );

   m_userLe = new QLineEdit(this);
   label = new QLabel(i18n("Default user name:"),this);
   label->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
   label->setBuddy( m_userLe );
   layout->addWidget(label,1,0);
   layout->addWidget(m_userLe,1,1);

   m_passwordLe = new QLineEdit(this);
   m_passwordLe->setEchoMode(QLineEdit::Password);
   label = new QLabel(i18n("Default password:"),this);
   label->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
   label->setBuddy( m_passwordLe );
   layout->addWidget(label,2,0);
   layout->addWidget(m_passwordLe,2,1);

   layout->addWidget(new QWidget(this),4,0);

   connect(m_userLe, &QLineEdit::textChanged, this, &SMBRoOptions::changed);
   connect(m_passwordLe, &QLineEdit::textChanged, this, &SMBRoOptions::changed);

   layout->setRowStretch(4, 1);
}

SMBRoOptions::~SMBRoOptions() = default;

void SMBRoOptions::load()
{
   KConfig *cfg = new KConfig(QStringLiteral("kioslaverc"));

   KConfigGroup group = cfg->group("Browser Settings/SMBro" );
   m_userLe->setText(group.readEntry("User"));

   // unscramble
   QString scrambled = group.readEntry( "Password" );
   QString password;
   const int passwordLength = scrambled.length() / 3;
   password.reserve(passwordLength);
   for (int i=0; i < passwordLength; ++i) {
      QChar qc1 = scrambled[i*3];
      QChar qc2 = scrambled[i*3+1];
      QChar qc3 = scrambled[i*3+2];
      unsigned int a1 = qc1.toLatin1() - '0';
      unsigned int a2 = qc2.toLatin1() - 'A';
      unsigned int a3 = qc3.toLatin1() - '0';
      unsigned int num = ((a1 & 0x3F) << 10) | ((a2& 0x1F) << 5) | (a3 & 0x1F);
      password[i] = QLatin1Char((num - 17) ^ 173); // restore
   }
   m_passwordLe->setText(password);

   delete cfg;
}

void SMBRoOptions::save()
{
   KConfig *cfg = new KConfig(QStringLiteral("kioslaverc"));

   KConfigGroup group = cfg->group("Browser Settings/SMBro" );
   group.writeEntry( "User", m_userLe->text());

   //taken from Nicola Brodu's smb ioslave
   //it's not really secure, but at
   //least better than storing the plain password
   QString password(m_passwordLe->text());
   QString scrambled;
   for (const QChar c : qAsConst(password)) {
      unsigned int num = (c.unicode() ^ 173) + 17;
      unsigned int a1 = (num & 0xFC00) >> 10;
      unsigned int a2 = (num & 0x3E0) >> 5;
      unsigned int a3 = (num & 0x1F);
      scrambled += QLatin1Char((char)(a1+'0'));
      scrambled += QLatin1Char((char)(a2+'A'));
      scrambled += QLatin1Char((char)(a3+'0'));
   }
   group.writeEntry( "Password", scrambled);

   delete cfg;
}

void SMBRoOptions::defaults()
{
   m_userLe->setText(QString());
   m_passwordLe->setText(QString());
}

void SMBRoOptions::changed()
{
   Q_EMIT KCModule::changed(true);
}

QString SMBRoOptions::quickHelp() const
{
   return i18n("<h1>Windows Shares</h1><p>Applications using the "
        "SMB kioslave (like Konqueror) are able to access shared Microsoft "
        "Windows file systems, if properly configured.</p><p>You can specify "
        "here the credentials used to access the shared resources. "
        "Passwords will be stored locally, and scrambled so as to render them "
        "unreadable to the human eye. For security reasons, you may not want to "
        "do that, as entries with passwords are clearly indicated as such.</p>");
}
