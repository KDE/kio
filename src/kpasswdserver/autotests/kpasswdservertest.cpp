/*
    This file is part of the KDE project
    SPDX-FileCopyrightText: 2010 David Faure <faure@kde.org>
    SPDX-FileCopyrightText: 2012 Dawit Alemayehu <adawit@kde.org>

    SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include <QApplication>
#include <QSignalSpy>
#include <QTest>
#include <kpasswdserver.h>
#include <KPasswordDialog>

static const char *sigQueryAuthInfoResult = SIGNAL(queryAuthInfoAsyncResult(qlonglong, qlonglong, KIO::AuthInfo));
static const char *sigCheckAuthInfoResult = SIGNAL(checkAuthInfoAsyncResult(qlonglong, qlonglong, KIO::AuthInfo));

// For the retry dialog (and only that one)
static QDialogButtonBox::StandardButton s_buttonYes = QDialogButtonBox::Yes;
static QDialogButtonBox::StandardButton s_buttonCancel = QDialogButtonBox::Cancel;

Q_DECLARE_METATYPE(QDialogButtonBox::StandardButton)
Q_DECLARE_METATYPE(QDialog::DialogCode)

static QString getUserNameFrom(const KIO::AuthInfo &auth)
{
    if (auth.username.isEmpty() && !auth.url.userName().isEmpty()) {
        return auth.url.userName();
    }

    return auth.username;
}

class KPasswdServerTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase()
    {
        qRegisterMetaType<QDialogButtonBox::StandardButton>();
        qRegisterMetaType<QDialog::DialogCode>();
    }

    void simpleTest()
    {
        KPasswdServer server(this);
        server.setWalletDisabled(true);

        // Check that processRequest doesn't crash when it has nothing to do
        server.processRequest();

        KIO::AuthInfo info;
        info.url = QUrl(QStringLiteral("http://www.example.com"));

        // Make a check for that host, should say "not found"
        QVERIFY(noCheckAuth(server, info));

        // Now add auth to the cache
        const qlonglong windowId = 42;
        KIO::AuthInfo realInfo = info;
        realInfo.username = QStringLiteral("toto"); // you can see I'm french
        realInfo.password = QStringLiteral("foobar");
        server.addAuthInfo(realInfo, windowId); // seqnr=2

        // queryAuth without the ability to prompt, will just return info unmodified
        KIO::AuthInfo resultInfo;
        queryAuth(server, info, resultInfo);
        QCOMPARE(resultInfo.url, info.url);
        QCOMPARE(resultInfo.username, QString());
        QCOMPARE(resultInfo.password, QString());
        QCOMPARE(resultInfo.isModified(), false);

        // Check that checkAuth finds it
        QVERIFY(successCheckAuth(server, info, realInfo));

        // Now remove auth
        server.removeAuthInfo(info.url.host(), info.url.scheme(), info.username);
        // Check we can't find that auth anymore
        QVERIFY(noCheckAuth(server, info));
    }

    void testCheckDuringQuery()
    {
        KPasswdServer server(this);
        server.setWalletDisabled(true);
        KIO::AuthInfo info;
        info.url = QUrl(QStringLiteral("http://www.kde.org"));

        // Start a query
        QSignalSpy spyQuery(&server, sigQueryAuthInfoResult);
        const qlonglong windowId = 42;
        const qlonglong seqNr = 2;
        const qlonglong id = server.queryAuthInfoAsync(info,
                                                       QStringLiteral("<NoAuthPrompt>"), // magic string to avoid a dialog
                                                       windowId,
                                                       seqNr,
                                                       16 /*usertime*/);

        // Before it is processed, do a check, it will reply delayed.
        QSignalSpy spyCheck(&server, sigCheckAuthInfoResult);
        const qlonglong idCheck = server.checkAuthInfoAsync(info, windowId, 17 /*usertime*/);
        QCOMPARE(idCheck, 0LL);        // always
        QCOMPARE(spyCheck.count(), 0); // no reply yet

        // Wait for the query to be processed
        QVERIFY(QSignalSpy(&server, sigQueryAuthInfoResult).wait(1000));
        QCOMPARE(spyQuery.count(), 1);
        QCOMPARE(spyQuery[0][0].toLongLong(), id);
        KIO::AuthInfo result = spyQuery[0][2].value<KIO::AuthInfo>();

        // Now the check will have replied
        QCOMPARE(spyCheck.count(), 1);
        QCOMPARE(spyCheck[0][0].toLongLong(), id + 1); // it was the next request after the query
        KIO::AuthInfo resultCheck = spyCheck[0][2].value<KIO::AuthInfo>();
        QCOMPARE(result.username, resultCheck.username);
        QCOMPARE(result.password, resultCheck.password);
    }

    void testExpiry()
    {
        KPasswdServer server(this);
        server.setWalletDisabled(true);
        KIO::AuthInfo info;
        info.url = QUrl(QStringLiteral("http://www.example.com"));

        // Add auth to the cache
        const qlonglong windowId = 42;
        KIO::AuthInfo realInfo = info;
        realInfo.username = QStringLiteral("toto");
        realInfo.password = QStringLiteral("foobar");
        server.addAuthInfo(realInfo, windowId);

        QVERIFY(successCheckAuth(server, info, realInfo));

        // Close another window, shouldn't hurt
        server.removeAuthForWindowId(windowId + 1);
        QVERIFY(successCheckAuth(server, info, realInfo));

        // Close window
        server.removeAuthForWindowId(windowId);

        // Check we can't find that auth anymore
        QVERIFY(noCheckAuth(server, info));
    }

    void testFillDialog()
    {
        KPasswdServer server(this);
        server.setWalletDisabled(true);
        // What the app would ask
        KIO::AuthInfo info;
        info.url = QUrl(QStringLiteral("http://www.example.com"));

        // What the user would type
        KIO::AuthInfo filledInfo(info);
        filledInfo.username = QStringLiteral("dfaure");
        filledInfo.password = QStringLiteral("toto");

        KIO::AuthInfo result;
        queryAuthWithDialog(server, info, filledInfo, result);
    }

    void testRejectRetryDialog()
    {
        KPasswdServer server(this);
        server.setWalletDisabled(true);

        // What the app would ask
        KIO::AuthInfo info;
        info.url = QUrl(QStringLiteral("http://www.example.com"));

        // What the user would type
        KIO::AuthInfo filledInfo(info);
        filledInfo.username = QStringLiteral("username");
        filledInfo.password = QStringLiteral("password");

        KIO::AuthInfo result;
        queryAuthWithDialog(server, info, filledInfo, result);

        // Pretend that the returned credentials failed and initiate a retry,
        // but cancel the retry dialog.
        info.password.clear();
        result = KIO::AuthInfo();
        queryAuthWithDialog(server, info, filledInfo, result, s_buttonCancel,
                            QDialog::Accepted /*unused*/, QStringLiteral("Invalid username or password"));
    }

    void testAcceptRetryDialog()
    {
        KPasswdServer server(this);
        server.setWalletDisabled(true);

        // What the app would ask
        KIO::AuthInfo info;
        info.url = QUrl(QStringLiteral("http://www.example.com"));

        // What the user would type
        KIO::AuthInfo filledInfo(info);
        filledInfo.username = QStringLiteral("username");
        filledInfo.password = QStringLiteral("password");

        KIO::AuthInfo result;
        queryAuthWithDialog(server, info, filledInfo, result);

        // Pretend that the returned credentials failed and initiate a retry,
        // but this time continue the retry.
        info.password.clear();
        result = KIO::AuthInfo();

        queryAuthWithDialog(server, info, filledInfo, result, s_buttonYes,
                            QDialog::Accepted, QStringLiteral("Invalid username or password"));
    }

    void testUsernameMistmatch()
    {
        KPasswdServer server(this);
        server.setWalletDisabled(true);

        // What the app would ask. Note the username in the URL.
        KIO::AuthInfo info;
        info.url = QUrl(QStringLiteral("http://foo@www.example.com"));

        // What the user would type
        KIO::AuthInfo filledInfo(info);
        filledInfo.username = QStringLiteral("bar");
        filledInfo.password = QStringLiteral("blah");

        KIO::AuthInfo result;
        queryAuthWithDialog(server, info, filledInfo, result);

        // Check the returned url does not match the request url because of the
        // username mismatch between the request URL and the filled in one.
        QVERIFY(result.url != filledInfo.url);

        // Verify there is NO cached auth data if the request URL contains the
        // original user name (foo).
        QVERIFY(noCheckAuth(server, info));

        // Verify there is a cached auth data if the request URL contains the
        // new user name (bar).
        filledInfo.url = QUrl(QStringLiteral("http://bar@www.example.com"));
        QVERIFY(successCheckAuth(server, filledInfo, result));

        // Now the URL check should be valid too.
        QCOMPARE(result.url, filledInfo.url);
    }

    void testCancelPasswordDialog()
    {
        KPasswdServer server(this);
        server.setWalletDisabled(true);

        // What the app would ask.
        KIO::AuthInfo info;
        info.url = QUrl(QStringLiteral("http://www.example.com"));
        info.username = info.url.userName();

        KIO::AuthInfo result;
        queryAuthWithDialog(server, info, KIO::AuthInfo(), result, QDialogButtonBox::NoButton, QDialog::Rejected);
    }

    void testVerifyPath()
    {
        KPasswdServer server(this);
        server.setWalletDisabled(true);

        // Add auth to the cache
        const qlonglong windowId = 42;
        KIO::AuthInfo authInfo;
        authInfo.url = QUrl(QStringLiteral("http://www.example.com/test/test.html"));
        authInfo.username = QStringLiteral("toto");
        authInfo.password = QStringLiteral("foobar");
        server.addAuthInfo(authInfo, windowId);

        KIO::AuthInfo queryAuthInfo;
        queryAuthInfo.url = QUrl(QStringLiteral("http://www.example.com/test/test2/test.html"));
        queryAuthInfo.verifyPath = true;

        KIO::AuthInfo expectedAuthInfo;
        expectedAuthInfo.username = QStringLiteral("toto");
        expectedAuthInfo.password = QStringLiteral("foobar");

        QVERIFY(successCheckAuth(server, queryAuthInfo, expectedAuthInfo));
    }

    void testConcurrentQueryAuth()
    {
        KPasswdServer server(this);
        server.setWalletDisabled(true);

        QList<KIO::AuthInfo> authInfos;
        for (int i = 0; i < 10; ++i) {
            KIO::AuthInfo info;
            info.url = QUrl(QLatin1String("http://www.example.com/test") + QString::number(i) + QLatin1String(".html"));
            authInfos << info;
        }

        // What the user would type
        KIO::AuthInfo filledInfo;
        filledInfo.username = QStringLiteral("bar");
        filledInfo.password = QStringLiteral("blah");

        QList<KIO::AuthInfo> results;
        concurrentQueryAuthWithDialog(server, authInfos, filledInfo, results);
    }

    void testConcurrentCheckAuth()
    {
        KPasswdServer server(this);
        server.setWalletDisabled(true);

        QList<KIO::AuthInfo> authInfos;
        for (int i = 0; i < 10; ++i) {
            KIO::AuthInfo info;
            info.url = QUrl(QLatin1String("http://www.example.com/test") + QString::number(i) + QStringLiteral(".html"));
            authInfos << info;
        }

        // What the user would type
        KIO::AuthInfo filledInfo;
        filledInfo.username = QStringLiteral("bar");
        filledInfo.password = QStringLiteral("blah");

        QList<KIO::AuthInfo> results;
        concurrentQueryAuthWithDialog(server, authInfos, filledInfo, results);
    }

