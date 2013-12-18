/* This file is part of the KDE libraries
   Copyright (C) 1999 Torben Weis <weis@kde.org>
   Copyright (C) 2003 Waldo Bastian <bastian@kde.org>
   Copyright     2012 David Faure <faure@kde.org>

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public
   License version 2 as published by the Free Software Foundation.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public License
   along with this library; see the file COPYING.LIB.  If not, write to
   the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
   Boston, MA 02110-1301, USA.
*/

#include "kprotocolinfo.h"
#include "kprotocolinfo_p.h"
#include "kprotocolinfofactory_p.h"

#include <kconfig.h>
#include <ksharedconfig.h>
#include <kconfiggroup.h>
#include <QUrl>

//
// Internal functions:
//
KProtocolInfoPrivate::KProtocolInfoPrivate(const QString &path)
{
  KConfig sconfig(path);
  KConfigGroup config(&sconfig, "Protocol");

  m_name = config.readEntry("protocol");
  m_exec = config.readPathEntry("exec", QString());
  m_isSourceProtocol = config.readEntry("source", true);
  m_isHelperProtocol = config.readEntry("helper", false);
  m_supportsReading = config.readEntry("reading", false);
  m_supportsWriting = config.readEntry("writing", false);
  m_supportsMakeDir = config.readEntry("makedir", false);
  m_supportsDeleting = config.readEntry("deleting", false);
  m_supportsLinking = config.readEntry("linking", false);
  m_supportsMoving = config.readEntry("moving", false);
  m_supportsOpening = config.readEntry("opening", false);
  m_canCopyFromFile = config.readEntry("copyFromFile", false);
  m_canCopyToFile = config.readEntry("copyToFile", false);
  m_canRenameFromFile = config.readEntry("renameFromFile", false);
  m_canRenameToFile = config.readEntry("renameToFile", false);
  m_canDeleteRecursive = config.readEntry("deleteRecursive", false);
  const QString fnu = config.readEntry("fileNameUsedForCopying", "FromURL");
  m_fileNameUsedForCopying = KProtocolInfo::FromUrl;
  if (fnu == QLatin1String("Name"))
    m_fileNameUsedForCopying = KProtocolInfo::Name;
  else if (fnu == QLatin1String("DisplayName"))
    m_fileNameUsedForCopying = KProtocolInfo::DisplayName;

  m_listing = config.readEntry("listing", QStringList());
  // Many .protocol files say "Listing=false" when they really mean "Listing=" (i.e. unsupported)
  if (m_listing.count() == 1 && m_listing.first() == QLatin1String("false"))
    m_listing.clear();
  m_supportsListing = (m_listing.count() > 0);
  m_defaultMimetype = config.readEntry("defaultMimetype");
  m_determineMimetypeFromExtension = config.readEntry("determineMimetypeFromExtension", true);
  m_archiveMimeTypes = config.readEntry("archiveMimetype", QStringList());
  m_icon = config.readEntry("Icon");
  m_config = config.readEntry("config", m_name);
  m_maxSlaves = config.readEntry("maxInstances", 1);
  m_maxSlavesPerHost = config.readEntry("maxInstancesPerHost", 0);

  QString tmp = config.readEntry("input");
  if (tmp == QLatin1String("filesystem"))
    m_inputType = KProtocolInfo::T_FILESYSTEM;
  else if (tmp == QLatin1String("stream"))
    m_inputType = KProtocolInfo::T_STREAM;
  else
    m_inputType = KProtocolInfo::T_NONE;

  tmp = config.readEntry("output");
  if (tmp == QLatin1String("filesystem"))
    m_outputType = KProtocolInfo::T_FILESYSTEM;
  else if (tmp == QLatin1String("stream"))
    m_outputType = KProtocolInfo::T_STREAM;
  else
    m_outputType = KProtocolInfo::T_NONE;

  m_docPath = config.readPathEntry("X-DocPath", QString());
  if (m_docPath.isEmpty())
    m_docPath = config.readPathEntry("DocPath", QString());
  m_protClass = config.readEntry("Class").toLower();
  if (m_protClass[0] != QLatin1Char(':'))
     m_protClass.prepend(QLatin1Char(':'));

  const QStringList extraNames = config.readEntry("ExtraNames", QStringList());
  const QStringList extraTypes = config.readEntry("ExtraTypes", QStringList());
  QStringList::const_iterator it = extraNames.begin();
  QStringList::const_iterator typeit = extraTypes.begin();
  for(; it != extraNames.end() && typeit != extraTypes.end(); ++it, ++typeit) {
      QVariant::Type type = QVariant::nameToType((*typeit).toLatin1());
      // currently QVariant::Type and ExtraField::Type use the same subset of values, so we can just cast.
      m_extraFields.append(KProtocolInfo::ExtraField(*it, static_cast<KProtocolInfo::ExtraField::Type>(type)));
  }

  m_showPreviews = config.readEntry("ShowPreviews", m_protClass == QLatin1String(":local"));

  m_capabilities = config.readEntry("Capabilities", QStringList());
  m_proxyProtocol = config.readEntry("ProxiedBy");
}

