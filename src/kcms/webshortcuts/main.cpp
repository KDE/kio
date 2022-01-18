/*
    SPDX-FileCopyrightText: 2000 Yves Arrouye <yves@realnames.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

// Own
#include "main.h"

// Qt
#include <QTabWidget>
#include <QVBoxLayout>

// KDE
#include <KAboutData>
#include <KLocalizedString>
#include <KPluginFactory>
#include <KPluginMetaData>
#include <kurifilter.h>

K_PLUGIN_CLASS_WITH_JSON(KURIFilterModule, "webshortcuts.json")

KURIFilterModule::KURIFilterModule(QWidget *parent, const QVariantList &args)
    : KCModule(parent, args)
    , m_widget(nullptr)
{
    KAboutData *about = new KAboutData(QStringLiteral("kcm_webshortcuts"),
                                       i18n("Web Search Keywords"),
                                       QStringLiteral("0.1"),
                                       i18n("Configure enhanced browsing features"),
                                       KAboutLicense::GPL);
    setAboutData(about);

    KCModule::setButtons(KCModule::Buttons(KCModule::Default | KCModule::Apply | KCModule::Help));

    filter = KUriFilter::self();

    setQuickHelp(
        i18n("<h1>Enhanced Browsing</h1> In this module you can configure some enhanced browsing"
             " features of KDE. "
             "<h2>Web Search Keywords</h2>Web Search Keywords are a quick way of using Web search engines. For example, type \"duckduckgo:frobozz\""
             " or \"dd:frobozz\" and your web browser will do a search on DuckDuckGo for \"frobozz\"."
             " Even easier: just press Alt+F2 (if you have not"
             " changed this keyboard shortcut) and enter the shortcut in the Run Command dialog."));

    QVBoxLayout *layout = new QVBoxLayout(this);

    QMap<QString, KCModule *> helper;
    // Load the plugins. This saves a public method in KUriFilter just for this.

    const QVector<KPluginMetaData> plugins = KPluginMetaData::findPlugins(QStringLiteral("kf" QT_STRINGIFY(QT_VERSION_MAJOR) "/urifilters"));
    for (const KPluginMetaData &pluginMetaData : plugins) {
        if (auto factory = KPluginFactory::loadFactory(pluginMetaData).plugin) {
            KCModule *module = nullptr;
#if KIOWIDGETS_BUILD_DEPRECATED_SINCE(5, 82)
            KUriFilterPlugin *plugin = factory->create<KUriFilterPlugin>(nullptr);
            if (plugin) {
                module = plugin->configModule(this, nullptr);
            }
#endif
            if (!module) {
                module = factory->create<KCModule>(this);
            }
            if (module) {
                modules.append(module);
                helper.insert(module->windowTitle(), module);
                connect(module, qOverload<bool>(&KCModule::changed), this, qOverload<bool>(&KCModule::changed));
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
            m_widget->layout()->setContentsMargins(0, 0, 0, 0);
        }
    }

    if (m_widget) {
        layout->addWidget(m_widget);
    }
    setMinimumWidth(700);
}

void KURIFilterModule::load()
{
    static bool firstLoad = true;

    // Modules automatically call load() when first shown, but subsequent
    // calls need to be propagated to make the`Reset` button work
    if (firstLoad) {
        firstLoad = false;
        return;
    }

    for (KCModule *module : std::as_const(modules)) {
        module->load();
    }
}

void KURIFilterModule::save()
{
    for (KCModule *module : std::as_const(modules)) {
        module->save();
    }
}

void KURIFilterModule::defaults()
{
    for (KCModule *module : std::as_const(modules)) {
        module->defaults();
    }
}

KURIFilterModule::~KURIFilterModule()
{
    qDeleteAll(modules);
}

#include "main.moc"
