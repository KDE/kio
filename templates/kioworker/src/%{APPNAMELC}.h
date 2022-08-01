/*
    SPDX-FileCopyrightText: %{CURRENT_YEAR} %{AUTHOR} <%{EMAIL}>

    SPDX-License-Identifier: LGPL-2.1-or-later
*/

#ifndef %{APPNAMEUC}_H
#define %{APPNAMEUC}_H

// KF
#include <KIO/WorkerBase>
// Std
#include <memory>

class %{APPNAME} : public KIO::WorkerBase
{
public:
    %{APPNAME}(const QByteArray &pool_socket, const QByteArray &app_socket);
    ~%{APPNAME}() override;

public: // KIO::WorkerBase API
    KIO::WorkerResult get(const QUrl &url) override;
    KIO::WorkerResult stat(const QUrl &url) override;
    KIO::WorkerResult listDir(const QUrl &url) override;

private: // sample data structure
    std::unique_ptr<class MyDataSystem> m_dataSystem;
};

#endif
