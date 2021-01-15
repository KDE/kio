/*
    This file is or will be part of KDE desktop environment
    SPDX-FileCopyrightText: 1999 Matt Koss <koss@miesto.sk>

    SPDX-License-Identifier: LGPL-2.0-only
*/

#include "kioslavetest.h"

#include <QApplication>
#include <QLocale>
#include <QLayout>
#include <QMessageBox>
#include <QDir>
#include <QGroupBox>
#include <QStatusBar>
#include <QDebug>
#include <QUrl>
#include <QThread>

#include <qplatformdefs.h>

#include <KJobUiDelegate>
#include <kio/job.h>
#include <kio/copyjob.h>
#include <kio/deletejob.h>
#include <kprotocolinfo.h>

// QT_STAT_LNK on Windows
#include "kioglobal_p.h"

#include <QTimer>
#include <QCommandLineParser>
#include <QCommandLineOption>

using namespace KIO;

KioslaveTest::KioslaveTest(QString src, QString dest, uint op, uint pr)
    : KMainWindow(nullptr)
{

    job = nullptr;

    main_widget = new QWidget(this);
    QBoxLayout *topLayout = new QVBoxLayout(main_widget);

    QGridLayout *grid = new QGridLayout();
    topLayout->addLayout(grid);

    grid->setRowStretch(0, 1);
    grid->setRowStretch(1, 1);

    grid->setColumnStretch(0, 1);
    grid->setColumnStretch(1, 100);

    lb_from = new QLabel(QStringLiteral("From:"), main_widget);
    grid->addWidget(lb_from, 0, 0);

    le_source = new QLineEdit(main_widget);
    grid->addWidget(le_source, 0, 1);
    le_source->setText(src);

    lb_to = new QLabel(QStringLiteral("To:"), main_widget);
    grid->addWidget(lb_to, 1, 0);

    le_dest = new QLineEdit(main_widget);
    grid->addWidget(le_dest, 1, 1);
    le_dest->setText(dest);

    // Operation groupbox & buttons
    opButtons = new QButtonGroup(main_widget);
    QGroupBox *box = new QGroupBox(QStringLiteral("Operation"), main_widget);
    topLayout->addWidget(box, 10);
    connect(opButtons, QOverload<QAbstractButton *>::of(&QButtonGroup::buttonClicked), this, &KioslaveTest::changeOperation);

    QBoxLayout *hbLayout = new QHBoxLayout(box);

    rbList = new QRadioButton(QStringLiteral("List"), box);
    opButtons->addButton(rbList);
    hbLayout->addWidget(rbList, 5);

    rbListRecursive = new QRadioButton(QStringLiteral("ListRecursive"), box);
    opButtons->addButton(rbListRecursive);
    hbLayout->addWidget(rbListRecursive, 5);

    rbStat = new QRadioButton(QStringLiteral("Stat"), box);
    opButtons->addButton(rbStat);
    hbLayout->addWidget(rbStat, 5);

    rbGet = new QRadioButton(QStringLiteral("Get"), box);
    opButtons->addButton(rbGet);
    hbLayout->addWidget(rbGet, 5);

    rbPut = new QRadioButton(QStringLiteral("Put"), box);
    opButtons->addButton(rbPut);
    hbLayout->addWidget(rbPut, 5);

    rbCopy = new QRadioButton(QStringLiteral("Copy"), box);
    opButtons->addButton(rbCopy);
    hbLayout->addWidget(rbCopy, 5);

    rbMove = new QRadioButton(QStringLiteral("Move"), box);
    opButtons->addButton(rbMove);
    hbLayout->addWidget(rbMove, 5);

    rbDelete = new QRadioButton(QStringLiteral("Delete"), box);
    opButtons->addButton(rbDelete);
    hbLayout->addWidget(rbDelete, 5);

    rbMkdir = new QRadioButton(QStringLiteral("Mkdir"), box);
    opButtons->addButton(rbMkdir);
    hbLayout->addWidget(rbMkdir, 5);

    rbMimetype = new QRadioButton(QStringLiteral("Mimetype"), box);
    opButtons->addButton(rbMimetype);
    hbLayout->addWidget(rbMimetype, 5);

    QAbstractButton *b = opButtons->buttons()[op];
    b->setChecked(true);
    changeOperation(b);

    // Progress groupbox & buttons
    progressButtons = new QButtonGroup(main_widget);
    box = new QGroupBox(QStringLiteral("Progress dialog mode"), main_widget);
    topLayout->addWidget(box, 10);
    connect(progressButtons, QOverload<QAbstractButton *>::of(&QButtonGroup::buttonClicked),
            this, &KioslaveTest::changeProgressMode);

    hbLayout = new QHBoxLayout(box);

    rbProgressNone = new QRadioButton(QStringLiteral("None"), box);
    progressButtons->addButton(rbProgressNone);
    hbLayout->addWidget(rbProgressNone, 5);

    rbProgressDefault = new QRadioButton(QStringLiteral("Default"), box);
    progressButtons->addButton(rbProgressDefault);
    hbLayout->addWidget(rbProgressDefault, 5);

    rbProgressStatus = new QRadioButton(QStringLiteral("Status"), box);
    progressButtons->addButton(rbProgressStatus);
    hbLayout->addWidget(rbProgressStatus, 5);

    b = progressButtons->buttons()[pr];
    b->setChecked(true);
    changeProgressMode(b);

    // statusbar progress widget
    statusTracker = new KStatusBarJobTracker(statusBar());

    // run & stop buttons
    hbLayout = new QHBoxLayout();
    topLayout->addLayout(hbLayout);
    hbLayout->setParent(topLayout);

    pbStart = new QPushButton(QStringLiteral("&Start"), main_widget);
    pbStart->setFixedSize(pbStart->sizeHint());
    connect(pbStart, &QAbstractButton::clicked, this, &KioslaveTest::startJob);
    hbLayout->addWidget(pbStart, 5);

    pbStop = new QPushButton(QStringLiteral("Sto&p"), main_widget);
    pbStop->setFixedSize(pbStop->sizeHint());
    pbStop->setEnabled(false);
    connect(pbStop, &QAbstractButton::clicked, this, &KioslaveTest::stopJob);
    hbLayout->addWidget(pbStop, 5);

    // close button
    close = new QPushButton(QStringLiteral("&Close"), main_widget);
    close->setFixedSize(close->sizeHint());
    connect(close, &QAbstractButton::clicked, this, &KioslaveTest::slotQuit);

    topLayout->addWidget(close, 5);

    main_widget->setMinimumSize(main_widget->sizeHint());
    setCentralWidget(main_widget);

    slave = nullptr;
//  slave = KIO::Scheduler::getConnectedSlave(QUrl("ftp://ftp.kde.org"));
    KIO::Scheduler::connect(SIGNAL(slaveConnected(KIO::Slave*)),
                            this, SLOT(slotSlaveConnected()));
    KIO::Scheduler::connect(SIGNAL(slaveError(KIO::Slave*,int,QString)),
                            this, SLOT(slotSlaveError()));
}

