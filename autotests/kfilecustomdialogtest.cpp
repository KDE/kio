/* This file is part of the KDE libraries
    Copyright (C) 2017    Klarälvdalens Datakonsult AB, a KDAB Group
                          company, info@kdab.com. Work sponsored by the
                          LiMux project of the city of Munich

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License version 2, as published by the Free Software Foundation.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.
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
