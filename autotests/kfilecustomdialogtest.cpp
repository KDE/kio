/*
    This file is part of the KDE libraries
    SPDX-FileCopyrightText: 2017 Klarälvdalens Datakonsult AB, a KDAB Group company <info@kdab.com>
    Work sponsored by the LiMux project of the city of Munich

    SPDX-License-Identifier: LGPL-2.0-only
*/

#include "kfilecustomdialogtest.h"
#include "kfilecustomdialog.h"

#include <KFileWidget>
#include <QTest>
#include <QVBoxLayout>
QTEST_MAIN(KFileCustomDialogTest)

void KFileCustomDialogTest::shouldHaveDefaultValue()
{
    KFileCustomDialog dlg;
    dlg.show();
    QVERIFY(QTest::qWaitForWindowExposed(&dlg));

    QVBoxLayout *mainLayout = dlg.findChild<QVBoxLayout *>();
    QVERIFY(mainLayout);

    KFileWidget *mFileWidget = dlg.findChild<KFileWidget *>(QStringLiteral("filewidget"));
    QVERIFY(mFileWidget);
    QCOMPARE(dlg.fileWidget(), mFileWidget);
}
