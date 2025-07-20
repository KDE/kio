/*
    This file is part of the KDE libraries
    SPDX-FileCopyrightText: 2019 Méven Car <meven.car@kdemail.net>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#include <KFileWidget>
#include <QApplication>
#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QDebug>
#include <QPushButton>
#include <QUrl>

int main(int argc, char **argv)
{
    QApplication app(argc, argv);

    // Do some args
    QCommandLineParser parser;
    parser.addOption(QCommandLineOption(QStringLiteral("folder"), QStringLiteral("Select folder")));
    parser.addOption(QCommandLineOption(QStringLiteral("multiple"), QStringLiteral("Allows multiple files selection")));
    parser.addOption(QCommandLineOption(QStringLiteral("existing-only"), QStringLiteral("Filter to only existing files/directories")));
    parser.addPositionalArgument(QStringLiteral("folder"), QStringLiteral("The initial folder"));
    parser.process(app);
    QStringList posargs = parser.positionalArguments();

    QUrl folder = QUrl(QStringLiteral("kfiledialog:///SaveDialog"));
    if (!posargs.isEmpty()) {
        folder = QUrl::fromUserInput(posargs.at(0));
    }
    qDebug() << "Starting at" << folder;
    KFileWidget *fileWidget = new KFileWidget(folder);
    fileWidget->setOperationMode(KFileWidget::Saving);

    KFile::Modes mode = static_cast<KFile::Mode>(0);
    if (parser.isSet(QStringLiteral("existing-only"))) {
        mode |= KFile::ExistingOnly;
    }
    if (parser.isSet(QStringLiteral("folder"))) {
        mode |= KFile::Directory;
    } else if (parser.isSet(QStringLiteral("multiple"))) {
        mode |= KFile::Files;
    } else {
        mode |= KFile::File;
    }
    fileWidget->setMode(mode);

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
        qDebug() << "Selected File:" << fileWidget->selectedFile();
        qDebug() << "Selected Url:" << fileWidget->selectedUrl();
        qDebug() << "Selected Files:" << fileWidget->selectedFiles();
        qDebug() << "Selected Urls:" << fileWidget->selectedUrls();
        app.exit();
    });

    fileWidget->show();

    return app.exec();
}
