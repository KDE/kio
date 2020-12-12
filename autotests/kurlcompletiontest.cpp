/*
    SPDX-FileCopyrightText: 2004 David Faure <faure@kde.org>

    SPDX-License-Identifier: LGPL-2.0-only
*/

#include <QTest>
#include <QApplication>
#include <kurlcompletion.h>
#include <KUser>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QThread>
#include <qplatformdefs.h>

class KUrlCompletionTest : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void test();

public:
    KUrlCompletionTest() {
#ifdef NO_WAIT // kurlcompletiontest-nowait sets this, to test what happens on slower systems (or systems with many dirs or users)
        qputenv("KURLCOMPLETION_WAIT", "1"); // 1ms, too short for a full listing of /usr/bin, but at least give a chance for a few items in the result
#endif
    }
    ~KUrlCompletionTest()
    {
        teardown();
    }
    void runAllTests();
    void setup();
    void teardown();
    void testLocalRelativePath();
    void testLocalAbsolutePath();
    void testLocalURL();
    void testEmptyCwd();
    void testBug346920();
    void testInvalidProtocol();
    void testUser();
    void testCancel();

    // remember to register new test methods in runAllTests

private:
    void waitForCompletion(KUrlCompletion *completion);
    KUrlCompletion *m_completion;
    KUrlCompletion *m_completionWithMimeFilter;
    QTemporaryDir *m_tempDir;
    QUrl m_dirURL;
    QString m_dir;
    KUrlCompletion *m_completionEmptyCwd;
};

void KUrlCompletionTest::setup()
{
    qDebug();
    m_completion = new KUrlCompletion;
    m_completionWithMimeFilter = new KUrlCompletion;
    m_completionWithMimeFilter->setMimeTypeFilters({QStringLiteral("text/x-c++src")});
    m_tempDir = new QTemporaryDir;
    m_dir = m_tempDir->path();
    m_dir += QLatin1String("/Dir With#Spaces/");
    QDir().mkdir(m_dir);
    qDebug() << "m_dir=" << m_dir;
    m_completion->setDir(QUrl::fromLocalFile(m_dir));
    m_completionWithMimeFilter->setDir(m_completion->dir());
    m_dirURL = QUrl::fromLocalFile(m_dir);

    QFile f1(m_dir + QStringLiteral("/file1"));
    bool ok = f1.open(QIODevice::WriteOnly);
    QVERIFY(ok);
    f1.close();

    QFile f2(m_dir + QStringLiteral("/file#a"));
    ok = f2.open(QIODevice::WriteOnly);
    QVERIFY(ok);
    f2.close();

    QFile f3(m_dir + QStringLiteral("/file."));
    ok = f3.open(QIODevice::WriteOnly);
    QVERIFY(ok);
    f3.close();

    QFile f4(m_dir + QStringLiteral("/source.cpp"));
    ok = f4.open(QIODevice::WriteOnly);
    QVERIFY(ok);
    f4.close();

    QFile f5(m_dir + QStringLiteral("/source.php"));
    ok = f5.open(QIODevice::WriteOnly);
    QVERIFY(ok);
    f5.close();

    QDir().mkdir(m_dir + QStringLiteral("/file_subdir"));
    QDir().mkdir(m_dir + QStringLiteral("/.1_hidden_file_subdir"));
    QDir().mkdir(m_dir + QStringLiteral("/.2_hidden_file_subdir"));

    m_completionEmptyCwd = new KUrlCompletion;
    m_completionEmptyCwd->setDir(QUrl());
}

void KUrlCompletionTest::teardown()
{
    delete m_completion;
    m_completion = nullptr;
    delete m_completionWithMimeFilter;
    m_completionWithMimeFilter = nullptr;
    delete m_tempDir;
    m_tempDir = nullptr;
    delete m_completionEmptyCwd;
    m_completionEmptyCwd = nullptr;
}

void KUrlCompletionTest::waitForCompletion(KUrlCompletion *completion)
{
    while (completion->isRunning()) {
        qDebug() << "waiting for thread...";
        QTest::qWait(5);
    }
    // The thread emitted a signal, process it.
    qApp->sendPostedEvents(nullptr, QEvent::MetaCall);
}

