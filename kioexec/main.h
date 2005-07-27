#ifndef _main_h
#define _main_h

#include <qobject.h>
#include <qstring.h>
#include <q3strlist.h>
#include <qtimer.h>

namespace KIO { class Job; }

class KIOExec : public QObject
{
    Q_OBJECT
public:
    KIOExec();

public slots:
    void slotResult( KIO::Job * );
    void slotRunApp();

protected:
    bool tempfiles;
    int counter;
    int expectedCounter;
    QString command;
    struct fileInfo {
       QString path;
       KURL url;
       int time;
    };
    Q3ValueList<fileInfo> fileList;
};

#endif
