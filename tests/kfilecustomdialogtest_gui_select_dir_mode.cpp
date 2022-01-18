/*
    This file is part of the KDE libraries
    SPDX-FileCopyrightText: 2017 Klarälvdalens Datakonsult AB, a KDAB Group company <info@kdab.com>
    Work sponsored by the LiMux project of the city of Munich

    SPDX-License-Identifier: LGPL-2.0-only
*/

#include "kfilecustomdialog.h"

#include <QApplication>
#include <QDebug>
#include <QDialog>
#include <QLabel>
#include <QObject>
#include <QUrl>

int main(int argc, char **argv)
{
    QApplication::setApplicationName(QStringLiteral("KFileCustomDialogTest_gui"));
    QApplication app(argc, argv);

    KFileCustomDialog dlg;
    KFileWidget *fileWidget = dlg.fileWidget();
    fileWidget->setMode(KFile::Directory);
    dlg.setOperationMode(KFileWidget::Opening);
    dlg.setWindowTitle("Select folder");

    dlg.connect(dlg.fileWidget(), &KFileWidget::accepted, &app, [&dlg]() {
        qDebug() << "Selected dir URL:" << dlg.fileWidget()->selectedUrl();
    });
    dlg.show();

    return app.exec();
}
