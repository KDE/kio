/*
    This file is part of the KDE libraries
    SPDX-FileCopyrightText: 2015 David Faure <faure@kde.org>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#include <KFileWidget>
#include <QApplication>
#include <QPushButton>
#include <QUrl>

int main(int argc, char **argv)
{
    QApplication app(argc, argv);

    KFileWidget *fileWidget = new KFileWidget(QUrl(QStringLiteral("kfiledialog:///OpenDialog")), nullptr);
    fileWidget->setMode(KFile::Files | KFile::ExistingOnly);
    fileWidget->setAttribute(Qt::WA_DeleteOnClose);
    fileWidget->show();

    app.connect(fileWidget, &KFileWidget::accepted, fileWidget, [&app, fileWidget]() {
        qDebug() << "accepted";
        fileWidget->accept();
        qDebug() << "Selected File:" << fileWidget->selectedFile();
        qDebug() << "Selected Url:" << fileWidget->selectedUrl();
        qDebug() << "Selected Files:" << fileWidget->selectedFiles();
        qDebug() << "Selected Urls:" << fileWidget->selectedUrls();
        app.exit();
    });

    QObject::connect(fileWidget, &KFileWidget::destroyed, &app, &QApplication::quit);

    fileWidget->okButton()->show();
    QObject::connect(fileWidget->okButton(), &QPushButton::clicked, fileWidget, &KFileWidget::slotOk);

    fileWidget->cancelButton()->show();
    QObject::connect(fileWidget->cancelButton(), &QPushButton::clicked, &app, &QApplication::quit);

    return app.exec();
}
