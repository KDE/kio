/* This file is part of the KDE project
    Copyright 2010 David Faure <faure@kde.org>

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

    // TODO check closing the window and auth should be cleared (keepPassword=false) or kept (keepPassword=true)
    void testExpiry()
    {
        KPasswdServer server(this);
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

    // TODO test cancelling
    // TODO test more concurrent requests
    // TODO set info.verifyPath

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
        QVERIFY(id > 0); // requestId, ever increasing
        if (spy.isEmpty())
            QVERIFY(QTest::kWaitForSignal(&server, sigQueryAuthInfoResult, 1000));
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy[0][0].toLongLong(), id);
        //QCOMPARE(spy[0][1].toLongLong(), 3LL); // seqNr
        result = spy[0][2].value<KIO::AuthInfo>();
    }

    void queryAuthWithDialog(KPasswdServer& server, const KIO::AuthInfo& info, const KIO::AuthInfo& filledInfo, KIO::AuthInfo& result)
    {
        QSignalSpy spy(&server, sigQueryAuthInfoResult);
        const qlonglong windowId = 42;
        const qlonglong seqNr = 2;
        const qlonglong id = server.queryAuthInfoAsync(
            info,
            QString("KPasswdServerTest"),
            windowId, seqNr, 16 /*usertime*/);
        QVERIFY(id > 0); // requestId, ever increasing
        QVERIFY(spy.isEmpty());
        QMetaObject::invokeMethod(this, "checkAndFillDialog", Qt::QueuedConnection, Q_ARG(KIO::AuthInfo, info), Q_ARG(KIO::AuthInfo, filledInfo));
        // Force KPasswdServer to process the request now, otherwise the checkAndFillDialog needs a timer too...
        server.processRequest();
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy[0][0].toLongLong(), id);
        //QCOMPARE(spy[0][1].toLongLong(), 3LL); // seqNr
        result = spy[0][2].value<KIO::AuthInfo>();
        QCOMPARE(result.username, filledInfo.username);
        QCOMPARE(result.password, filledInfo.password);
        QCOMPARE(result.isModified(), true);
    }

protected Q_SLOTS:
    void checkAndFillDialog(const KIO::AuthInfo& info, const KIO::AuthInfo& filledInfo)
    {
        Q_FOREACH(QWidget *widget, QApplication::topLevelWidgets()) {
            kDebug() << widget;
            if (KPasswordDialog* dialog = qobject_cast<KPasswordDialog *>(widget)) {
                QCOMPARE(dialog->username(), info.username);
                QCOMPARE(dialog->password(), info.password);
                dialog->setUsername(filledInfo.username);
                dialog->setPassword(filledInfo.password);
                dialog->done(QDialog::Accepted);
                return;
            }
        }
        kWarning() << "No KPasswordDialog found!";
    }
};

QTEST_KDEMAIN( KPasswdServerTest, GUI )

#include "kpasswdservertest.moc"
