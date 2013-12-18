/*
 *  Copyright (C) 2002, 2003 Stephan Kulow <coolo@kde.org>
 *  Copyright (C) 2003       David Faure   <faure@kde.org>
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

#include <QDebug>
#include <QApplication>
#include <QUrl>
#include <QDir>
#include <kio/job.h>
#include <kio/global.h>
#include <kio/udsentry.h>

class KJob;
namespace KIO {
    class Job;
}

class SpeedTest : public QObject {
    Q_OBJECT

public:
    SpeedTest(const QUrl & url);

private Q_SLOTS:
    void entries( KIO::Job *, const KIO::UDSEntryList& );
    void finished( KJob *job );

};

using namespace KIO;

SpeedTest::SpeedTest( const QUrl & url )
    : QObject(0)
{
    Job *job = listRecursive( url );
    connect(job, SIGNAL(result(KJob*)),
	    SLOT(finished(KJob*)));
    /*connect(job, SIGNAL(entries(KIO::Job*,KIO::UDSEntryList)),
	    SLOT(entries(KIO::Job*,KIO::UDSEntryList)));
    */
}

void SpeedTest::entries(KIO::Job*, const UDSEntryList& list) {

    UDSEntryList::ConstIterator it = list.begin();
    const UDSEntryList::ConstIterator end = list.end();
    for (; it != end; ++it)
        qDebug() << (*it).stringValue( UDSEntry::UDS_NAME );
}


void SpeedTest::finished(KJob*) {
    qDebug() << "job finished";
    qApp->quit();
}

int main(int argc, char **argv) {

    // "A KIO::listRecursive testing tool"

    //KCmdLineOptions options;
    //options.add("+[URL]", qi18n("the URL to list"));

    QApplication app(argc, argv);

    QUrl url;
    if (argc > 1)
      url = QUrl::fromUserInput(argv[1]);
    else
      url = QUrl::fromLocalFile(QDir::currentPath());

    SpeedTest test( url );
    app.exec();
}

#include "listrecursivetest.moc"