void KioslaveTest::slotQuit()
{
    qApp->quit();
}

void KioslaveTest::changeOperation(QAbstractButton *b)
{
    // only two urls for copy and move
    bool enab = rbCopy->isChecked() || rbMove->isChecked();

    le_dest->setEnabled(enab);

    selectedOperation = opButtons->buttons().indexOf(b);
}

void KioslaveTest::changeProgressMode(QAbstractButton *b)
{
    progressMode = progressButtons->buttons().indexOf(b);

    if (progressMode == ProgressStatus) {
        statusBar()->show();
    } else {
        statusBar()->hide();
    }
}

void KioslaveTest::startJob()
{
    QUrl sCurrent(QUrl::fromLocalFile(QDir::currentPath()));
    QString sSrc(le_source->text());
    QUrl src = QUrl(sCurrent).resolved(QUrl(sSrc));

    if (!src.isValid()) {
        QMessageBox::critical(this, QStringLiteral("Kioslave Error Message"), QStringLiteral("Source URL is malformed"));
        return;
    }

    QString sDest(le_dest->text());
    QUrl dest = QUrl(sCurrent).resolved(QUrl(sDest));

    if (!dest.isValid() &&
            (selectedOperation == Copy || selectedOperation == Move)) {
        QMessageBox::critical(this, QStringLiteral("Kioslave Error Message"),
                              QStringLiteral("Destination URL is malformed"));
        return;
    }

    pbStart->setEnabled(false);

    KIO::JobFlags observe = DefaultFlags;
    if (progressMode != ProgressDefault) {
        observe = HideProgressInfo;
    }

    SimpleJob *myJob = nullptr;

    switch (selectedOperation) {
    case List: {
        KIO::ListJob *listJob = KIO::listDir(src);
        myJob = listJob;
        connect(listJob, &KIO::ListJob::entries, this, &KioslaveTest::slotEntries);
        break;
    }

    case ListRecursive: {
        KIO::ListJob *listJob = KIO::listRecursive(src);
        myJob = listJob;
        connect(listJob, &KIO::ListJob::entries, this, &KioslaveTest::slotEntries);
        break;
    }

    case Stat:
        myJob = KIO::statDetails(src, KIO::StatJob::SourceSide);
        break;

    case Get: {
        KIO::TransferJob *tjob = KIO::get(src, KIO::Reload);
        myJob = tjob;
        connect(tjob, &KIO::TransferJob::data, this, &KioslaveTest::slotData);
        break;
    }

    case Put: {
        putBuffer = 0;
        KIO::TransferJob *tjob = KIO::put(src, -1, KIO::Overwrite);
        tjob->setTotalSize(48 * 1024 * 1024);
        myJob = tjob;
        connect(tjob, &TransferJob::dataReq, this, &KioslaveTest::slotDataReq);
        break;
    }

    case Copy:
        job = KIO::copy(src, dest, observe);
        break;

    case Move:
        job = KIO::move(src, dest, observe);
        break;

    case Delete:
        job = KIO::del(src, observe);
        break;

    case Mkdir:
        myJob = KIO::mkdir(src);
        break;

    case Mimetype:
        myJob = KIO::mimetype(src);
        break;
    }
    if (myJob) {
        if (slave) {
            KIO::Scheduler::assignJobToSlave(slave, myJob);
        }
        job = myJob;
    }

    statusBar()->addWidget(statusTracker->widget(job), 0);

    connect(job, &KJob::result,
            this, &KioslaveTest::slotResult);

    if (progressMode == ProgressStatus) {
        statusTracker->registerJob(job);
    }

    pbStop->setEnabled(true);
}

