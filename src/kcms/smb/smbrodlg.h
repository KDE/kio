/*
    This file is part of the KDE project
    SPDX-FileCopyrightText: 2000, 2005 Alexander Neundorf <neundorf@kde.org>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#ifndef __SMBRODLG_H
#define __SMBRODLG_H

#include <QCheckBox>
#include <QLineEdit>
#include <QWidget>

#include <KCModule>

class KComboBox;

class SMBRoOptions : public KCModule
{
    Q_OBJECT
public:
    SMBRoOptions(QObject *parent, const KPluginMetaData &data, const QVariantList &args);
    ~SMBRoOptions() override;

    void load() override;
    void save() override;
    void defaults() override;

private Q_SLOTS:
    void changed();

private:
    QLineEdit *m_userLe;
    QLineEdit *m_passwordLe;
    //      QLineEdit *m_workgroupLe; //currently unused, Alex
    //      QCheckBox *m_showHiddenShares; //currently unused, Alex
    //      KComboBox *m_encodingList; //currently unused
};

#endif
