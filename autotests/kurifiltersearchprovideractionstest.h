/*
    SPDX-FileCopyrightText: 2015 Montel Laurent <montel@kde.org>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#ifndef KURIFILTERSEARCHPROVIDERACTIONSTEST_H
#define KURIFILTERSEARCHPROVIDERACTIONSTEST_H

#include <QObject>

class KUriFilterSearchProviderActionsTest : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void initTestCase();
    void shouldHaveDefaultValue();
    void shouldAssignSelectedText();
    void shouldAddActionToMenu();
};

#endif // KURIFILTERSEARCHPROVIDERACTIONSTEST_H
