/*
    This file is part of the KDE libraries
    SPDX-FileCopyrightText: 2013 David Faure <faure@kde.org>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#include <QApplication>
#include <kencodingfiledialog.h>
#include <QUrl>
#include <QDebug>

int main(int argc, char **argv)
{
    QApplication app(argc, argv);

    KEncodingFileDialog::Result result = KEncodingFileDialog::getOpenUrlsAndEncoding(QString(), QUrl(QStringLiteral("file:///etc/passwd")));
    qDebug() << result.fileNames << result.URLs << result.encoding;

    return 0;
}

