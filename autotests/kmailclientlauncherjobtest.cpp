/*
    This file is part of the KDE libraries
    SPDX-FileCopyrightText: 2021 David Faure <faure@kde.org>

    SPDX-License-Identifier: LGPL-2.0-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/

#include "kmailclientlauncherjobtest.h"
#include "kmailclientlauncherjob.h"

#include <QStandardPaths>
#include <QTest>

QTEST_GUILESS_MAIN(KMailClientLauncherJobTest)

void KMailClientLauncherJobTest::initTestCase()
{
    QStandardPaths::setTestModeEnabled(true);
}

void KMailClientLauncherJobTest::testEmpty()
{
    KMailClientLauncherJob job;
    QCOMPARE(job.mailToUrl().toString(), QString());
    QCOMPARE(job.thunderbirdCommandLine("thunderbird"), QStringLiteral("thunderbird -compose"));
}

void KMailClientLauncherJobTest::testTo()
{
    KMailClientLauncherJob job;
    job.setTo({"someone@example.com"});
    QCOMPARE(job.mailToUrl().toString(), "mailto:someone@example.com");

    const QString expected = R"(thunderbird -compose 'to='\''someone@example.com'\''')";
    QCOMPARE(job.thunderbirdCommandLine("thunderbird"), expected);
}

void KMailClientLauncherJobTest::testManyFields()
{
    KMailClientLauncherJob job;
    job.setTo({"someone@example.com", "Someone Else <someoneelse@example.com>"});
    job.setCc({"Boss who likes €£¥ <boss@example.com>", "ceo@example.com"});
    job.setSubject("See you on Hauptstraße");
    job.setBody("Hauptstraße is an excuse to test UTF-8 & URLs.\nBest regards.");
    const QString expected =
        "mailto:someone@example.com?to=Someone Else %3Csomeoneelse@example.com%3E&cc=Boss who likes €£¥ %3Cboss@example.com%3E&cc=ceo@example.com&subject=See "
        "you on Hauptstraße&body=Hauptstraße is an excuse to test UTF-8 %26 URLs.%0ABest regards.";
    QCOMPARE(job.mailToUrl().toString(), expected);
    const QString tbExpected =
        R"(thunderbird -compose 'to='\''someone@example.com,Someone Else <someoneelse@example.com>'\'',cc='\''Boss who likes €£¥ <boss@example.com>,ceo@example.com'\'',subject='\''See you on Hauptstraße'\'',body='\''Hauptstraße is an excuse to test UTF-8 & URLs.)"
        "\n"
        R"(Best regards.'\''')";
    QCOMPARE(job.thunderbirdCommandLine("thunderbird"), tbExpected);
}

void KMailClientLauncherJobTest::testAttachments()
{
    KMailClientLauncherJob job;
    const QUrl thisExe = QUrl::fromLocalFile(QCoreApplication::applicationFilePath());
    job.setAttachments({thisExe, thisExe});
    const QString path = thisExe.toString(); // let's hope no '&' or '#' in the path :)
    const QString expected = "mailto:?attach=" + path + "&attach=" + path;
    QCOMPARE(job.mailToUrl().toString(), expected);
}
