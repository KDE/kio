/*
    This file is part of the KDE libraries
    SPDX-FileCopyrightText: 2015 David Edmundson <davidedmundson@kde.org>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#include <QApplication>
#include <KUrlNavigator>
#include <KFilePlacesModel>
#include <QUrl>
#include <QDir>

int main(int argc, char **argv)
{
    QApplication::setApplicationName(QStringLiteral("kurlnavigatortest"));
    QApplication::setAttribute(Qt::AA_UseHighDpiPixmaps, true);
    QApplication app(argc, argv);

    KUrlNavigator urlNavigator(new KFilePlacesModel, QUrl::fromLocalFile(QDir::homePath()), nullptr);
    urlNavigator.show();

    return app.exec();
}

