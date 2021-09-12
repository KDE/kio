/*
    This file is part of the KDE libraries
    SPDX-FileCopyrightText: 2021 David Faure <faure@kde.org>

    SPDX-License-Identifier: LGPL-2.0-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/

#include "kemailclientlauncherjobtest.h"
#include "kemailclientlauncherjob.h"

#include <QStandardPaths>
#include <QTest>

QTEST_GUILESS_MAIN(KEMailClientLauncherJobTest)

void KEMailClientLauncherJobTest::initTestCase()
{
    QStandardPaths::setTestModeEnabled(true);
}

void KEMailClientLauncherJobTest::testEmpty()
{
    KEMailClientLauncherJob job;
    QCOMPARE(job.mailToUrl().toString(), QString());
    const QStringList expected{QStringLiteral("-compose")};
    QCOMPARE(job.thunderbirdArguments(), expected);
}

void KEMailClientLauncherJobTest::testTo()
{
    KEMailClientLauncherJob job;
    job.setTo({"someone@example.com"});
    QCOMPARE(job.mailToUrl().toString(), "mailto:someone@example.com");

    const QStringList expected{QStringLiteral("-compose"), QStringLiteral("to='someone@example.com'")};
    QCOMPARE(job.thunderbirdArguments(), expected);
}

void KEMailClientLauncherJobTest::testManyFields()
{
    KEMailClientLauncherJob job;
    job.setTo({"someone@example.com", "Someone Else <someoneelse@example.com>"});
    job.setCc({"Boss who likes €£¥ <boss@example.com>", "ceo@example.com"});
    job.setSubject("See you on Hauptstraße");
    job.setBody("Hauptstraße is an excuse to test UTF-8 & URLs.\nBest regards.");
    const QString expected =
        "mailto:someone@example.com?to=Someone Else %3Csomeoneelse@example.com%3E&cc=Boss who likes €£¥ %3Cboss@example.com%3E&cc=ceo@example.com&subject=See "
        "you on Hauptstraße&body=Hauptstraße is an excuse to test UTF-8 %26 URLs.%0ABest regards.";
    QCOMPARE(job.mailToUrl().toString(), expected);
    const QStringList tbExpected{QStringLiteral("-compose"),
                                 "to='someone@example.com,Someone Else <someoneelse@example.com>',cc='Boss who likes €£¥ "
                                 "<boss@example.com>,ceo@example.com',subject='See you on Hauptstraße',body='Hauptstraße is an excuse to test UTF-8 & URLs."
                                 "\nBest regards.'"};
    QCOMPARE(job.thunderbirdArguments(), tbExpected);
}

void KEMailClientLauncherJobTest::testAttachments()
{
    KEMailClientLauncherJob job;
    const QUrl thisExe = QUrl::fromLocalFile(QCoreApplication::applicationFilePath());
    job.setAttachments({thisExe, thisExe});
    const QString path = thisExe.toString(); // let's hope no '&' or '#' in the path :)
    const QString expected = "mailto:?attach=" + path + "&attach=" + path;
    QCOMPARE(job.mailToUrl().toString(), expected);
}
