/*
    This file is part of the KDE libraries
    SPDX-FileCopyrightText: 2015 David Faure <faure@kde.org>

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

    QUrl folder = QUrl(QStringLiteral("kfiledialog:///OpenDialog"));
    if (!posargs.isEmpty()) {
        folder = QUrl::fromUserInput(posargs.at(0));
    }
    KFileWidget *fileWidget = new KFileWidget(folder);
    fileWidget->setOperationMode(KFileWidget::Opening);
    fileWidget->setAttribute(Qt::WA_DeleteOnClose);

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

    fileWidget->show();
    return app.exec();
}