void KioslaveTest::slotResult(KJob *_job)
{
    if (_job->error()) {
        _job->uiDelegate()->showErrorMessage();
    } else if (selectedOperation == Stat) {
        UDSEntry entry = ((KIO::StatJob *)_job)->statResult();
        printUDSEntry(entry);
    } else if (selectedOperation == Mimetype) {
        qDebug() << "MIME type is " << ((KIO::MimetypeJob *)_job)->mimetype();
    }

    if (job == _job) {
        job = nullptr;
    }

    pbStart->setEnabled(true);
    pbStop->setEnabled(false);

    //statusBar()->removeWidget( statusTracker->widget(job) );
}

void KioslaveTest::slotSlaveConnected()
{
    qDebug() << "Slave connected.";
}

void KioslaveTest::slotSlaveError()
{
    qDebug() << "Error connected.";
    slave = nullptr;
}

void KioslaveTest::printUDSEntry(const KIO::UDSEntry &entry)
{
    // It's rather rare to iterate that way, usually you'd use numberValue/stringValue directly.
    // This is just to print out all that we got

    QDateTime timestamp;

    const QVector<uint> keys = entry.fields();
    QVector<uint>::const_iterator it = keys.begin();
    for (; it != keys.end(); ++it) {
        switch (*it) {
        case KIO::UDSEntry::UDS_FILE_TYPE: {
            mode_t mode = (mode_t)entry.numberValue(*it);
            qDebug() << "File Type : " << mode;
            if ((mode & QT_STAT_MASK) == QT_STAT_DIR) {
                qDebug() << "is a dir";
            }
            if ((mode & QT_STAT_MASK) == QT_STAT_LNK) {
                qDebug() << "is a link";
            }
        }
        break;
        case KIO::UDSEntry::UDS_ACCESS:
            qDebug() << "Access permissions : " << (mode_t)(entry.numberValue(*it));
            break;
        case KIO::UDSEntry::UDS_USER:
            qDebug() << "User : " << (entry.stringValue(*it));
            break;
        case KIO::UDSEntry::UDS_GROUP:
            qDebug() << "Group : " << (entry.stringValue(*it));
            break;
        case KIO::UDSEntry::UDS_NAME:
            qDebug() << "Name : " << (entry.stringValue(*it));
            //m_strText = decodeFileName( it.value().toString() );
            break;
        case KIO::UDSEntry::UDS_URL:
            qDebug() << "URL : " << (entry.stringValue(*it));
            break;
        case KIO::UDSEntry::UDS_MIME_TYPE:
            qDebug() << "MimeType : " << (entry.stringValue(*it));
            break;
        case KIO::UDSEntry::UDS_LINK_DEST:
            qDebug() << "LinkDest : " << (entry.stringValue(*it));
            break;
        case KIO::UDSEntry::UDS_SIZE:
            qDebug() << "Size: " << KIO::convertSize(entry.numberValue(*it));
            break;
        case KIO::UDSEntry::UDS_CREATION_TIME:
            timestamp = QDateTime::fromSecsSinceEpoch(entry.numberValue(*it));
            qDebug() << "CreationTime: " << QLocale().toString(timestamp, QLocale::ShortFormat);
            break;
        case KIO::UDSEntry::UDS_MODIFICATION_TIME:
            timestamp = QDateTime::fromSecsSinceEpoch(entry.numberValue(*it));
            qDebug() << "ModificationTime: " << QLocale().toString(timestamp, QLocale::ShortFormat);
            break;
        case KIO::UDSEntry::UDS_ACCESS_TIME:
            timestamp = QDateTime::fromSecsSinceEpoch(entry.numberValue(*it));
            qDebug() << "AccessTime: " << QLocale().toString(timestamp, QLocale::ShortFormat);
            break;
        }
    }
}

