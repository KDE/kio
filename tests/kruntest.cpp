/*
    SPDX-FileCopyrightText: 2002 David Faure <faure@kde.org>
    SPDX-FileCopyrightText: 2003 Waldo Bastian <bastian@kde.org>

    SPDX-License-Identifier: LGPL-2.0-only
*/

#include "kruntest.h"

#include <KIO/ApplicationLauncherJob>
#include <KIO/JobUiDelegate>
#include <KIO/OpenUrlJob>

#include <QLabel>
#include <QApplication>
#include <QDebug>
#include <KService>
#include <QPushButton>
#include <QLayout>
#include <QTest> // QFINDTESTDATA

#include <QDir>
#include <qplatformdefs.h>

static const int s_maxJobs = 100;

static KIO::OpenUrlJob *jobArray[s_maxJobs];

static const char testFile[] = "kruntest.cpp";

static const struct {
    const char *text;
    const char *expectedResult;
    const char *exec;
    const char *url;
} s_tests[] = {
    { "run(kwrite, no url)", "should work normally", "kwrite", nullptr },
    { "run(kwrite, file url)", "should work normally", "kwrite", testFile },
    { "run(kwrite, remote url)", "should work normally", "kwrite", "http://www.kde.org" },
    { "run(doesnotexit, no url)", "should show error message", "doesnotexist", nullptr },
    { "run(doesnotexit, file url)", "should show error message", "doesnotexist", testFile },
    { "run(doesnotexit, remote url)", "should use kioexec and show error message", "doesnotexist", "http://www.kde.org" },
    { "run(not-executable-desktopfile)", "should ask for confirmation", "nonexec", nullptr },
    { "run(missing lib, no url)", "should show error message (remove libqca-qt5.so.2 for this, e.g. by editing LD_LIBRARY_PATH if qca is in its own prefix)", "qcatool-qt5", nullptr },
    { "run(missing lib, file url)", "should show error message (remove libqca-qt5.so.2 for this, e.g. by editing LD_LIBRARY_PATH if qca is in its own prefix)", "qcatool-qt5", testFile },
    { "run(missing lib, remote url)", "should show error message (remove libqca-qt5.so.2 for this, e.g. by editing LD_LIBRARY_PATH if qca is in its own prefix)", "qcatool-qt5", "http://www.kde.org" },
};

Receiver::Receiver()
{
    QVBoxLayout *lay = new QVBoxLayout(this);
    QPushButton *btn = new QPushButton(QStringLiteral("Press here to terminate"), this);
    lay->addWidget(btn);
    connect(btn, &QPushButton::clicked, qApp, &QApplication::quit);

    start = new QPushButton(QStringLiteral("Launch OpenUrlJobs"), this);
    lay->addWidget(start);
    connect(start, &QAbstractButton::clicked, this, &Receiver::slotStart);

    stop = new QPushButton(QStringLiteral("Stop those OpenUrlJobs"), this);
    stop->setEnabled(false);
    lay->addWidget(stop);
    connect(stop, &QAbstractButton::clicked, this, &Receiver::slotStop);

    QPushButton *launchOne = new QPushButton(QStringLiteral("Launch one http OpenUrlJob"), this);
    lay->addWidget(launchOne);
    connect(launchOne, &QAbstractButton::clicked, this, &Receiver::slotLaunchOne);

    for (uint i = 0; i < sizeof(s_tests) / sizeof(*s_tests); ++i) {
        QHBoxLayout *hbox = new QHBoxLayout;
        lay->addLayout(hbox);
        QPushButton *button = new QPushButton(s_tests[i].text, this);
        button->setProperty("testNumber", i);
        hbox->addWidget(button);
        QLabel *label = new QLabel(s_tests[i].expectedResult, this);
        hbox->addWidget(label);
        connect(button, &QAbstractButton::clicked, this, &Receiver::slotLaunchTest);
        hbox->addStretch();
    }

    adjustSize();
    show();
}

void Receiver::slotLaunchTest()
{
    QPushButton *button = qobject_cast<QPushButton *>(sender());
    Q_ASSERT(button);
    const int testNumber = button->property("testNumber").toInt();
    QList<QUrl> urls;
    if (s_tests[testNumber].url) {
        QString urlStr(s_tests[testNumber].url);
        if (urlStr == QLatin1String(testFile)) {
            urlStr = QFINDTESTDATA(testFile);
        }
        urls << QUrl::fromUserInput(urlStr);
    }
    KService::Ptr service;
    if (QByteArray(s_tests[testNumber].exec) == "nonexec") {
        const QString desktopFile = QFINDTESTDATA("../src/ioslaves/trash/kcmtrash.desktop");
        if (desktopFile.isEmpty()) {
            qWarning() << "kcmtrash.desktop not found!";
        }
        const QString dest = QStringLiteral("kcmtrash.desktop");
        QFile::remove(dest);
        bool ok = QFile::copy(desktopFile, dest);
        if (!ok) {
            qWarning() << "Failed to copy" << desktopFile << "to" << dest;
        }
        service = KService::Ptr(new KService(QDir::currentPath() + QLatin1Char('/') + dest));
    } else {
        service = KService::Ptr(new KService("Some Name", s_tests[testNumber].exec, QString()));
    }
    auto *job = new KIO::ApplicationLauncherJob(service, this);
    job->setUrls(urls);
    job->setUiDelegate(new KIO::JobUiDelegate(KJobUiDelegate::AutoHandlingEnabled, this));
    job->start();
}

void Receiver::slotStop()
{
    for (int i = 0; i < s_maxJobs; i++) {
        qDebug() << "deleting job" << i;
        delete jobArray[i];
    }
    start->setEnabled(true);
    stop->setEnabled(false);
}

void Receiver::slotStart()
{
    for (int i = 0; i < s_maxJobs; i++) {
        qDebug() << "creating testjob" << i;
        jobArray[i] = new KIO::OpenUrlJob(QUrl::fromLocalFile(QDir::tempPath()));
        jobArray[i]->setAutoDelete(false);
        jobArray[i]->start();
    }
    start->setEnabled(false);
    stop->setEnabled(true);
}

void Receiver::slotLaunchOne()
{
    auto *job = new KIO::OpenUrlJob(QUrl(QStringLiteral("http://www.kde.org")));
    job->setUiDelegate(new KIO::JobUiDelegate(KJobUiDelegate::AutoHandlingEnabled, this));
    job->start();
}

int main(int argc, char **argv)
{
    QApplication::setApplicationName(QStringLiteral("kruntest"));
    QApplication app(argc, argv);

    Receiver receiver;
    return app.exec();
}
