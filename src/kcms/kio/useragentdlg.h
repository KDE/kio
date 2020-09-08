/*
    Original Authors:
    SPDX-FileCopyrightText: 1997 Kalle Dalheimer <kalle@kde.org>
    SPDX-FileCopyrightText: 1998 David Faure <faure@kde.org>
    SPDX-FileCopyrightText: 2000 Dirk Mueller <mueller@kde.org>

    Completely re-written by:
    SPDX-FileCopyrightText: 2000 Dawit Alemayehu <adawit@kde.org>

    SPDX-License-Identifier: GPL-2.0-only
*/

#ifndef _USERAGENTDLG_H
#define _USERAGENTDLG_H

#include <KCModule>
#include "ui_useragentdlg.h"

class KConfig;
class UserAgentInfo;
class QTreeWidgetItem;

class UserAgentDlg : public KCModule
{
  Q_OBJECT

public:
  UserAgentDlg(QWidget *parent, const QVariantList &args);
  ~UserAgentDlg();

  void load() override;
  void save() override;
  void defaults() override;
  QString quickHelp() const override;

private Q_SLOTS:
  void updateButtons();

  void newSitePolicy();
  void changeSitePolicy(QTreeWidgetItem*);
  void deleteSitePolicies();
  void deleteAllSitePolicies();

private:
  void changeDefaultUAModifiers();
  void configChanged(bool enable = true);
  bool handleDuplicate( const QString&, const QString&, const QString& );

  enum
  {
    SHOW_OS = 0,
    SHOW_OS_VERSION,
    SHOW_PLATFORM,
    SHOW_MACHINE,
    SHOW_LANGUAGE
  };

  // Useragent modifiers...
  QString m_ua_keys;

  // Fake user-agent modifiers...
  UserAgentInfo* m_userAgentInfo;
  KConfig *m_config;

  // Interface...
  Ui::UserAgentUI ui;
};

#endif
