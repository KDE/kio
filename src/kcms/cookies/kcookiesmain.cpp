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

K_PLUGIN_CLASS_WITH_JSON(KCookiesMain, "cookies.json")

KCookiesMain::KCookiesMain(QWidget *parent, const QVariantList &args)
    : KCModule(parent, args)
{
    management = nullptr;
    bool managerOK = true;

    QVBoxLayout *layout = new QVBoxLayout(this);
    tab = new QTabWidget(this);
    layout->addWidget(tab);

    policies = new KCookiesPolicies(this, args);
    tab->addTab(policies, i18n("&Policy"));
    connect(policies, qOverload<bool>(&KCModule::changed), this, qOverload<bool>(&KCModule::changed));

    if (managerOK) {
        management = new KCookiesManagement(this, args);
        tab->addTab(management, i18n("&Management"));
        connect(management, qOverload<bool>(&KCModule::changed), this, qOverload<bool>(&KCModule::changed));
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
    KCModule *module = static_cast<KCModule *>(tab->currentWidget());

    if (module == policies) {
        policies->defaults();
    } else if (management) {
        management->defaults();
    }
}

QString KCookiesMain::quickHelp() const
{
    return i18n(
        "<h1>Cookies</h1><p>Cookies contain information that KDE applications"
        " using the HTTP protocol (like Konqueror) store on your"
        " computer, initiated by a remote Internet server. This means that"
        " a web server can store information about you and your browsing activities"
        " on your machine for later use. You might consider this an invasion of"
        " privacy.</p><p> However, cookies are useful in certain situations. For example, they"
        " are often used by Internet shops, so you can 'put things into a shopping basket'."
        " Some sites require you have a browser that supports cookies.</p><p>"
        " Because most people want a compromise between privacy and the benefits cookies offer,"
        " the HTTP KIO worker offers you the ability to customize the way it handles cookies. So you might want"
        " to set the default policy to ask you whenever a server wants to set a cookie,"
        " allowing you to decide. For your favorite shopping web sites that you trust, you might"
        " want to set the policy to accept, then you can access the web sites without being prompted"
        " every time a cookie is received.</p>");
}

#include "kcookiesmain.moc"
#include "moc_kcookiesmain.cpp"
