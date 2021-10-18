/*
    SPDX-FileCopyrightText: 2000 Yves Arrouye <yves@realnames.com>
    SPDX-FileCopyrightText: 2002, 2003 Dawit Alemayehu <adawit@kde.org>
    SPDX-FileCopyrightText: 2009 Nick Shaforostoff <shaforostoff@kde.ru>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef IKWSOPTS_H
#define IKWSOPTS_H

#include <QLayout>
#include <QTabWidget>

#include <KCModule>

#include "searchproviderregistry.h"
#include "ui_ikwsopts_ui.h"

class SearchProvider;
class ProvidersModel;
class WebShortcutsSettings;

class FilterOptions : public KCModule
{
    Q_OBJECT

public:
    explicit FilterOptions(QWidget *parent = nullptr, const QVariantList &args = {});

    void load() override;
    void save() override;
    void defaults() override;
    QString quickHelp() const override;

private Q_SLOTS:
    void updateSearchProviderEditingButons();
    void addSearchProvider();
    void changeSearchProvider();
    void deleteSearchProvider();
    void updateUnmanagedState();

private:
    void setDelimiter(char);
    char delimiter();
    void setDefaultEngine(const QString &engine);

private:
    // The names of the providers that the user deleted,
    // these are marked as deleted in the user's homedirectory
    // on save if a global service file exists for it.
    QStringList m_deletedProviders;

    ProvidersModel *m_providersModel;
    SearchProviderRegistry m_registry;
    WebShortcutsSettings *m_settings;

    Ui::FilterOptionsUI m_dlg;
};

#endif
