/*
 * Copyright (c) 2000 Yves Arrouye <yves@realnames.com>
 * Copyright (c) 2001, 2002 Dawit Alemayehu <adawit@kde.org>
 * Copyright (c) 2009 Nick Shaforostoff <shaforostoff@kde.ru>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#include "ikwsopts.h"
#include "ikwsopts_p.h"

#include "kuriikwsfiltereng.h"
#include "searchprovider.h"
#include "searchproviderdlg.h"

#include <KServiceTypeTrader>
#include <KBuildSycocaProgressDialog>
#include <klocalizedstring.h>
#include <kconfiggroup.h>
#include <ksharedconfig.h>

#include <QtCore/QFile>
#include <QtDBus/QtDBus>
#include <QSortFilterProxyModel>



//BEGIN ProvidersModel

ProvidersModel::~ProvidersModel()
{
  qDeleteAll(m_providers);
}

QVariant ProvidersModel::headerData(int section, Qt::Orientation orientation, int role ) const
{
  Q_UNUSED(orientation);
  if (role == Qt::DisplayRole)
  {
    switch (section) {
    case Name:
      return i18nc("@title:column Name label from web shortcuts column", "Name");
    case Shortcuts:
      return i18nc("@title:column", "Shortcuts");
    case Preferred:
      return i18nc("@title:column", "Preferred");
    default:
      break;
    }
  }
  return QVariant();
}

Qt::ItemFlags ProvidersModel::flags(const QModelIndex& index) const
{
  if (!index.isValid())
    return Qt::ItemIsEnabled;
  if (index.column()==Preferred)
    return Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsUserCheckable;
  return Qt::ItemIsEnabled | Qt::ItemIsSelectable;
}

bool ProvidersModel::setData (const QModelIndex& index, const QVariant& value, int role)
{
  if (role==Qt::CheckStateRole)
  {
    if (value.toInt() == Qt::Checked)
        m_favoriteEngines.insert(m_providers.at(index.row())->desktopEntryName());
    else
        m_favoriteEngines.remove(m_providers.at(index.row())->desktopEntryName());
    emit dataModified();
    return true;
  }
  return false;
}

QVariant ProvidersModel::data(const QModelIndex& index, int role) const
{
  if (index.isValid())
  {
    if (role == Qt::CheckStateRole && index.column()==Preferred)
      return (m_favoriteEngines.contains(m_providers.at(index.row())->desktopEntryName()) ? Qt::Checked : Qt::Unchecked);

    if (role == Qt::DisplayRole)
    {
      if (index.column()==Name)
        return m_providers.at(index.row())->name();
      if (index.column()==Shortcuts)
        return m_providers.at(index.row())->keys().join(",");
    }

    if (role == Qt::ToolTipRole || role == Qt::WhatsThisRole)
    {
      if (index.column() == Preferred)
        return i18nc("@info:tooltip", "Check this box to select the highlighted web shortcut "
                    "as preferred.<nl/>Preferred web shortcuts are used in "
                    "places where only a few select shortcuts can be shown "
                    "at one time.");
    }

    if (role == Qt::UserRole)
      return index.row();//a nice way to bypass proxymodel
  }

  return QVariant();
}

void ProvidersModel::setProviders(const QList<SearchProvider*>& providers, const QStringList& favoriteEngines)
{
  m_providers = providers;
  setFavoriteProviders(favoriteEngines);
}

void ProvidersModel::setFavoriteProviders(const QStringList& favoriteEngines)
{
  m_favoriteEngines = QSet<QString>::fromList(favoriteEngines);
  reset();
}

int ProvidersModel::rowCount(const QModelIndex & parent) const
{
  if (parent.isValid())
    return 0;
  return m_providers.size();
}

QAbstractListModel* ProvidersModel::createListModel()
{
  ProvidersListModel* pListModel = new ProvidersListModel(m_providers, this);
  connect(this, SIGNAL(modelAboutToBeReset()),        pListModel, SIGNAL(modelAboutToBeReset()));
  connect(this, SIGNAL(modelReset()),                 pListModel, SIGNAL(modelReset()));
  connect(this, SIGNAL(layoutAboutToBeChanged()),     pListModel, SIGNAL(modelReset()));
  connect(this, SIGNAL(layoutChanged()),              pListModel, SIGNAL(modelReset()));
  connect(this, SIGNAL(dataChanged(QModelIndex,QModelIndex)),       pListModel, SLOT(emitDataChanged(QModelIndex,QModelIndex)));
  connect(this, SIGNAL(rowsAboutToBeInserted(QModelIndex,int,int)), pListModel, SLOT(emitRowsAboutToBeInserted(QModelIndex,int,int)));
  connect(this, SIGNAL(rowsAboutToBeRemoved(QModelIndex,int,int)),  pListModel, SLOT(emitRowsAboutToBeRemoved(QModelIndex,int,int)));
  connect(this, SIGNAL(rowsInserted(QModelIndex,int,int)),          pListModel, SLOT(emitRowsInserted(QModelIndex,int,int)));
  connect(this, SIGNAL(rowsRemoved(QModelIndex,int,int)),           pListModel, SLOT(emitRowsRemoved(QModelIndex,int,int)));

  return pListModel;
}

void ProvidersModel::deleteProvider(SearchProvider* p)
{
  const int row = m_providers.indexOf(p);
  beginRemoveRows(QModelIndex(), row, row);
  m_favoriteEngines.remove(m_providers.takeAt(row)->desktopEntryName());
  endRemoveRows();
  delete p;
  emit dataModified();
}

void ProvidersModel::addProvider(SearchProvider* p)
{
  beginInsertRows(QModelIndex(), m_providers.size(), m_providers.size());
  m_providers.append(p);
  endInsertRows();
  emit dataModified();
}

void ProvidersModel::changeProvider(SearchProvider* p)
{
  const int row = m_providers.indexOf(p);
  emit dataChanged(index(row,0),index(row,ColumnCount-1));
  emit dataModified();
}

QStringList ProvidersModel::favoriteEngines() const
{
  return m_favoriteEngines.toList();
}
//END ProvidersModel

//BEGIN ProvidersListModel
ProvidersListModel::ProvidersListModel(QList<SearchProvider*>& providers,  QObject* parent)
    : QAbstractListModel(parent)
    , m_providers(providers)
{}

QVariant ProvidersListModel::data(const QModelIndex& index, int role) const
{
  if (index.isValid())
  {
    if (role==Qt::DisplayRole)
    {
      if (index.row() == m_providers.size())
        return i18nc("@item:inlistbox No default web shortcut", "None");
      return m_providers.at(index.row())->name();
    }

    if (role==ShortNameRole)
    {
      if (index.row() == m_providers.size())
        return QString();
      return m_providers.at(index.row())->desktopEntryName();
    }
  }
  return QVariant();
}

int ProvidersListModel::rowCount (const QModelIndex& parent) const
{
  if (parent.isValid())
    return 0;
  return m_providers.size() + 1;
}
//END ProvidersListModel

static QSortFilterProxyModel* wrapInProxyModel(QAbstractItemModel* model)
{
  QSortFilterProxyModel* proxyModel = new QSortFilterProxyModel(model);
  proxyModel->setSourceModel(model);
  proxyModel->setDynamicSortFilter(true);
  proxyModel->setSortCaseSensitivity(Qt::CaseInsensitive);
  proxyModel->setFilterCaseSensitivity(Qt::CaseInsensitive);
  proxyModel->setFilterKeyColumn(-1);
  return proxyModel;
}

FilterOptions::FilterOptions(const KAboutData* about, QWidget *parent)
              : KCModule(about, parent),
                m_providersModel(new ProvidersModel(this))
{
  m_dlg.setupUi(this);

  QSortFilterProxyModel* searchProviderModel = wrapInProxyModel(m_providersModel);
  m_dlg.lvSearchProviders->setModel(searchProviderModel);
  m_dlg.cmbDefaultEngine->setModel(wrapInProxyModel(m_providersModel->createListModel()));

  // Connect all the signals/slots...
  connect(m_dlg.cbEnableShortcuts, SIGNAL(toggled(bool)), SLOT(changed()));
  connect(m_dlg.cbEnableShortcuts, SIGNAL(toggled(bool)), SLOT(updateSearchProviderEditingButons()));
  connect(m_dlg.cbUseSelectedShortcutsOnly, SIGNAL(toggled(bool)), SLOT(changed()));

  connect(m_providersModel, SIGNAL(dataModified()), SLOT(changed()));
  connect(m_dlg.cmbDefaultEngine, SIGNAL(currentIndexChanged(int)),  SLOT(changed()));
  connect(m_dlg.cmbDelimiter,     SIGNAL(currentIndexChanged(int)),  SLOT(changed()));

  connect(m_dlg.pbNew,    SIGNAL(clicked()), SLOT(addSearchProvider()));
  connect(m_dlg.pbDelete, SIGNAL(clicked()), SLOT(deleteSearchProvider()));
  connect(m_dlg.pbChange, SIGNAL(clicked()), SLOT(changeSearchProvider()));
  connect(m_dlg.lvSearchProviders->selectionModel(),
           SIGNAL(currentChanged(QModelIndex,QModelIndex)),
           SLOT(updateSearchProviderEditingButons()));
  connect(m_dlg.lvSearchProviders, SIGNAL(doubleClicked(QModelIndex)),SLOT(changeSearchProvider()));
  connect(m_dlg.searchLineEdit, SIGNAL(textEdited(QString)), searchProviderModel, SLOT(setFilterFixedString(QString)));
}

QString FilterOptions::quickHelp() const
{
  return i18nc("@info:whatsthis", "<para>In this module you can configure the web shortcuts feature. "
              "Web shortcuts allow you to quickly search or lookup words on "
              "the Internet. For example, to search for information about the "
              "KDE project using the Google engine, you simply type <emphasis>gg:KDE</emphasis> "
              "or <emphasis>google:KDE</emphasis>.</para>"
              "<para>If you select a default search engine, then you can search for "
              "normal words or phrases by simply typing them into the input widget "
              "of applications that have built-in support for such a feature, e.g "
              "Konqueror.</para>");
}

void FilterOptions::setDefaultEngine(int index)
{
  QSortFilterProxyModel* proxy = qobject_cast<QSortFilterProxyModel*>(m_dlg.cmbDefaultEngine->model());
  if (index == -1)
    index = proxy->rowCount()-1;//"None" is the last

  const QModelIndex modelIndex = proxy->mapFromSource(proxy->sourceModel()->index(index,0));
  m_dlg.cmbDefaultEngine->setCurrentIndex(modelIndex.row());
  m_dlg.cmbDefaultEngine->view()->setCurrentIndex(modelIndex);  //TODO: remove this when Qt bug is fixed
}

void FilterOptions::load()
{
  KConfig config(KURISearchFilterEngine::self()->name() + "rc", KConfig::NoGlobals);
  KConfigGroup group = config.group("General");

  const QString defaultSearchEngine = group.readEntry("DefaultWebShortcut");
  const QStringList favoriteEngines = group.readEntry("PreferredWebShortcuts", DEFAULT_PREFERRED_SEARCH_PROVIDERS);

  QList<SearchProvider*> providers;
  const KService::List services = KServiceTypeTrader::self()->query("SearchProvider");
  int defaultProviderIndex = services.size(); //default is "None", it is last in the list

  Q_FOREACH(const KService::Ptr &service, services)
  {
    SearchProvider* provider = new SearchProvider(service);
    if (defaultSearchEngine == provider->desktopEntryName())
      defaultProviderIndex = providers.size();
    providers.append(provider);
  }

  m_providersModel->setProviders(providers, favoriteEngines);
  m_dlg.lvSearchProviders->setColumnWidth(0,200);
  m_dlg.lvSearchProviders->resizeColumnToContents(1);
  m_dlg.lvSearchProviders->sortByColumn(0,Qt::AscendingOrder);
  m_dlg.cmbDefaultEngine->model()->sort(0,Qt::AscendingOrder);
  setDefaultEngine(defaultProviderIndex);

  m_dlg.cbEnableShortcuts->setChecked(group.readEntry("EnableWebShortcuts", true));
  m_dlg.cbUseSelectedShortcutsOnly->setChecked(group.readEntry("UsePreferredWebShortcutsOnly", false));

  const QString delimiter = group.readEntry ("KeywordDelimiter", ":");
  setDelimiter(delimiter.at(0).toLatin1());
}

char FilterOptions::delimiter()
{
  const char delimiters[]={':',' '};
  return delimiters[m_dlg.cmbDelimiter->currentIndex()];
}

void FilterOptions::setDelimiter (char sep)
{
  m_dlg.cmbDelimiter->setCurrentIndex(sep==' ');
}

void FilterOptions::save()
{
  KConfig config(KURISearchFilterEngine::self()->name() + "rc", KConfig::NoGlobals );

  KConfigGroup group = config.group("General");
  group.writeEntry("EnableWebShortcuts", m_dlg.cbEnableShortcuts->isChecked());
  group.writeEntry("KeywordDelimiter", QString(QLatin1Char(delimiter())));
  group.writeEntry("DefaultWebShortcut", m_dlg.cmbDefaultEngine->view()->currentIndex().data(ProvidersListModel::ShortNameRole));
  group.writeEntry("PreferredWebShortcuts", m_providersModel->favoriteEngines());
  group.writeEntry("UsePreferredWebShortcutsOnly", m_dlg.cbUseSelectedShortcutsOnly->isChecked());

  int changedProviderCount = 0;
  QList<SearchProvider*> providers = m_providersModel->providers();
  const QString path = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) + "kservices5/searchproviders/";

  Q_FOREACH(SearchProvider* provider, providers)
  {
    if (!provider->isDirty())
      continue;

    changedProviderCount++;

    KConfig _service(path + provider->desktopEntryName() + ".desktop", KConfig::SimpleConfig );
    KConfigGroup service(&_service, "Desktop Entry");
    service.writeEntry("Type", "Service");
    service.writeEntry("ServiceTypes", "SearchProvider");
    service.writeEntry("Name", provider->name());
    service.writeEntry("Query", provider->query());
    service.writeEntry("Keys", provider->keys());
    service.writeEntry("Charset", provider->charset());
    service.writeEntry("Hidden", false); // we might be overwriting a hidden entry
  }
 
 const QStringList servicesDirs = QStandardPaths::locateAll(QStandardPaths::GenericDataLocation, "kservices5/searchproviders/", QStandardPaths::LocateDirectory);
  Q_FOREACH(const QString& providerName, m_deletedProviders)
  {
    QStringList matches;
    foreach(const QString& dir, servicesDirs) {
      QString current = dir + '/' + providerName + ".desktop";
      if(QFile::exists(current))
        matches += current;
    }

    // Shouldn't happen
    if (!matches.size())
      continue;

    changedProviderCount++;

    if (matches.size() == 1 && matches.first().startsWith(path))
    {
      // If only the local copy existed, unlink it
      // TODO: error handling
      QFile::remove(matches.first());
      continue;
    }

    KConfig _service(path + providerName + ".desktop", KConfig::SimpleConfig );
    KConfigGroup service(&_service,     "Desktop Entry");
    service.writeEntry("Type",          "Service");
    service.writeEntry("ServiceTypes",  "SearchProvider");
    service.writeEntry("Hidden",        true);
  }

  config.sync();

  emit changed(false);

  // Update filters in running applications...
  QDBusMessage msg = QDBusMessage::createSignal("/", "org.kde.KUriFilterPlugin", "configure");
  QDBusConnection::sessionBus().send(msg);

  // If the providers changed, tell sycoca to rebuild its database...
  if (changedProviderCount)
    KBuildSycocaProgressDialog::rebuildKSycoca(this);
}

void FilterOptions::defaults()
{
  m_dlg.cbEnableShortcuts->setChecked(true);
  m_dlg.cbUseSelectedShortcutsOnly->setChecked(false);
  m_providersModel->setFavoriteProviders(DEFAULT_PREFERRED_SEARCH_PROVIDERS);
  setDelimiter(':');
  setDefaultEngine(-1);
}

void FilterOptions::addSearchProvider()
{
  QList<SearchProvider*> providers = m_providersModel->providers();
  QPointer<SearchProviderDialog> dlg = new SearchProviderDialog(0, providers, this);

  if (dlg->exec()) {
    m_providersModel->addProvider(dlg->provider());
    m_providersModel->changeProvider(dlg->provider());
  }
  delete dlg;
}

void FilterOptions::changeSearchProvider()
{
  QList<SearchProvider*> providers = m_providersModel->providers();
  SearchProvider* provider = providers.at(m_dlg.lvSearchProviders->currentIndex().data(Qt::UserRole).toInt());
  QPointer<SearchProviderDialog> dlg = new SearchProviderDialog(provider, providers, this);

  if (dlg->exec())
    m_providersModel->changeProvider(dlg->provider());

  delete dlg;
}

void FilterOptions::deleteSearchProvider()
{
  SearchProvider* provider = m_providersModel->providers().at(m_dlg.lvSearchProviders->currentIndex().data(Qt::UserRole).toInt());
  m_deletedProviders.append(provider->desktopEntryName());
  m_providersModel->deleteProvider(provider);
}

void FilterOptions::updateSearchProviderEditingButons()
{
  const bool enable = (m_dlg.cbEnableShortcuts->isChecked() &&
                       m_dlg.lvSearchProviders->currentIndex().isValid());
  m_dlg.pbChange->setEnabled(enable);
  m_dlg.pbDelete->setEnabled(enable);
}

#include "ikwsopts.moc"

// kate: replace-tabs 1; indent-width 2;
