/*
    SPDX-FileCopyrightText: 2003 Malte Starostik <malte@kde.org>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#ifndef KPAC_DISCOVERY_H
#define KPAC_DISCOVERY_H

#include "downloader.h"

class QProcess;

namespace KPAC
{
class Discovery : public Downloader
{
    Q_OBJECT
public:
    explicit Discovery(QObject *);

protected Q_SLOTS:
    void failed() override;

private Q_SLOTS:
    void helperOutput();

private:
    bool initDomainName();
    bool checkDomain() const;

    QProcess *m_helper;
    QString m_domainName;
};
}

#endif // KPAC_DISCOVERY_H

