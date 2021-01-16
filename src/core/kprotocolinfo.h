/*
    This file is part of the KDE libraries
    SPDX-FileCopyrightText: 1999 Torben Weis <weis@kde.org>
    SPDX-FileCopyrightText: 2000-2001 Waldo Bastian <bastian@kde.org>
    SPDX-FileCopyrightText: 2012 David Faure <faure@kde.org>

    SPDX-License-Identifier: LGPL-2.0-only
*/

#ifndef KPROTOCOLINFO_H
#define KPROTOCOLINFO_H

#include "kiocore_export.h"
#include <QVariant>
#include <QStringList>

/**
 * \class KProtocolInfo kprotocolinfo.h <KProtocolInfo>
 *
 * Information about I/O (Internet, etc.) protocols supported by KDE.

 * KProtocolInfo is useful if you want to know which protocols
 * KDE supports. In addition you can find out lots of information
 * about a certain protocol. All of the functionality is provided by the static
 * methods.
 * The implementation scans the *.protocol files of all installed kioslaves to get
 * this information and stores the result into an internal cache.
 *
 * *.protocol files are installed in the "services" resource.
 *
 * The KProtocolInfo methods are reentrant (i.e. can be called from multiple threads simultaneously).
 */
class KIOCORE_EXPORT KProtocolInfo
{
public:
    //
    // Static functions:
    //

    /**
     * Returns list of all known protocols.
     * @return a list of all known protocols
     */
    static QStringList protocols();

    /**
     * Returns whether a protocol is installed that is able to handle @p url.
     *
     * @param url the url to check
     * @return true if the protocol is known
     * @see name()
     */
    static bool isKnownProtocol(const QUrl &url);

    /**
     * Same as above except you can supply just the protocol instead of
     * the whole URL.
     */
    static bool isKnownProtocol(const QString &protocol);

    /**
     * Returns the library / executable to open for the protocol @p protocol
     * Example : "kio_ftp", meaning either the executable "kio_ftp" or
     * the library "kio_ftp.la" (recommended), whichever is available.
     *
     * This corresponds to the "exec=" field in the protocol description file.
     * @param protocol the protocol to check
     * @return the executable of library to open, or QString() for
     *         unsupported protocols
     * @see KUrl::protocol()
     */
    static QString exec(const QString &protocol);

    /**
     * Describes the type of a protocol.
     * For instance ftp:// appears as a filesystem with folders and files,
     * while bzip2:// appears as a single file (a stream of data),
     * and telnet:// doesn't output anything.
     * @see outputType
     */
    enum Type { T_STREAM, ///< stream of data (e.g. single file)
                T_FILESYSTEM, ///< structured directory
                T_NONE,   ///< no information about the type available
                T_ERROR,   ///< used to signal an error
              };

    /**
     * Definition of an extra field in the UDS entries, returned by a listDir operation.
     *
     * The name is the name of the column, translated.
     *
     * The type name comes from QVariant::typeName()
     * Currently supported types: "QString", "QDateTime" (ISO-8601 format)
     */
    struct ExtraField {

        enum Type { String = QVariant::String, DateTime = QVariant::DateTime, Invalid = QVariant::Invalid };

        ExtraField() : type(Invalid) {}
        ExtraField(const QString &_name, Type _type)
            : name(_name), type(_type)
        {
        }
        QString name;
        Type type;
    };
    typedef QList<ExtraField> ExtraFieldList;
    /**
     * Definition of extra fields in the UDS entries, returned by a listDir operation.
     *
     * This corresponds to the "ExtraNames=" and "ExtraTypes=" fields in the protocol description file.
     * Those two lists should be separated with ',' in the protocol description file.
     * See ExtraField for details about names and types
     */
    static ExtraFieldList extraFields(const QUrl &url);

    /**
     * Returns whether the protocol can act as a helper protocol.
     * A helper protocol invokes an external application and does not return
     * a file or stream.
     *
     * This corresponds to the "helper=" field in the protocol description file.
     * Valid values for this field are "true" or "false" (default).
     *
     * @param url the url to check
     * @return true if the protocol is a helper protocol (e.g. vnc), false
     *              if not (e.g. http)
     */
    static bool isHelperProtocol(const QUrl &url);

    /**
     * Same as above except you can supply just the protocol instead of
     * the whole URL.
     */
    static bool isHelperProtocol(const QString &protocol);

    /**
     * Returns whether the protocol can act as a filter protocol.
     *
     * A filter protocol can operate on data that is passed to it
     * but does not retrieve/store data itself, like gzip.
     * A filter protocol is the opposite of a source protocol.
     *
     * The "source=" field in the protocol description file determines
     * whether a protocol is a source protocol or a filter protocol.
     * Valid values for this field are "true" (default) for source protocol or
     * "false" for filter protocol.
     *
     * @param url the url to check
     * @return true if the protocol is a filter (e.g. gzip), false if the
     *         protocol is a helper or source
     */
    static bool isFilterProtocol(const QUrl &url);

    /**
     * Same as above except you can supply just the protocol instead of
     * the whole URL.
     */
    static bool isFilterProtocol(const QString &protocol);

    /**
     * Returns the name of the icon, associated with the specified protocol.
     *
     * This corresponds to the "Icon=" field in the protocol description file.
     *
     * @param protocol the protocol to check
     * @return the icon of the protocol, or an empty string if unknown
     */
    static QString icon(const QString &protocol);

