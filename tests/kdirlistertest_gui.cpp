/*
    This file is part of the KDE desktop environment
    SPDX-FileCopyrightText: 2001, 2002 Michael Brade <brade@kde.org>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#include "kdirlistertest_gui.h"

#include <QApplication>
#include <QPushButton>
#include <QDir>
#include <QVBoxLayout>
#include <QCommandLineParser>
#include <QDebug>

#include <cstdlib>

KDirListerTest::KDirListerTest(QWidget *parent, const QUrl &initialUrl)
    : QWidget(parent)
{
    lister = new KDirLister(this);
    debug = new PrintSignals;

    QVBoxLayout *layout = new QVBoxLayout(this);

    QPushButton *startH = new QPushButton(QStringLiteral("Start listing Home"), this);
    QPushButton *startR = new QPushButton(QStringLiteral("Start listing Root"), this);
    QPushButton *test = new QPushButton(QStringLiteral("Many"), this);
    QPushButton *startT = new QPushButton(QStringLiteral("tarfile"), this);

    layout->addWidget(startH);
    layout->addWidget(startR);
    layout->addWidget(startT);
    layout->addWidget(test);
    resize(layout->sizeHint());

    connect(startR, &QAbstractButton::clicked, this, &KDirListerTest::startRoot);
    connect(startH, &QAbstractButton::clicked, this, &KDirListerTest::startHome);
    connect(startT, &QAbstractButton::clicked, this, &KDirListerTest::startTar);
    connect(test, &QAbstractButton::clicked, this, &KDirListerTest::test);

    connect(lister, &KCoreDirLister::started, debug, &PrintSignals::started);
    connect(lister, QOverload<>::of(&KDirLister::completed), debug, &PrintSignals::completed);
    connect(lister, &KCoreDirLister::listingDirCompleted, debug, &PrintSignals::listingDirCompleted);
    connect(lister, QOverload<>::of(&KDirLister::canceled), debug, &PrintSignals::canceled);
    connect(lister, &KCoreDirLister::listingDirCanceled, debug, &PrintSignals::listingDirCanceled);
    connect(lister, QOverload<const QUrl &>::of(&KDirLister::redirection),
            debug, QOverload<const QUrl &>::of(&PrintSignals::redirection));
    connect(lister, QOverload<const QUrl &, const QUrl &>::of(&KDirLister::redirection),
            debug, QOverload<const QUrl &, const QUrl &>::of(&PrintSignals::redirection));
    connect(lister, QOverload<>::of(&KDirLister::clear), debug, &PrintSignals::clear);
    connect(lister, &KCoreDirLister::newItems, debug, &PrintSignals::newItems);
    connect(lister, &KCoreDirLister::itemsFilteredByMime, debug, &PrintSignals::itemsFilteredByMime);
    connect(lister, &KCoreDirLister::itemsDeleted, debug, &PrintSignals::itemsDeleted);
    connect(lister, &KCoreDirLister::refreshItems, debug, &PrintSignals::refreshItems);
    connect(lister, &KCoreDirLister::infoMessage, debug, &PrintSignals::infoMessage);
    connect(lister, &KCoreDirLister::percent, debug, &PrintSignals::percent);
    connect(lister, &KCoreDirLister::totalSize, debug, &PrintSignals::totalSize);
    connect(lister, &KCoreDirLister::processedSize, debug, &PrintSignals::processedSize);
    connect(lister, &KCoreDirLister::speed, debug, &PrintSignals::speed);

    connect(lister, QOverload<>::of(&KDirLister::completed), this, &KDirListerTest::completed);

    if (initialUrl.isValid()) {
        lister->openUrl(initialUrl, KDirLister::NoFlags);
    }
}

KDirListerTest::~KDirListerTest()
{
}

void KDirListerTest::startHome()
{
    QUrl home = QUrl::fromLocalFile(QDir::homePath());
    lister->openUrl(home, KDirLister::NoFlags);
//  lister->stop();
}

void KDirListerTest::startRoot()
{
    QUrl root = QUrl::fromLocalFile(QDir::rootPath());
    lister->openUrl(root, KDirLister::Keep | KDirLister::Reload);
// lister->stop( root );
}

void KDirListerTest::startTar()
{
    QUrl root = QUrl::fromLocalFile(QDir::homePath() + "/aclocal_1.tgz");
    lister->openUrl(root, KDirLister::Keep | KDirLister::Reload);
// lister->stop( root );
}

void KDirListerTest::test()
{
    QUrl home = QUrl::fromLocalFile(QDir::homePath());
    QUrl root = QUrl::fromLocalFile(QDir::rootPath());
#ifdef Q_OS_WIN
    lister->openUrl(home, KDirLister::Keep);
    lister->openUrl(root, KDirLister::Keep | KDirLister::Reload);
#else
    /*  lister->openUrl( home, KDirLister::Keep );
      lister->openUrl( root, KDirLister::Keep | KDirLister::Reload );
      lister->openUrl( QUrl::fromLocalFile("file:/etc"), KDirLister::Keep | KDirLister::Reload );
      lister->openUrl( root, KDirLister::Keep | KDirLister::Reload );
      lister->openUrl( QUrl::fromLocalFile("file:/dev"), KDirLister::Keep | KDirLister::Reload );
      lister->openUrl( QUrl::fromLocalFile("file:/tmp"), KDirLister::Keep | KDirLister::Reload );
      lister->openUrl( QUrl::fromLocalFile("file:/usr/include"), KDirLister::Keep | KDirLister::Reload );
      lister->updateDirectory( QUrl::fromLocalFile("file:/usr/include") );
      lister->updateDirectory( QUrl::fromLocalFile("file:/usr/include") );
      lister->openUrl( QUrl::fromLocalFile("file:/usr/"), KDirLister::Keep | KDirLister::Reload );
    */
    lister->openUrl(QUrl::fromLocalFile(QStringLiteral("/dev")), KDirLister::Keep | KDirLister::Reload);
#endif
}

void KDirListerTest::completed()
{
    if (lister->url().toLocalFile() == QDir::rootPath()) {
        const KFileItem item = lister->findByUrl(QUrl::fromLocalFile(QDir::tempPath()));
        if (!item.isNull()) {
            qDebug() << "Found " << QDir::tempPath() << ": " << item.name();
        } else {
            qWarning() << QDir::tempPath() << " not found! Bug in findByURL?";
        }
    }
}

int main(int argc, char *argv[])
{
    QApplication::setApplicationName(QStringLiteral("kdirlistertest"));
    QApplication app(argc, argv);

    QCommandLineParser parser;
    parser.addHelpOption();
    parser.addPositionalArgument(QStringLiteral("URL"), QStringLiteral("URL to a directory to list."), QStringLiteral("[URL...]"));
    parser.process(app);
    QUrl url;
    if (!parser.positionalArguments().isEmpty()) {
        url = QUrl::fromUserInput(parser.positionalArguments().at(0));
    }

    KDirListerTest *test = new KDirListerTest(nullptr, url);
    test->show();
    return app.exec();
}

#include "moc_kdirlistertest_gui.cpp"
