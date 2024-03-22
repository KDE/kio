/*

    SPDX-FileCopyrightText: 2023 Nicolas Fella <nicolas.fella@gmx.de>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#include <KIO/TransferJob>
#include <QApplication>
#include <QUrl>

int main(int argc, char **argv)
{
    QApplication app(argc, argv);

    KIO::get(QUrl("https://expired.badssl.com/"));

    return app.exec();
}
