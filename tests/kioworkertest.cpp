/*
    This file is or will be part of KDE desktop environment
    SPDX-FileCopyrightText: 1999 Matt Koss <koss@miesto.sk>

    SPDX-License-Identifier: LGPL-2.0-only
*/

#include "kioworkertest.h"

#include "../src/utils_p.h"

#include <kio/copyjob.h>
#include <kio/deletejob.h>
#include <kio/listjob.h>
#include <kio/mimetypejob.h>
#include <kio/mkdirjob.h>
#include <kio/statjob.h>
#include <kio/transferjob.h>
#include <kprotocolinfo.h>

// QT_STAT_LNK on Windows
#include "kioglobal_p.h"

#include <KJobUiDelegate>

#include <QApplication>
#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QDebug>
#include <QDir>
#include <QGroupBox>
#include <QLayout>
#include <QLocale>
#include <QMessageBox>
#include <QStatusBar>
#include <QThread>
#include <QTimer>
#include <QUrl>
#include <qplatformdefs.h>

using namespace KIO;

KioWorkerTest::KioWorkerTest(QString src, QString dest, uint op, uint pr)
    : QMainWindow(nullptr)
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
    connect(opButtons, &QButtonGroup::buttonClicked, this, &KioWorkerTest::changeOperation);

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
    connect(progressButtons, &QButtonGroup::buttonClicked, this, &KioWorkerTest::changeProgressMode);

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
    connect(pbStart, &QAbstractButton::clicked, this, &KioWorkerTest::startJob);
    hbLayout->addWidget(pbStart, 5);

    pbStop = new QPushButton(QStringLiteral("Sto&p"), main_widget);
    pbStop->setFixedSize(pbStop->sizeHint());
    pbStop->setEnabled(false);
    connect(pbStop, &QAbstractButton::clicked, this, &KioWorkerTest::stopJob);
    hbLayout->addWidget(pbStop, 5);

    // close button
    close = new QPushButton(QStringLiteral("&Close"), main_widget);
    close->setFixedSize(close->sizeHint());
    connect(close, &QAbstractButton::clicked, this, &KioWorkerTest::slotQuit);

    topLayout->addWidget(close, 5);

    main_widget->setMinimumSize(main_widget->sizeHint());
    setCentralWidget(main_widget);
}

void KioWorkerTest::slotQuit()
{
    qApp->quit();
}

void KioWorkerTest::changeOperation(QAbstractButton *b)
{
    // only two urls for copy and move
    bool enab = rbCopy->isChecked() || rbMove->isChecked();

    le_dest->setEnabled(enab);

    selectedOperation = opButtons->buttons().indexOf(b);
}

void KioWorkerTest::changeProgressMode(QAbstractButton *b)
{
    progressMode = progressButtons->buttons().indexOf(b);

    if (progressMode == ProgressStatus) {
        statusBar()->show();
    } else {
        statusBar()->hide();
    }
}