void KUrlCompletionTest::testLocalRelativePath()
{
    qDebug();
    // Completion from relative path, with two matches
    m_completion->makeCompletion(QStringLiteral("f"));
    waitForCompletion(m_completion);
    QStringList comp1all = m_completion->allMatches();
    qDebug() << comp1all;
    QCOMPARE(comp1all.count(), 4);
    QVERIFY(comp1all.contains(QLatin1String("file1")));
    QVERIFY(comp1all.contains(QLatin1String("file#a")));
    QVERIFY(comp1all.contains(QLatin1String("file.")));
    QVERIFY(comp1all.contains(QLatin1String("file_subdir/")));
    QString comp1 = m_completion->replacedPath(QStringLiteral("file1")); // like KUrlRequester does
    QCOMPARE(comp1, QStringLiteral("file1"));

    // Completion from relative path
    qDebug() << "\nnow completing on 'file#'";
    m_completion->makeCompletion(QStringLiteral("file#"));
    QVERIFY(!m_completion->isRunning()); // last listing reused
    QStringList compall = m_completion->allMatches();
    qDebug() << compall;
    QCOMPARE(compall.count(), 1);
    QCOMPARE(compall.first(), QStringLiteral("file#a"));
    QString comp2 = m_completion->replacedPath(compall.first()); // like KUrlRequester does
    QCOMPARE(comp2, QStringLiteral("file#a"));

    // Completion with empty string
    qDebug() << "\nnow completing on ''";
    m_completion->makeCompletion(QLatin1String(""));
    waitForCompletion(m_completion);
    QStringList compEmpty = m_completion->allMatches();
    QCOMPARE(compEmpty.count(), 0);

    m_completion->makeCompletion(QStringLiteral("."));
    waitForCompletion(m_completion);
    const auto compAllHidden = m_completion->allMatches();
    QCOMPARE(compAllHidden.count(), 2);
    QVERIFY(compAllHidden.contains(QLatin1String(".1_hidden_file_subdir/")));
    QVERIFY(compAllHidden.contains(QLatin1String(".2_hidden_file_subdir/")));

    // Completion with '.2', should find only hidden folders starting with '2'
    m_completion->makeCompletion(QStringLiteral(".2"));
    waitForCompletion(m_completion);
    const auto compHiddenStartingWith2 = m_completion->allMatches();
    QCOMPARE(compHiddenStartingWith2.count(), 1);
    QVERIFY(compHiddenStartingWith2.contains(QLatin1String(".2_hidden_file_subdir/")));

    // Completion with 'file.', should only find one file
    m_completion->makeCompletion(QStringLiteral("file."));
    waitForCompletion(m_completion);
    const auto compFileEndingWithDot = m_completion->allMatches();
    QCOMPARE(compFileEndingWithDot.count(), 1);
    QVERIFY(compFileEndingWithDot.contains(QLatin1String("file.")));

    // Completion with 'source' should only find the C++ file
    m_completionWithMimeFilter->makeCompletion(QStringLiteral("source"));
    waitForCompletion(m_completionWithMimeFilter);
    const auto compSourceFile = m_completionWithMimeFilter->allMatches();
    QCOMPARE(compSourceFile.count(), 1);
    QVERIFY(compSourceFile.contains(QLatin1String("source.cpp")));

    // But it should also be able to find folders
    m_completionWithMimeFilter->makeCompletion(QStringLiteral("file_subdir"));
    waitForCompletion(m_completionWithMimeFilter);
    const auto compMimeFolder = m_completionWithMimeFilter->allMatches();
    QCOMPARE(compMimeFolder.count(), 1);
    QVERIFY(compMimeFolder.contains(QLatin1String("file_subdir/")));
}

