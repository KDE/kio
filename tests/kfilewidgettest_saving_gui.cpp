/*
    This file is part of the KDE libraries
    SPDX-FileCopyrightText: 2019 Méven Car <meven.car@kdemail.net>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#include <QApplication>
#include <KFileWidget>
#include <QUrl>
#include <QDebug>
#include <QPushButton>

int main(int argc, char **argv)
{
    QApplication app(argc, argv);

    KFileWidget* fileWidget = new KFileWidget(QUrl(QStringLiteral("kfiledialog:///SaveDialog")), nullptr);
    fileWidget->setOperationMode(KFileWidget::Saving);
    fileWidget->setMode(KFile::File);
    fileWidget->setAttribute(Qt::WA_DeleteOnClose);

    fileWidget->okButton()->show();
    fileWidget->cancelButton()->show();
    app.connect(fileWidget->okButton(), &QPushButton::clicked, fileWidget, &KFileWidget::slotOk);
    app.connect(fileWidget->cancelButton(), &QPushButton::clicked, fileWidget, [&app, fileWidget]() {
        qDebug() << "canceled";
        fileWidget->slotCancel();
        app.exit();
    });

    app.connect(fileWidget, &KFileWidget::accepted, fileWidget, [&app, fileWidget]() {
        qDebug() << "accepted";
        fileWidget->accept();
        qDebug() << fileWidget->selectedFile();
        qDebug() << fileWidget->selectedUrl();
        qDebug() << fileWidget->selectedFiles();
        qDebug() << fileWidget->selectedUrls();
        app.exit();
    });

    fileWidget->show();

    return app.exec();
}

