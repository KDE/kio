/*
    This file is part of the KDE libraries
    SPDX-FileCopyrightText: 2015 David Faure <faure@kde.org>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#include <QApplication>
#include <KFileWidget>
#include <QUrl>

int main(int argc, char **argv)
{
    QApplication app(argc, argv);

    KFileWidget* fileWidget = new KFileWidget(QUrl(QStringLiteral("kfiledialog:///OpenDialog")), nullptr);
    fileWidget->setMode(KFile::Files | KFile::ExistingOnly);
    fileWidget->setAttribute(Qt::WA_DeleteOnClose);
    fileWidget->show();

    QObject::connect(fileWidget, &KFileWidget::destroyed, &app, &QApplication::quit);

    return app.exec();
}

