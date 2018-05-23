/*  This file is part of the KDE project
    Copyright (C) 2017 Renato Araujo Oliveira Filho <renato.araujo@kdab.com>

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License version 2
    as published by the Free Software Foundation.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
    02110-1301, USA.

*/

#include <QObject>
#include <QDebug>
#include <QStandardPaths>
#include <QFile>
#include <QTemporaryDir>

#include <kfileplacesview.h>
#include <kfileplacesmodel.h>
#include <kconfig.h>
#include <kconfiggroup.h>

#include <QTest>
#include <QSignalSpy>

static QString bookmarksFile()
{
    return QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) + "/user-places.xbel";
}

class KFilePlacesViewTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase();
    void cleanupTestCase();

    void testUrlChanged_data();
    void testUrlChanged();
private:
    QTemporaryDir m_tmpHome;
};

void KFilePlacesViewTest::initTestCase()
{
    QVERIFY(m_tmpHome.isValid());
    qputenv("HOME", m_tmpHome.path().toUtf8());
    qputenv("KDE_FORK_SLAVES", "yes"); // to avoid a runtime dependency on klauncher
    QStandardPaths::setTestModeEnabled(true);

    cleanupTestCase();

    KConfig config(QStringLiteral("baloofilerc"));
    KConfigGroup basicSettings = config.group("Basic Settings");
    basicSettings.writeEntry("Indexing-Enabled", true);
    config.sync();

    qRegisterMetaType<QModelIndex>();
}

void KFilePlacesViewTest::cleanupTestCase()
{
    QFile::remove(bookmarksFile());
}

void KFilePlacesViewTest::testUrlChanged_data()
{
    QTest::addColumn<int>("row");
    QTest::addColumn<QString>("expectedUrl");

    const QDate currentDate = QDate::currentDate();
    const QDate yesterdayDate = currentDate.addDays(-1);
    QTest::newRow("Today") << 4 << QStringLiteral("timeline:/today");
    QTest::newRow("Yesterday") << 5 << QString("timeline:/%1-%2/%1-%2-%3")
                                  .arg(yesterdayDate.year())
                                  .arg(yesterdayDate.month(), 2, 10, QChar('0'))
                                  .arg(yesterdayDate.day(), 2, 10, QChar('0'));

    // search
    const QString baloonurl = QStringLiteral("baloosearch:?json=%7B%22dayFilter%22: 0, %22monthFilter%22: 0, %22yearFilter%22: 0, %22type%22: [ %22$TYPE$%22]%7D");
    const QString typeToReplace = QStringLiteral("$TYPE$");

    QTest::newRow("Documents") << 6 << QString(baloonurl).replace(typeToReplace, QStringLiteral("Document"));
    QTest::newRow("Images") << 7 << QString(baloonurl).replace(typeToReplace, QStringLiteral("Image"));
    QTest::newRow("Audio Files") << 8 << QString(baloonurl).replace(typeToReplace, QStringLiteral("Audio"));
    QTest::newRow("Videos") << 9 << QString(baloonurl).replace(typeToReplace, QStringLiteral("Video"));
}

void KFilePlacesViewTest::testUrlChanged()
{
    QFETCH(int, row);
    QFETCH(QString, expectedUrl);

    KFilePlacesView pv;
    pv.show();
    pv.setModel(new KFilePlacesModel());
    QVERIFY(QTest::qWaitForWindowActive(&pv));

    QSignalSpy urlChangedSpy(&pv, &KFilePlacesView::urlChanged);
    const QModelIndex targetIndex = pv.model()->index(row, 0);
    pv.scrollTo(targetIndex);
    pv.clicked(targetIndex);
    QTRY_COMPARE(urlChangedSpy.count(), 1);
    const QList<QVariant> args = urlChangedSpy.takeFirst();
    QCOMPARE(args.at(0).toUrl().toString(), expectedUrl);
}

QTEST_MAIN(KFilePlacesViewTest)

#include "kfileplacesviewtest.moc"
