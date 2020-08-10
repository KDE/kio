/*
    This file is part of the KDE libraries
    SPDX-FileCopyrightText: 2017 Klarälvdalens Datakonsult AB, a KDAB Group company <info@kdab.com>
    Work sponsored by the LiMux project of the city of Munich

    SPDX-License-Identifier: LGPL-2.0-only
*/

#ifndef KFILECUSTOMDIALOGTEST_H
#define KFILECUSTOMDIALOGTEST_H

#include <QObject>

class KFileCustomDialogTest : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void shouldHaveDefaultValue();
};

#endif // KFILECUSTOMDIALOGTEST_H
