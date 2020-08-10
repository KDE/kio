/*
    SPDX-FileCopyrightText: 2015 David Faure <faure@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include <KProtocolInfo>
#include <QCoreApplication>
#include <QDebug>

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);

    qDebug() << KProtocolInfo::protocols();

    return 0;
}
