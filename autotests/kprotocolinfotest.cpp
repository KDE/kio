/*
    SPDX-FileCopyrightText: 2002 David Faure <faure@kde.org>

    SPDX-License-Identifier: LGPL-2.0-only
*/

#include <KConfig>
#include <KConfigGroup>
#include <KPluginMetaData>
#include <QDebug>
#include <QFile>
#include <QJsonObject>
#include <QStandardPaths>
#include <QTest>
#include <QUrl>
#include <algorithm>
#include <kprotocolmanager.h>
#include <kprotocolmanager_p.h>

// Tests both KProtocolInfo and KProtocolManager

class KProtocolInfoTest : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void initTestCase();

    void testBasic();
    void testExtraFields();
    void testShowFilePreview();
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
    QVERIFY(KProtocolInfo::exec(QStringLiteral("file")).contains(QStringLiteral("kf6/kio/kio_file")));
    QCOMPARE(KProtocolInfo::protocolClass(QStringLiteral("file")), QStringLiteral(":local"));

    QCOMPARE(KProtocolInfo::protocolClass(QStringLiteral("http")), QStringLiteral(":internet"));

    QCOMPARE(KProtocolInfo::defaultMimetype(QStringLiteral("help")), QString("text/html"));
    QCOMPARE(KProtocolInfo::defaultMimetype(QStringLiteral("http")), QString("application/octet-stream"));

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

void KProtocolInfoTest::testCapabilities()
{
    QStringList capabilities = KProtocolInfo::capabilities(QStringLiteral("imap"));
    qDebug() << "kio_imap capabilities: " << capabilities;
    // QVERIFY(capabilities.contains("ACL"));
}

void KProtocolInfoTest::testProtocolForArchiveMimetype()
{
    // The zip protocol is available at least with the kio_archive worker from kio-extras repo (11 2022)
    auto supportsZipProtocol = [](const KPluginMetaData &metaData) {
        const QJsonObject protocols = metaData.rawData().value(QStringLiteral("KDE-KIO-Protocols")).toObject();
        return (protocols.find(QLatin1String("zip")) != protocols.end());
    };

    const QList<KPluginMetaData> workers = KPluginMetaData::findPlugins(QStringLiteral("kf6/kio"));
    if (std::none_of(workers.cbegin(), workers.cend(), supportsZipProtocol)) {
        QSKIP("kio-extras not installed");
    } else {
        const QString zip = KProtocolManager::protocolForArchiveMimetype(QStringLiteral("application/zip"));
        // Krusader's kio_krarc.so also provides the zip protocol and might be found before/instead
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
    // QVERIFY(KProtocolInfo::isHelperProtocol("telnet"));

    // To test that compat still works
    if (KProtocolInfo::isKnownProtocol(QStringLiteral("tel"))) {
        QVERIFY(KProtocolInfo::isHelperProtocol(QStringLiteral("tel")));
    }
}

QTEST_MAIN(KProtocolInfoTest)

#include "kprotocolinfotest.moc"
