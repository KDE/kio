/*
    Original Authors:
    SPDX-FileCopyrightText: 1997 Kalle Dalheimer <kalle@kde.org>
    SPDX-FileCopyrightText: 1998 David Faure <faure@kde.org>
    SPDX-FileCopyrightText: 2000 Dirk Mueller <mueller@kde.org>

    Completely re-written by:
    SPDX-FileCopyrightText: 2000 Dawit Alemayehu <adawit@kde.org>

    SPDX-License-Identifier: GPL-2.0-only
*/

// Own
#include "useragentdlg.h"

// Local
#include "ksaveioconfig.h"
#include "useragentinfo.h"
#include "useragentselectordlg.h"

// Qt
#include <QCheckBox>
#include <QPushButton>
#include <QTreeWidget>
#include <QLoggingCategory>

// KDE
#include <KConfig>
#include <KConfigGroup>
#include <KLocalizedString>
#include <KMessageBox>
#include <http_slave_defaults.h>
#include <KPluginFactory>

Q_DECLARE_LOGGING_CATEGORY(KIO_USERAGENTDLG)
Q_LOGGING_CATEGORY(KIO_USERAGENTDLG, "kf.configwidgets.cms.kf.kio.useragentdlg")

K_PLUGIN_FACTORY_DECLARATION (KioConfigFactory)

typedef QList<QTreeWidgetItem*> SiteList;
typedef SiteList::iterator SiteListIterator;

UserAgentDlg::UserAgentDlg (QWidget* parent, const QVariantList&)
    : KCModule (parent),
      m_userAgentInfo (nullptr),
      m_config (nullptr)
{
    ui.setupUi (this);
    ui.newButton->setIcon (QIcon::fromTheme(QStringLiteral("list-add")));
    ui.changeButton->setIcon (QIcon::fromTheme(QStringLiteral("edit-rename")));
    ui.deleteButton->setIcon (QIcon::fromTheme(QStringLiteral("list-remove")));
    ui.deleteAllButton->setIcon (QIcon::fromTheme(QStringLiteral("edit-delete")));

    connect(ui.newButton, &QAbstractButton::clicked, this, &UserAgentDlg::newSitePolicy);
    connect(ui.changeButton, &QAbstractButton::clicked,
            this, [this]() { changeSitePolicy(ui.sitePolicyTreeWidget->currentItem()); });
    connect(ui.deleteButton, &QAbstractButton::clicked, this, &UserAgentDlg::deleteSitePolicies);
    connect(ui.deleteAllButton, &QAbstractButton::clicked, this, &UserAgentDlg::deleteAllSitePolicies);

    connect(ui.sendUACheckBox, &QAbstractButton::clicked, this, [this]() { configChanged(); });
    connect(ui.osNameCheckBox, &QAbstractButton::clicked, this, &UserAgentDlg::changeDefaultUAModifiers);
    connect(ui.osVersionCheckBox, &QAbstractButton::clicked, this, &UserAgentDlg::changeDefaultUAModifiers);
    connect(ui.processorTypeCheckBox, &QAbstractButton::clicked, this, &UserAgentDlg::changeDefaultUAModifiers);
    connect(ui.languageCheckBox, &QAbstractButton::clicked, this, &UserAgentDlg::changeDefaultUAModifiers);

    connect(ui.sitePolicyTreeWidget, &QTreeWidget::itemSelectionChanged, this, &UserAgentDlg::updateButtons);
    connect(ui.sitePolicyTreeWidget, &QTreeWidget::itemDoubleClicked, this, &UserAgentDlg::changeSitePolicy);
}

UserAgentDlg::~UserAgentDlg()
{
    delete m_userAgentInfo;
    delete m_config;
}

void UserAgentDlg::newSitePolicy()
{
    const QPointer<UserAgentSelectorDlg> pdlg (new UserAgentSelectorDlg (m_userAgentInfo, this));
    pdlg->setWindowTitle(i18nc ("@title:window", "Add Identification"));

    if (pdlg->exec() == QDialog::Accepted && pdlg) {
        if (!handleDuplicate (pdlg->siteName(), pdlg->identity(), pdlg->alias())) {
            QTreeWidgetItem* item = new QTreeWidgetItem (ui.sitePolicyTreeWidget);
            item->setText (0, pdlg->siteName());
            item->setText (1, pdlg->identity());
            item->setText (2, pdlg->alias());
            ui.sitePolicyTreeWidget->setCurrentItem (item);
            configChanged();
        }
    }
    delete pdlg;
}

void UserAgentDlg::deleteSitePolicies()
{
    SiteList selectedItems = ui.sitePolicyTreeWidget->selectedItems();
    SiteListIterator endIt = selectedItems.end();

    for (SiteListIterator it = selectedItems.begin(); it != endIt; ++it)
        delete (*it);

    updateButtons();
    configChanged();
}

void UserAgentDlg::deleteAllSitePolicies()
{
    ui.sitePolicyTreeWidget->clear();
    updateButtons();
    configChanged();
}

