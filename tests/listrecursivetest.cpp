/*
    SPDX-FileCopyrightText: 2002, 2003 Stephan Kulow <coolo@kde.org>
    SPDX-FileCopyrightText: 2003 David Faure <faure@kde.org>

    SPDX-License-Identifier: LGPL-2.0-only
*/

#include <QDebug>
#include <QApplication>
#include <QUrl>
#include <QDir>
#include <kio/job.h>
#include <kio/global.h>
#include <kio/udsentry.h>

class KJob;
namespace KIO
{
class Job;
}

class SpeedTest : public QObject
{
    Q_OBJECT

public:
    SpeedTest(const QUrl &url);

private Q_SLOTS:
    void entries(KIO::Job *, const KIO::UDSEntryList &);
    void finished(KJob *job);

};

using namespace KIO;

SpeedTest::SpeedTest(const QUrl &url)
    : QObject(nullptr)
{
    Job *job = listRecursive(url);
    connect(job, &KJob::result,
            this, &SpeedTest::finished);
    /*connect(job, SIGNAL(entries(KIO::Job*,KIO::UDSEntryList)),
        SLOT(entries(KIO::Job*,KIO::UDSEntryList)));
    */
}

void SpeedTest::entries(KIO::Job *, const UDSEntryList &list)
{

    UDSEntryList::ConstIterator it = list.begin();
    const UDSEntryList::ConstIterator end = list.end();
    for (; it != end; ++it) {
        qDebug() << (*it).stringValue(UDSEntry::UDS_NAME);
    }
}

void SpeedTest::finished(KJob *)
{
    qDebug() << "job finished";
    qApp->quit();
}

int main(int argc, char **argv)
{

    // "A KIO::listRecursive testing tool"

    //KCmdLineOptions options;
    //options.add("+[URL]", qi18n("the URL to list"));

    QApplication app(argc, argv);

    QUrl url;
    if (argc > 1) {
        url = QUrl::fromUserInput(argv[1]);
    } else {
        url = QUrl::fromLocalFile(QDir::currentPath());
    }

    SpeedTest test(url);
    app.exec();
}

#include "listrecursivetest.moc"
