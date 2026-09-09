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

QString getValueAsPercentEncoded(const QString &input)
{
    QByteArray qbAr = QUrl::toPercentEncoding(input);
    return QString::fromUtf8(qbAr);
}

void KEMailClientLauncherJobTest::testTo()
{
    KEMailClientLauncherJob job;
    /* clang-format off */
    QString to      = "someone@example.com";
    QString toExp   = "someone%40example.com";
    QString toExpTb = to;
    /* clang-format on */
    job.setTo({to});
    QCOMPARE(job.mailToUrl().toString(), "mailto:" + toExp);

    const QStringList expected{QStringLiteral("-compose"), QStringLiteral("to='") + toExpTb + QStringLiteral("'")};
    QCOMPARE(job.thunderbirdArguments(), expected);
}

void KEMailClientLauncherJobTest::testManyFields()
{
    KEMailClientLauncherJob job;

    /* clang-format off */
    QString to1      = "someone@example.com";
    QString to1Exp   = "someone%40example.com";
    QString to1ExpTb = to1;
    QString to2      = "Someone Else <someoneelse@example.com>";
    QString to2Exp   = "Someone Else %3Csomeoneelse%40example.com%3E";
    QString to2ExpTb = to2;
    QString to3      = "Else, Someone 'more <else,someone'more@example.com>";
    QString to3Exp   = "Else%2C Someone %27more %3Celse%2Csomeone%27more%40example.com%3E";
    QString to3ExpTb = "Else\\, Someone \\'more <else\\,someone\\'more@example.com>";
    job.setTo({to1, to2, to3});

    QString cc1      = "Boss who likes €£¥ <boss@example.com>";
    QString cc1Exp   = "Boss who likes €£¥ %3Cboss%40example.com%3E";
    QString cc1ExpTb = cc1;
    QString cc2      = "ceo@example.com";
    QString cc2Exp   = "ceo%40example.com";
    QString cc2ExpTb = cc2;
    job.setCc({cc1, cc2});

    QString subject      = "See you on Hauptstraße, the ' %25 third";
    QString subjectExp   = "See you on Hauptstraße%2C the %27 %2525 third";
    QString subjectExpTb = getValueAsPercentEncoded(subject);
    job.setSubject(subject);

    QString body     = "Hauptstraße is an excuse to test UTF-8 & URLs.";
            body    += " Now check characters , ' then check percent %25.";
            body    += " HTML entities &uuuml; and tags <b>shouldn't</b> interpreted.";
            body    += "\nBest regards.";
    QString bodyExp  = "Hauptstraße is an excuse to test UTF-8 %26 URLs.";
            bodyExp += " Now check characters %2C %27 then check percent %2525.";
            bodyExp += " HTML entities %26uuuml%3B and tags %3Cb%3Eshouldn%27t%3C%2Fb%3E interpreted.";
            bodyExp += "%0ABest regards.";
    QString bodyExpTb = getValueAsPercentEncoded(body.toHtmlEscaped());
    job.setBody(body);
    /* clang-format on */

    const QString expected =
            "mailto:"  + to1Exp     + "?"
            "to="      + to2Exp     + "&to=" + to3Exp + "&"
            "cc="      + cc1Exp     + "&cc=" + cc2Exp + "&"
            "subject=" + subjectExp + "&"
            "body="    + bodyExp;
    QCOMPARE(job.mailToUrl().toString(), expected);

    const QStringList tbExpected{QStringLiteral("-compose"),
            "to='"      + to1ExpTb     + "," + to2ExpTb + "," + to3ExpTb + "',"
            "cc='"      + cc1ExpTb     + "," + cc2ExpTb + "',"
            "subject="  + subjectExpTb + ","
            "body="     + bodyExpTb    + ""};
    QCOMPARE(job.thunderbirdArguments(), tbExpected);
}

void KEMailClientLauncherJobTest::testAttachments()
{
    KEMailClientLauncherJob job;
    const QUrl thisExe = QUrl::fromLocalFile(QCoreApplication::applicationFilePath());
    job.setAttachments({thisExe, thisExe});
    const QString path = thisExe.toString();
    const QString pathEncoded = QUrl::toPercentEncoding(path);
    const QString expected = "mailto:?attach=" + pathEncoded + "&attach=" + pathEncoded;
    QCOMPARE(job.mailToUrl().toString(), expected);

    KEMailClientLauncherJob jobTb;
    const QString strFilenameTb = "/tmp/testfile%25_&=#test,'-.txt";
    const QUrl thisExeTb = QUrl::fromLocalFile(strFilenameTb);
    jobTb.setAttachments({thisExeTb, thisExeTb});
    const QString pathTb = thisExeTb.toString();
    QString pathEscapeTb = pathTb;
    pathEscapeTb.replace('\'', "%27");
    pathEscapeTb.replace(',', "%2C");
    const QStringList tbExpectedTb{QStringLiteral("-compose"), "attachment='" + pathEscapeTb + "," + pathEscapeTb + "'"};
    QCOMPARE(jobTb.thunderbirdArguments(), tbExpectedTb);
}

#include "moc_kemailclientlauncherjobtest.cpp"