//
// Static functions:
//

QStringList KProtocolInfo::protocols()
{
  return KProtocolInfoFactory::self()->protocols();
}

bool KProtocolInfo::isFilterProtocol(const QString& _protocol)
{
  // We call the findProtocol directly (not via KProtocolManager) to bypass any proxy settings.
  KProtocolInfoPrivate* prot = KProtocolInfoFactory::self()->findProtocol(_protocol);
  if (!prot)
    return false;

  return !prot->m_isSourceProtocol;
}

QString KProtocolInfo::icon(const QString& _protocol)
{
  // We call the findProtocol directly (not via KProtocolManager) to bypass any proxy settings.
  KProtocolInfoPrivate * prot = KProtocolInfoFactory::self()->findProtocol(_protocol);
  if (!prot)
    return QString();

  return prot->m_icon;
}

QString KProtocolInfo::config(const QString& _protocol)
{
  // We call the findProtocol directly (not via KProtocolManager) to bypass any proxy settings.
  KProtocolInfoPrivate * prot = KProtocolInfoFactory::self()->findProtocol(_protocol);
  if (!prot)
    return QString();

  return QString::fromLatin1("kio_%1rc").arg(prot->m_config);
}

int KProtocolInfo::maxSlaves(const QString& _protocol)
{
  KProtocolInfoPrivate * prot = KProtocolInfoFactory::self()->findProtocol(_protocol);
  if (!prot)
    return 1;

  return prot->m_maxSlaves;
}

int KProtocolInfo::maxSlavesPerHost(const QString& _protocol)
{
  KProtocolInfoPrivate * prot = KProtocolInfoFactory::self()->findProtocol(_protocol);
  if (!prot)
    return 0;

  return prot->m_maxSlavesPerHost;
}

bool KProtocolInfo::determineMimetypeFromExtension(const QString &_protocol)
{
  KProtocolInfoPrivate * prot = KProtocolInfoFactory::self()->findProtocol(_protocol);
  if (!prot)
    return true;

  return prot->m_determineMimetypeFromExtension;
}

QString KProtocolInfo::exec(const QString& protocol)
{
    KProtocolInfoPrivate * prot = KProtocolInfoFactory::self()->findProtocol(protocol);
    if (!prot)
        return QString();
    return prot->m_exec;
}

KProtocolInfo::ExtraFieldList KProtocolInfo::extraFields(const QUrl &url)
{
  KProtocolInfoPrivate * prot = KProtocolInfoFactory::self()->findProtocol(url.scheme());
  if (!prot)
    return ExtraFieldList();

  return prot->m_extraFields;
}

QString KProtocolInfo::docPath(const QString& _protocol)
{
  KProtocolInfoPrivate * prot = KProtocolInfoFactory::self()->findProtocol(_protocol);
  if (!prot)
    return QString();

  return prot->m_docPath;
}

QString KProtocolInfo::protocolClass(const QString& _protocol)
{
  KProtocolInfoPrivate * prot = KProtocolInfoFactory::self()->findProtocol(_protocol);
  if (!prot)
    return QString();

  return prot->m_protClass;
}

bool KProtocolInfo::showFilePreview(const QString& _protocol)
{
  KProtocolInfoPrivate * prot = KProtocolInfoFactory::self()->findProtocol(_protocol);
  const bool defaultSetting = prot ? prot->m_showPreviews : false;

  KConfigGroup group(KSharedConfig::openConfig(), "PreviewSettings");
  return group.readEntry(_protocol, defaultSetting);
}

QStringList KProtocolInfo::capabilities(const QString& _protocol)
{
  KProtocolInfoPrivate * prot = KProtocolInfoFactory::self()->findProtocol(_protocol);
  if (!prot)
    return QStringList();

  return prot->m_capabilities;
}

QString KProtocolInfo::proxiedBy(const QString& _protocol)
{
  KProtocolInfoPrivate * prot = KProtocolInfoFactory::self()->findProtocol(_protocol);
  if (!prot)
    return QString();

  return prot->m_proxyProtocol;
}

bool KProtocolInfo::isFilterProtocol(const QUrl &url)
{
    return isFilterProtocol(url.scheme());
}

bool KProtocolInfo::isHelperProtocol(const QUrl &url)
{
    return isHelperProtocol(url.scheme());
}

bool KProtocolInfo::isHelperProtocol(const QString &protocol)
{
  // We call the findProtocol directly (not via KProtocolManager) to bypass any proxy settings.
  KProtocolInfoPrivate * prot = KProtocolInfoFactory::self()->findProtocol(protocol);
  if (prot)
      return prot->m_isHelperProtocol;
  return false;
}

bool KProtocolInfo::isKnownProtocol(const QUrl &url)
{
    return isKnownProtocol(url.scheme());
}

bool KProtocolInfo::isKnownProtocol(const QString &protocol)
{
  // We call the findProtocol (const QString&) to bypass any proxy settings.
  KProtocolInfoPrivate * prot = KProtocolInfoFactory::self()->findProtocol(protocol);
  return prot;
}
