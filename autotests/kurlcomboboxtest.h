/*
    SPDX-FileCopyrightText: 2015 Alejandro Fiestas Olivares <afiestas@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef KURLCOMBOBOXTEST_H
#define KURLCOMBOBOXTEST_H

#include <QObject>

class KUrlComboBoxTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void testTextForItem();
    void testTextForItem_data();
    void testSetUrlMultipleTimes();
    void testRemoveUrl();
    void testAddUrls();
    void testSetMaxItems();
};

#endif //KURLCOMBOBOXTEST_H

