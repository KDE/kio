/*
    SPDX-FileCopyrightText: 2017 Chinmoy Ranjan Pradhan <chinmoyrp65@gmail.com>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/

#ifndef FDRECEIVER_H
#define FDRECEIVER_H

#include <QObject>

class QSocketNotifier;
class FdReceiver : public QObject
{
    Q_OBJECT

public:
    explicit FdReceiver(const std::string &path, QObject *parent = nullptr);
    ~FdReceiver();

    bool isListening() const;
    int fileDescriptor() const;

private:
    Q_SLOT void receiveFileDescriptor();

    QSocketNotifier *m_readNotifier;
    std::string m_path;
    int m_socketDes;
    int m_fileDes;
};

#endif
