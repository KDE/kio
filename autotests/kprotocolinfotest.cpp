/*
 *  Copyright (C) 2002 David Faure   <faure@kde.org>
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Library General Public
 *  License version 2 as published by the Free Software Foundation;
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Library General Public License for more details.
 *
 *  You should have received a copy of the GNU Library General Public License
 *  along with this library; see the file COPYING.LIB.  If not, write to
 *  the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 *  Boston, MA 02110-1301, USA.
 */

#include <QFile>
#include <QUrl>
#include <kprotocolmanager.h>
#include <QDebug>
#include <QtTest>
#include <qstandardpaths.h>
#include <kservice.h>

// Tests both KProtocolInfo and KProtocolManager

class KProtocolInfoTest : public QObject
{
    Q_OBJECT
private Q_SLOTS:

    void testBasic();
    void testExtraFields();
    void testShowFilePreview();
    void testSlaveProtocol();
    void testCapabilities();
    void testProtocolForArchiveMimetype();
    void testHelperProtocols();
};

void KProtocolInfoTest::testBasic()
{
    QVERIFY( KProtocolInfo::isKnownProtocol(QUrl("http:/")) );
    QVERIFY( KProtocolInfo::isKnownProtocol(QUrl("file:/")) );
    QCOMPARE(KProtocolInfo::exec("file"), QString::fromLatin1("kio_file"));
    QCOMPARE(KProtocolInfo::protocolClass("file"), QString::fromLatin1(":local"));

    QCOMPARE(KProtocolInfo::protocolClass("http"), QString::fromLatin1(":internet"));

    QVERIFY( KProtocolManager::supportsListing( QUrl( "ftp://10.1.1.10") ) );

    const QUrl url = QUrl::fromLocalFile("/tmp");
    QCOMPARE( KProtocolManager::inputType(url), KProtocolInfo::T_NONE );
    QCOMPARE( KProtocolManager::outputType(url), KProtocolInfo::T_FILESYSTEM );
    QVERIFY(KProtocolManager::supportsReading(url));
}

void KProtocolInfoTest::testExtraFields()
{
    KProtocolInfo::ExtraFieldList extraFields = KProtocolInfo::extraFields(QUrl("trash:/"));
    KProtocolInfo::ExtraFieldList::Iterator extraFieldsIt = extraFields.begin();
    for ( ; extraFieldsIt != extraFields.end() ; ++extraFieldsIt )
        qDebug() << (*extraFieldsIt).name << " " << (*extraFieldsIt).type;
    // TODO
}

void KProtocolInfoTest::testShowFilePreview()
{
    QVERIFY(KProtocolInfo::showFilePreview("file"));
    QVERIFY(!KProtocolInfo::showFilePreview("audiocd"));
}

void KProtocolInfoTest::testSlaveProtocol()
{
    QString proxy;
    QString protocol = KProtocolManager::slaveProtocol(QUrl("http://bugs.kde.org"), proxy);
    QCOMPARE(protocol, QString::fromLatin1("http"));
}

void KProtocolInfoTest::testCapabilities()
{
    QStringList capabilities = KProtocolInfo::capabilities( "imap" );
    qDebug() << "kio_imap capabilities: " << capabilities;
    //QVERIFY(capabilities.contains("ACL"));
}

void KProtocolInfoTest::testProtocolForArchiveMimetype()
{
    if (!QFile::exists(QStandardPaths::locate(QStandardPaths::GenericDataLocation, QLatin1String("kde5/services/") + "zip.protocol"))) {
        QSKIP("kdebase not installed");
    } else {
        const QString zip = KProtocolManager::protocolForArchiveMimetype("application/zip");
        QCOMPARE(zip, QString("zip"));
    }
}

void KProtocolInfoTest::testHelperProtocols()
{
    QVERIFY(!KProtocolInfo::isHelperProtocol("http"));
    QVERIFY(!KProtocolInfo::isHelperProtocol("ftp"));
    QVERIFY(!KProtocolInfo::isHelperProtocol("file"));
    QVERIFY(!KProtocolInfo::isHelperProtocol("unknown"));
    // Comes from ktelnetservice.desktop:MimeType=x-scheme-handler/telnet;x-scheme-handler/rlogin;x-scheme-handler/ssh;
    // TODO: this logic has moved to KRun. Should it be public API, so we can unittest it?
    //QVERIFY(KProtocolInfo::isHelperProtocol("telnet"));

    // To test that compat still works
    if (KProtocolInfo::isKnownProtocol("tel")) {
        QVERIFY(KProtocolInfo::isHelperProtocol("tel"));
    }

    // TODO: this logic has moved to KRun. Should it be public API, so we can unittest it?
#if 0
    QVERIFY(KProtocolInfo::isKnownProtocol("mailto"));
    QVERIFY(KProtocolInfo::isHelperProtocol("mailto"));
    QVERIFY(KProtocolInfo::isHelperProtocol(QUrl("mailto:faure@kde.org")));

    // "mailto" is associated with kmail2 when present, and with kmailservice otherwise.
    KService::Ptr kmail2 = KService::serviceByStorageId("KMail2.desktop");
    if (kmail2) {
        //qDebug() << kmail2->entryPath();
        QVERIFY2(KProtocolInfo::exec("mailto").contains(QLatin1String("kmail -caption \"%c\"")), // comes from KMail2.desktop
                 qPrintable(KProtocolInfo::exec("mailto")));
    } else {
        QCOMPARE(KProtocolInfo::exec("mailto"), QLatin1String("kmailservice %u"));
    }
#endif
}

QTEST_MAIN(KProtocolInfoTest)

#include "kprotocolinfotest.moc"
