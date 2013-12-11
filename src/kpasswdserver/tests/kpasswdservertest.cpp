/* This file is part of the KDE project
    Copyright 2010 David Faure <faure@kde.org>
    Copyright 2012 Dawit Alemayehu <adawit@kde.org>

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License or ( at
   your option ) version 3 or, at the discretion of KDE e.V. ( which shall
   act as a proxy as in section 14 of the GPLv3 ), any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.
   Boston, MA 02110-1301, USA.
*/

#include <kdebug.h>
#include <kpassworddialog.h>
#include <QApplication>
#include <qtest_kde.h>
#include <kpasswdserver.h>

static const char* sigQueryAuthInfoResult = SIGNAL(queryAuthInfoAsyncResult(qlonglong,qlonglong,KIO::AuthInfo));
static const char* sigCheckAuthInfoResult = SIGNAL(checkAuthInfoAsyncResult(qlonglong,qlonglong,KIO::AuthInfo));

static QString getUserNameFrom(const KIO::AuthInfo& auth)
{
    if (auth.username.isEmpty() && !auth.url.user().isEmpty()) {
        return auth.url.user();
    }

    return auth.username;
}

class KPasswdServerTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase()
    {
    }

    void simpleTest()
    {
        KPasswdServer server(this);
        server.setWalletDisabled(true);

        // Check that processRequest doesn't crash when it has nothing to do
        server.processRequest();

        KIO::AuthInfo info;
        info.url = KUrl("http://www.example.com");

        // Make a check for that host, should say "not found"
        QVERIFY(noCheckAuth(server, info));

        // Now add auth to the cache
        const qlonglong windowId = 42;
        KIO::AuthInfo realInfo = info;
        realInfo.username = "toto"; // you can see I'm french
        realInfo.password = "foobar";
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
        server.removeAuthInfo(info.url.host(), info.url.protocol(), info.username);
        // Check we can't find that auth anymore
        QVERIFY(noCheckAuth(server, info));
    }

    void testCheckDuringQuery()
    {
        KPasswdServer server(this);
        server.setWalletDisabled(true);
        KIO::AuthInfo info;
        info.url = KUrl("http://www.kde.org");

        // Start a query
        QSignalSpy spyQuery(&server, sigQueryAuthInfoResult);
        const qlonglong windowId = 42;
        const qlonglong seqNr = 2;
        const qlonglong id = server.queryAuthInfoAsync(
            info,
            QString("<NoAuthPrompt>"), // magic string to avoid a dialog
            windowId, seqNr, 16 /*usertime*/);

        // Before it is processed, do a check, it will reply delayed.
        QSignalSpy spyCheck(&server, sigCheckAuthInfoResult);
        const qlonglong idCheck = server.checkAuthInfoAsync(info, windowId, 17 /*usertime*/);
        QCOMPARE(idCheck, 0LL); // always
        QCOMPARE(spyCheck.count(), 0); // no reply yet

        // Wait for the query to be processed
        QVERIFY(QTest::kWaitForSignal(&server, sigQueryAuthInfoResult, 1000));
        QCOMPARE(spyQuery.count(), 1);
        QCOMPARE(spyQuery[0][0].toLongLong(), id);
        KIO::AuthInfo result = spyQuery[0][2].value<KIO::AuthInfo>();

        // Now the check will have replied
        QCOMPARE(spyCheck.count(), 1);
        QCOMPARE(spyCheck[0][0].toLongLong(), id+1); // it was the next request after the query
        KIO::AuthInfo resultCheck = spyCheck[0][2].value<KIO::AuthInfo>();
        QCOMPARE(result.username, resultCheck.username);
        QCOMPARE(result.password, resultCheck.password);
    }

    void testExpiry()
    {
        KPasswdServer server(this);
        server.setWalletDisabled(true);
        KIO::AuthInfo info;
        info.url = KUrl("http://www.example.com");

        // Add auth to the cache
        const qlonglong windowId = 42;
        KIO::AuthInfo realInfo = info;
        realInfo.username = "toto";
        realInfo.password = "foobar";
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
        info.url = KUrl("http://www.example.com");

        // What the user would type
        KIO::AuthInfo filledInfo(info);
        filledInfo.username = "dfaure";
        filledInfo.password = "toto";

        KIO::AuthInfo result;
        queryAuthWithDialog(server, info, filledInfo, result);
    }

    void testRejectRetryDialog()
    {
        KPasswdServer server(this);
        server.setWalletDisabled(true);

       // What the app would ask
        KIO::AuthInfo info;
        info.url = KUrl("http://www.example.com");

        // What the user would type
        KIO::AuthInfo filledInfo(info);
        filledInfo.username = "username";
        filledInfo.password = "password";

        KIO::AuthInfo result;
        queryAuthWithDialog(server, info, filledInfo, result);

        // Pretend that the returned credentials failed and initiate a retry,
        // but cancel the retry dialog.
        info.password.clear();
        result = KIO::AuthInfo();
        queryAuthWithDialog(server, info, filledInfo, result, QDialog::Rejected, QLatin1String("Invalid username or password"));
    }

    void testAcceptRetryDialog()
    {
        KPasswdServer server(this);
        server.setWalletDisabled(true);

       // What the app would ask
        KIO::AuthInfo info;
        info.url = KUrl("http://www.example.com");

        // What the user would type
        KIO::AuthInfo filledInfo(info);
        filledInfo.username = "username";
        filledInfo.password = "password";

        KIO::AuthInfo result;
        queryAuthWithDialog(server, info, filledInfo, result);

        // Pretend that the returned credentials failed and initiate a retry,
        // but this time continue the retry.
        info.password.clear();
        result = KIO::AuthInfo();
        queryAuthWithDialog(server, info, filledInfo, result, QDialog::Accepted, QLatin1String("Invalid username or password"));
    }

    void testUsernameMistmatch()
    {
        KPasswdServer server(this);
        server.setWalletDisabled(true);

        // What the app would ask. Note the username in the URL.
        KIO::AuthInfo info;
        info.url = KUrl("http://foo@www.example.com");

        // What the user would type
        KIO::AuthInfo filledInfo(info);
        filledInfo.username = "bar";
        filledInfo.password = "blah";

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
        filledInfo.url = KUrl("http://bar@www.example.com");
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
        info.url = KUrl("http://www.example.com");
        info.username = info.url.user();

        KIO::AuthInfo result;
        queryAuthWithDialog(server, info, KIO::AuthInfo(), result, QDialog::Rejected);
    }

    void testVerifyPath()
    {
        KPasswdServer server(this);
        server.setWalletDisabled(true);

        // Add auth to the cache
        const qlonglong windowId = 42;
        KIO::AuthInfo authInfo;
        authInfo.url = KUrl("http://www.example.com/test/test.html");
        authInfo.username = "toto";
        authInfo.password = "foobar";
        server.addAuthInfo(authInfo, windowId);

        KIO::AuthInfo queryAuthInfo;
        queryAuthInfo.url = KUrl("http://www.example.com/test/test2/test.html");
        queryAuthInfo.verifyPath = true;

        KIO::AuthInfo expectedAuthInfo;
        expectedAuthInfo.username = "toto";
        expectedAuthInfo.password = "foobar";

        QVERIFY(successCheckAuth(server, queryAuthInfo, expectedAuthInfo));
    }

    void testConcurrentQueryAuth()
    {
        KPasswdServer server(this);
        server.setWalletDisabled(true);

        QList<KIO::AuthInfo> authInfos;
        for (int i=0; i < 10; ++i) {
           KIO::AuthInfo info;
           info.url = KUrl("http://www.example.com/test" + QString::number(i) + ".html");
           authInfos << info;
        }

        // What the user would type
        KIO::AuthInfo filledInfo;
        filledInfo.username = "bar";
        filledInfo.password = "blah";

        QList<KIO::AuthInfo> results;
        concurrentQueryAuthWithDialog(server, authInfos, filledInfo, results);
    }

    void testConcurrentCheckAuth()
    {
        KPasswdServer server(this);
        server.setWalletDisabled(true);

        QList<KIO::AuthInfo> authInfos;
        for (int i=0; i < 10; ++i) {
           KIO::AuthInfo info;
           info.url = KUrl("http://www.example.com/test" + QString::number(i) + ".html");
           authInfos << info;
        }

        // What the user would type
        KIO::AuthInfo filledInfo;
        filledInfo.username = "bar";
        filledInfo.password = "blah";

        QList<KIO::AuthInfo> results;
        concurrentQueryAuthWithDialog(server, authInfos, filledInfo, results);
    }