private:
    // Checks that no auth is available for @p info
    bool noCheckAuth(KPasswdServer &server, const KIO::AuthInfo &info)
    {
        KIO::AuthInfo result;
        checkAuth(server, info, result);
        return (result.username == info.username) && (result.password == info.password) && !result.isModified();
    }

    // Check that the auth is available and equal to @expectedInfo
    bool successCheckAuth(KPasswdServer &server, const KIO::AuthInfo &info, const KIO::AuthInfo &expectedInfo)
    {
        KIO::AuthInfo result;
        checkAuth(server, info, result);
        return (result.username == expectedInfo.username) && (result.password == expectedInfo.password) && result.isModified();
    }

    void checkAuth(KPasswdServer &server, const KIO::AuthInfo &info, KIO::AuthInfo &result)
    {
        QSignalSpy spy(&server, sigCheckAuthInfoResult);
        const qlonglong windowId = 42;
        const qlonglong id = server.checkAuthInfoAsync(info, windowId, 17 /*usertime*/);
        QCOMPARE(id, 0LL); // always
        if (spy.isEmpty()) {
            QVERIFY(QSignalSpy(&server, sigCheckAuthInfoResult).wait(1000));
        }
        QCOMPARE(spy.count(), 1);
        // kpasswdserver emits a requestId via dbus, we can't get that id here
        QVERIFY(spy[0][0].toLongLong() >= 0);
        // QCOMPARE(spy[0][1].toLongLong(), 3LL); // seqNr
        result = spy[0][2].value<KIO::AuthInfo>();
    }

    void queryAuth(KPasswdServer &server, const KIO::AuthInfo &info, KIO::AuthInfo &result)
    {
        QSignalSpy spy(&server, sigQueryAuthInfoResult);
        const qlonglong windowId = 42;
        const qlonglong seqNr = 2;
        const qlonglong id = server.queryAuthInfoAsync(info,
                                                       QStringLiteral("<NoAuthPrompt>"), // magic string to avoid a dialog
                                                       windowId,
                                                       seqNr,
                                                       16 /*usertime*/);
        QVERIFY(id >= 0); // requestId, ever increasing
        if (spy.isEmpty())
            QVERIFY(QSignalSpy(&server, sigQueryAuthInfoResult).wait(1000));
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy[0][0].toLongLong(), id);
        // QCOMPARE(spy[0][1].toLongLong(), 3LL); // seqNr
        result = spy[0][2].value<KIO::AuthInfo>();
    }

    void queryAuthWithDialog(KPasswdServer &server,
                             const KIO::AuthInfo &info,
                             const KIO::AuthInfo &filledInfo,
                             KIO::AuthInfo &result,
                             QDialogButtonBox::StandardButton retryButton = s_buttonYes,
                             QDialog::DialogCode code = QDialog::Accepted,
                             const QString &errMsg = QString())
    {
        QSignalSpy spy(&server, sigQueryAuthInfoResult);
        const qlonglong windowId = 42;
        const qlonglong seqNr = 2;
        const qlonglong id = server.queryAuthInfoAsync(info, errMsg, windowId, seqNr, 16 /*usertime*/);
        QVERIFY(id >= 0); // requestId, ever increasing
        QVERIFY(spy.isEmpty());

        const bool hasErrorMessage = (!errMsg.isEmpty());
        const bool isCancelRetryDialogTest = (hasErrorMessage && retryButton == s_buttonCancel);

        if (hasErrorMessage) {
            // Retry dialog only knows Yes/No
            QMetaObject::invokeMethod(this, "checkRetryDialog", Qt::QueuedConnection,
                                      Q_ARG(QDialogButtonBox::StandardButton, retryButton));
        }

        if (!isCancelRetryDialogTest) {
            QMetaObject::invokeMethod(this, "checkAndFillDialog", Qt::QueuedConnection, Q_ARG(KIO::AuthInfo, info),
                                      Q_ARG(KIO::AuthInfo, filledInfo), Q_ARG(QDialog::DialogCode, code));
        }
        // Force KPasswdServer to process the request now, otherwise the checkAndFillDialog needs a timer too...
        server.processRequest();
        if (spy.isEmpty())
            QVERIFY(QSignalSpy(&server, sigQueryAuthInfoResult).wait(1000));
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy[0][0].toLongLong(), id);
        // QCOMPARE(spy[0][1].toLongLong(), 3LL); // seqNr
        result = spy[0][2].value<KIO::AuthInfo>();
        const QString username = (isCancelRetryDialogTest ? QString() : filledInfo.username);
        const QString password = (isCancelRetryDialogTest ? QString() : filledInfo.password);
        QCOMPARE(result.username, username);
        QCOMPARE(result.password, password);
        QCOMPARE(result.isModified(), retryButton == s_buttonYes && code == QDialog::Accepted);
    }

    void concurrentQueryAuthWithDialog(KPasswdServer &server, const QList<KIO::AuthInfo> &infos, const KIO::AuthInfo &filledInfo, QList<KIO::AuthInfo> &results, QDialog::DialogCode code = QDialog::Accepted)
    {
        QSignalSpy spy(&server, sigQueryAuthInfoResult);
        const qlonglong windowId = 42;
        qlonglong seqNr = 0;
        QList<qlonglong> idList;

        for (const KIO::AuthInfo &info : infos) {
            const qlonglong id = server.queryAuthInfoAsync(info, QString(), windowId, seqNr, 16 /*usertime*/);
            QVERIFY(id >= 0); // requestId, ever increasing
            idList << id;
        }

        QVERIFY(spy.isEmpty());
        QMetaObject::invokeMethod(this, "checkAndFillDialog", Qt::QueuedConnection, Q_ARG(KIO::AuthInfo, infos.first()), Q_ARG(KIO::AuthInfo, filledInfo), Q_ARG(QDialog::DialogCode, code));

        // Force KPasswdServer to process the request now, otherwise the checkAndFillDialog needs a timer too...
        server.processRequest();
        while (spy.count() < infos.count())
            QVERIFY(QSignalSpy(&server, sigQueryAuthInfoResult).wait(1000));

        QCOMPARE(spy.count(), infos.count());

        for (int i = 0, count = spy.count(); i < count; ++i) {
            QCOMPARE(spy[i][0].toLongLong(), idList.at(i));
            // QCOMPARE(spy[0][1].toLongLong(), 3LL); // seqNr
            KIO::AuthInfo result = spy[i][2].value<KIO::AuthInfo>();
            QCOMPARE(result.username, filledInfo.username);
            QCOMPARE(result.password, filledInfo.password);
            QCOMPARE(result.isModified(), code == QDialog::Accepted);
            results << result;
        }
    }

    void concurrentCheckAuthWithDialog(KPasswdServer &server, const QList<KIO::AuthInfo> &infos, const KIO::AuthInfo &filledInfo, QList<KIO::AuthInfo> &results, QDialog::DialogCode code = QDialog::Accepted)
    {
        QSignalSpy spy(&server, sigQueryAuthInfoResult);
        const qlonglong windowId = 42;
        qlonglong seqNr = 0;
        QList<qlonglong> idList;

        QListIterator<KIO::AuthInfo> it(infos);
        if (it.hasNext()) {
            const qlonglong id = server.queryAuthInfoAsync(it.next(), QString(), windowId, seqNr, 16 /*usertime*/);
            QVERIFY(id >= 0); // requestId, ever increasing
            idList << id;
        }

        while (it.hasNext()) {
            const qlonglong id = server.checkAuthInfoAsync(it.next(), windowId, 16 /*usertime*/);
            QVERIFY(id >= 0); // requestId, ever increasing
            idList << id;
        }

        QVERIFY(spy.isEmpty());
        QMetaObject::invokeMethod(this, "checkAndFillDialog", Qt::QueuedConnection, Q_ARG(KIO::AuthInfo, infos.first()), Q_ARG(KIO::AuthInfo, filledInfo), Q_ARG(QDialog::DialogCode, code));

        // Force KPasswdServer to process the request now, otherwise the checkAndFillDialog needs a timer too...
        server.processRequest();
        if (spy.isEmpty()) {
            QVERIFY(QSignalSpy(&server, sigQueryAuthInfoResult).wait(1000));
        }

        while ((spy.count() - 1) < infos.count()) {
            QVERIFY(QSignalSpy(&server, sigCheckAuthInfoResult).wait(1000));
        }

        for (int i = 0, count = spy.count(); i < count; ++i) {
            QCOMPARE(spy[i][0].toLongLong(), idList.at(i));
            // QCOMPARE(spy[0][1].toLongLong(), 3LL); // seqNr
            KIO::AuthInfo result = spy[i][2].value<KIO::AuthInfo>();
            QCOMPARE(result.username, filledInfo.username);
            QCOMPARE(result.password, filledInfo.password);
            QCOMPARE(result.isModified(), code == QDialog::Accepted);
            results << result;
        }
    }

