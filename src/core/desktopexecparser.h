/*
    SPDX-FileCopyrightText: 2013 David Faure <faure@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/

#ifndef KIO_DESKTOPEXECPARSER_H
#define KIO_DESKTOPEXECPARSER_H

#include "kiocore_export.h"

#include <QList>
#include <QScopedPointer>
class QUrl;
class QStringList;
class KService;

namespace KIO
{

class DesktopExecParserPrivate;

/**
 * @class KIO::DesktopExecParser desktopexecparser.h <KIO/DesktopExecParser>
 *
 * Parses the Exec= line from a .desktop file,
 * and process all the '\%' placeholders, e.g. handling URLs vs local files.
 *
 * The processing actually happens when calling resultingArguments(), after
 * setting everything up.
 *
 * @since 5.0
 */
class KIOCORE_EXPORT DesktopExecParser
{
public:
    /**
     * Creates a parser for a desktop file Exec line.
     *
     * @param service the service to extract information from.
     * The KService instance must remain alive as long as the parser is alive.
     * @param urls The urls the service should open.
     */
    DesktopExecParser(const KService &service, const QList<QUrl> &urls);

    /**
     * Destructor
     */
    ~DesktopExecParser();

    /**
     * If @p tempFiles is set to true and the urls given to the constructor are local files,
     * they will be deleted when the application exits.
     */
    void setUrlsAreTempFiles(bool tempFiles);

    /**
     * Sets the file name to use in the case of downloading the file to a tempfile
     * in order to give to a non-url-aware application. Some apps rely on the extension
     * to determine the MIME type of the file. Usually the file name comes from the URL,
     * but in the case of the HTTP Content-Disposition header, we need to override the
     * file name.
     */
    void setSuggestedFileName(const QString &suggestedFileName);

    /**
     * @return a list of arguments suitable for QProcess.
     * Returns an empty list on error, check errorMessage() for details.
     */
    QStringList resultingArguments() const;

    /**
     * @return an error message for when resultingArguments() returns an empty list
     * @since 5.71
     */
    QString errorMessage() const;

    /**
     * Returns the list of protocols which the application supports.
     * This can be a list of actual protocol names, or just "KIO" for KIO-based apps.
     */
    static QStringList supportedProtocols(const KService &service);

    /**
     * Returns true if @p protocol is in the list of protocols returned by supportedProtocols().
     * The only reason for this method is the special handling of "KIO".
     */
    static bool isProtocolInSupportedList(const QUrl &url, const QStringList &supportedProtocols);

    /**
     * Returns true if @p protocol should be opened by a "handler" application, i.e. an application
     * associated to _all_ URLs using this protocol (a.k.a. scheme).
     */
    static bool hasSchemeHandler(const QUrl &url);

    /**
     * Given a full command line (e.g. the Exec= line from a .desktop file),
     * extract the name of the executable being run (removing the path, if specified).
     * @param execLine the full command line
     * @return the name of the executable to run, example: "ls"
     */
    static QString executableName(const QString &execLine);

    /**
     * Given a full command line (e.g. the Exec= line from a .desktop file),
     * extract the name of the executable being run, including its full path, if specified.
     * @param execLine the full command line
     * @return the name of the executable to run, example: "/bin/ls"
     */
    static QString executablePath(const QString &execLine);

private:
    QScopedPointer<DesktopExecParserPrivate> d;
};

} // namespace KIO

#endif
