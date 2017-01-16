#include <kbuildsycocaprogressdialog.h>

#include <QApplication>

int main(int argc, char *argv[])
{
    QApplication::setApplicationName(QStringLiteral("whatever"));
    QApplication k(argc, argv);

    KBuildSycocaProgressDialog::rebuildKSycoca(nullptr);
    return 0;
}
