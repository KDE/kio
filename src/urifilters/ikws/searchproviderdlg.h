/*
 * Copyright (c) 2000 Malte Starostik <malte@kde.org>
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


#ifndef SEARCHPROVIDERDLG_H
#define SEARCHPROVIDERDLG_H

#include <qdialog.h>

#include "ui_searchproviderdlg_ui.h"

class QDialogButtonBox;
class SearchProvider;

class SearchProviderDialog : public QDialog
{
    Q_OBJECT

public:
    explicit SearchProviderDialog(SearchProvider *provider, QList<SearchProvider*> &providers, QWidget *parent = 0);

    SearchProvider *provider() { return m_provider; }

protected Q_SLOTS:
    void slotChanged();
    void shortcutsChanged(const QString& newShorthands);
    void pastePlaceholder();
    void slotAcceptClicked();

private:
    SearchProvider *m_provider;
    QList<SearchProvider*> m_providers; // The list of all search providers, used for checking for already assigned shortcuts.
    Ui::SearchProviderDlgUI m_dlg;
    QDialogButtonBox* m_buttons;
};

#endif
