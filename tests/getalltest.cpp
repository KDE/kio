#include <kservice.h>
#include <qmimedatabase.h>
#include <kservicetype.h>

#include <qapplication.h>
#include <QDebug>

int main(int argc, char *argv[])
{
    QApplication::setApplicationName("getalltest");
    QApplication k(argc, argv);

//for (int i = 0 ; i < 2 ; ++i ) { // test twice to see if they got deleted
   qDebug() << "All services";
   KService::List services = KService::allServices();
   qDebug() << "got " << services.count() << " services";
   Q_FOREACH(const KService::Ptr s, services) {
     qDebug() << s->name() << " " << s->entryPath();
   }
//}

   qDebug() << "All mimeTypes";
   QMimeDatabase db;
   QList<QMimeType> mimeTypes = db.allMimeTypes();
   qDebug() << "got " << mimeTypes.count() << " mimeTypes";
   Q_FOREACH(const QMimeType& m, mimeTypes) {
     qDebug() << m.name();
   }

   qDebug() << "All service types";
   KServiceType::List list = KServiceType::allServiceTypes();
   qDebug() << "got " << list.count() << " service types";
   Q_FOREACH(const KServiceType::Ptr st, list) {
     qDebug() << st->name();
   }

   qDebug() << "done";

   return 0;
}
