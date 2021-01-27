/*
    SPDX-FileCopyrightText: 2000 Yves Arrouye <yves@realnames.com>
    SPDX-FileCopyrightText: 2001, 2002 Dawit Alemayehu <adawit@kde.org>
    SPDX-FileCopyrightText: 2009 Nick Shaforostoff <shaforostoff@kde.ru>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "ikwsopts.h"
#include "ikwsopts_p.h"

#include "kuriikwsfiltereng.h"
#include "searchprovider.h"
#include "searchproviderdlg.h"

#include <KBuildSycocaProgressDialog>
#include <KLocalizedString>
#include <KConfigGroup>
#include <KSharedConfig>

#include <QDBusConnection>
#include <QDBusMessage>
#include <QFile>
#include <QSortFilterProxyModel>

//BEGIN ProvidersModel

ProvidersModel::~ProvidersModel()
{
}

QVariant ProvidersModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    Q_UNUSED(orientation);
    if (role == Qt::DisplayRole) {
        switch (section) {
        case Name:
            return i18nc("@title:column Name label from web search keyword column", "Name");
        case Shortcuts:
            return i18nc("@title:column", "Keywords");
        case Preferred:
            return i18nc("@title:column", "Preferred");
        default:
            break;
        }
    }
    return QVariant();
}

Qt::ItemFlags ProvidersModel::flags(const QModelIndex &index) const
{
    if (!index.isValid()) {
        return Qt::ItemIsEnabled;
    }
    if (index.column() == Preferred) {
        return Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsUserCheckable;
    }
    return Qt::ItemIsEnabled | Qt::ItemIsSelectable;
}

bool ProvidersModel::setData(const QModelIndex &index, const QVariant &value, int role)
{
    if (role == Qt::CheckStateRole) {
        if (value.toInt() == Qt::Checked) {
            m_favoriteEngines.insert(m_providers.at(index.row())->desktopEntryName());
        } else {
            m_favoriteEngines.remove(m_providers.at(index.row())->desktopEntryName());
        }
        Q_EMIT dataModified();
        return true;
    }
    return false;
}

QVariant ProvidersModel::data(const QModelIndex &index, int role) const
{
    if (index.isValid()) {
        if (role == Qt::CheckStateRole && index.column() == Preferred) {
            return m_favoriteEngines.contains(m_providers.at(index.row())->desktopEntryName()) ? Qt::Checked : Qt::Unchecked;
        }

        if (role == Qt::DisplayRole) {
            if (index.column() == Name) {
                return m_providers.at(index.row())->name();
            }
            if (index.column() == Shortcuts) {
                return m_providers.at(index.row())->keys().join(QLatin1Char(','));
            }
        }

        if (role == Qt::ToolTipRole || role == Qt::WhatsThisRole) {
            if (index.column() == Preferred) {
                return xi18nc("@info:tooltip", "Check this box to select the highlighted web search keyword "
                                               "as preferred.<nl/>Preferred web search keywords are used in "
                                               "places where only a few select keywords can be shown "
                                               "at one time.");
            }
        }

        if (role == Qt::UserRole) {
            return index.row();//a nice way to bypass proxymodel
        }
    }

    return QVariant();
}

void ProvidersModel::setProviders(const QList<SearchProvider *> &providers, const QStringList &favoriteEngines)
{
    m_providers = providers;
    setFavoriteProviders(favoriteEngines);
}

void ProvidersModel::setFavoriteProviders(const QStringList &favoriteEngines)
{
    beginResetModel();

    m_favoriteEngines = QSet<QString>(favoriteEngines.begin(), favoriteEngines.end());

    endResetModel();
}

int ProvidersModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid()) {
        return 0;
    }
    return m_providers.size();
}

QAbstractListModel *ProvidersModel::createListModel()
{
    ProvidersListModel *pListModel = new ProvidersListModel(m_providers, this);
    connect(this, &QAbstractItemModel::modelAboutToBeReset, pListModel, &QAbstractItemModel::modelAboutToBeReset);
    connect(this, &QAbstractItemModel::modelReset, pListModel, &QAbstractItemModel::modelReset);
    connect(this, &QAbstractItemModel::dataChanged, pListModel, &ProvidersListModel::emitDataChanged);
    connect(this, &QAbstractItemModel::rowsAboutToBeInserted, pListModel, &ProvidersListModel::emitRowsAboutToBeInserted);
    connect(this, &QAbstractItemModel::rowsAboutToBeRemoved, pListModel, &ProvidersListModel::emitRowsAboutToBeRemoved);
    connect(this, &QAbstractItemModel::rowsInserted, pListModel, &ProvidersListModel::emitRowsInserted);
    connect(this, &QAbstractItemModel::rowsRemoved, pListModel, &ProvidersListModel::emitRowsRemoved);

    return pListModel;
}

void ProvidersModel::deleteProvider(SearchProvider *p)
{
    const int row = m_providers.indexOf(p);
    beginRemoveRows(QModelIndex(), row, row);
    m_favoriteEngines.remove(m_providers.takeAt(row)->desktopEntryName());
    endRemoveRows();
    Q_EMIT dataModified();
}

void ProvidersModel::addProvider(SearchProvider *p)
{
    beginInsertRows(QModelIndex(), m_providers.size(), m_providers.size());
    m_providers.append(p);
    endInsertRows();
    Q_EMIT dataModified();
}

void ProvidersModel::changeProvider(SearchProvider *p)
{
    const int row = m_providers.indexOf(p);
    Q_EMIT dataChanged(index(row, 0), index(row, ColumnCount-1));
    Q_EMIT dataModified();
}

QStringList ProvidersModel::favoriteEngines() const
{
    return QStringList(m_favoriteEngines.cbegin(), m_favoriteEngines.cend());
}

//END ProvidersModel

//BEGIN ProvidersListModel
ProvidersListModel::ProvidersListModel(QList<SearchProvider *> &providers, QObject *parent)
    : QAbstractListModel(parent)
    , m_providers(providers)
{
}

QVariant ProvidersListModel::data(const QModelIndex &index, int role) const
{
    if (index.isValid()) {
        if (role == Qt::DisplayRole) {
            if (index.row() == m_providers.size()) {
                return i18nc("@item:inlistbox No default web search keyword", "None");
            }
            return m_providers.at(index.row())->name();
        }

        if (role == ShortNameRole) {
            if (index.row() == m_providers.size()) {
                return QString();
            }
            return m_providers.at(index.row())->desktopEntryName();
        }
    }
    return QVariant();
}

int ProvidersListModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid()) {
        return 0;
    }
    return m_providers.size() + 1;
}

//END ProvidersListModel

static QSortFilterProxyModel *wrapInProxyModel(QAbstractItemModel *model)
{
    QSortFilterProxyModel *proxyModel = new QSortFilterProxyModel(model);
    proxyModel->setSourceModel(model);
    proxyModel->setDynamicSortFilter(true);
    proxyModel->setSortCaseSensitivity(Qt::CaseInsensitive);
    proxyModel->setFilterCaseSensitivity(Qt::CaseInsensitive);
    proxyModel->setFilterKeyColumn(-1);
    return proxyModel;
}

FilterOptions::FilterOptions(QWidget *parent)
    : KCModule(parent)
    , m_providersModel(new ProvidersModel(this))
{
    m_dlg.setupUi(this);

    QSortFilterProxyModel *searchProviderModel = wrapInProxyModel(m_providersModel);
    m_dlg.lvSearchProviders->setModel(searchProviderModel);
    m_dlg.cmbDefaultEngine->setModel(wrapInProxyModel(m_providersModel->createListModel()));

    // Connect all the signals/slots...
    connect(m_dlg.cbEnableShortcuts, &QAbstractButton::toggled, this, &FilterOptions::markAsChanged);
    connect(m_dlg.cbEnableShortcuts, &QAbstractButton::toggled, this, &FilterOptions::updateSearchProviderEditingButons);
    connect(m_dlg.cbUseSelectedShortcutsOnly, &QAbstractButton::toggled, this, &FilterOptions::markAsChanged);

    connect(m_providersModel, &ProvidersModel::dataModified, this, &FilterOptions::markAsChanged);
    connect(m_dlg.cmbDefaultEngine, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &FilterOptions::markAsChanged);
    connect(m_dlg.cmbDelimiter, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &FilterOptions::markAsChanged);

    connect(m_dlg.pbNew, &QAbstractButton::clicked, this, &FilterOptions::addSearchProvider);
    connect(m_dlg.pbDelete, &QAbstractButton::clicked, this, &FilterOptions::deleteSearchProvider);
    connect(m_dlg.pbChange, &QAbstractButton::clicked, this, &FilterOptions::changeSearchProvider);
    connect(m_dlg.lvSearchProviders->selectionModel(), &QItemSelectionModel::currentChanged,
            this, &FilterOptions::updateSearchProviderEditingButons);
    connect(m_dlg.lvSearchProviders, &QAbstractItemView::doubleClicked, this, &FilterOptions::changeSearchProvider);
    connect(m_dlg.searchLineEdit, &QLineEdit::textEdited, searchProviderModel, &QSortFilterProxyModel::setFilterFixedString);
}

QString FilterOptions::quickHelp() const
{
    return xi18nc("@info:whatsthis", "<para>In this module you can configure the web search keywords feature. "
                                     "Web search keywords allow you to quickly search or lookup words on "
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
    QSortFilterProxyModel *proxy = qobject_cast<QSortFilterProxyModel *>(m_dlg.cmbDefaultEngine->model());
    if (index == -1) {
        index = proxy->rowCount()-1;//"None" is the last
    }
    const QModelIndex modelIndex = proxy->mapFromSource(proxy->sourceModel()->index(index, 0));
    m_dlg.cmbDefaultEngine->setCurrentIndex(modelIndex.row());
    m_dlg.cmbDefaultEngine->view()->setCurrentIndex(modelIndex); //TODO: remove this when Qt bug is fixed
}

void FilterOptions::load()
{
    KConfig config(QString::fromUtf8(KURISearchFilterEngine::self()->name()) + QLatin1String("rc"), KConfig::NoGlobals);
    KConfigGroup group = config.group("General");

    const QString defaultSearchEngine = group.readEntry("DefaultWebShortcut");
    const QStringList favoriteEngines = group.readEntry("PreferredWebShortcuts", DEFAULT_PREFERRED_SEARCH_PROVIDERS);

    const QList<SearchProvider *> allProviders = m_registry.findAll();
    QList<SearchProvider *> providers;
    for (auto *provider : allProviders) {
        if (!provider->isHidden()) {
            providers << provider;
        }
    }

    int defaultProviderIndex = providers.size(); //default is "None", it is last in the list

    for (SearchProvider *provider : qAsConst(providers)) {
        if (defaultSearchEngine == provider->desktopEntryName()) {
            defaultProviderIndex = providers.indexOf(provider);
            break;
        }
    }

    m_providersModel->setProviders(providers, favoriteEngines);
    m_dlg.lvSearchProviders->setColumnWidth(0, 200);
    m_dlg.lvSearchProviders->resizeColumnToContents(1);
    m_dlg.lvSearchProviders->sortByColumn(0, Qt::AscendingOrder);
    m_dlg.cmbDefaultEngine->model()->sort(0, Qt::AscendingOrder);
    setDefaultEngine(defaultProviderIndex);

    m_dlg.cbEnableShortcuts->setChecked(group.readEntry("EnableWebShortcuts", true));
    m_dlg.cbUseSelectedShortcutsOnly->setChecked(group.readEntry("UsePreferredWebShortcutsOnly", false));

    const QString delimiter = group.readEntry("KeywordDelimiter", ":");
    setDelimiter(delimiter.at(0).toLatin1());
}

char FilterOptions::delimiter()
{
    const char delimiters[] = {':', ' '};
    return delimiters[m_dlg.cmbDelimiter->currentIndex()];
}

void FilterOptions::setDelimiter(char sep)
{
    m_dlg.cmbDelimiter->setCurrentIndex(sep == ' ');
}

void FilterOptions::save()
{
    KConfig config(QString::fromUtf8(KURISearchFilterEngine::self()->name()) + QLatin1String("rc"), KConfig::NoGlobals);

    KConfigGroup group = config.group("General");
    group.writeEntry("EnableWebShortcuts", m_dlg.cbEnableShortcuts->isChecked());
    group.writeEntry("KeywordDelimiter", QString(QLatin1Char(delimiter())));
    group.writeEntry("DefaultWebShortcut", m_dlg.cmbDefaultEngine->view()->currentIndex().data(ProvidersListModel::ShortNameRole));
    group.writeEntry("PreferredWebShortcuts", m_providersModel->favoriteEngines());
    group.writeEntry("UsePreferredWebShortcutsOnly", m_dlg.cbUseSelectedShortcutsOnly->isChecked());

    int changedProviderCount = 0;
    const QList<SearchProvider *> providers = m_providersModel->providers();
    const QString path = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) + QLatin1String("/kservices5/searchproviders/");

    for (SearchProvider *provider : providers) {
        if (!provider->isDirty()) {
            continue;
        }

        changedProviderCount++;

        KConfig _service(path + provider->desktopEntryName() + QLatin1String(".desktop"), KConfig::SimpleConfig);
        KConfigGroup service(&_service, "Desktop Entry");
        service.writeEntry("Type", "Service");
        service.writeEntry("X-KDE-ServiceTypes", "SearchProvider");
        service.writeEntry("Name", provider->name());
        service.writeEntry("Query", provider->query());
        service.writeEntry("Keys", provider->keys());
        service.writeEntry("Charset", provider->charset());
        service.writeEntry("Hidden", false); // we might be overwriting a hidden entry
    }

    const QStringList servicesDirs = QStandardPaths::locateAll(QStandardPaths::GenericDataLocation, QStringLiteral("kservices5/searchproviders/"), QStandardPaths::LocateDirectory);
    for (const QString &providerName : qAsConst(m_deletedProviders)) {
        QStringList matches;
        for (const QString &dir : servicesDirs) {
            QString current = dir + QLatin1Char('/') + providerName + QLatin1String(".desktop");
            if (QFile::exists(current)) {
                matches += current;
            }
        }

        // Shouldn't happen
        if (matches.isEmpty()) {
            continue;
        }

        changedProviderCount++;

        if (matches.size() == 1 && matches.first().startsWith(path)) {
            // If only the local copy existed, unlink it
            // TODO: error handling
            QFile::remove(matches.first());
            continue;
        }

        KConfig _service(path + providerName + QLatin1String(".desktop"), KConfig::SimpleConfig);
        KConfigGroup service(&_service, "Desktop Entry");
        service.writeEntry("Type", "Service");
        service.writeEntry("X-KDE-ServiceTypes", "SearchProvider");
        service.writeEntry("Hidden", true);
    }

    config.sync();

    Q_EMIT changed(false);

    // Update filters in running applications...
    QDBusMessage msg = QDBusMessage::createSignal(QStringLiteral("/"), QStringLiteral("org.kde.KUriFilterPlugin"), QStringLiteral("configure"));
    QDBusConnection::sessionBus().send(msg);

    // If the providers changed, tell sycoca to rebuild its database...
    if (changedProviderCount) {
        KBuildSycocaProgressDialog::rebuildKSycoca(this);
    }
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
    QList<SearchProvider *> providers = m_providersModel->providers();
    QPointer<SearchProviderDialog> dlg = new SearchProviderDialog(nullptr, providers, this);

    if (dlg->exec()) {
        m_providersModel->addProvider(dlg->provider());
        m_providersModel->changeProvider(dlg->provider());
    }
    delete dlg;
}

void FilterOptions::changeSearchProvider()
{
    QList<SearchProvider *> providers = m_providersModel->providers();
    SearchProvider *provider = providers.at(m_dlg.lvSearchProviders->currentIndex().data(Qt::UserRole).toInt());
    QPointer<SearchProviderDialog> dlg = new SearchProviderDialog(provider, providers, this);

    if (dlg->exec()) {
        m_providersModel->changeProvider(dlg->provider());
    }

    delete dlg;
}

void FilterOptions::deleteSearchProvider()
{
    SearchProvider *provider = m_providersModel->providers().at(m_dlg.lvSearchProviders->currentIndex().data(Qt::UserRole).toInt());
    m_deletedProviders.append(provider->desktopEntryName());
    m_providersModel->deleteProvider(provider);
}

void FilterOptions::updateSearchProviderEditingButons()
{
    const bool enable = (m_dlg.cbEnableShortcuts->isChecked()
                         && m_dlg.lvSearchProviders->currentIndex().isValid());
    m_dlg.pbChange->setEnabled(enable);
    m_dlg.pbDelete->setEnabled(enable);
}