    /**
     * Returns the name of the config file associated with the
     * specified protocol. This is useful if two similar protocols
     * need to share a single config file, e.g. http and https.
     *
     * This corresponds to the "config=" field in the protocol description file.
     * The default is the protocol name, see name()
     *
     * @param protocol the protocol to check
     * @return the config file, or an empty string if unknown
     */
    static QString config(const QString &protocol);

    /**
     * Returns the soft limit on the number of slaves for this protocol.
     * This limits the number of slaves used for a single operation, note
     * that multiple operations may result in a number of instances that
     * exceeds this soft limit.
     *
     * This corresponds to the "maxInstances=" field in the protocol description file.
     * The default is 1.
     *
     * @param protocol the protocol to check
     * @return the maximum number of slaves, or 1 if unknown
     */
    static int maxSlaves(const QString &protocol);

    /**
     * Returns the limit on the number of slaves for this protocol per host.
     *
     * This corresponds to the "maxInstancesPerHost=" field in the protocol description file.
     * The default is 0 which means there is no per host limit.
     *
     * @param protocol the protocol to check
     * @return the maximum number of slaves, or 1 if unknown
     *
     * @since 4.4
     */
    static int maxSlavesPerHost(const QString &protocol);

    /**
     * Returns whether MIME types can be determined based on extension for this
     * protocol. For some protocols, e.g. http, the filename extension in the URL
     * can not be trusted to truly reflect the file type.
     *
     * This corresponds to the "determineMimetypeFromExtension=" field in the protocol description file.
     * Valid values for this field are "true" (default) or "false".
     *
     * @param protocol the protocol to check
     * @return true if the MIME types can be determined by extension
     */
    static bool determineMimetypeFromExtension(const QString &protocol);

    /**
     * Returns the default MIME type for the specified protocol, if one exists.
     *
     * This corresponds to the "defaultMimetype=" field in the protocol description file.
     *
     * @param protocol the protocol to check
     * @return the default MIME type of the protocol, or an empty string if none set or protocol unknown
     * @since 5.60
     */
    static QString defaultMimetype(const QString &protocol);

    /**
     * Returns the documentation path for the specified protocol.
     *
     * This corresponds to the "X-DocPath=" or "DocPath=" field in the protocol description file.
     *
     * @param protocol the protocol to check
     * @return the docpath of the protocol, or an empty string if unknown
     */
    static QString docPath(const QString &protocol);

    /**
     * Returns the protocol class for the specified protocol.
     *
     * This corresponds to the "Class=" field in the protocol description file.
     *
     * The following classes are defined:
     * @li ":internet" for common internet protocols
     * @li ":local" for protocols that access local resources
     *
     * Protocol classes always start with a ':' so that they can not be confused with
     * the protocols themselves.
     *
     * @param protocol the protocol to check
     * @return the class of the protocol, or an empty string if unknown
     */
    static QString protocolClass(const QString &protocol);

    /**
     * Returns whether file previews should be shown for the specified protocol.
     *
     * This corresponds to the "ShowPreviews=" field in the protocol description file.
     *
     * By default previews are shown if protocolClass is :local.
     *
     * @param protocol the protocol to check
     * @return true if previews should be shown by default, false otherwise
     */
    static bool showFilePreview(const QString &protocol);

    /**
     * Returns the list of capabilities provided by the kioslave implementing
     * this protocol.
     *
     * This corresponds to the "Capabilities=" field in the protocol description file.
     *
     * The capability names are not defined globally, they are up to each
     * slave implementation. For example when adding support for a new
     * special command for mounting, one would add the string "Mount" to the
     * capabilities list, and applications could check for that string
     * before sending a special() command that would otherwise do nothing
     * on older kioslave implementations.
     *
     * @param protocol the protocol to check
     * @return the list of capabilities.
     */
    static QStringList capabilities(const QString &protocol);

    /**
     * Returns the list of archive MIME types handled by the kioslave implementing
     * this protocol.
     *
     * This corresponds to the "archiveMimetype=" field in the protocol description file.
     *
     * @param protocol the protocol to check
     * @return the list of archive MIME types (e.g. application/x-zip) handled.
     * @since 5.23
     */
    static QStringList archiveMimetypes(const QString &protocol);

    /**
     * Returns the list of notification types the kioslave implementing this
     * protocol will produce on its own, making it unnecessary for job
     * implementations to do so. An example would be returning "Rename"
     * if the kioslave's rename() method takes care of calling
     * KDirNotify::emitFileRenameWithLocalPath on its own.
     *
     * This corresponds to "slaveHandlesNotify=" in the protocol description file.
     *
     * @since 5.20
     */
    static QStringList slaveHandlesNotify(const QString &protocol);

    /**
     * Returns the name of the protocol through which the request
     * will be routed if proxy support is enabled.
     *
     * A good example of this is the ftp protocol for which proxy
     * support is commonly handled by the http protocol.
     *
     * This corresponds to the "ProxiedBy=" in the protocol description file.
     */
    static QString proxiedBy(const QString &protocol);

    typedef enum { Name, FromUrl, DisplayName } FileNameUsedForCopying;

private:
    Q_DISABLE_COPY(KProtocolInfo)
};

#endif
