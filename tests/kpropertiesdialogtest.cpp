#include <QApplication>
#include <QDebug>
#include <QDir>
#include <kpropertiesdialog.h>

int main(int argc, char **argv)
{
    QApplication app(argc, argv);

    if (argc <= 1) {
        qWarning() << "Expected argument: [url], the path or url to the file/dir for which to show properties";
        return 1;
    }
    const QUrl u = QUrl::fromUserInput(argv[1], QDir::currentPath());

    {
        KPropertiesDialog dlg(u);
        QObject::connect(&dlg, &KPropertiesDialog::applied, [](){ qDebug() << "applied"; });
        QObject::connect(&dlg, &KPropertiesDialog::canceled, [](){ qDebug() << "canceled"; });
        dlg.exec();
    }

    return 0;
}
