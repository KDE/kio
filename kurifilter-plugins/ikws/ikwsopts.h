/*
 * Copyright (c) 2000 Yves Arrouye <yves@realnames.com>
 * Copyright (c) 2002, 2003 Dawit Alemayehu <adawit@kde.org>
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

#ifndef IKWSOPTS_H
#define IKWSOPTS_H

#include <QLayout>
#include <QTabWidget>

#include <kcmodule.h>
#include <kservice.h>

#include "ui_ikwsopts_ui.h"

class SearchProvider;
class ProvidersModel;

class FilterOptions : public KCModule
{
    Q_OBJECT

public:
    explicit FilterOptions(const KAboutData* about, QWidget *parent = 0);

    void load();
    void save();
    void defaults();
    QString quickHelp() const;


private Q_SLOTS:
    void updateSearchProviderEditingButons();
    void addSearchProvider();
    void changeSearchProvider();
    void deleteSearchProvider();
    
private:
    void setDelimiter(char);
    char delimiter();
    void setDefaultEngine(int);

    // The names of the providers that the user deleted,
    // these are marked as deleted in the user's homedirectory
    // on save if a global service file exists for it.
    QStringList m_deletedProviders;
    ProvidersModel* m_providersModel;

    Ui::FilterOptionsUI m_dlg;
};

#endif
