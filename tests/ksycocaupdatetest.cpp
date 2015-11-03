#include <kbuildsycocaprogressdialog.h>

#include <QApplication>

int main(int argc, char *argv[])
{
    QApplication::setApplicationName(QStringLiteral("whatever"));
    QApplication k(argc, argv);

    KBuildSycocaProgressDialog::rebuildKSycoca(0);
    return 0;
}