void KioslaveTest::slotEntries(KIO::Job *job, const KIO::UDSEntryList &list)
{

    QUrl url = static_cast<KIO::ListJob *>(job)->url();
    KProtocolInfo::ExtraFieldList extraFields = KProtocolInfo::extraFields(url);
    UDSEntryList::ConstIterator it = list.begin();
    for (; it != list.end(); ++it) {
        // For each file...
        QString name = (*it).stringValue(KIO::UDSEntry::UDS_NAME);
        qDebug() << name;

        KProtocolInfo::ExtraFieldList::Iterator extraFieldsIt = extraFields.begin();
        const QVector<uint> fields = it->fields();
        QVector<uint>::ConstIterator it2 = fields.begin();
        for (; it2 != fields.end(); it2++) {
            if (*it2 >= UDSEntry::UDS_EXTRA && *it2 <= UDSEntry::UDS_EXTRA_END) {
                if (extraFieldsIt != extraFields.end()) {
                    QString column = (*extraFieldsIt).name;
                    //QString type = (*extraFieldsIt).type;
                    qDebug() << "  Extra data (" << column << ") :" << it->stringValue(*it2);
                    ++extraFieldsIt;
                } else {
                    qDebug() << "  Extra data (UNDEFINED) :" << it->stringValue(*it2);
                }
            }
        }
    }
}

void KioslaveTest::slotData(KIO::Job *, const QByteArray &data)
{
    if (data.size() == 0) {
        qDebug() << "Data: <End>";
    } else {
        qDebug() << "Data: \"" << QString(data) << "\"";
    }
}

