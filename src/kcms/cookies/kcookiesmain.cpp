/*
    kcookiesmain.cpp - Cookies configuration

    First version of cookies configuration:
        SPDX-FileCopyrightText: Waldo Bastian <bastian@kde.org>
    This dialog box:
        SPDX-FileCopyrightText: David Faure <faure@kde.org>
*/

// Own
#include "kcookiesmain.h"

// Local
#include "kcookiesmanagement.h"
#include "kcookiespolicies.h"

// Qt
#include <QTabWidget>

// KDE
#include <KLocalizedString>
#include <KPluginFactory>

K_PLUGIN_CLASS_WITH_JSON(KCookiesMain, "kcm_cookies.json")

KCookiesMain::KCookiesMain(QObject *parent, const KPluginMetaData &data, const QVariantList &args)
    : KCModule(parent, data, args)
{
    management = nullptr;
    bool managerOK = true;

    QVBoxLayout *layout = new QVBoxLayout(widget());
    tab = new QTabWidget(widget());
    layout->addWidget(tab);

    policies = new KCookiesPolicies(widget(), data, args);
    tab->addTab(policies->widget(), i18n("&Policy"));
    connect(policies, &KCModule::needsSaveChanged, this, [this]() {
        setNeedsSave(policies->needsSave());
    });

    if (managerOK) {
        management = new KCookiesManagement(widget(), data, args);
        tab->addTab(management->widget(), i18n("&Management"));
        connect(management, &KCModule::needsSaveChanged, this, [this]() {
            setNeedsSave(management->needsSave());
        });
    }
}

KCookiesMain::~KCookiesMain()
{
}

void KCookiesMain::save()
{
    policies->save();
    if (management) {
        management->save();
    }
}

void KCookiesMain::load()
{
    policies->load();
    if (management) {
        management->load();
    }
}

void KCookiesMain::defaults()
{
    QWidget *current = tab->currentWidget();
    if (current == policies->widget()) {
        policies->defaults();
    } else if (management && current == management->widget()) {
        management->defaults();
    }
}

#include "kcookiesmain.moc"
