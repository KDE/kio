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

K_PLUGIN_CLASS_WITH_JSON(KURIFilterModule, "kcm_webshortcuts.json")

KURIFilterModule::KURIFilterModule(QObject *parent, const KPluginMetaData &data, const QVariantList &args)
    : KCModule(parent, data, args)
    , m_widget(nullptr)
{
    KCModule::setButtons(KCModule::Buttons(KCModule::Default | KCModule::Apply | KCModule::Help));

    filter = KUriFilter::self();

    QMap<QString, KCModule *> helper;
    // Load the plugins. This saves a public method in KUriFilter just for this.

    const QList<KPluginMetaData> plugins = KPluginMetaData::findPlugins(QStringLiteral("kf6/urifilters"));
    for (const KPluginMetaData &pluginMetaData : plugins) {
        if (auto factory = KPluginFactory::loadFactory(pluginMetaData).plugin) {
            if (KCModule *module = factory->create<KCModule>(widget())) {
                modules.append(module);
                helper.insert(module->name(), module);
                connect(module, &KCModule::needsSaveChanged, this, [module, this]() {
                    setNeedsSave(module->needsSave());
                });
            }
        }
    }

    if (modules.count() > 1) {
        QTabWidget *tab = new QTabWidget(widget());

        QMap<QString, KCModule *>::iterator it2;
        for (it2 = helper.begin(); it2 != helper.end(); ++it2) {
            tab->addTab(it2.value()->widget(), it2.key());
        }

        tab->setCurrentIndex(tab->indexOf(modules.first()->widget()));
        m_widget = tab;
    } else if (modules.count() == 1) {
        m_widget = modules.first()->widget();
        if (m_widget->layout()) {
            m_widget->layout()->setContentsMargins(0, 0, 0, 0);
        }
    }

    widget()->setMinimumWidth(700);
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
}

#include "main.moc"