void KUrlCompletionTest::testLocalAbsolutePath()
{
    // Completion from absolute path
    qDebug() << m_dir + "file#";
    m_completion->makeCompletion(m_dir + "file#");
    waitForCompletion(m_completion);
    QStringList compall = m_completion->allMatches();
    qDebug() << compall;
    QCOMPARE(compall.count(), 1);
    QString comp = compall.first();
    QCOMPARE(comp, QString(m_dir + "file#a"));
    comp = m_completion->replacedPath(comp); // like KUrlRequester does
    QCOMPARE(comp, QString(m_dir + "file#a"));

    // Completion with '.', should find all hidden folders
    m_completion->makeCompletion(m_dir + QLatin1Char('.'));
    waitForCompletion(m_completion);
    const auto compAllHidden = m_completion->allMatches();
    QCOMPARE(compAllHidden.count(), 2);
    QVERIFY(compAllHidden.contains(m_dir + ".1_hidden_file_subdir/"));
    QVERIFY(compAllHidden.contains(m_dir + ".2_hidden_file_subdir/"));

    // Completion with '.2', should find only hidden folders starting with '2'
    m_completion->makeCompletion(m_dir + ".2");
    waitForCompletion(m_completion);
    const auto compHiddenStartingWith2 = m_completion->allMatches();
    QCOMPARE(compHiddenStartingWith2.count(), 1);
    QVERIFY(compHiddenStartingWith2.contains(m_dir + ".2_hidden_file_subdir/"));

    // Completion with 'file.', should only find one file
    m_completion->makeCompletion(m_dir + "file.");
    waitForCompletion(m_completion);
    const auto compFileEndingWithDot = m_completion->allMatches();
    QCOMPARE(compFileEndingWithDot.count(), 1);
    QVERIFY(compFileEndingWithDot.contains(m_dir + "file."));

    // Completion with 'source' should only find the C++ file
    m_completionWithMimeFilter->makeCompletion(m_dir + "source");
    waitForCompletion(m_completionWithMimeFilter);
    const auto compSourceFile = m_completionWithMimeFilter->allMatches();
    QCOMPARE(compSourceFile.count(), 1);
    QVERIFY(compSourceFile.contains(m_dir + "source.cpp"));

    // But it should also be able to find folders
    m_completionWithMimeFilter->makeCompletion(m_dir + "file_subdir");
    waitForCompletion(m_completionWithMimeFilter);
    const auto compMimeFolder = m_completionWithMimeFilter->allMatches();
    QCOMPARE(compMimeFolder.count(), 1);
    QVERIFY(compMimeFolder.contains(m_dir + "file_subdir/"));
}

void KUrlCompletionTest::testLocalURL()
{
    // Completion from URL
    qDebug();
    QUrl url = QUrl::fromLocalFile(m_dirURL.toLocalFile() + "file");
    m_completion->makeCompletion(url.toString());
    waitForCompletion(m_completion);
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
    waitForCompletion(m_completion);
    QVERIFY(m_completion->allMatches().isEmpty());

    // Completion from URL with a ref -> no match
    url = QUrl::fromLocalFile(m_dirURL.toLocalFile() + 'f');
    url.setFragment(QStringLiteral("ref"));
    qDebug() << "makeCompletion(" << url << ")";
    m_completion->makeCompletion(url.toString());
    waitForCompletion(m_completion);
    QVERIFY(m_completion->allMatches().isEmpty());

    // Completion with '.', should find all hidden folders
    qDebug() << "makeCompletion(" << (m_dirURL.toString() + QLatin1Char('.')) << ")";
    m_completion->makeCompletion(m_dirURL.toString() + QLatin1Char('.'));
    waitForCompletion(m_completion);
    const auto compAllHidden = m_completion->allMatches();
    QCOMPARE(compAllHidden.count(), 2);
    QVERIFY(compAllHidden.contains(m_dirURL.toString() + ".1_hidden_file_subdir/"));
    QVERIFY(compAllHidden.contains(m_dirURL.toString() + ".2_hidden_file_subdir/"));

    // Completion with '.2', should find only hidden folders starting with '2'
    url = QUrl::fromLocalFile(m_dirURL.toLocalFile() + ".2");
    qDebug() << "makeCompletion(" << url << ")";
    m_completion->makeCompletion(url.toString());
    waitForCompletion(m_completion);
    const auto compHiddenStartingWith2 = m_completion->allMatches();
    QCOMPARE(compHiddenStartingWith2.count(), 1);
    QVERIFY(compHiddenStartingWith2.contains(m_dirURL.toString() + QStringLiteral(".2_hidden_file_subdir/")));

    // Completion with 'file.', should only find one file
    url = QUrl::fromLocalFile(m_dirURL.toLocalFile() + QStringLiteral("file."));
    qDebug() << "makeCompletion(" << url << ")";
    m_completion->makeCompletion(url.toString());
    waitForCompletion(m_completion);
    const auto compFileEndingWithDot = m_completion->allMatches();
    QCOMPARE(compFileEndingWithDot.count(), 1);
    QVERIFY(compFileEndingWithDot.contains(m_dirURL.toString() + QStringLiteral("file.")));

    // Completion with 'source' should only find the C++ file
    m_completionWithMimeFilter->makeCompletion(m_dirURL.toString() + QStringLiteral("source"));
    waitForCompletion(m_completionWithMimeFilter);
    const auto compSourceFile = m_completionWithMimeFilter->allMatches();
    QCOMPARE(compSourceFile.count(), 1);
    QVERIFY(compSourceFile.contains(m_dirURL.toString() + QStringLiteral("source.cpp")));

    // But it should also be able to find folders
    m_completionWithMimeFilter->makeCompletion(m_dirURL.toString() + QStringLiteral("file_subdir"));
    waitForCompletion(m_completionWithMimeFilter);
    const auto compMimeFolder = m_completionWithMimeFilter->allMatches();
    QCOMPARE(compMimeFolder.count(), 1);
    QVERIFY(compMimeFolder.contains(m_dirURL.toString() + QStringLiteral("file_subdir/")));
}