void UserAgentDlg::changeSitePolicy(QTreeWidgetItem* item)
{
    if (item) {
        // Store the current site name...
        const QString currentSiteName = item->text (0);

        UserAgentSelectorDlg pdlg (m_userAgentInfo, this);
        pdlg.setWindowTitle (i18nc ("@title:window", "Modify Identification"));
        pdlg.setSiteName (currentSiteName);
        pdlg.setIdentity (item->text (1));

        if (pdlg.exec() == QDialog::Accepted) {
            if (pdlg.siteName() == currentSiteName ||
                    !handleDuplicate (pdlg.siteName(), pdlg.identity(), pdlg.alias())) {
                item->setText (0, pdlg.siteName());
                item->setText (1, pdlg.identity());
                item->setText (2, pdlg.alias());
                configChanged();
            }
        }
    }
}

void UserAgentDlg::changeDefaultUAModifiers()
{
    m_ua_keys = QLatin1Char(':'); // Make sure it's not empty

    if (ui.osNameCheckBox->isChecked())
        m_ua_keys += QLatin1Char('o');

    if (ui.osVersionCheckBox->isChecked())
        m_ua_keys += QLatin1Char('v');

    if (ui.processorTypeCheckBox->isChecked())
        m_ua_keys += QLatin1Char('m');

    if (ui.languageCheckBox->isChecked())
        m_ua_keys += QLatin1Char('l');

    ui.osVersionCheckBox->setEnabled(m_ua_keys.contains(QLatin1Char('o')));

    QString modVal = KProtocolManager::defaultUserAgent (m_ua_keys);
    if (ui.defaultIdLineEdit->text() != modVal) {
        ui.defaultIdLineEdit->setText (modVal);
        configChanged();
    }
}

bool UserAgentDlg::handleDuplicate (const QString& site,
                                    const QString& identity,
                                    const QString& alias)
{
    SiteList list = ui.sitePolicyTreeWidget->findItems (site, Qt::MatchExactly, 0);

    if (!list.isEmpty()) {
        QString msg = i18n ("<qt><center>Found an existing identification for"
                            "<br/><b>%1</b><br/>"
                            "Do you want to replace it?</center>"
                            "</qt>", site);
        int res = KMessageBox::warningContinueCancel (this, msg,
                  i18nc ("@title:window", "Duplicate Identification"),
                  KGuiItem (i18n ("Replace")));
        if (res == KMessageBox::Continue) {
            list[0]->setText (0, site);
            list[0]->setText (1, identity);
            list[0]->setText (2, alias);
            configChanged();
        }

        return true;
    }

    return false;
}

void UserAgentDlg::configChanged (bool enable)
{
    Q_EMIT changed (enable);
}

void UserAgentDlg::updateButtons()
{
    const int selectedItemCount = ui.sitePolicyTreeWidget->selectedItems().count();
    const bool hasItems = ui.sitePolicyTreeWidget->topLevelItemCount() > 0;

    ui.changeButton->setEnabled ( (hasItems && selectedItemCount == 1));
    ui.deleteButton->setEnabled ( (hasItems && selectedItemCount > 0));
    ui.deleteAllButton->setEnabled (hasItems);
}

void UserAgentDlg::load()
{
    ui.sitePolicyTreeWidget->clear();

    if (!m_config)
        m_config = new KConfig (QStringLiteral("kio_httprc"), KConfig::NoGlobals);
    else
        m_config->reparseConfiguration();

    if (!m_userAgentInfo)
        m_userAgentInfo = new UserAgentInfo();

    const QStringList list = m_config->groupList();
    QStringList::ConstIterator endIt = list.end();
    QString agentStr;

    for (QStringList::ConstIterator it = list.begin(); it != endIt; ++it) {
        if ( (*it) == QLatin1String("<default>"))
            continue;

        KConfigGroup cg (m_config, *it);
        agentStr = cg.readEntry ("UserAgent");
        if (!agentStr.isEmpty()) {
            QTreeWidgetItem* item = new QTreeWidgetItem (ui.sitePolicyTreeWidget);
            item->setText (0, (*it).toLower());
            item->setText (1, m_userAgentInfo->aliasStr (agentStr));
            item->setText (2, agentStr);
        }
    }

    // Update buttons and checkboxes...
    KConfigGroup cg2 (m_config, QString());
    bool b = cg2.readEntry ("SendUserAgent", true);
    ui.sendUACheckBox->setChecked (b);
    m_ua_keys = cg2.readEntry ("UserAgentKeys", DEFAULT_USER_AGENT_KEYS).toLower();
    ui.defaultIdLineEdit->setText (KProtocolManager::defaultUserAgent (m_ua_keys));
    ui.osNameCheckBox->setChecked (m_ua_keys.contains(QLatin1Char('o')));
    ui.osVersionCheckBox->setChecked (m_ua_keys.contains(QLatin1Char('v')));
    ui.processorTypeCheckBox->setChecked (m_ua_keys.contains(QLatin1Char('m')));
    ui.languageCheckBox->setChecked (m_ua_keys.contains(QLatin1Char('l')));

    updateButtons();
    configChanged (false);
}

