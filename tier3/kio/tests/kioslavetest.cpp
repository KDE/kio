 /*
  This file is or will be part of KDE desktop environment

  Copyright 1999 Matt Koss <koss@miesto.sk>

  It is licensed under GPL version 2.

  If it is part of KDE libraries than this file is licensed under
  LGPL version 2.
 */

#include <QApplication>
#include <QLayout>
#include <QMessageBox>
#include <QtCore/QDir>
#include <QGroupBox>
#include <QStatusBar>
#include <QDebug>
#include <QUrl>

#include <unistd.h>

#include <kjobuidelegate.h>
#include <kio/job.h>
#include <kio/copyjob.h>
#include <kio/deletejob.h>
#include <kio/scheduler.h>
#include <kprotocolinfo.h>
#include <QtCore/QTimer>
#include <qcommandlineparser.h>
#include <qcommandlineoption.h>
#include <qplatformdefs.h>

#include "kioslavetest.h"

using namespace KIO;

KioslaveTest::KioslaveTest( QString src, QString dest, uint op, uint pr )
  : KMainWindow(0)
{

  job = 0L;

  main_widget = new QWidget( this );
  QBoxLayout *topLayout = new QVBoxLayout( main_widget );

  QGridLayout *grid = new QGridLayout();
  topLayout->addLayout( grid );

  grid->setRowStretch(0,1);
  grid->setRowStretch(1,1);

  grid->setColumnStretch(0,1);
  grid->setColumnStretch(1,100);

  lb_from = new QLabel( "From:", main_widget );
  grid->addWidget( lb_from, 0, 0 );

  le_source = new QLineEdit( main_widget );
  grid->addWidget( le_source, 0, 1 );
  le_source->setText( src );

  lb_to = new QLabel( "To:", main_widget );
  grid->addWidget( lb_to, 1, 0 );

  le_dest = new QLineEdit( main_widget );
  grid->addWidget( le_dest, 1, 1 );
  le_dest->setText( dest );

  // Operation groupbox & buttons
  opButtons = new QButtonGroup( main_widget );
  QGroupBox *box = new QGroupBox( "Operation", main_widget );
  topLayout->addWidget( box, 10 );
  connect( opButtons, SIGNAL(buttonClicked(QAbstractButton*)), SLOT(changeOperation(QAbstractButton*)) );

  QBoxLayout *hbLayout = new QHBoxLayout( box );

  rbList = new QRadioButton( "List", box );
  opButtons->addButton( rbList );
  hbLayout->addWidget( rbList, 5 );

  rbListRecursive = new QRadioButton( "ListRecursive", box );
  opButtons->addButton( rbListRecursive );
  hbLayout->addWidget( rbListRecursive, 5 );

  rbStat = new QRadioButton( "Stat", box );
  opButtons->addButton( rbStat );
  hbLayout->addWidget( rbStat, 5 );

  rbGet = new QRadioButton( "Get", box );
  opButtons->addButton( rbGet );
  hbLayout->addWidget( rbGet, 5 );

  rbPut = new QRadioButton( "Put", box );
  opButtons->addButton( rbPut );
  hbLayout->addWidget( rbPut, 5 );

  rbCopy = new QRadioButton( "Copy", box );
  opButtons->addButton( rbCopy );
  hbLayout->addWidget( rbCopy, 5 );

  rbMove = new QRadioButton( "Move", box );
  opButtons->addButton( rbMove );
  hbLayout->addWidget( rbMove, 5 );

  rbDelete = new QRadioButton( "Delete", box );
  opButtons->addButton( rbDelete );
  hbLayout->addWidget( rbDelete, 5 );

  rbMkdir = new QRadioButton( "Mkdir", box );
  opButtons->addButton( rbMkdir );
  hbLayout->addWidget( rbMkdir, 5 );

  rbMimetype = new QRadioButton( "Mimetype", box );
  opButtons->addButton( rbMimetype );
  hbLayout->addWidget( rbMimetype, 5 );

  QAbstractButton *b = opButtons->buttons()[op];
  b->setChecked( true );
  changeOperation( b );

  // Progress groupbox & buttons
  progressButtons = new QButtonGroup( main_widget );
  box = new QGroupBox( "Progress dialog mode", main_widget );
  topLayout->addWidget( box, 10 );
  connect( progressButtons, SIGNAL(buttonClicked(QAbstractButton*)), SLOT(changeProgressMode(QAbstractButton*)) );

  hbLayout = new QHBoxLayout( box );

  rbProgressNone = new QRadioButton( "None", box );
  progressButtons->addButton( rbProgressNone );
  hbLayout->addWidget( rbProgressNone, 5 );

  rbProgressDefault = new QRadioButton( "Default", box );
  progressButtons->addButton( rbProgressDefault );
  hbLayout->addWidget( rbProgressDefault, 5 );

  rbProgressStatus = new QRadioButton( "Status", box );
  progressButtons->addButton( rbProgressStatus );
  hbLayout->addWidget( rbProgressStatus, 5 );

  b = progressButtons->buttons()[pr];
  b->setChecked( true );
  changeProgressMode( b );

  // statusbar progress widget
  statusTracker = new KStatusBarJobTracker( statusBar() );

  // run & stop buttons
  hbLayout = new QHBoxLayout();
  topLayout->addLayout( hbLayout );
  hbLayout->setParent( topLayout );

  pbStart = new QPushButton( "&Start", main_widget );
  pbStart->setFixedSize( pbStart->sizeHint() );
  connect( pbStart, SIGNAL(clicked()), SLOT(startJob()) );
  hbLayout->addWidget( pbStart, 5 );

  pbStop = new QPushButton( "Sto&p", main_widget );
  pbStop->setFixedSize( pbStop->sizeHint() );
  pbStop->setEnabled( false );
  connect( pbStop, SIGNAL(clicked()), SLOT(stopJob()) );
  hbLayout->addWidget( pbStop, 5 );

  // close button
  close = new QPushButton( "&Close", main_widget );
  close->setFixedSize( close->sizeHint() );
  connect(close, SIGNAL(clicked()), this, SLOT(slotQuit()));

  topLayout->addWidget( close, 5 );

  main_widget->setMinimumSize( main_widget->sizeHint() );
  setCentralWidget( main_widget );

  slave = 0;
//  slave = KIO::Scheduler::getConnectedSlave(QUrl("ftp://ftp.kde.org"));
  KIO::Scheduler::connect(SIGNAL(slaveConnected(KIO::Slave*)),
	this, SLOT(slotSlaveConnected()));
  KIO::Scheduler::connect(SIGNAL(slaveError(KIO::Slave*,int,QString)),
	this, SLOT(slotSlaveError()));
}

