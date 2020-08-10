/*
    This file is part of the KDE project.
    SPDX-FileCopyrightText: 2001 Carsten Pfeiffer <pfeiffer@kde.org>

    SPDX-License-Identifier: LGPL-2.0-only
*/

#include <QApplication>
#include <kurlrequester.h>
#include <kurlrequesterdialog.h>
#include <QDebug>

int main(int argc, char **argv)
{
    QApplication app(argc, argv);
    app.setQuitOnLastWindowClosed(false);

    QUrl url = KUrlRequesterDialog::getUrl(QUrl(QStringLiteral("ftp://ftp.kde.org")));
    qDebug() << "Selected url:" << url;

    KUrlRequester *req = new KUrlRequester();
    KEditListWidget *el = new KEditListWidget(req->customEditor());
    el->setWindowTitle(QStringLiteral("Test"));
    el->show();

    KUrlRequester *req1 = new KUrlRequester();
    req1->setWindowTitle(QStringLiteral("AAAAAAAAAAAA"));
    req1->show();

    KUrlComboRequester *comboReq = new KUrlComboRequester();
    comboReq->setWindowTitle(QStringLiteral("KUrlComboRequester"));
    comboReq->show();

    auto *mimeFilterReq = new KUrlRequester();
    mimeFilterReq->setMimeTypeFilters({QStringLiteral("text/x-c++src")});
    mimeFilterReq->setWindowTitle(QStringLiteral("MimeFilter"));
    mimeFilterReq->show();

    return app.exec();
}