void KUrlCompletionTest::testEmptyCwd()
{
    // Completion with empty string (with a KUrlCompletion whose cwd is "")
    qDebug() << "\nnow completing on '' with empty cwd";
    m_completionEmptyCwd->makeCompletion(QLatin1String(""));
    waitForCompletion(m_completionEmptyCwd);
    QStringList compEmpty = m_completionEmptyCwd->allMatches();
    QCOMPARE(compEmpty.count(), 0);
}

void KUrlCompletionTest::testBug346920()
{
    m_completionEmptyCwd->makeCompletion(QStringLiteral("~/."));
    waitForCompletion(m_completionEmptyCwd);
    m_completionEmptyCwd->allMatches();
    // just don't crash
}

void KUrlCompletionTest::testInvalidProtocol()
{
    m_completion->makeCompletion(QStringLiteral(":/"));
    waitForCompletion(m_completion);
    m_completion->allMatches();
    // just don't crash
}

void KUrlCompletionTest::testUser()
{
    m_completionEmptyCwd->makeCompletion(QStringLiteral("~"));
    waitForCompletion(m_completionEmptyCwd);
    const auto matches = m_completionEmptyCwd->allMatches();
    const QStringList allUsers = KUser::allUserNames();
    if (!allUsers.isEmpty()) {
        Q_ASSERT(!matches.isEmpty());
    }
    for (const auto &user : allUsers) {
        QVERIFY2(matches.contains(QLatin1Char('~') + user), qPrintable(matches.join(QLatin1Char(' '))));
    }

    // Check that the same query doesn't re-list
    m_completionEmptyCwd->makeCompletion(QStringLiteral("~"));
    QVERIFY(!m_completionEmptyCwd->isRunning());
    QCOMPARE(m_completionEmptyCwd->allMatches(), matches);
}

// Test cancelling a running thread
// In a normal run (./kurlcompletiontest) and a reasonable amount of files, we have few chances of making this happen
// But in a "nowait" run (./kurlcompletiontest-nowait), this will cancel the thread before it even starts listing the dir.
void KUrlCompletionTest::testCancel()
{
    KUrlCompletion comp;
    comp.setDir(QUrl::fromLocalFile("/usr/bin"));
    comp.makeCompletion(QStringLiteral("g"));
    const QStringList matchesG = comp.allMatches();
    // We get many matches in a normal run, and usually 0 matches when testing "no wait" (thread is sleeping) -> this is where this method can test cancelling
    //qDebug() << "got" << matchesG.count() << "matches";
    bool done = !comp.isRunning();

    // Doing the same search again, should hopefully not restart everything from scratch
    comp.makeCompletion(QStringLiteral("g"));
    const QStringList matchesG2 = comp.allMatches();
    QVERIFY(matchesG2.count() >= matchesG.count());
    if (done) {
        QVERIFY(!comp.isRunning()); // it had no reason to restart
    }
    done = !comp.isRunning();

    // Search for something else, should reuse dir listing but not mix up results
    comp.makeCompletion(QStringLiteral("a"));
    if (done) {
        QVERIFY(!comp.isRunning()); // it had no reason to restart
    }
    const QStringList matchesA = comp.allMatches();
    //qDebug() << "got" << matchesA.count() << "matches";
    for (const QString &match : matchesA) {
        QVERIFY2(!match.startsWith(QLatin1Char('g')), qPrintable(match));
    }
    waitForCompletion(&comp);
    const QStringList matchesB = comp.allMatches();
    for (const QString &match : matchesB) {
        QVERIFY2(!match.startsWith(QLatin1Char('g')), qPrintable(match));
    }
}

void KUrlCompletionTest::test()
{
    runAllTests();
    // Try again, with another QTemporaryDir (to check that the caching doesn't give us wrong results)
    runAllTests();
}

void KUrlCompletionTest::runAllTests()
{
    setup();
    testLocalRelativePath();
    testLocalAbsolutePath();
    testLocalURL();
    testEmptyCwd();
    testBug346920();
    testInvalidProtocol();
    testUser();
    testCancel();
    teardown();
}

QTEST_MAIN(KUrlCompletionTest)

#include "kurlcompletiontest.moc"
