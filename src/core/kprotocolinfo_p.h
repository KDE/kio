/*
    This file is part of the KDE libraries
    SPDX-FileCopyrightText: 1999 Torben Weis <weis@kde.org>
    SPDX-FileCopyrightText: 2000-2001 Waldo Bastian <bastian@kde.org>
    SPDX-FileCopyrightText: 2012 David Faure <faure@kde.org>

    SPDX-License-Identifier: LGPL-2.0-only
*/

#ifndef KPROTOCOLINFOPRIVATE_H
#define KPROTOCOLINFOPRIVATE_H

#include "kprotocolinfo.h"

#include <QJsonObject>

/**
 * @internal
 */
class KProtocolInfoPrivate
{
public:
    explicit KProtocolInfoPrivate(const QString &path);
    KProtocolInfoPrivate(const QString &name, const QString &exec, const QJsonObject &json);

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
    bool m_supportsTruncating : 1;
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
    QStringList m_slaveHandlesNotify;
    QString m_proxyProtocol;
    int m_maxSlavesPerHost;
};

#endif
