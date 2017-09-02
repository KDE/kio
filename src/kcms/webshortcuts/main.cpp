/*
 *  Copyright (c) 2000 Yves Arrouye <yves@realnames.com>
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

// Own
#include "main.h"

// Qt
#include <QTabWidget>
#include <QBoxLayout>

// KDE
#include <kurifilter.h>
#include <KAboutData>
#include <KLocalizedString>
#include <KPluginMetaData>
#include <KPluginFactory>
#include <KPluginLoader>

K_PLUGIN_FACTORY(KURIFilterModuleFactory, registerPlugin<KURIFilterModule>();)

KURIFilterModule::KURIFilterModule(QWidget *parent, const QVariantList &args)
    : KCModule(parent, args),
      m_widget(nullptr)
{
    KAboutData *about = new KAboutData(QStringLiteral("kcm_webshortcuts"), i18n("Web Shortcuts"),
                                       QStringLiteral("0.1"), i18n("Configure enhanced browsing features"),
                                       KAboutLicense::GPL);
    setAboutData(about);

    KCModule::setButtons(KCModule::Buttons(KCModule::Default | KCModule::Apply | KCModule::Help));

    filter = KUriFilter::self();

    setQuickHelp(i18n("<h1>Enhanced Browsing</h1> In this module you can configure some enhanced browsing"
                      " features of KDE. "
                      "<h2>Web Shortcuts</h2>Web Shortcuts are a quick way of using Web search engines. For example, type \"duckduckgo:frobozz\""
                      " or \"dd:frobozz\" and your web browser will do a search on DuckDuckGo for \"frobozz\"."
                      " Even easier: just press Alt+F2 (if you have not"
                      " changed this keyboard shortcut) and enter the shortcut in the Run Command dialog."));

    QVBoxLayout *layout = new QVBoxLayout(this);

    QMap<QString, KCModule *> helper;
    // Load the plugins. This saves a public method in KUriFilter just for this.

    QVector<KPluginMetaData> plugins = KPluginLoader::findPlugins("kf5/urifilters");
    for (const KPluginMetaData &pluginMetaData : plugins) {
        KPluginFactory *factory = qobject_cast<KPluginFactory *>(pluginMetaData.instantiate());
        if (factory) {
            KUriFilterPlugin *plugin = factory->create<KUriFilterPlugin>(nullptr);
            if (plugin) {
                KCModule *module = plugin->configModule(this, nullptr);
                if (module) {
                    modules.append(module);
                    helper.insert(plugin->configName(), module);
                    connect(module, SIGNAL(changed(bool)), SIGNAL(changed(bool)));
                }
            }
        }
    }

    if (modules.count() > 1) {
        QTabWidget *tab = new QTabWidget(this);

        QMap<QString, KCModule *>::iterator it2;
        for (it2 = helper.begin(); it2 != helper.end(); ++it2) {
            tab->addTab(it2.value(), it2.key());
        }

        tab->setCurrentIndex(tab->indexOf(modules.first()));
        m_widget = tab;
    } else if (modules.count() == 1) {
        m_widget = modules.first();
        if (m_widget->layout()) {
            m_widget->layout()->setMargin(0);
        }
    }

    if (m_widget) {
        layout->addWidget(m_widget);
    }
}

void KURIFilterModule::load()
{
// seems not to be necessary, since modules automatically call load() on show (uwolfer)
//     foreach( KCModule* module, modules )
//     {
//    module->load();
//     }
}

void KURIFilterModule::save()
{
    foreach(KCModule * module, modules) {
        module->save();
    }
}

void KURIFilterModule::defaults()
{
    foreach(KCModule * module, modules) {
        module->defaults();
    }
}

KURIFilterModule::~KURIFilterModule()
{
    qDeleteAll(modules);
}

#include "main.moc"
