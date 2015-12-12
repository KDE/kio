/*
 *  Copyright (C) 2004 David Faure <faure@kde.org>
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

#include <QtTest/QtTest>
#include <qapplication.h>
#include <kurlcompletion.h>
#include <KUser>
#include <QDebug>
#include <QtCore/QDir>
#include <QtCore/QFile>
#include <qtemporarydir.h>
#include <QThread>
#include <qplatformdefs.h>

class KUrlCompletionTest : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void test();

public:
    KUrlCompletionTest() {}
    ~KUrlCompletionTest()
    {
        teardown();
    }
    void setup();
    void teardown();
    void testLocalRelativePath();
    void testLocalAbsolutePath();
    void testLocalURL();
    void testEmptyCwd();
    void testBug346920();
    void testUser();

private:
    void waitForCompletion();
    KUrlCompletion *m_completion;
    QTemporaryDir *m_tempDir;
    QUrl m_dirURL;
    QString m_dir;
    KUrlCompletion *m_completionEmptyCwd;
};

void KUrlCompletionTest::setup()
{
    qDebug();
    m_completion = new KUrlCompletion;
    m_tempDir = new QTemporaryDir;
    m_dir = m_tempDir->path();
    m_dir += QLatin1String("/Dir With#Spaces/");
    QDir().mkdir(m_dir);
    qDebug() << "m_dir=" << m_dir;
    m_completion->setDir(QUrl::fromLocalFile(m_dir));
    m_dirURL = QUrl::fromLocalFile(m_dir);

    QFile f1(m_dir + "/file1");
    bool ok = f1.open(QIODevice::WriteOnly);
    QVERIFY(ok);
    f1.close();

    QFile f2(m_dir + "/file#a");
    ok = f2.open(QIODevice::WriteOnly);
    QVERIFY(ok);
    f2.close();

    QFile f3(m_dir + "/file.");
    ok = f3.open(QIODevice::WriteOnly);
    QVERIFY(ok);
    f3.close();

    QDir().mkdir(m_dir + "/file_subdir");
    QDir().mkdir(m_dir + "/.1_hidden_file_subdir");
    QDir().mkdir(m_dir + "/.2_hidden_file_subdir");

    m_completionEmptyCwd = new KUrlCompletion;
    m_completionEmptyCwd->setDir(QUrl());
}

void KUrlCompletionTest::teardown()
{
    delete m_completion;
    m_completion = 0;
    delete m_tempDir;
    m_tempDir = 0;
    delete m_completionEmptyCwd;
    m_completionEmptyCwd = 0;
}
void KUrlCompletionTest::waitForCompletion()
{
    while (m_completion->isRunning()) {
        qDebug() << "waiting for thread...";
        QThread::usleep(10);
    }
}

void KUrlCompletionTest::testLocalRelativePath()
{
    qDebug();
    // Completion from relative path, with two matches
    m_completion->makeCompletion(QStringLiteral("f"));
    waitForCompletion();
    QStringList comp1all = m_completion->allMatches();
    qDebug() << comp1all;
    QCOMPARE(comp1all.count(), 4);
    QVERIFY(comp1all.contains("file1"));
    QVERIFY(comp1all.contains("file#a"));
    QVERIFY(comp1all.contains("file."));
    QVERIFY(comp1all.contains("file_subdir/"));
    QString comp1 = m_completion->replacedPath(QStringLiteral("file1")); // like KUrlRequester does
    QCOMPARE(comp1, QString("file1"));

    // Completion from relative path
    qDebug() << endl << "now completing on 'file#'";
    m_completion->makeCompletion(QStringLiteral("file#"));
    waitForCompletion();
    QStringList compall = m_completion->allMatches();
    qDebug() << compall;
    QCOMPARE(compall.count(), 1);
    QCOMPARE(compall.first(), QString("file#a"));
    QString comp2 = m_completion->replacedPath(compall.first()); // like KUrlRequester does
    QCOMPARE(comp2, QString("file#a"));

    // Completion with empty string
    qDebug() << endl << "now completing on ''";
    m_completion->makeCompletion(QLatin1String(""));
    waitForCompletion();
    QStringList compEmpty = m_completion->allMatches();
    QCOMPARE(compEmpty.count(), 0);

    // Completion with '.', should find all hidden folders
    // This is broken in Qt 5.2 to 5.5 due to aba336c2b4a in qtbase,
    // fixed in https://codereview.qt-project.org/143134.
#if QT_VERSION >= QT_VERSION_CHECK(5, 6, 1)
    m_completion->makeCompletion(".");
    waitForCompletion();
    const auto compAllHidden = m_completion->allMatches();
    QCOMPARE(compAllHidden.count(), 2);
    QVERIFY(compAllHidden.contains(".1_hidden_file_subdir/"));
    QVERIFY(compAllHidden.contains(".2_hidden_file_subdir/"));
#endif

    // Completion with '.2', should find only hidden folders starting with '2'
    m_completion->makeCompletion(".2");
    waitForCompletion();
    const auto compHiddenStartingWith2 = m_completion->allMatches();
    QCOMPARE(compHiddenStartingWith2.count(), 1);
    QVERIFY(compHiddenStartingWith2.contains(".2_hidden_file_subdir/"));

    // Completion with 'file.', should only find one file
    m_completion->makeCompletion("file.");
    waitForCompletion();
    const auto compFileEndingWithDot = m_completion->allMatches();
    QCOMPARE(compFileEndingWithDot.count(), 1);
    QVERIFY(compFileEndingWithDot.contains("file."));
}

void KUrlCompletionTest::testLocalAbsolutePath()
{
    // Completion from absolute path
    qDebug() << m_dir + "file#";
    m_completion->makeCompletion(m_dir + "file#");
    waitForCompletion();
    QStringList compall = m_completion->allMatches();
    qDebug() << compall;
    QCOMPARE(compall.count(), 1);
    QString comp = compall.first();
    QCOMPARE(comp, QString(m_dir + "file#a"));
    comp = m_completion->replacedPath(comp); // like KUrlRequester does
    QCOMPARE(comp, QString(m_dir + "file#a"));

    // Completion with '.', should find all hidden folders
#if QT_VERSION >= QT_VERSION_CHECK(5, 6, 1)
    m_completion->makeCompletion(m_dir + ".");
    waitForCompletion();
    const auto compAllHidden = m_completion->allMatches();
    QCOMPARE(compAllHidden.count(), 2);
    QVERIFY(compAllHidden.contains(m_dir + ".1_hidden_file_subdir/"));
    QVERIFY(compAllHidden.contains(m_dir + ".2_hidden_file_subdir/"));
#endif

    // Completion with '.2', should find only hidden folders starting with '2'
    m_completion->makeCompletion(m_dir + ".2");
    waitForCompletion();
    const auto compHiddenStartingWith2 = m_completion->allMatches();
    QCOMPARE(compHiddenStartingWith2.count(), 1);
    QVERIFY(compHiddenStartingWith2.contains(m_dir + ".2_hidden_file_subdir/"));

    // Completion with 'file.', should only find one file
    m_completion->makeCompletion(m_dir + "file.");
    waitForCompletion();
    const auto compFileEndingWithDot = m_completion->allMatches();
    QCOMPARE(compFileEndingWithDot.count(), 1);
    QVERIFY(compFileEndingWithDot.contains(m_dir + "file."));
}

void KUrlCompletionTest::testLocalURL()
{
    // Completion from URL
    qDebug();
    QUrl url = QUrl::fromLocalFile(m_dirURL.toLocalFile() + "file");
    m_completion->makeCompletion(url.toString());
    waitForCompletion();
    QStringList comp1all = m_completion->allMatches();
    qDebug() << comp1all;
    QCOMPARE(comp1all.count(), 4);
    qDebug() << "Looking for" << m_dirURL.toString() + "file1";
    QVERIFY(comp1all.contains(m_dirURL.toString() + "file1"));
    qDebug() << "Looking for" << m_dirURL.toString() + "file.";
    QVERIFY(comp1all.contains(m_dirURL.toString() + "file."));
    QVERIFY(comp1all.contains(m_dirURL.toString() + "file_subdir/"));
    QString filehash = m_dirURL.toString() + "file%23a";
    qDebug() << "Looking for" << filehash;
    QVERIFY(comp1all.contains(filehash));
    QString filehashPath = m_completion->replacedPath(filehash); // note that it returns a path!!
    qDebug() << filehashPath;
    QCOMPARE(filehashPath, QString(m_dirURL.toLocalFile() + "file#a"));

    // Completion from URL with no match
    url = QUrl::fromLocalFile(m_dirURL.toLocalFile() + "foobar");
    qDebug() << "makeCompletion(" << url << ")";
    QString comp2 = m_completion->makeCompletion(url.toString());
    QVERIFY(comp2.isEmpty());
    waitForCompletion();
    QVERIFY(m_completion->allMatches().isEmpty());

    // Completion from URL with a ref -> no match
    url = QUrl::fromLocalFile(m_dirURL.toLocalFile() + 'f');
    url.setFragment(QStringLiteral("ref"));
    qDebug() << "makeCompletion(" << url << ")";
    m_completion->makeCompletion(url.toString());
    waitForCompletion();
    QVERIFY(m_completion->allMatches().isEmpty());

    // Completion with '.', should find all hidden folders
#if QT_VERSION >= QT_VERSION_CHECK(5, 6, 1)
    qDebug() << "makeCompletion(" << (m_dirURL.toString() + ".") << ")";
    m_completion->makeCompletion(m_dirURL.toString() + ".");
    waitForCompletion();
    const auto compAllHidden = m_completion->allMatches();
    QCOMPARE(compAllHidden.count(), 2);
    QVERIFY(compAllHidden.contains(m_dirURL.toString() + ".1_hidden_file_subdir/"));
    QVERIFY(compAllHidden.contains(m_dirURL.toString() + ".2_hidden_file_subdir/"));
#endif

    // Completion with '.2', should find only hidden folders starting with '2'
    url = QUrl::fromLocalFile(m_dirURL.toLocalFile() + ".2");
    qDebug() << "makeCompletion(" << url << ")";
    m_completion->makeCompletion(url.toString());
    waitForCompletion();
    const auto compHiddenStartingWith2 = m_completion->allMatches();
    QCOMPARE(compHiddenStartingWith2.count(), 1);
    QVERIFY(compHiddenStartingWith2.contains(m_dirURL.toString() + ".2_hidden_file_subdir/"));

    // Completion with 'file.', should only find one file
    url = QUrl::fromLocalFile(m_dirURL.toLocalFile() + "file.");
    qDebug() << "makeCompletion(" << url << ")";
    m_completion->makeCompletion(url.toString());
    waitForCompletion();
    const auto compFileEndingWithDot = m_completion->allMatches();
    QCOMPARE(compFileEndingWithDot.count(), 1);
    QVERIFY(compFileEndingWithDot.contains(m_dirURL.toString() + "file."));
}

void KUrlCompletionTest::testEmptyCwd()
{
    qDebug();
    // Completion with empty string (with a KUrlCompletion whose cwd is "")
    qDebug() << endl << "now completing on '' with empty cwd";
    m_completionEmptyCwd->makeCompletion(QLatin1String(""));
    waitForCompletion();
    QStringList compEmpty = m_completionEmptyCwd->allMatches();
    QCOMPARE(compEmpty.count(), 0);
}

void KUrlCompletionTest::testBug346920()
{
    m_completionEmptyCwd->makeCompletion(QStringLiteral("~/."));
    waitForCompletion();
    m_completionEmptyCwd->allMatches();
    // just don't crash
}

void KUrlCompletionTest::testUser()
{
    m_completionEmptyCwd->makeCompletion(QStringLiteral("~"));
    waitForCompletion();
    const auto matches = m_completionEmptyCwd->allMatches();
    foreach(const auto& user, KUser::allUserNames()) {
        QVERIFY(matches.contains(QLatin1Char('~') + user));
    }
}

void KUrlCompletionTest::test()
{
    setup();
    testLocalRelativePath();
    testLocalAbsolutePath();
    testLocalURL();
    testEmptyCwd();
    testBug346920();
    testUser();
    teardown();

    // Try again, with another QTemporaryDir (to check that the caching doesn't give us wrong results)
    setup();
    testLocalRelativePath();
    testLocalAbsolutePath();
    testLocalURL();
    testEmptyCwd();
    testBug346920();
    testUser();
    teardown();
}

QTEST_MAIN(KUrlCompletionTest)

#include "kurlcompletiontest.moc"
