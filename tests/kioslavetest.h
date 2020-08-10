/*
    This file is or will be part of KDE desktop environment
    SPDX-FileCopyrightText: 1999 Matt Koss <koss@miesto.sk>

    SPDX-License-Identifier: LGPL-2.0-only
*/

#ifndef _KIOSLAVETEST_H
#define _KIOSLAVETEST_H

#include <QLabel>
#include <QLineEdit>
#include <QRadioButton>
#include <QPushButton>
#include <QButtonGroup>
#include <QWidget>

#include <KMainWindow>

#include "kio/job.h"
#include "kio/global.h"
#include <kio/scheduler.h>
#include <KStatusBarJobTracker>

namespace KIO {
class Slave;
}

class KioslaveTest : public KMainWindow
{
    Q_OBJECT

public:
    KioslaveTest(QString src, QString dest, uint op, uint pr);

    ~KioslaveTest()
    {
        if (job) {
            job->kill(KJob::Quietly);    // kill the job quietly
        }
        if (slave) {
            KIO::Scheduler::disconnectSlave(slave);
        }
    }

    enum Operations { List = 0, ListRecursive, Stat, Get, Put, Copy, Move, Delete, Mkdir, Mimetype };

    enum ProgressModes { ProgressNone = 0, ProgressDefault, ProgressStatus };

protected:
    void printUDSEntry(const KIO::UDSEntry &entry);

    // info stuff
    QLabel *lb_from;
    QLineEdit *le_source;

    QLabel *lb_to;
    QLineEdit *le_dest;

    // operation stuff
    QButtonGroup *opButtons;

    QRadioButton *rbList;
    QRadioButton *rbListRecursive;
    QRadioButton *rbStat;
    QRadioButton *rbGet;
    QRadioButton *rbPut;
    QRadioButton *rbCopy;
    QRadioButton *rbMove;
    QRadioButton *rbDelete;
    QRadioButton *rbMkdir;
    QRadioButton *rbMimetype;

    // progress stuff
    QButtonGroup *progressButtons;

    QRadioButton *rbProgressNone;
    QRadioButton *rbProgressDefault;
    QRadioButton *rbProgressStatus;

    QPushButton *pbStart;
    QPushButton *pbStop;

    QPushButton *close;

protected Q_SLOTS:
    void changeOperation(QAbstractButton *b);
    void changeProgressMode(QAbstractButton *b);

    void startJob();
    void stopJob();

    void slotResult(KJob *);
    void slotEntries(KIO::Job *, const KIO::UDSEntryList &);
    void slotData(KIO::Job *, const QByteArray &data);
    void slotDataReq(KIO::Job *, QByteArray &data);

    void slotQuit();
    void slotSlaveConnected();
    void slotSlaveError();

private:
    KIO::Job *job;
    QWidget *main_widget;

    KStatusBarJobTracker *statusTracker;

    int selectedOperation;
    int progressMode;
    int putBuffer;
    KIO::Slave *slave;
};

#endif // _KIOSLAVETEST_H