private:
    // Checks that no auth is available for @p info
    bool noCheckAuth(KPasswdServer& server, const KIO::AuthInfo& info)
    {
        KIO::AuthInfo result;
        checkAuth(server, info, result);
        return (result.username == info.username)
            && (result.password == info.password)
            && !result.isModified();
    }

    // Check that the auth is available and equal to @expectedInfo
    bool successCheckAuth(KPasswdServer& server, const KIO::AuthInfo& info, const KIO::AuthInfo& expectedInfo)
    {
        KIO::AuthInfo result;
        checkAuth(server, info, result);
        return (result.username == expectedInfo.username)
            && (result.password == expectedInfo.password)
            && result.isModified();
    }

    void checkAuth(KPasswdServer& server, const KIO::AuthInfo& info, KIO::AuthInfo& result)
    {
        QSignalSpy spy(&server, sigCheckAuthInfoResult);
        const qlonglong windowId = 42;
        const qlonglong id = server.checkAuthInfoAsync(info, windowId, 17 /*usertime*/);
        QCOMPARE(id, 0LL); // always
        if (spy.isEmpty()) {
            QVERIFY(QTest::kWaitForSignal(&server, sigCheckAuthInfoResult, 1000));
        }
        QCOMPARE(spy.count(), 1);
        // kpasswdserver emits a requestId via dbus, we can't get that id here
        QVERIFY(spy[0][0].toLongLong() >= 0);
        //QCOMPARE(spy[0][1].toLongLong(), 3LL); // seqNr
        result = spy[0][2].value<KIO::AuthInfo>();
    }

    void queryAuth(KPasswdServer& server, const KIO::AuthInfo& info, KIO::AuthInfo& result)
    {
        QSignalSpy spy(&server, sigQueryAuthInfoResult);
        const qlonglong windowId = 42;
        const qlonglong seqNr = 2;
        const qlonglong id = server.queryAuthInfoAsync(
            info,
            QString("<NoAuthPrompt>"), // magic string to avoid a dialog
            windowId, seqNr, 16 /*usertime*/);
        QVERIFY(id >= 0); // requestId, ever increasing
        if (spy.isEmpty())
            QVERIFY(QTest::kWaitForSignal(&server, sigQueryAuthInfoResult, 1000));
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy[0][0].toLongLong(), id);
        //QCOMPARE(spy[0][1].toLongLong(), 3LL); // seqNr
        result = spy[0][2].value<KIO::AuthInfo>();
    }

    void queryAuthWithDialog(KPasswdServer& server, const KIO::AuthInfo& info,
                             const KIO::AuthInfo& filledInfo, KIO::AuthInfo& result,
                             int code = QDialog::Accepted, const QString& errMsg = QString())
    {
        QSignalSpy spy(&server, sigQueryAuthInfoResult);
        const qlonglong windowId = 42;
        const qlonglong seqNr = 2;
        const qlonglong id = server.queryAuthInfoAsync(
            info,
            errMsg,
            windowId, seqNr, 16 /*usertime*/);
        QVERIFY(id >= 0); // requestId, ever increasing
        QVERIFY(spy.isEmpty());

        const bool hasErrorMessage = (!errMsg.isEmpty());
        const bool isCancelRetryDialogTest = (hasErrorMessage && code == QDialog::Rejected);

        if (hasErrorMessage) {
            // Retry dialog only knows Yes/No
            const int retryCode = (code == QDialog::Accepted ? KDialog::Yes : KDialog::No);
            QMetaObject::invokeMethod(this, "checkRetryDialog",
                                      Qt::QueuedConnection, Q_ARG(int, retryCode));
        }

        if (!isCancelRetryDialogTest) {
            QMetaObject::invokeMethod(this, "checkAndFillDialog", Qt::QueuedConnection,
                                      Q_ARG(KIO::AuthInfo, info),
                                      Q_ARG(KIO::AuthInfo, filledInfo),
                                      Q_ARG(int, code));
        }
        // Force KPasswdServer to process the request now, otherwise the checkAndFillDialog needs a timer too...
        server.processRequest();
        if (spy.isEmpty())
            QVERIFY(QTest::kWaitForSignal(&server, sigQueryAuthInfoResult, 1000));
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy[0][0].toLongLong(), id);
        //QCOMPARE(spy[0][1].toLongLong(), 3LL); // seqNr
        result = spy[0][2].value<KIO::AuthInfo>();
        const QString username = (isCancelRetryDialogTest ? QString() : filledInfo.username);
        const QString password = (isCancelRetryDialogTest ? QString() : filledInfo.password);
        const bool modified = (code == QDialog::Accepted ? true : false);
        QCOMPARE(result.username, username);
        QCOMPARE(result.password, password);
        QCOMPARE(result.isModified(), modified);
    }

    void concurrentQueryAuthWithDialog(KPasswdServer& server, const QList<KIO::AuthInfo>& infos,
                                       const KIO::AuthInfo& filledInfo, QList<KIO::AuthInfo>& results,
                                       int code = QDialog::Accepted)
    {
        QSignalSpy spy(&server, sigQueryAuthInfoResult);
        const qlonglong windowId = 42;
        qlonglong seqNr = 0;
        QList<qlonglong> idList;

        Q_FOREACH(const KIO::AuthInfo& info, infos) {
            const qlonglong id = server.queryAuthInfoAsync(
                info,
                QString(),
                windowId, seqNr, 16 /*usertime*/);
            QVERIFY(id >= 0); // requestId, ever increasing
            idList << id;
        }

        QVERIFY(spy.isEmpty());
        QMetaObject::invokeMethod(this, "checkAndFillDialog", Qt::QueuedConnection,
                                  Q_ARG(KIO::AuthInfo, infos.first()),
                                  Q_ARG(KIO::AuthInfo,filledInfo),
                                  Q_ARG(int, code));

        // Force KPasswdServer to process the request now, otherwise the checkAndFillDialog needs a timer too...
        server.processRequest();
        while (spy.count() < infos.count())
            QVERIFY(QTest::kWaitForSignal(&server, sigQueryAuthInfoResult, 1000));

        QCOMPARE(spy.count(), infos.count());

        for(int i = 0, count = spy.count(); i < count; ++i) {
            QCOMPARE(spy[i][0].toLongLong(), idList.at(i));
            //QCOMPARE(spy[0][1].toLongLong(), 3LL); // seqNr
            KIO::AuthInfo result = spy[i][2].value<KIO::AuthInfo>();
            QCOMPARE(result.username, filledInfo.username);
            QCOMPARE(result.password, filledInfo.password);
            QCOMPARE(result.isModified(), (code == QDialog::Accepted ? true : false));
            results << result;
        }
    }

    void concurrentCheckAuthWithDialog(KPasswdServer& server, const QList<KIO::AuthInfo>& infos,
                                       const KIO::AuthInfo& filledInfo, QList<KIO::AuthInfo>& results,
                                       int code = QDialog::Accepted)
    {
        QSignalSpy spy(&server, sigQueryAuthInfoResult);
        const qlonglong windowId = 42;
        qlonglong seqNr = 0;
        QList<qlonglong> idList;

        QListIterator<KIO::AuthInfo> it (infos);
        if (it.hasNext()) {
            const qlonglong id = server.queryAuthInfoAsync(
                it.next(),
                QString(),
                windowId, seqNr, 16 /*usertime*/);
            QVERIFY(id >= 0); // requestId, ever increasing
            idList << id;
        }

        while (it.hasNext()) {
            const qlonglong id = server.checkAuthInfoAsync(it.next(), windowId,16 /*usertime*/);
            QVERIFY(id >= 0); // requestId, ever increasing
            idList << id;
        }

        QVERIFY(spy.isEmpty());
        QMetaObject::invokeMethod(this, "checkAndFillDialog", Qt::QueuedConnection,
                                  Q_ARG(KIO::AuthInfo, infos.first()),
                                  Q_ARG(KIO::AuthInfo,filledInfo),
                                  Q_ARG(int, code));

        // Force KPasswdServer to process the request now, otherwise the checkAndFillDialog needs a timer too...
        server.processRequest();
        if (spy.isEmpty())
            QVERIFY(QTest::kWaitForSignal(&server, sigQueryAuthInfoResult, 1000));

        while ((spy.count()-1) < infos.count()) {
            QVERIFY(QTest::kWaitForSignal(&server, sigCheckAuthInfoResult, 1000));
        }

        for(int i = 0, count = spy.count(); i < count; ++i) {
            QCOMPARE(spy[i][0].toLongLong(), idList.at(i));
            //QCOMPARE(spy[0][1].toLongLong(), 3LL); // seqNr
            KIO::AuthInfo result = spy[i][2].value<KIO::AuthInfo>();
            QCOMPARE(result.username, filledInfo.username);
            QCOMPARE(result.password, filledInfo.password);
            QCOMPARE(result.isModified(), (code == QDialog::Accepted ? true : false));
            results << result;
        }
    }

protected Q_SLOTS:
    void checkAndFillDialog(const KIO::AuthInfo& info, const KIO::AuthInfo& filledInfo, int code = QDialog::Accepted)
    {
        Q_FOREACH(QWidget *widget, QApplication::topLevelWidgets()) {
            if (KPasswordDialog* dialog = qobject_cast<KPasswordDialog *>(widget)) {
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
        kWarning() << "No KPasswordDialog found!";
    }

    void checkRetryDialog(int code = QDialog::Accepted)
    {
        Q_FOREACH(QWidget *widget, QApplication::topLevelWidgets()) {
            KDialog* dialog = qobject_cast<KDialog*>(widget);
            if (dialog && !dialog->inherits("KPasswordDialog")) {
                dialog->done(code);
                return;
            }
        }
    }
};

QTEST_KDEMAIN( KPasswdServerTest, GUI )

#include "kpasswdservertest.moc"
