#include <KService>
#include <QMimeDatabase>

#include <QApplication>
#include <QDebug>

int main(int argc, char *argv[])
{
    QApplication::setApplicationName(QStringLiteral("getalltest"));
    QApplication k(argc, argv);

    // for (int i = 0 ; i < 2 ; ++i ) { // test twice to see if they got deleted
    qDebug() << "All services";
    const KService::List services = KService::allServices();
    qDebug() << "got " << services.count() << " services";
    for (const KService::Ptr &s : services) {
        qDebug() << s->name() << " " << s->entryPath();
    }
    //}

    qDebug() << "All mimeTypes";
    QMimeDatabase db;
    const QList<QMimeType> mimeTypes = db.allMimeTypes();
    qDebug() << "got " << mimeTypes.count() << " mimeTypes";
    for (const QMimeType &m : mimeTypes) {
        qDebug() << m.name();
    }

    qDebug() << "done";

    return 0;
}
