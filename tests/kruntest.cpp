/*
 *  Copyright (C) 2002 David Faure   <faure@kde.org>
 *  Copyright (C) 2003 Waldo Bastian <bastian@kde.org>
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

#include "kruntest.h"

#include <QLabel>
#include <QApplication>
#include <QDebug>
#include <kservice.h>
#include <QPushButton>
#include <QLayout>
#include <QtTest/QtTest>

#include <stdlib.h>
#include <unistd.h>

const int MAXKRUNS = 100;

testKRun * myArray[MAXKRUNS];

void testKRun::foundMimeType( const QString& _type )
{
  qDebug() << "found mime type" << _type << "for URL=" << url();
  setFinished( true );
  return;
}

static const char testFile[] = "kruntest.cpp";

static const struct {
    const char* text;
    const char* expectedResult;
    const char* exec;
    const char* url;
} s_tests[] = {
    { "run(kwrite, no url)", "should work normally", "kwrite", 0 },
    { "run(kwrite, file url)", "should work normally", "kwrite", testFile },
    { "run(kwrite, remote url)", "should work normally", "kwrite", "http://www.kde.org" },
    { "run(doesnotexit, no url)", "should show error message", "doesnotexist", 0 },
    { "run(doesnotexit, file url)", "should show error message", "doesnotexist", testFile },
    { "run(doesnotexit, remote url)", "should use kioexec and show error message", "doesnotexist", "http://www.kde.org" },
    { "run(missing lib, no url)", "should show error message (remove libqca.so.2 for this, e.g. by editing LD_LIBRARY_PATH if qca is in its own prefix)", "qcatool", 0 },
    { "run(missing lib, file url)", "should show error message (remove libqca.so.2 for this, e.g. by editing LD_LIBRARY_PATH if qca is in its own prefix)", "qcatool", testFile },
    { "run(missing lib, remote url)", "should show error message (remove libqca.so.2 for this, e.g. by editing LD_LIBRARY_PATH if qca is in its own prefix)", "qcatool", "http://www.kde.org" },
    { "runCommand(empty)", "should error", "", "" }, // #186036
    { "runCommand(full path)", "should work normally", "../../kdecore/tests/kurltest", "" }
};

Receiver::Receiver()
{
    QVBoxLayout *lay = new QVBoxLayout(this);
    QPushButton * h = new QPushButton( "Press here to terminate", this );
    lay->addWidget( h );
    connect(h, SIGNAL(clicked()), qApp, SLOT(quit()));

    start = new QPushButton( "Launch KRuns", this );
    lay->addWidget( start );
    connect(start, SIGNAL(clicked()), this, SLOT(slotStart()));

    stop = new QPushButton( "Stop those KRuns", this );
    stop->setEnabled(false);
    lay->addWidget( stop );
    connect(stop, SIGNAL(clicked()), this, SLOT(slotStop()));

    QPushButton* launchOne = new QPushButton( "Launch one http KRun", this );
    lay->addWidget(launchOne);
    connect(launchOne, SIGNAL(clicked()), this, SLOT(slotLaunchOne()));

    for (uint i = 0; i < sizeof(s_tests)/sizeof(*s_tests); ++i) {
        QHBoxLayout* hbox = new QHBoxLayout;
        lay->addLayout(hbox);
        QPushButton* button = new QPushButton(s_tests[i].text, this);
        button->setProperty("testNumber", i);
        hbox->addWidget(button);
        QLabel* label = new QLabel(s_tests[i].expectedResult, this);
        hbox->addWidget(label);
        connect(button, SIGNAL(clicked()), this, SLOT(slotLaunchTest()));
        hbox->addStretch();
    }

    adjustSize();
    show();
}

void Receiver::slotLaunchTest()
{
    QPushButton* button = qobject_cast<QPushButton *>(sender());
    Q_ASSERT(button);
    const int testNumber = button->property("testNumber").toInt();
    QList<QUrl> urls;
    if (QByteArray(s_tests[testNumber].text).startsWith("runCommand")) {
        KRun::runCommand(s_tests[testNumber].exec, this);
    } else {
        if (s_tests[testNumber].url){
            QString urlStr(s_tests[testNumber].url);
            if (urlStr == QLatin1String(testFile))
                urlStr = QFINDTESTDATA(testFile);
            urls << QUrl::fromUserInput(urlStr);
        }
        KRun::run(s_tests[testNumber].exec, urls, this);
    }
}

void Receiver::slotStop()
{
  for (int i = 0 ; i < MAXKRUNS ; i++ )
  {
    qDebug() << " deleting krun " << i;
    delete myArray[i];
  }
  start->setEnabled(true);
  stop->setEnabled(false);
}

void Receiver::slotStart()
{
  for (int i = 0 ; i < MAXKRUNS ; i++ )
  {
    qDebug() << "creating testKRun " << i;
    myArray[i] = new testKRun(QUrl::fromLocalFile("file:///tmp"), window());
    myArray[i]->setAutoDelete(false);
  }
  start->setEnabled(false);
  stop->setEnabled(true);
}

void Receiver::slotLaunchOne()
{
    new testKRun(QUrl("http://www.kde.org"), window());
}

int main(int argc, char **argv)
{
    QApplication::setApplicationName("kruntest");
    QApplication app(argc, argv);

    Receiver receiver;
    return app.exec();
}

