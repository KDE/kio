/***************************************************************************
 *   Copyright (C) 2014 by Frank Reininghaus <frank78ac@googlemail.com>    *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA            *
 ***************************************************************************/

#include <kio/job.h>

#include <iostream>

#include <QCoreApplication>
#include <QDebug>
#include <QElapsedTimer>

class Test : public QObject
{
    Q_OBJECT

public:
    Test() : QObject() {}

public Q_SLOTS:
    void slotEntries(KIO::Job*, const KIO::UDSEntryList &entries)
    {
        m_entries += entries;
        qDebug() << "Listed" << m_entries.count() << "files.";
    }

private:
    KIO::UDSEntryList m_entries;
};


int main(int argc, char **argv)
{
    if (argc != 2) {
        qWarning() << "Expected a path or URL.";
        return 1;
    }

    QCoreApplication app(argc, argv);
    Test testObject;

    QUrl url = QUrl::fromUserInput(QString::fromLocal8Bit(argv[1]));
    qDebug() << "Starting listJob for the URL:" << url;

    QElapsedTimer timer;
    timer.start();

    KIO::ListJob *job = KIO::listDir(url, KIO::HideProgressInfo);
    job->setUiDelegate(nullptr);
    job->addMetaData(QStringLiteral("details"), QStringLiteral("2")); // Default is 2 which means all details. 0 means just a few essential fields (KIO::UDSEntry::UDS_NAME, KIO::UDSEntry::UDS_FILE_TYPE and KIO::UDSEntry::UDS_LINK_DEST if it is a symbolic link. Not provided otherwise.

    QObject::connect(job, SIGNAL(entries(KIO::Job*,KIO::UDSEntryList)), &testObject, SLOT(slotEntries(KIO::Job*,KIO::UDSEntryList)));
    QObject::connect(job, SIGNAL(finished(KJob*)), &app, SLOT(quit()));

    int result = app.exec();

    qDebug() << "Runtime:" << timer.elapsed() << "milliseconds.";
    qDebug() << "Press Enter to quit.";
    std::cin.ignore();

    return result;
}

#include "listjobtest.moc"
