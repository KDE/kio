/*
    SPDX-FileCopyrightText: %{CURRENT_YEAR} %{AUTHOR} <%{EMAIL}>

    SPDX-License-Identifier: LGPL-2.1-or-later
*/

#ifndef %{APPNAMEUC}_H
#define %{APPNAMEUC}_H

// KF
#include <KIO/SlaveBase>
// Std
#include <memory>

class %{APPNAME} : public KIO::SlaveBase
{
public:
    %{APPNAME}(const QByteArray &pool_socket, const QByteArray &app_socket);
    ~%{APPNAME}() override;

public: // KIO::SlaveBase API
    void get(const QUrl &url) override;
    void stat(const QUrl &url) override;
    void listDir(const QUrl &url) override;

private: // sample data structure
    std::unique_ptr<class MyDataSystem> m_dataSystem;
};

#endif
