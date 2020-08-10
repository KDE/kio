/*
    SPDX-FileCopyrightText: 2000 Malte Starostik <malte@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef SEARCHPROVIDERDLG_H
#define SEARCHPROVIDERDLG_H

#include <QDialog>

#include "ui_searchproviderdlg_ui.h"

class QDialogButtonBox;
class SearchProvider;

class SearchProviderDialog : public QDialog
{
    Q_OBJECT

public:
    explicit SearchProviderDialog(SearchProvider *provider, QList<SearchProvider *> &providers, QWidget *parent = nullptr);

    SearchProvider *provider()
    {
        return m_provider;
    }

public Q_SLOTS:
    void accept() override;

protected Q_SLOTS:
    void slotChanged();
    void shortcutsChanged(const QString &newShorthands);
    void pastePlaceholder();

private:
    SearchProvider *m_provider;
    QList<SearchProvider *> m_providers; // The list of all search providers, used for checking for already assigned shortcuts.
    Ui::SearchProviderDlgUI m_dlg;
    QDialogButtonBox *m_buttons;
};

#endif
