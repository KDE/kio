/* 
   Copyright (c) 2003 Malte Starostik <malte@kde.org>

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public
   License as published by the Free Software Foundation; either
   version 2 of the License, or (at your option) any later version.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public License
   along with this library; see the file COPYING.LIB.  If not, write to
   the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
   Boston, MA 02110-1301, USA.
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
        Discovery( QObject* );

    protected Q_SLOTS:
        virtual void failed();

    private Q_SLOTS:
        void helperOutput();

    private:
        bool initDomainName();
        bool checkDomain() const;

        QProcess* m_helper;
        QString m_domainName;
    };
}

#endif // KPAC_DISCOVERY_H

// vim: ts=4 sw=4 et