void KioslaveTest::slotQuit(){
  qApp->quit();
}


void KioslaveTest::changeOperation( QAbstractButton *b ) {
  // only two urls for copy and move
  bool enab = rbCopy->isChecked() || rbMove->isChecked();

  le_dest->setEnabled( enab );

  selectedOperation = opButtons->buttons().indexOf( b );
}


void KioslaveTest::changeProgressMode( QAbstractButton *b ) {
  progressMode = progressButtons->buttons().indexOf( b );

  if ( progressMode == ProgressStatus ) {
    statusBar()->show();
  } else {
    statusBar()->hide();
  }
}


void KioslaveTest::startJob() {
  QUrl sCurrent( QUrl::fromLocalFile( QDir::currentPath() ) );
  QString sSrc( le_source->text() );
  QUrl src = QUrl( sCurrent ).resolved( QUrl(sSrc) );

  if ( !src.isValid() ) {
    QMessageBox::critical(this, "Kioslave Error Message", "Source URL is malformed" );
    return;
  }

  QString sDest( le_dest->text() );
  QUrl dest = QUrl( sCurrent ).resolved( QUrl(sDest) );

  if ( !dest.isValid() &&
       ( selectedOperation == Copy || selectedOperation == Move ) ) {
    QMessageBox::critical(this, "Kioslave Error Message",
                       "Destination URL is malformed" );
    return;
  }

  pbStart->setEnabled( false );

  KIO::JobFlags observe = DefaultFlags;
  if (progressMode != ProgressDefault) {
    observe = HideProgressInfo;
  }

  SimpleJob *myJob = 0;

  switch ( selectedOperation ) {
  case List:
    myJob = KIO::listDir( src );
    connect(myJob, SIGNAL(entries(KIO::Job*,KIO::UDSEntryList)),
            SLOT(slotEntries(KIO::Job*,KIO::UDSEntryList)));
    break;

  case ListRecursive:
    myJob = KIO::listRecursive( src );
    connect(myJob, SIGNAL(entries(KIO::Job*,KIO::UDSEntryList)),
            SLOT(slotEntries(KIO::Job*,KIO::UDSEntryList)));
    break;

  case Stat:
    myJob = KIO::stat( src, KIO::StatJob::SourceSide, 2 );
    break;

  case Get:
    myJob = KIO::get( src, KIO::Reload );
    connect(myJob, SIGNAL(data(KIO::Job*,QByteArray)),
            SLOT(slotData(KIO::Job*,QByteArray)));
    break;

  case Put:
  {
    putBuffer = 0;
    KIO::TransferJob* tjob = KIO::put( src, -1, KIO::Overwrite );
    tjob->setTotalSize(48*1024*1024);
    myJob = tjob;
    connect(tjob, SIGNAL(dataReq(KIO::Job*,QByteArray&)),
            SLOT(slotDataReq(KIO::Job*,QByteArray&)));
    break;
  }

  case Copy:
    job = KIO::copy( src, dest, observe );
    break;

  case Move:
    job = KIO::move( src, dest, observe );
    break;

  case Delete:
    job = KIO::del( src, observe );
    break;

  case Mkdir:
    myJob = KIO::mkdir( src );
    break;

  case Mimetype:
    myJob = KIO::mimetype( src );
    break;
  }
  if (myJob)
  {
    if (slave)
      KIO::Scheduler::assignJobToSlave(slave, myJob);
    job = myJob;
  }

  statusBar()->addWidget( statusTracker->widget(job), 0 );

  connect( job, SIGNAL(result(KJob*)),
           SLOT(slotResult(KJob*)) );

  connect( job, SIGNAL(canceled(KJob*)),
           SLOT(slotResult(KJob*)) );

  if (progressMode == ProgressStatus) {
    statusTracker->registerJob( job );
  }

  pbStop->setEnabled( true );
}


