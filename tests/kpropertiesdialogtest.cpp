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

    // This is the test for the KPropertiesDialog constructor that is now
    // documented to NOT work. Passing only a URL means a KIO::NetAccess::stat will happen,
    // and asking for the dialog to be modal too creates problems.
    // (A non-modal, URL-only dialog is the one kicker uses for app buttons, no problem there)
    {
        KPropertiesDialog dlg(u, 0);
        QObject::connect(&dlg, &KPropertiesDialog::applied, [](){ qDebug() << "applied"; });
        QObject::connect(&dlg, &KPropertiesDialog::canceled, [](){ qDebug() << "canceled"; });
        dlg.exec();
    }

    return 0;
}
