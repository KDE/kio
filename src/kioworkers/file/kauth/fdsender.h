/*
    SPDX-FileCopyrightText: 2017 Chinmoy Ranjan Pradhan <chinmoyrp65@gmail.com>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/

#ifndef FDSENDER_H
#define FDSENDER_H

// std
#include <string>

class FdSender
{
public:
    explicit FdSender(const std::string &path);
    ~FdSender();

    FdSender(const FdSender &) = delete;
    FdSender &operator=(const FdSender &) = delete;

    bool sendFileDescriptor(int fd);
    bool isConnected() const;

private:
    int m_socketDes;
};

#endif
