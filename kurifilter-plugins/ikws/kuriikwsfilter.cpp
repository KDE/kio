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

#include "kuriikwsfilter.h"
#include "kuriikwsfiltereng.h"
#include "searchprovider.h"
#include "ikwsopts.h"

#include <KPluginFactory>
#include <KLocalizedString>

#include <QtDBus/QtDBus>

#define QL1S(x)  QLatin1String(x)
#define QL1C(x)  QLatin1Char(x)

QLoggingCategory category("org.kde.kurlfilter-ikws");

/**
 * IMPORTANT: If you change anything here, please run the regression test
 * ../tests/kurifiltertest
 */

K_PLUGIN_FACTORY(KAutoWebSearchFactory, registerPlugin<KAutoWebSearch>();)
K_EXPORT_PLUGIN(KAutoWebSearchFactory("kcmkurifilt"))

KAutoWebSearch::KAutoWebSearch(QObject *parent, const QVariantList&)
               :KUriFilterPlugin( "kuriikwsfilter", parent )
{
  KLocalizedString::insertQtDomain("kurifilter");
  QDBusConnection::sessionBus().connect(QString(), "/", "org.kde.KUriFilterPlugin",
                                        "configure", this, SLOT(configure()));
}

KAutoWebSearch::~KAutoWebSearch()
{
}

void KAutoWebSearch::configure()
{
  qCDebug(category) << "Config reload requested...";
  KURISearchFilterEngine::self()->loadConfig();
}

void KAutoWebSearch::populateProvidersList(QList<KUriFilterSearchProvider*>& searchProviders,
                                           const KUriFilterData& data, bool allproviders) const
{
  QList<SearchProvider*> providers;
  KURISearchFilterEngine *filter = KURISearchFilterEngine::self();
  const QString searchTerm = filter->keywordDelimiter() + data.typedString();

  if (allproviders)
    providers = SearchProvider::findAll();
  else
  {
    // Start with the search engines marked as preferred...
    QStringList favEngines = filter->favoriteEngineList();
    if (favEngines.isEmpty())
      favEngines = data.alternateSearchProviders();

    // Get rid of duplicates...
    favEngines.removeDuplicates();
    
    // Sort the items...
    qStableSort(favEngines);

    // Add the search engine set as the default provider...
    const QString defaultEngine = filter->defaultSearchEngine();
    if (!defaultEngine.isEmpty()) {
        favEngines.removeAll(defaultEngine);
        favEngines.insert(0, defaultEngine);
    }

    QStringListIterator it (favEngines);
    while (it.hasNext())
    {
      SearchProvider *favProvider = SearchProvider::findByDesktopName(it.next());
      if (favProvider)
          providers << favProvider;
    }
  }

  for (int i = 0, count = providers.count(); i < count; ++i)
  {
      searchProviders << providers[i];
  }
}

bool KAutoWebSearch::filterUri( KUriFilterData &data ) const
{
  qCDebug(category) << data.typedString();

  KUriFilterData::SearchFilterOptions option = data.searchFilteringOptions();

  // Handle the flag to retrieve only preferred providers, no filtering...
  if (option & KUriFilterData::RetrievePreferredSearchProvidersOnly)
  {
    QList<KUriFilterSearchProvider*> searchProviders;
    populateProvidersList(searchProviders, data);
    if (searchProviders.isEmpty())
    {
      if (!(option & KUriFilterData::RetrieveSearchProvidersOnly))
      {
        setUriType(data, KUriFilterData::Error);
        setErrorMsg(data, i18n("No preferred search providers were found."));
        return false;
      }
    }
    else
    {
      setSearchProvider(data, QString(), data.typedString(), QL1C(KURISearchFilterEngine::self()->keywordDelimiter()));
      setSearchProviders(data, searchProviders);
      return true;
    }
  }

  if (option & KUriFilterData::RetrieveSearchProvidersOnly)
  {
    QList<KUriFilterSearchProvider*> searchProviders;
    populateProvidersList(searchProviders, data, true);
    if (searchProviders.isEmpty())
    {
      setUriType(data, KUriFilterData::Error);
      setErrorMsg(data, i18n("No search providers were found."));
      return false;
    }

    setSearchProvider(data, QString(), data.typedString(), QL1C(KURISearchFilterEngine::self()->keywordDelimiter()));
    setSearchProviders(data, searchProviders);
    return true;
  }

  if ( data.uriType() == KUriFilterData::Unknown && data.uri().password().isEmpty() )
  {
    KURISearchFilterEngine *filter = KURISearchFilterEngine::self();
    SearchProvider *provider = filter->autoWebSearchQuery( data.typedString(), data.alternateDefaultSearchProvider() );
    if( provider )
    {
      const QUrl result = filter->formatResult(provider->query(), provider->charset(),
                                                  QString(), data.typedString(), true);
      setFilteredUri(data, result);
      setUriType( data, KUriFilterData::NetProtocol );
      setSearchProvider(data, provider->name(), data.typedString(), QL1C(filter->keywordDelimiter()));

      QList<KUriFilterSearchProvider*> searchProviders;
      populateProvidersList(searchProviders, data);
      setSearchProviders(data, searchProviders);
      delete provider;
      return true;
    }
  }
  return false;
}

#include "kuriikwsfilter.moc"
