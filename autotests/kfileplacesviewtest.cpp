/*
    This file is part of the KDE project
    SPDX-FileCopyrightText: 2017 Renato Araujo Oliveira Filho <renato.araujo@kdab.com>

    SPDX-License-Identifier: GPL-2.0-only
*/

#include <QFile>
#include <QObject>
#include <QStandardPaths>
#include <QTemporaryDir>

#include <KConfig>
#include <KConfigGroup>
#include <KProtocolInfo>
#include <kfileplacesmodel.h>
#include <kfileplacesview.h>

#include <QApplication>
#include <QHelpEvent>
#include <QPixmap>
#include <QSignalSpy>
#include <QTest>
#include <QToolTip>

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
    void testSetUrl_data();
    void testSetUrl();
    void testElidedNameGetsATooltip();

private:
    QTemporaryDir m_tmpHome;
};

void KFilePlacesViewTest::initTestCase()
{
    QVERIFY(m_tmpHome.isValid());
    qputenv("HOME", m_tmpHome.path().toUtf8());
    qputenv("KDE_FULL_SESSION", "1"); // attempt to enable recentlyused:/ if present, so we only need to test for isKnownProtocol below
    QStandardPaths::setTestModeEnabled(true);

    cleanupTestCase();

    KConfig config(QStringLiteral("baloofilerc"));
    KConfigGroup basicSettings = config.group(QStringLiteral("Basic Settings"));
    basicSettings.writeEntry("Indexing-Enabled", true);
    config.sync();

    qRegisterMetaType<QModelIndex>();

    // Debug code, to help understanding the actual test
    KFilePlacesModel model;
    for (int row = 0; row < model.rowCount(); ++row) {
        const QModelIndex index = model.index(row, 0);
        qDebug() << model.url(index);
    }
}

void KFilePlacesViewTest::cleanupTestCase()
{
    QFile::remove(bookmarksFile());
}

void KFilePlacesViewTest::testUrlChanged_data()
{
    QTest::addColumn<int>("row");
    QTest::addColumn<QString>("expectedUrl");

    int idx = 3; // skip home, trash, remote
    if (KProtocolInfo::isKnownProtocol(QStringLiteral("recentlyused"))) {
        QTest::newRow("Recent Files") << idx++ << QStringLiteral("recentlyused:/files");
        QTest::newRow("Recent Locations") << idx++ << QStringLiteral("recentlyused:/locations");
    } else {
        QTest::newRow("Modified Today") << idx++ << QStringLiteral("timeline:/today");
        ++idx; // Modified Yesterday gets turned into "timeline:/2020-06/2020-06-05"
    }
}

void KFilePlacesViewTest::testUrlChanged()
{
    QFETCH(int, row);
    QFETCH(QString, expectedUrl);

    KFilePlacesView pv;
    pv.setModel(new KFilePlacesModel(&pv));

    QSignalSpy urlChangedSpy(&pv, &KFilePlacesView::urlChanged);
    const QModelIndex targetIndex = pv.model()->index(row, 0);
    pv.scrollTo(targetIndex);
    Q_EMIT pv.clicked(targetIndex);
    QTRY_COMPARE(urlChangedSpy.count(), 1);
    const QList<QVariant> args = urlChangedSpy.takeFirst();
    QCOMPARE(args.at(0).toUrl().toString(), expectedUrl);
}

void KFilePlacesViewTest::testSetUrl_data()
{
    QTest::addColumn<QUrl>("place");
    QTest::addColumn<QUrl>("url");

    QString testPath = QString("file://%1/testSetUrl").arg(m_tmpHome.path());
    QUrl bareUrl = QUrl(testPath);
    QUrl trailingUrl = QUrl(testPath.append("/"));

    QTest::newRow("place-bare-url-bare") << bareUrl << bareUrl;
    QTest::newRow("place-bare-url-trailing") << bareUrl << trailingUrl;
    QTest::newRow("place-trailing-url-bare") << trailingUrl << bareUrl;
    QTest::newRow("place-trailing-url-trailing") << trailingUrl << trailingUrl;
}

void KFilePlacesViewTest::testSetUrl()
{
    QFETCH(QUrl, place);
    QFETCH(QUrl, url);

    KFilePlacesView pv;
    KFilePlacesModel pm;
    pv.setModel(&pm);

    pm.addPlace("testSetUrl", place);
    QModelIndex added = pm.closestItem(place);

    QSignalSpy selectionChangedSpy(pv.selectionModel(), &QItemSelectionModel::selectionChanged);
    pv.setUrl(url);

    QVERIFY(!selectionChangedSpy.isEmpty());
    const QList<QVariant> args = selectionChangedSpy.takeFirst();
    QVERIFY(args.at(0).value<QItemSelection>().indexes().contains(added));
}

void KFilePlacesViewTest::testElidedNameGetsATooltip()
{
    KFilePlacesModel model;
    KFilePlacesView view;
    view.setModel(&model);
    // Narrow enough that no name of a place fits.
    view.resize(60, 400);
    view.show();
    QVERIFY(QTest::qWaitForWindowExposed(&view));

    // Not the first row of a section: the top of that one is the header area, which has a
    // tooltip rule of its own.
    const QModelIndex index = model.index(1, 0);
    QVERIFY(index.isValid());
    const QString name = index.data(Qt::DisplayRole).toString();

    // The tooltip says what the row holds, which the delegate knows from having drawn it.
    QPixmap target(view.viewport()->size());
    view.viewport()->render(&target);

    const QPoint pos = view.visualRect(index).center();
    QHelpEvent event(QEvent::ToolTip, pos, view.viewport()->mapToGlobal(pos));
    QApplication::sendEvent(view.viewport(), &event);

    QTRY_VERIFY(QToolTip::text().contains(name));
}

QTEST_MAIN(KFilePlacesViewTest)

#include "kfileplacesviewtest.moc"