void KioslaveTest::slotResult( KJob * _job )
{
  if ( _job->error() )
  {
      job->uiDelegate()->showErrorMessage();
  }
  else if ( selectedOperation == Stat )
  {
      UDSEntry entry = ((KIO::StatJob*)_job)->statResult();
      printUDSEntry( entry );
  }
  else if ( selectedOperation == Mimetype )
  {
      qDebug() << "mimetype is " << ((KIO::MimetypeJob*)_job)->mimetype();
  }

  if (job == _job)
     job = 0L;

  pbStart->setEnabled( true );
  pbStop->setEnabled( false );

  //statusBar()->removeWidget( statusTracker->widget(job) );
}

void KioslaveTest::slotSlaveConnected()
{
   qDebug() << "Slave connected.";
}

void KioslaveTest::slotSlaveError()
{
   qDebug() << "Error connected.";
   slave = 0;
}

void KioslaveTest::printUDSEntry( const KIO::UDSEntry & entry )
{
    // It's rather rare to iterate that way, usually you'd use numberValue/stringValue directly.
    // This is just to print out all that we got

    const QList<uint> keys = entry.listFields();
    QList<uint>::const_iterator it = keys.begin();
    for( ; it != keys.end(); ++it ) {
        switch ( *it ) {
            case KIO::UDSEntry::UDS_FILE_TYPE:
                {
                    mode_t mode = (mode_t)entry.numberValue(*it);
                    qDebug() << "File Type : " << mode;
                    if ( (mode & QT_STAT_MASK) == QT_STAT_DIR )
                    {
                        qDebug() << "is a dir";
                    }
                }
                break;
            case KIO::UDSEntry::UDS_ACCESS:
                qDebug() << "Access permissions : " << (mode_t)( entry.numberValue(*it) ) ;
                break;
            case KIO::UDSEntry::UDS_USER:
                qDebug() << "User : " << ( entry.stringValue(*it) );
                break;
            case KIO::UDSEntry::UDS_GROUP:
                qDebug() << "Group : " << ( entry.stringValue(*it) );
                break;
            case KIO::UDSEntry::UDS_NAME:
                qDebug() << "Name : " << ( entry.stringValue(*it) );
                //m_strText = decodeFileName( it.value().toString() );
                break;
            case KIO::UDSEntry::UDS_URL:
                qDebug() << "URL : " << ( entry.stringValue(*it) );
                break;
            case KIO::UDSEntry::UDS_MIME_TYPE:
                qDebug() << "MimeType : " << ( entry.stringValue(*it) );
                break;
            case KIO::UDSEntry::UDS_LINK_DEST:
                qDebug() << "LinkDest : " << ( entry.stringValue(*it) );
                break;
            case KIO::UDSEntry::UDS_SIZE:
                qDebug() << "Size: " << KIO::convertSize(entry.numberValue(*it));
                break;
        }
    }
}