void KioslaveTest::slotDataReq(KIO::Job *, QByteArray &data)
{
    const char *fileDataArray[] = {
        "Hello world\n",
        "This is a test file\n",
        "You can safely delete it.\n",
        "BIG\n",
        "BIG1\n",
        "BIG2\n",
        "BIG3\n",
        "BIG4\n",
        "BIG5\n",
        nullptr
    };
    const char *fileData = fileDataArray[putBuffer++];

    if (!fileData) {
        qDebug() << "DataReq: <End>";
        return;
    }
    if (!strncmp(fileData, "BIG", 3)) {
        data.fill(0, 8 * 1024 * 1024);
    } else {
        data = QByteArray(fileData, strlen(fileData));
    }
    qDebug() << "DataReq: \"" << fileData << "\"";
    QThread::sleep(1); // want to see progress info...
}

void KioslaveTest::stopJob()
{
    qDebug() << "KioslaveTest::stopJob()";
    job->kill();
    job = nullptr;

    pbStop->setEnabled(false);
    pbStart->setEnabled(true);
}

int main(int argc, char **argv)
{

    const char version[] = "v0.0.0 0000";   // :-)

    QApplication app(argc, argv);
    app.setApplicationVersion(version);

    uint op = KioslaveTest::Copy;
    uint pr = 0;
    QString src, dest, operation;
    {
        QCommandLineParser parser;
        parser.addVersionOption();
        parser.setApplicationDescription(QStringLiteral("Test for kioslaves"));
        parser.addHelpOption();
        parser.addOption(QCommandLineOption(QStringList() << QStringLiteral("s") << QStringLiteral("src"), QStringLiteral("Source URL"), QStringLiteral("url")));
        parser.addOption(QCommandLineOption(QStringList() << QStringLiteral("d") << QStringLiteral("dest"), QStringLiteral("Destination URL"), QStringLiteral("url")));
        parser.addOption(QCommandLineOption(QStringList() << QStringLiteral("o") << QStringLiteral("operation"), QStringLiteral("Operation (list,listrecursive,stat,get,put,copy,move,del,mkdir)"), QStringLiteral("operation")));
        parser.addOption(QCommandLineOption(QStringList() << QStringLiteral("p") << QStringLiteral("progress"), QStringLiteral("Progress Type (none,default,status)"), QStringLiteral("progress"), QStringLiteral("default")));
        parser.process(app);

        src = parser.value(QStringLiteral("src"));
        dest = parser.value(QStringLiteral("dest"));

        operation = parser.value(QStringLiteral("operation"));
        if (operation == QLatin1String("list")) {
            op = KioslaveTest::List;
        } else if (operation == QLatin1String("listrecursive")) {
            op = KioslaveTest::ListRecursive;
        } else if (operation == QLatin1String("stat")) {
            op = KioslaveTest::Stat;
        } else if (operation == QLatin1String("get")) {
            op = KioslaveTest::Get;
        } else if (operation == QLatin1String("put")) {
            op = KioslaveTest::Put;
        } else if (operation == QLatin1String("copy")) {
            op = KioslaveTest::Copy;
        } else if (operation == QLatin1String("move")) {
            op = KioslaveTest::Move;
        } else if (operation == QLatin1String("del")) {
            op = KioslaveTest::Delete;
        } else if (operation == QLatin1String("mkdir")) {
            op = KioslaveTest::Mkdir;
        } else if (!operation.isEmpty()) {
            qWarning("Unknown operation, see --help");
            return 1;
        }

        QString progress = parser.value(QStringLiteral("progress"));
        if (progress == QLatin1String("none")) {
            pr = KioslaveTest::ProgressNone;
        } else if (progress == QLatin1String("default")) {
            pr = KioslaveTest::ProgressDefault;
        } else if (progress == QLatin1String("status")) {
            pr = KioslaveTest::ProgressStatus;
        } else {
            qWarning("Unknown progress mode, see --help");
            return 1;
        }
    }

    KioslaveTest *test = new KioslaveTest(src, dest, op, pr);
    if (!operation.isEmpty()) {
        QTimer::singleShot(100, test, SLOT(startJob()));
    }
    test->show();
    test->resize(test->sizeHint());

    app.exec();
}

#include "moc_kioslavetest.cpp"
