/* This file is part of the KDE libraries
   Copyright (C) 1999 Torben Weis <weis@kde.org>
   Copyright (C) 2000-2001 Waldo Bastian <bastian@kde.org>
   Copyright     2012 David Faure <faure@kde.org>

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public
   License version 2 as published by the Free Software Foundation.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public License
   along with this library; see the file COPYING.LIB.  If not, write to
   the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
   Boston, MA 02110-1301, USA.
*/
#ifndef KPROTOCOLINFOPRIVATE_H
#define KPROTOCOLINFOPRIVATE_H

#include "kprotocolinfo.h"

/**
 * @internal
 */
class KProtocolInfoPrivate
{
public:
    KProtocolInfoPrivate(const QString& path);

    QString m_name;
    QString m_exec;
    KProtocolInfo::Type m_inputType;
    KProtocolInfo::Type m_outputType;
    QStringList m_listing;
    bool m_isSourceProtocol : 1;
    bool m_isHelperProtocol : 1;
    bool m_supportsListing : 1;
    bool m_supportsReading : 1;
    bool m_supportsWriting : 1;
    bool m_supportsMakeDir : 1;
    bool m_supportsDeleting : 1;
    bool m_supportsLinking : 1;
    bool m_supportsMoving : 1;
    bool m_supportsOpening : 1;
    bool m_determineMimetypeFromExtension : 1;
    bool m_canCopyFromFile : 1;
    bool m_canCopyToFile : 1;
    bool m_showPreviews : 1;
    bool m_canRenameFromFile : 1;
    bool m_canRenameToFile : 1;
    bool m_canDeleteRecursive : 1;
    QString m_defaultMimetype;
    QString m_icon;
    QString m_config;
    int m_maxSlaves;


    QString m_docPath;
    QString m_protClass;
    QStringList m_archiveMimeTypes;
    KProtocolInfo::ExtraFieldList m_extraFields;
    KProtocolInfo::FileNameUsedForCopying m_fileNameUsedForCopying;
    QStringList m_capabilities;
    QString m_proxyProtocol;
    int m_maxSlavesPerHost;
};


#endif
