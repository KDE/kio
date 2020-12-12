/*
    SPDX-FileCopyrightText: 2002 David Faure <faure@kde.org>

    SPDX-License-Identifier: LGPL-2.0-only
*/

#include <QFile>
#include <QUrl>
#include <kprotocolmanager.h>
#include <KConfig>
#include <KConfigGroup>
#include <QDebug>
#include <QTest>
#include <QStandardPaths>

// Tests both KProtocolInfo and KProtocolManager

class KProtocolInfoTest : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void initTestCase();

    void testBasic();
    void testExtraFields();
    void testShowFilePreview();
    void testSlaveProtocol();
    void testProxySettings_data();
    void testProxySettings();
    void testCapabilities();
    void testProtocolForArchiveMimetype();
    void testHelperProtocols();
};

void KProtocolInfoTest::initTestCase()
{
    QString configFile = QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation) + "/kioslaverc";
    QFile::remove(configFile);
}

void KProtocolInfoTest::testBasic()
{
    QVERIFY(KProtocolInfo::isKnownProtocol(QUrl(QStringLiteral("http:/"))));
    QVERIFY(KProtocolInfo::isKnownProtocol(QUrl(QStringLiteral("file:/"))));
    QVERIFY(KProtocolInfo::exec(QStringLiteral("file")).contains(QLatin1String("kf5/kio/file")));
    QCOMPARE(KProtocolInfo::protocolClass(QStringLiteral("file")), QStringLiteral(":local"));

    QCOMPARE(KProtocolInfo::protocolClass(QStringLiteral("http")), QStringLiteral(":internet"));

    QCOMPARE(KProtocolInfo::defaultMimetype(QStringLiteral("ftp")), QString());
    QCOMPARE(KProtocolInfo::defaultMimetype(QStringLiteral("rtsp")), QString("audio/x-pn-realaudio"));
    QCOMPARE(KProtocolInfo::defaultMimetype(QStringLiteral("data")), QString("application/octet-stream"));

    QVERIFY(KProtocolManager::supportsListing(QUrl(QStringLiteral("ftp://10.1.1.10"))));

    const QUrl url = QUrl::fromLocalFile(QStringLiteral("/tmp"));
    QCOMPARE(KProtocolManager::inputType(url), KProtocolInfo::T_NONE);
    QCOMPARE(KProtocolManager::outputType(url), KProtocolInfo::T_FILESYSTEM);
    QVERIFY(KProtocolManager::supportsReading(url));
}

void KProtocolInfoTest::testExtraFields()
{
    KProtocolInfo::ExtraFieldList extraFields = KProtocolInfo::extraFields(QUrl(QStringLiteral("trash:/")));
    KProtocolInfo::ExtraFieldList::Iterator extraFieldsIt = extraFields.begin();
    for (; extraFieldsIt != extraFields.end(); ++extraFieldsIt) {
        qDebug() << (*extraFieldsIt).name << " " << (*extraFieldsIt).type;
    }
    // TODO
}

void KProtocolInfoTest::testShowFilePreview()
{
    QVERIFY(KProtocolInfo::showFilePreview(QStringLiteral("file")));
    QVERIFY(!KProtocolInfo::showFilePreview(QStringLiteral("audiocd")));
}

void KProtocolInfoTest::testSlaveProtocol()
{
    QString proxy;
    QString protocol = KProtocolManager::slaveProtocol(QUrl(QStringLiteral("http://bugs.kde.org")), proxy);
    QCOMPARE(protocol, QStringLiteral("http"));
    QVERIFY(!KProtocolManager::useProxy());

    // Just to test it doesn't deadlock
    KProtocolManager::reparseConfiguration();
    protocol = KProtocolManager::slaveProtocol(QUrl(QStringLiteral("http://bugs.kde.org")), proxy);
    QCOMPARE(protocol, QStringLiteral("http"));
}

void KProtocolInfoTest::testProxySettings_data()
{
    QTest::addColumn<int>("proxyType");

    // Just to test it doesn't deadlock (bug 346214)
    QTest::newRow("manual") << static_cast<int>(KProtocolManager::ManualProxy);
    QTest::newRow("wpad") << static_cast<int>(KProtocolManager::WPADProxy);
    // Same for bug 350890
    QTest::newRow("envvar") << static_cast<int>(KProtocolManager::EnvVarProxy);
}

void KProtocolInfoTest::testProxySettings()
{
    QFETCH(int, proxyType);
    KConfig config(QStringLiteral("kioslaverc"), KConfig::NoGlobals);
    KConfigGroup cfg(&config, "Proxy Settings");
    cfg.writeEntry("ProxyType", proxyType);
    cfg.sync();
    KProtocolManager::reparseConfiguration();
    QString proxy;
    QString protocol = KProtocolManager::slaveProtocol(QUrl(QStringLiteral("http://bugs.kde.org")), proxy);
    QCOMPARE(protocol, QStringLiteral("http"));
    QVERIFY(KProtocolManager::useProxy());

    // restore
    cfg.writeEntry("ProxyType", static_cast<int>(KProtocolManager::NoProxy));
    cfg.sync();
    KProtocolManager::reparseConfiguration();
}

void KProtocolInfoTest::testCapabilities()
{
    QStringList capabilities = KProtocolInfo::capabilities(QStringLiteral("imap"));
    qDebug() << "kio_imap capabilities: " << capabilities;
    //QVERIFY(capabilities.contains("ACL"));
}

void KProtocolInfoTest::testProtocolForArchiveMimetype()
{
    if (!QFile::exists(QStandardPaths::locate(QStandardPaths::GenericDataLocation, QLatin1String("kservices5/") + "zip.protocol"))) {
        QSKIP("kdebase not installed");
    } else {
        const QString zip = KProtocolManager::protocolForArchiveMimetype(QStringLiteral("application/zip"));
        QVERIFY(zip == QLatin1String("zip") || zip == QLatin1String("krarc"));
    }
}

void KProtocolInfoTest::testHelperProtocols()
{
    QVERIFY(!KProtocolInfo::isHelperProtocol(QStringLiteral("http")));
    QVERIFY(!KProtocolInfo::isHelperProtocol(QStringLiteral("ftp")));
    QVERIFY(!KProtocolInfo::isHelperProtocol(QStringLiteral("file")));
    QVERIFY(!KProtocolInfo::isHelperProtocol(QStringLiteral("unknown")));
    // Comes from ktelnetservice.desktop:MimeType=x-scheme-handler/telnet;x-scheme-handler/rlogin;x-scheme-handler/ssh;
    // TODO: this logic has moved to KRun. Should it be public API, so we can unittest it?
    //QVERIFY(KProtocolInfo::isHelperProtocol("telnet"));

    // To test that compat still works
    if (KProtocolInfo::isKnownProtocol(QStringLiteral("tel"))) {
        QVERIFY(KProtocolInfo::isHelperProtocol(QStringLiteral("tel")));
    }
}

QTEST_MAIN(KProtocolInfoTest)

#include "kprotocolinfotest.moc"