void KioWorkerTest::startJob()
{
    QUrl sCurrent(QUrl::fromLocalFile(QDir::currentPath()));
    QString sSrc(le_source->text());
    QUrl src = QUrl(sCurrent).resolved(QUrl(sSrc));

    if (!src.isValid()) {
        QMessageBox::critical(this, QStringLiteral("KioWorker Error Message"), QStringLiteral("Source URL is malformed"));
        return;
    }

    QString sDest(le_dest->text());
    QUrl dest = QUrl(sCurrent).resolved(QUrl(sDest));

    if (!dest.isValid() && (selectedOperation == Copy || selectedOperation == Move)) {
        QMessageBox::critical(this, QStringLiteral("KioWorker Error Message"), QStringLiteral("Destination URL is malformed"));
        return;
    }

    pbStart->setEnabled(false);

    KIO::JobFlags observe = DefaultFlags;
    if (progressMode != ProgressDefault) {
        observe = HideProgressInfo;
    }

    switch (selectedOperation) {
    case List: {
        KIO::ListJob *listJob = KIO::listDir(src);
        job = listJob;
        connect(listJob, &KIO::ListJob::entries, this, &KioWorkerTest::slotEntries);
        break;
    }

    case ListRecursive: {
        KIO::ListJob *listJob = KIO::listRecursive(src);
        job = listJob;
        connect(listJob, &KIO::ListJob::entries, this, &KioWorkerTest::slotEntries);
        break;
    }

    case Stat:
        job = KIO::stat(src);
        break;

    case Get: {
        KIO::TransferJob *tjob = KIO::get(src, KIO::Reload);
        job = tjob;
        connect(tjob, &KIO::TransferJob::data, this, &KioWorkerTest::slotData);
        break;
    }

    case Put: {
        putBuffer = 0;
        KIO::TransferJob *tjob = KIO::put(src, -1, KIO::Overwrite);
        tjob->setTotalSize(48 * 1024 * 1024);
        job = tjob;
        connect(tjob, &TransferJob::dataReq, this, &KioWorkerTest::slotDataReq);
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
        job = KIO::mkdir(src);
        break;

    case Mimetype:
        job = KIO::mimetype(src);
        break;
    }

    statusBar()->addWidget(statusTracker->widget(job), 0);

    connect(job, &KJob::result, this, &KioWorkerTest::slotResult);

    if (progressMode == ProgressStatus) {
        statusTracker->registerJob(job);
    }

    pbStop->setEnabled(true);
}

void KioWorkerTest::slotResult(KJob *_job)
{
    if (_job->error()) {
        _job->uiDelegate()->showErrorMessage();
    } else if (selectedOperation == Stat) {
        UDSEntry entry = static_cast<KIO::StatJob *>(_job)->statResult();
        printUDSEntry(entry);
    } else if (selectedOperation == Mimetype) {
        qDebug() << "MIME type is " << static_cast<KIO::MimetypeJob *>(_job)->mimetype();
    }

    if (job == _job) {
        job = nullptr;
    }

    pbStart->setEnabled(true);
    pbStop->setEnabled(false);

    // statusBar()->removeWidget( statusTracker->widget(job) );
}

void KioWorkerTest::printUDSEntry(const KIO::UDSEntry &entry)
{
    // It's rather rare to iterate that way, usually you'd use numberValue/stringValue directly.
    // This is just to print out all that we got

    QDateTime timestamp;

    const QList<uint> keys = entry.fields();
    for (auto it = keys.cbegin(); it != keys.cend(); ++it) {
        switch (*it) {
        case KIO::UDSEntry::UDS_FILE_TYPE: {
            mode_t mode = (mode_t)entry.numberValue(*it);
            qDebug() << "File Type : " << mode;
            if (Utils::isDirMask(mode)) {
                qDebug() << "is a dir";
            }
            if (Utils::isLinkMask(mode)) {
                qDebug() << "is a link";
            }
            break;
        }
        case KIO::UDSEntry::UDS_ACCESS:
            qDebug() << "Access permissions : " << (mode_t)(entry.numberValue(*it));
            break;
        case KIO::UDSEntry::UDS_USER:
            qDebug() << "User : " << (entry.stringValue(*it));
            break;
        case KIO::UDSEntry::UDS_GROUP:
            qDebug() << "Group : " << (entry.stringValue(*it));
            break;
        case KIO::UDSEntry::UDS_LOCAL_USER_ID:
            qDebug() << "User id : " << (entry.numberValue(*it));
            break;
        case KIO::UDSEntry::UDS_LOCAL_GROUP_ID:
            qDebug() << "Group id : " << (entry.numberValue(*it));
            break;
        case KIO::UDSEntry::UDS_NAME:
            qDebug() << "Name : " << (entry.stringValue(*it));
            // m_strText = decodeFileName( it.value().toString() );
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

void KioWorkerTest::slotEntries(KIO::Job *job, const KIO::UDSEntryList &list)
{
    QUrl url = static_cast<KIO::ListJob *>(job)->url();
    KProtocolInfo::ExtraFieldList extraFields = KProtocolInfo::extraFields(url);
    UDSEntryList::ConstIterator it = list.begin();
    for (; it != list.end(); ++it) {
        // For each file...
        QString name = (*it).stringValue(KIO::UDSEntry::UDS_NAME);
        qDebug() << name;

        KProtocolInfo::ExtraFieldList::Iterator extraFieldsIt = extraFields.begin();
        const QList<uint> fields = it->fields();
        QList<uint>::ConstIterator it2 = fields.begin();
        for (; it2 != fields.end(); it2++) {
            if (*it2 >= UDSEntry::UDS_EXTRA && *it2 <= UDSEntry::UDS_EXTRA_END) {
                if (extraFieldsIt != extraFields.end()) {
                    QString column = (*extraFieldsIt).name;
                    // QString type = (*extraFieldsIt).type;
                    qDebug() << "  Extra data (" << column << ") :" << it->stringValue(*it2);
                    ++extraFieldsIt;
                } else {
                    qDebug() << "  Extra data (UNDEFINED) :" << it->stringValue(*it2);
                }
            }
        }
    }
}

void KioWorkerTest::slotData(KIO::Job *, const QByteArray &data)
{
    if (data.size() == 0) {
        qDebug() << "Data: <End>";
    } else {
        qDebug() << "Data: \"" << QString::fromUtf8(data) << "\"";
    }
}

void KioWorkerTest::slotDataReq(KIO::Job *, QByteArray &data)
{
    /* clang-format off */
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
    /* clang-format on */

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

void KioWorkerTest::stopJob()
{
    qDebug() << "KioWorkerTest::stopJob()";
    job->kill();
    job = nullptr;

    pbStop->setEnabled(false);
    pbStart->setEnabled(true);
}

int main(int argc, char **argv)
{
    const char version[] = "v0.0.0 0000"; // :-)

    QApplication app(argc, argv);
    app.setApplicationVersion(QString::fromLatin1(version));

    uint op = KioWorkerTest::Copy;
    uint pr = 0;
    QString src;
    QString dest;
    QString operation;
    {
        QCommandLineParser parser;
        parser.addVersionOption();
        parser.setApplicationDescription(QStringLiteral("Test for KIO workers"));
        parser.addHelpOption();
        parser.addOption(
            QCommandLineOption(QStringList() << QStringLiteral("s") << QStringLiteral("src"), QStringLiteral("Source URL"), QStringLiteral("url")));
        parser.addOption(
            QCommandLineOption(QStringList() << QStringLiteral("d") << QStringLiteral("dest"), QStringLiteral("Destination URL"), QStringLiteral("url")));
        parser.addOption(QCommandLineOption(QStringList() << QStringLiteral("o") << QStringLiteral("operation"),
                                            QStringLiteral("Operation (list,listrecursive,stat,get,put,copy,move,del,mkdir)"),
                                            QStringLiteral("operation")));
        parser.addOption(QCommandLineOption(QStringList() << QStringLiteral("p") << QStringLiteral("progress"),
                                            QStringLiteral("Progress Type (none,default,status)"),
                                            QStringLiteral("progress"),
                                            QStringLiteral("default")));
        parser.process(app);

        src = parser.value(QStringLiteral("src"));
        dest = parser.value(QStringLiteral("dest"));

        operation = parser.value(QStringLiteral("operation"));
        if (operation == QLatin1String("list")) {
            op = KioWorkerTest::List;
        } else if (operation == QLatin1String("listrecursive")) {
            op = KioWorkerTest::ListRecursive;
        } else if (operation == QLatin1String("stat")) {
            op = KioWorkerTest::Stat;
        } else if (operation == QLatin1String("get")) {
            op = KioWorkerTest::Get;
        } else if (operation == QLatin1String("put")) {
            op = KioWorkerTest::Put;
        } else if (operation == QLatin1String("copy")) {
            op = KioWorkerTest::Copy;
        } else if (operation == QLatin1String("move")) {
            op = KioWorkerTest::Move;
        } else if (operation == QLatin1String("del")) {
            op = KioWorkerTest::Delete;
        } else if (operation == QLatin1String("mkdir")) {
            op = KioWorkerTest::Mkdir;
        } else if (!operation.isEmpty()) {
            qWarning("Unknown operation, see --help");
            return 1;
        }

        QString progress = parser.value(QStringLiteral("progress"));
        if (progress == QLatin1String("none")) {
            pr = KioWorkerTest::ProgressNone;
        } else if (progress == QLatin1String("default")) {
            pr = KioWorkerTest::ProgressDefault;
        } else if (progress == QLatin1String("status")) {
            pr = KioWorkerTest::ProgressStatus;
        } else {
            qWarning("Unknown progress mode, see --help");
            return 1;
        }
    }

    KioWorkerTest *test = new KioWorkerTest(src, dest, op, pr);
    if (!operation.isEmpty()) {
        QTimer::singleShot(100, test, SLOT(startJob()));
    }
    test->show();
    test->resize(test->sizeHint());

    return app.exec();
}

#include "moc_kioworkertest.cpp"