protected Q_SLOTS:
    void checkAndFillDialog(const KIO::AuthInfo &info, const KIO::AuthInfo &filledInfo, QDialog::DialogCode code)
    {
        const QList<QWidget *> widgetsList = QApplication::topLevelWidgets();
        for (QWidget *widget : widgetsList) {
            if (KPasswordDialog *dialog = qobject_cast<KPasswordDialog *>(widget)) {
                if (code == QDialog::Accepted) {
                    QCOMPARE(dialog->username(), getUserNameFrom(info));
                    QCOMPARE(dialog->password(), info.password);
                    dialog->setUsername(filledInfo.username);
                    dialog->setPassword(filledInfo.password);
                }
                dialog->done(code);
                return;
            }
        }
        qWarning() << "No KPasswordDialog found!";
    }

    void checkRetryDialog(QDialogButtonBox::StandardButton code = s_buttonYes)
    {
        const QList<QWidget *> widgetsList = QApplication::topLevelWidgets();
        for (QWidget *widget : widgetsList) {
            QDialog *dialog = qobject_cast<QDialog *>(widget);
            if (dialog && !dialog->inherits("KPasswordDialog")) {
                dialog->done(code);
                return;
            }
        }
        qWarning() << "No retry dialog found";
    }
};

QTEST_MAIN(KPasswdServerTest)

#include "kpasswdservertest.moc"
