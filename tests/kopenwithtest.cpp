/*
    This file is part of the KDE libraries
    SPDX-FileCopyrightText: 2002 Dirk Mueller <mueller@kde.org>
    SPDX-FileCopyrightText: 2003 David Faure <faure@kde.org>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#include <QApplication>
#include <kopenwithdialog.h>
#include <QUrl>
#include <QDebug>

int main(int argc, char **argv)
{
    QApplication::setApplicationName(QStringLiteral("kopenwithdialogtest"));
    QApplication app(argc, argv);
    QList<QUrl> list;

    list += QUrl(QStringLiteral("file:///tmp/testfile.txt"));

    // Test with one URL
    KOpenWithDialog *dlg = new KOpenWithDialog(list, QString(), QString());
    if (dlg->exec()) {
        qDebug() << "Dialog ended successfully\ntext: " << dlg->text();
    } else {
        qDebug() << "Dialog was canceled.";
    }
    delete dlg;

    // Test with two URLs
    list += QUrl(QStringLiteral("http://www.kde.org/index.html"));
    dlg = new KOpenWithDialog(list, QString(), QString(), nullptr);
    if (dlg->exec()) {
        qDebug() << "Dialog ended successfully\ntext: " << dlg->text();
    } else {
        qDebug() << "Dialog was canceled.";
    }
    delete dlg;

    // Test with a MIME type
    QString mimetype = QStringLiteral("text/plain");
    dlg = new KOpenWithDialog(mimetype, QStringLiteral("kwrite"), nullptr);
    if (dlg->exec()) {
        qDebug() << "Dialog ended successfully\ntext: " << dlg->text();
    } else {
        qDebug() << "Dialog was canceled.";
    }
    delete dlg;

    return 0;
}

