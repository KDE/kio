/*
    This file is part of the KDE libraries
    SPDX-FileCopyrightText: 2017 Klarälvdalens Datakonsult AB, a KDAB Group company <info@kdab.com>
    Work sponsored by the LiMux project of the city of Munich

    SPDX-License-Identifier: LGPL-2.0-only
*/

#include "kfilecustomdialog.h"
#include <QApplication>
#include <QLabel>

int main(int argc, char **argv)
{
    QApplication::setApplicationName(QStringLiteral("KFileCustomDialogTest_gui"));
    QApplication app(argc, argv);
    KFileCustomDialog dlg;
    QLabel *lab = new QLabel(QStringLiteral("foo"));
    dlg.setCustomWidget(lab);
    dlg.exec();


    //Save dialog box
    KFileCustomDialog dlg2;
    dlg2.setOperationMode(KFileWidget::Saving);
    lab = new QLabel(QStringLiteral("Second"));
    dlg2.setCustomWidget(lab);
    dlg2.exec();

    //Open dialog box
    KFileCustomDialog dlg3;
    dlg3.setOperationMode(KFileWidget::Opening);
    lab = new QLabel(QStringLiteral("Third"));
    dlg3.setCustomWidget(lab);
    dlg3.exec();

    return 0;
}
