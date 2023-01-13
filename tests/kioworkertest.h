/*
    This file is or will be part of KDE desktop environment
    SPDX-FileCopyrightText: 1999 Matt Koss <koss@miesto.sk>

    SPDX-License-Identifier: LGPL-2.0-only
*/

#ifndef _KIOWORKERTEST_H
#define _KIOWORKERTEST_H

#include <QButtonGroup>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QRadioButton>
#include <QWidget>

#include <KMainWindow>

#include "kio/global.h"
#include <kio/simplejob.h>
#include <kio/udsentry.h>

#include <KStatusBarJobTracker>

class KioWorkerTest : public KMainWindow
{
    Q_OBJECT

public:
    KioWorkerTest(QString src, QString dest, uint op, uint pr);

    ~KioWorkerTest() override
    {
        if (job) {
            job->kill(KJob::Quietly); // kill the job quietly
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

private:
    KIO::Job *job;
    QWidget *main_widget;

    KStatusBarJobTracker *statusTracker;

    int selectedOperation;
    int progressMode;
    int putBuffer;
};

#endif // _KIOWORKERTEST_H
