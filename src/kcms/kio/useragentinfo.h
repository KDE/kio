/*
    SPDX-FileCopyrightText: 2001 Dawit Alemayehu <adawit@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef USERAGENTINFO_H
#define USERAGENTINFO_H

#include <KService>

class QString;
class QStringList;

class UserAgentInfo
{
public:
  enum StatusCode {
    SUCCEEDED=0,
    ALREADY_EXISTS,
    DUPLICATE_ENTRY,
  };

  UserAgentInfo();
  ~UserAgentInfo(){}

  StatusCode createNewUAProvider( const QString& );
  QString aliasStr( const QString& );
  QString agentStr( const QString& );
  QStringList userAgentStringList();
  QStringList userAgentAliasList();
  bool isListDirty() const { return m_bIsDirty; }
  void setListDirty( bool dirty ) { m_bIsDirty = dirty; }

protected:
  void loadFromDesktopFiles();
  void parseDescription();

private:
  KService::List m_providers;
  QStringList m_lstIdentity;
  QStringList m_lstAlias;
  bool m_bIsDirty;
};

#endif // USERAGENTINFO_H