void UserAgentDlg::defaults()
{
    ui.sitePolicyTreeWidget->clear();
    m_ua_keys = QStringLiteral(DEFAULT_USER_AGENT_KEYS);
    ui.defaultIdLineEdit->setText (KProtocolManager::defaultUserAgent (m_ua_keys));
    ui.osNameCheckBox->setChecked (m_ua_keys.contains (QLatin1Char('o')));
    ui.osVersionCheckBox->setChecked (m_ua_keys.contains (QLatin1Char('v')));
    ui.processorTypeCheckBox->setChecked (m_ua_keys.contains (QLatin1Char('m')));
    ui.languageCheckBox->setChecked (m_ua_keys.contains(QLatin1Char('l')));
    ui.sendUACheckBox->setChecked (true);

    updateButtons();
    configChanged();
}

void UserAgentDlg::save()
{
    Q_ASSERT (m_config);

    // Put all the groups except the default into the delete list.
    QStringList deleteList = m_config->groupList();

    //Remove all the groups that DO NOT contain a "UserAgent" entry...
    QStringList::ConstIterator endIt = deleteList.constEnd();
    for (QStringList::ConstIterator it = deleteList.constBegin(); it != endIt; ++it) {
        if ( (*it) == QLatin1String ("<default>"))
            continue;

        KConfigGroup cg (m_config, *it);
        if (!cg.hasKey ("UserAgent"))
            deleteList.removeAll (*it);
    }

    QString domain;
    QTreeWidgetItem* item;
    int itemCount = ui.sitePolicyTreeWidget->topLevelItemCount();

    // Save and remove from the delete list all the groups that were
    // not deleted by the end user.
    for (int i = 0; i < itemCount; i++) {
        item = ui.sitePolicyTreeWidget->topLevelItem (i);
        domain = item->text (0);
        KConfigGroup cg (m_config, domain);
        cg.writeEntry ("UserAgent", item->text (2));
        deleteList.removeAll (domain);
        qCDebug (KIO_USERAGENTDLG, "UserAgentDlg::save: Removed [%s] from delete list", domain.toLatin1().constData());
    }

    // Write the global configuration information...
    KConfigGroup cg (m_config, QString());
    cg.writeEntry ("SendUserAgent", ui.sendUACheckBox->isChecked());
    cg.writeEntry ("UserAgentKeys", m_ua_keys);

    // Sync up all the changes so far...
    m_config->sync();

    // If delete list is not empty, delete the specified domains.
    if (!deleteList.isEmpty()) {
        // Remove entries from local file.
        endIt = deleteList.constEnd();
        KConfig cfg (QStringLiteral("kio_httprc"), KConfig::SimpleConfig);

        for (QStringList::ConstIterator it = deleteList.constBegin(); it != endIt; ++it) {
            KConfigGroup cg (&cfg, *it);
            cg.deleteEntry ("UserAgent");
            qCDebug (KIO_USERAGENTDLG, "UserAgentDlg::save: Deleting UserAgent of group [%s]", (*it).toLatin1().constData());
            if (cg.keyList().count() < 1)
                cg.deleteGroup();
        }

        // Sync up the configuration...
        cfg.sync();

        // Check everything is gone, reset to blank otherwise.
        m_config->reparseConfiguration();
        endIt = deleteList.constEnd();
        for (QStringList::ConstIterator it = deleteList.constBegin(); it != endIt; ++it) {
            KConfigGroup cg (m_config, *it);
            if (cg.hasKey ("UserAgent"))
                cg.writeEntry ("UserAgent", QString());
        }

        // Sync up the configuration...
        m_config->sync();
    }

    KSaveIOConfig::updateRunningIOSlaves (this);
    configChanged (false);
}

QString UserAgentDlg::quickHelp() const
{
    return i18n ("<h1>Browser Identification</h1>"
                 "<p>The browser-identification module allows you to have "
                 "full control over how KDE applications using the HTTP "
                 "protocol (like Konqueror) will identify itself to web sites "
                 "you browse.</p>"
                 "<p>This ability to fake identification is necessary because "
                 "some web sites do not display properly when they detect that "
                 "they are not talking to current versions of either Netscape "
                 "Navigator or Internet Explorer, even if the browser actually "
                 "supports all the necessary features to render those pages "
                 "properly. "
                 "For such sites, you can use this feature to try to browse "
                 "them. Please understand that this might not always work, since "
                 "such sites might be using non-standard web protocols and or "
                 "specifications.</p>"
                 "<p><u>NOTE:</u> To obtain specific help on a particular section "
                 "of the dialog box, simply click on the quick help button on "
                 "the window title bar, then click on the section "
                 "for which you are seeking help.</p>");
}