void KioslaveTest::slotEntries(KIO::Job* job, const KIO::UDSEntryList& list) {

    QUrl url = static_cast<KIO::ListJob*>( job )->url();
    KProtocolInfo::ExtraFieldList extraFields = KProtocolInfo::extraFields(url);
    UDSEntryList::ConstIterator it=list.begin();
    for (; it != list.end(); ++it) {
        // For each file...
        QString name = (*it).stringValue( KIO::UDSEntry::UDS_NAME );
        qDebug() << name;

        KProtocolInfo::ExtraFieldList::Iterator extraFieldsIt = extraFields.begin();
        const QList<uint> fields = it->listFields();
        QList<uint>::ConstIterator it2 = fields.begin();
        for( ; it2 != fields.end(); it2++ ) {
            if ( *it2 >= UDSEntry::UDS_EXTRA && *it2 <= UDSEntry::UDS_EXTRA_END) {
                if ( extraFieldsIt != extraFields.end() ) {
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

void KioslaveTest::slotData(KIO::Job*, const QByteArray &data)
{
    if (data.size() == 0)
    {
       qDebug() << "Data: <End>";
    }
    else
    {
       qDebug() << "Data: \"" << QString( data ) << "\"";
    }
}

void KioslaveTest::slotDataReq(KIO::Job*, QByteArray &data)
{
    const char *fileDataArray[] =
       {
         "Hello world\n",
         "This is a test file\n",
         "You can safely delete it.\n",
	 "BIG\n",
	 "BIG1\n",
	 "BIG2\n",
	 "BIG3\n",
	 "BIG4\n",
	 "BIG5\n",
         0
       };
    const char *fileData = fileDataArray[putBuffer++];

    if (!fileData)
    {
       qDebug() << "DataReq: <End>";
       return;
    }
    if (!strncmp(fileData, "BIG", 3))
	data.fill(0, 8*1024*1024);
    else
	data = QByteArray(fileData, strlen(fileData));
    qDebug() << "DataReq: \"" << fileData << "\"";
    sleep(1); // want to see progress info...
}

void KioslaveTest::stopJob() {
  qDebug() << "KioslaveTest::stopJob()";
  job->kill();
  job = 0L;

  pbStop->setEnabled( false );
  pbStart->setEnabled( true );
}

int main(int argc, char **argv) {

  const char version[] = "v0.0.0 0000";   // :-)

  QApplication app(argc, argv);
  app.setApplicationVersion(version);

  uint op = KioslaveTest::Copy;
  uint pr = 0;
  QString src, dest, operation;
  {
    QCommandLineParser parser;
    parser.addVersionOption();
    parser.setApplicationDescription("Test for kioslaves");
    parser.addHelpOption();
    parser.addOption(QCommandLineOption(QStringList() << "s" << "src", "Source URL", "url"));
    parser.addOption(QCommandLineOption(QStringList() << "d" << "dest", "Destination URL", "url"));
    parser.addOption(QCommandLineOption(QStringList() << "o" << "operation", "Operation (list,listrecursive,stat,get,put,copy,move,del,mkdir)", "operation"));
    parser.addOption(QCommandLineOption(QStringList() << "p" << "progress", "Progress Type (none,default,status)", "progress", "default"));
    parser.process(app);

    src = parser.value("src");
    dest = parser.value("dest");

    operation = parser.value("operation");
    if ( operation == "list") {
        op = KioslaveTest::List;
    } else if ( operation == "listrecursive") {
        op = KioslaveTest::ListRecursive;
    } else if ( operation == "stat") {
        op = KioslaveTest::Stat;
    } else if ( operation == "get") {
        op = KioslaveTest::Get;
    } else if ( operation == "put") {
        op = KioslaveTest::Put;
    } else if ( operation == "copy") {
        op = KioslaveTest::Copy;
    } else if ( operation == "move") {
        op = KioslaveTest::Move;
    } else if ( operation == "del") {
        op = KioslaveTest::Delete;
    } else if ( operation == "mkdir") {
        op = KioslaveTest::Mkdir;
    } else if (!operation.isEmpty()) {
        qWarning("Unknown operation, see --help");
        return 1;
    }

    QString progress = parser.value("progress");
    if ( progress == "none") {
        pr = KioslaveTest::ProgressNone;
    } else if ( progress == "default") {
        pr = KioslaveTest::ProgressDefault;
    } else if ( progress == "status") {
        pr = KioslaveTest::ProgressStatus;
    } else {
        qWarning("Unknown progress mode, see --help");
        return 1;
    }
  }

  KioslaveTest* test = new KioslaveTest( src, dest, op, pr );
  if (!operation.isEmpty())
      QTimer::singleShot(100, test, SLOT(startJob()));
  test->show();
  test->resize( test->sizeHint() );

  app.exec();
}

#include "moc_kioslavetest.cpp"
