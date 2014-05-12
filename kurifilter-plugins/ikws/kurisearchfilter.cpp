/*  This file is part of the KDE project
    Copyright (C) 1999 Simon Hausmann <hausmann@kde.org>
    Copyright (C) 2000 Yves Arrouye <yves@realnames.com>
    Copyright (C) 2002, 2003 Dawit Alemayehu <adawit@kde.org>

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.

*/

#include "kurisearchfilter.h"
#include "kuriikwsfiltereng.h"
#include "searchprovider.h"
#include "ikwsopts.h"

#include <QtDBus/QtDBus>
#include <klocalizedstring.h>
#include <kaboutdata.h>

/**
 * IMPORTANT: If you change anything here, please run the regression test
 * ../tests/kurifiltertest
 */

K_PLUGIN_FACTORY(KUriSearchFilterFactory, registerPlugin<KUriSearchFilter>();)
K_EXPORT_PLUGIN(KUriSearchFilterFactory("kcmkurifilt"))

namespace {
QLoggingCategory category("org.kde.kurifilter-ikws");
}

KUriSearchFilter::KUriSearchFilter(QObject *parent, const QVariantList &)
                 :KUriFilterPlugin( "kurisearchfilter", parent )
{
  KLocalizedString::insertQtDomain("kurifilter");
  QDBusConnection::sessionBus().connect(QString(), "/", "org.kde.KUriFilterPlugin",
                                        "configure", this, SLOT(configure()));
}

KUriSearchFilter::~KUriSearchFilter()
{
}

void KUriSearchFilter::configure()
{
  qCDebug(category) << "Config reload requested...";
  KURISearchFilterEngine::self()->loadConfig();
}

bool KUriSearchFilter::filterUri( KUriFilterData &data ) const
{
  qCDebug(category) << data.typedString() << ":" << data.uri() << ", type =" << data.uriType();


  // some URLs like gg:www.kde.org are not accepted by QUrl, but we still want them
  // This means we also have to allow KUriFilterData::Error
  if (data.uriType() != KUriFilterData::Unknown && data.uriType() != KUriFilterData::Error) {
      return false;
  }

  QString searchTerm;
  KURISearchFilterEngine *filter = KURISearchFilterEngine::self();
  QScopedPointer<SearchProvider> provider(filter->webShortcutQuery(data.typedString(), searchTerm));
  if (!provider) {
    return false;
  }

  const QUrl result = filter->formatResult(provider->query(), provider->charset(), QString(), searchTerm, true );
  setFilteredUri(data, result);
  setUriType( data, KUriFilterData::NetProtocol );
  setSearchProvider( data, provider->name(), searchTerm,  QLatin1Char(filter->keywordDelimiter()));
  return true;
}

KCModule *KUriSearchFilter::configModule(QWidget *parent, const char *) const
{
  return new FilterOptions( KAboutData::pluginData("kcmkurifilt"), parent);
}

QString KUriSearchFilter::configName() const
{
  return i18n("Search F&ilters");
}

#include "kurisearchfilter.moc"
