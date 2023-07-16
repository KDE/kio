/*

    SPDX-FileCopyrightText: 2023 Nicolas Fella <nicolas.fella@gmx.de>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#include <QApplication>
#include <QNetworkAccessManager>
#include <QNetworkReply>

#include <KIO/SslUi>

#include <KIO/TransferJob>

int main(int argc, char **argv)
{
    QNetworkAccessManager nam;

    QApplication app(argc, argv);

    KIO::get(QUrl("https://expired.badssl.com/"));

    return app.exec();
}
