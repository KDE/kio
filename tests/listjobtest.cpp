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
#include <QDir>

int main(int argc, char **argv)
{
    if (argc < 2) {
        qWarning() << "Expected a path or URL.";
        return 1;
    }

    QCoreApplication app(argc, argv);
    quint64 entriesListed = 0;

    for (int i = 1; i < argc; ++i) {
        QUrl url = QUrl::fromUserInput(QString::fromLocal8Bit(argv[i]), QDir::currentPath());
        qDebug() << "Starting listJob for the URL:" << url;

        KIO::ListJob *job = KIO::listDir(url, KIO::HideProgressInfo);
        job->setUiDelegate(nullptr);
        job->addMetaData(QStringLiteral("statDetails"), QString::number(KIO::StatDefaultDetails));

        QObject::connect(job, &KIO::ListJob::entries,
                         [&entriesListed] (KIO::Job*, const KIO::UDSEntryList &entries) {
                            entriesListed += entries.size();
                            qDebug() << "Listed" << entriesListed << "files.";
                         });
    }

    return app.exec();
}
