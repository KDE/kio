/*
    SPDX-FileCopyrightText: 2026 Méven Car <meven@kde.org>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#include <KApplicationTrader>
#include <KConfigGroup>
#include <KIO/OpenWith>
#include <KOpenWithDialog>
#include <KService>
#include <KSharedConfig>

#include <QCheckBox>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QMimeDatabase>
#include <QProcess>
#include <QStandardPaths>
#include <QTest>

#include "openwith_p.h"

using namespace Qt::StringLiterals;

extern KSERVICE_EXPORT int ksycoca_ms_between_checks;

// A file of no known type gets a type of its own for its extension when the user remembers an
// application for it, instead of the association going to application/octet-stream, which every
// file of no known type has, and which every other type inherits (bug 425154).
class OpenWithTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase()
    {
        QStandardPaths::setTestModeEnabled(true);
        ksycoca_ms_between_checks = 0; // the test rebuilds the sycoca and reads it at once
        const QString dataDir = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation);
        m_definition = dataDir + u"/mime/packages/user-extension-kiotest.xml"_s;
        QFile::remove(m_definition);
        removeAssociations();

        // An application without a MimeType key, as one that the user writes by hand. KService reads
        // the applications of ApplicationsLocation, which is not under GenericDataLocation on Windows.
        const QString applicationsDir = QStandardPaths::writableLocation(QStandardPaths::ApplicationsLocation);
        QVERIFY(QDir().mkpath(applicationsDir));
        m_application = applicationsDir + u"/org.kde.kio.openwithtest.desktop"_s;
        QFile application(m_application);
        QVERIFY(application.open(QIODevice::WriteOnly));
        application.write(
            "[Desktop Entry]\n"
            "Type=Application\n"
            "Name=Open With Test\n"
            "Exec=true %f\n");
    }

    void cleanupTestCase()
    {
        QFile::remove(m_definition);
        QFile::remove(m_application);
        removeAssociations();
        const QString mimeDir = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) + u"/mime"_s;
        const QString updateMimeDatabase = QStandardPaths::findExecutable(u"update-mime-database"_s);
        if (!updateMimeDatabase.isEmpty()) {
            QProcess::execute(updateMimeDatabase, {mimeDir});
        }
        rebuildSycoca();
    }

    void createExtensionMimeType()
    {
        if (QStandardPaths::findExecutable(u"update-mime-database"_s).isEmpty()) {
            QSKIP("update-mime-database is not installed");
        }
        // The type name is lowercase, as GLib makes it, and the glob matches any case.
        const KIO::ExtensionMimeTypeResult created = KIO::createExtensionMimeType(u"KioTest"_s);
        QVERIFY2(created.error.isEmpty(), qPrintable(created.error));
        QCOMPARE(created.mimeType, u"application/x-extension-kiotest"_s);

        QFile definition(m_definition);
        QVERIFY(definition.open(QIODevice::ReadOnly));
        const QByteArray content = definition.readAll();
        QVERIFY(content.contains(R"(<mime-type type="application/x-extension-kiotest">)"));
        QVERIFY(content.contains(R"(<glob pattern="*.kiotest"/>)"));

        // QMimeDatabase rereads the database when it has changed, at most every few seconds.
        QTRY_COMPARE_WITH_TIMEOUT(QMimeDatabase().mimeTypeForFile(u"file.kiotest"_s, QMimeDatabase::MatchExtension).name(),
                                  u"application/x-extension-kiotest"_s,
                                  10000);
        QCOMPARE(QMimeDatabase().mimeTypeForFile(u"FILE.KIOTEST"_s, QMimeDatabase::MatchExtension).name(), u"application/x-extension-kiotest"_s);

        // A type that exists already is used as it is.
        QCOMPARE(KIO::createExtensionMimeType(u"kiotest"_s).mimeType, u"application/x-extension-kiotest"_s);
    }

    void rememberedAssociationUsesTheExtensionType()
    {
        // What the dialog does on accept with "Remember" ticked for a file of no known type: define
        // the type, remember the association, and rebuild the sycoca in a process of its own.
        if (QStandardPaths::findExecutable(u"update-mime-database"_s).isEmpty() || QStandardPaths::findExecutable(u"kbuildsycoca6"_s).isEmpty()) {
            QSKIP("update-mime-database or kbuildsycoca6 is not installed");
        }
        // The type of createExtensionMimeType(), which this process's QMimeDatabase knows already. It
        // rereads the database at most every few seconds, and KApplicationTrader looks types up in it.
        const KIO::ExtensionMimeTypeResult created = KIO::createExtensionMimeType(u"kiotest"_s);
        QVERIFY2(created.error.isEmpty(), qPrintable(created.error));
        QVERIFY(QMimeDatabase().mimeTypeForName(created.mimeType).isValid());
        // KSycoca reads the MIME types from the files that update-mime-database generates. Built with
        // MSVC, update-mime-database 2.4 only prints its version, so they are missing on Windows.
        const QString generated = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) + u"/mime/"_s + created.mimeType + u".xml"_s;
        if (!QFileInfo::exists(generated)) {
            QSKIP("update-mime-database did not generate the definition of the new type");
        }

        rebuildSycoca();
        KService::Ptr service = KService::serviceByDesktopName(u"org.kde.kio.openwithtest"_s);
        QVERIFY(service);

        const KIO::OpenWith::AcceptResult result = KIO::OpenWith::accept(service, QString(), true, created.mimeType, false, false, false);
        QVERIFY2(result.accept, qPrintable(result.error));
        QVERIFY(result.rebuildSycoca);
        rebuildSycoca();

        // The sycoca knows the new type, so the association holds.
        const KService::Ptr preferred = KApplicationTrader::preferredService(created.mimeType);
        QVERIFY(preferred);
        QCOMPARE(preferred->desktopEntryName(), u"org.kde.kio.openwithtest"_s);

        // The application did not become the one for every file of no known type.
        const KSharedConfig::Ptr mimeApps = KSharedConfig::openConfig(u"mimeapps.list"_s, KConfig::NoGlobals, QStandardPaths::GenericConfigLocation);
        QVERIFY(!mimeApps->group(u"Default Applications"_s).hasKey("application/octet-stream"));
        const KService::Ptr octetStream = KApplicationTrader::preferredService(u"application/octet-stream"_s);
        QVERIFY(!octetStream || octetStream->desktopEntryName() != u"org.kde.kio.openwithtest"_s);
    }

    void invalidExtension_data()
    {
        QTest::addColumn<QString>("extension");
        QTest::newRow("empty") << QString();
        QTest::newRow("slash") << u"a/b"_s;
        QTest::newRow("dot") << u"tar.xz"_s;
        QTest::newRow("space") << u"a b"_s;
        QTest::newRow("not ascii") << u"é"_s;
    }

    void invalidExtension()
    {
        QFETCH(QString, extension);
        const KIO::ExtensionMimeTypeResult created = KIO::createExtensionMimeType(extension);
        QVERIFY(created.mimeType.isEmpty());
        QVERIFY(!created.error.isEmpty());
    }

    void rememberCheckboxNamesTheExtension()
    {
        // A file of no known type: the association can only be remembered for its extension.
        KOpenWithDialog dialog({QUrl::fromLocalFile(u"/tmp/model.kiounknown"_s)}, u"application/octet-stream"_s, QString(), QString());
        const QList<QCheckBox *> boxes = dialog.findChildren<QCheckBox *>();
        const auto remember = std::find_if(boxes.cbegin(), boxes.cend(), [](QCheckBox *box) {
            return box->text().contains(u"Remember"_s);
        });
        QVERIFY(remember != boxes.cend());
        QVERIFY((*remember)->text().contains(u"*.kiounknown"_s));
        QVERIFY(!(*remember)->text().contains(u"octet-stream"_s));
    }

    void noRememberCheckboxWithoutExtension()
    {
        KOpenWithDialog dialog({QUrl::fromLocalFile(u"/tmp/model"_s)}, u"application/octet-stream"_s, QString(), QString());
        const QList<QCheckBox *> boxes = dialog.findChildren<QCheckBox *>();
        QVERIFY(std::none_of(boxes.cbegin(), boxes.cend(), [](QCheckBox *box) {
            return box->text().contains(u"Remember"_s);
        }));
    }

private:
    // As KBuildSycocaProgressDialog does, in a process of its own, which reads the new MIME database.
    static void rebuildSycoca()
    {
        const QString kbuildsycoca = QStandardPaths::findExecutable(u"kbuildsycoca6"_s);
        if (!kbuildsycoca.isEmpty()) {
            QProcess::execute(kbuildsycoca, {u"--testmode"_s});
        }
    }

    static void removeAssociations()
    {
        const KSharedConfig::Ptr mimeApps = KSharedConfig::openConfig(u"mimeapps.list"_s, KConfig::NoGlobals, QStandardPaths::GenericConfigLocation);
        for (const QString &group : {u"Default Applications"_s, u"Added Associations"_s}) {
            mimeApps->group(group).deleteEntry("application/x-extension-kiotest");
        }
        mimeApps->sync();
    }

    QString m_definition;
    QString m_application;
};

QTEST_MAIN(OpenWithTest)

#include "openwithtest.moc"
