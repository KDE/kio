/*
    This file is part of the KDE libraries
    SPDX-FileCopyrightText: 2000 David Faure <faure@kde.org>

    SPDX-License-Identifier: LGPL-2.0-only
*/

#include "job.h"
#include "kioglobal_p.h"
#include <kprotocolmanager.h>
#include <KLocalizedString>
#include <KStringHandler>

#include <QLocale>
#include <QUrl>
#include <QDateTime>
#include <QDataStream>
#include <sys/stat.h> // S_IRUSR etc

static const int s_maxFilePathLength = 80;

QString KIO::Job::errorString() const
{
    return KIO::buildErrorString(error(), errorText());
}

KIOCORE_EXPORT QString KIO::buildErrorString(int errorCode, const QString &errorText)
{
    QString result;

    switch (errorCode) {
    case  KIO::ERR_CANNOT_OPEN_FOR_READING:
        result = i18n("Could not read %1.",  errorText);
        break;
    case  KIO::ERR_CANNOT_OPEN_FOR_WRITING:
        result = i18n("Could not write to %1.",  KStringHandler::csqueeze(errorText, s_maxFilePathLength));
        break;
    case  KIO::ERR_CANNOT_LAUNCH_PROCESS:
        result = i18n("Could not start process %1.",  errorText);
        break;
    case  KIO::ERR_INTERNAL:
        result = i18n("Internal Error\nPlease send a full bug report at https://bugs.kde.org\n%1",  errorText);
        break;
    case  KIO::ERR_MALFORMED_URL:
        result = i18n("Malformed URL %1.",  errorText);
        break;
    case  KIO::ERR_UNSUPPORTED_PROTOCOL:
        result = i18n("The protocol %1 is not supported.",  errorText);
        break;
    case  KIO::ERR_NO_SOURCE_PROTOCOL:
        result = i18n("The protocol %1 is only a filter protocol.",  errorText);
        break;
    case  KIO::ERR_UNSUPPORTED_ACTION:
        result = errorText;
//       result = i18n( "Unsupported action %1" ).arg( errorText );
        break;
    case  KIO::ERR_IS_DIRECTORY:
        result = i18n("%1 is a folder, but a file was expected.",  errorText);
        break;
    case  KIO::ERR_IS_FILE:
        result = i18n("%1 is a file, but a folder was expected.",  errorText);
        break;
    case  KIO::ERR_DOES_NOT_EXIST:
        result = i18n("The file or folder %1 does not exist.",  errorText);
        break;
    case  KIO::ERR_FILE_ALREADY_EXIST:
        result = i18n("A file named %1 already exists.",  errorText);
        break;
    case  KIO::ERR_DIR_ALREADY_EXIST:
        result = i18n("A folder named %1 already exists.",  errorText);
        break;
    case  KIO::ERR_UNKNOWN_HOST:
        result = errorText.isEmpty() ? i18n("No hostname specified.") : i18n("Unknown host %1",  errorText);
        break;
    case  KIO::ERR_ACCESS_DENIED:
        result = i18n("Access denied to %1.",  errorText);
        break;
    case  KIO::ERR_WRITE_ACCESS_DENIED:
        result = i18n("Access denied.\nCould not write to %1.",  errorText);
        break;
    case  KIO::ERR_CANNOT_ENTER_DIRECTORY:
        result = i18n("Could not enter folder %1.",  errorText);
        break;
    case  KIO::ERR_PROTOCOL_IS_NOT_A_FILESYSTEM:
        result = i18n("The protocol %1 does not implement a folder service.",  errorText);
        break;
    case  KIO::ERR_CYCLIC_LINK:
        result = i18n("Found a cyclic link in %1.",  errorText);
        break;
    case  KIO::ERR_USER_CANCELED:
        // Do nothing in this case. The user doesn't need to be told what he just did.
        break;
    case  KIO::ERR_CYCLIC_COPY:
        result = i18n("Found a cyclic link while copying %1.",  errorText);
        break;
    case  KIO::ERR_CANNOT_CREATE_SOCKET:
        result = i18n("Could not create socket for accessing %1.",  errorText);
        break;
    case  KIO::ERR_CANNOT_CONNECT:
        result = i18n("Could not connect to host %1.",  errorText.isEmpty() ? QStringLiteral("localhost") : errorText);
        break;
    case  KIO::ERR_CONNECTION_BROKEN:
        result = i18n("Connection to host %1 is broken.",  errorText);
        break;
    case  KIO::ERR_NOT_FILTER_PROTOCOL:
        result = i18n("The protocol %1 is not a filter protocol.",  errorText);
        break;
    case  KIO::ERR_CANNOT_MOUNT:
        result = i18n("Could not mount device.\nThe reported error was:\n%1",  errorText);
        break;
    case  KIO::ERR_CANNOT_UNMOUNT:
        result = i18n("Could not unmount device.\nThe reported error was:\n%1",  errorText);
        break;
    case  KIO::ERR_CANNOT_READ:
        result = i18n("Could not read file %1.",  errorText);
        break;
    case  KIO::ERR_CANNOT_WRITE:
        result = i18n("Could not write to file %1.",  errorText);
        break;
    case  KIO::ERR_CANNOT_BIND:
        result = i18n("Could not bind %1.",  errorText);
        break;
    case  KIO::ERR_CANNOT_LISTEN:
        result = i18n("Could not listen %1.",  errorText);
        break;
    case  KIO::ERR_CANNOT_ACCEPT:
        result = i18n("Could not accept %1.",  errorText);
        break;
    case  KIO::ERR_CANNOT_LOGIN:
        result = errorText;
        break;
    case  KIO::ERR_CANNOT_STAT:
        result = i18n("Could not access %1.",  errorText);
        break;
    case  KIO::ERR_CANNOT_CLOSEDIR:
        result = i18n("Could not terminate listing %1.",  errorText);
        break;
    case  KIO::ERR_CANNOT_MKDIR:
        result = i18n("Could not make folder %1.",  KStringHandler::csqueeze(errorText, s_maxFilePathLength));
        break;
    case  KIO::ERR_CANNOT_RMDIR:
        result = i18n("Could not remove folder %1.",  errorText);
        break;
    case  KIO::ERR_CANNOT_RESUME:
        result = i18n("Could not resume file %1.",  errorText);
        break;
    case  KIO::ERR_CANNOT_RENAME:
        result = i18n("Could not rename file %1.",  KStringHandler::csqueeze(errorText, s_maxFilePathLength));
        break;
    case  KIO::ERR_CANNOT_CHMOD:
        result = i18n("Could not change permissions for %1.",  errorText);
        break;
    case  KIO::ERR_CANNOT_CHOWN:
        result = i18n("Could not change ownership for %1.",  errorText);
        break;
    case  KIO::ERR_CANNOT_DELETE:
        result = i18n("Could not delete file %1.",  errorText);
        break;
    case  KIO::ERR_SLAVE_DIED:
        result = i18n("The process for the %1 protocol died unexpectedly.",  errorText);
        break;
    case  KIO::ERR_OUT_OF_MEMORY:
        result = i18n("Error. Out of memory.\n%1",  errorText);
        break;
    case  KIO::ERR_UNKNOWN_PROXY_HOST:
        result = i18n("Unknown proxy host\n%1",  errorText);
        break;
    case  KIO::ERR_CANNOT_AUTHENTICATE:
        result = i18n("Authorization failed, %1 authentication not supported",  errorText);
        break;
    case  KIO::ERR_ABORTED:
        result = i18n("User canceled action\n%1",  errorText);
        break;
    case  KIO::ERR_INTERNAL_SERVER:
        result = i18n("Internal error in server\n%1",  errorText);
        break;
    case  KIO::ERR_SERVER_TIMEOUT:
        result = i18n("Timeout on server\n%1",  errorText);
        break;
    case  KIO::ERR_UNKNOWN:
        result = i18n("Unknown error\n%1",  errorText);
        break;
    case  KIO::ERR_UNKNOWN_INTERRUPT:
        result = i18n("Unknown interrupt\n%1",  errorText);
        break;
    /*
        case  KIO::ERR_CHECKSUM_MISMATCH:
          if (errorText)
            result = i18n( "Warning: MD5 Checksum for %1 does not match checksum returned from server" ).arg(errorText);
          else
            result = i18n( "Warning: MD5 Checksum for %1 does not match checksum returned from server" ).arg("document");
          break;
    */
    case KIO::ERR_CANNOT_DELETE_ORIGINAL:
        result = i18n("Could not delete original file %1.\nPlease check permissions.",  errorText);
        break;
    case KIO::ERR_CANNOT_DELETE_PARTIAL:
        result = i18n("Could not delete partial file %1.\nPlease check permissions.",  errorText);
        break;
    case KIO::ERR_CANNOT_RENAME_ORIGINAL:
        result = i18n("Could not rename original file %1.\nPlease check permissions.",  errorText);
        break;
    case KIO::ERR_CANNOT_RENAME_PARTIAL:
        result = i18n("Could not rename partial file %1.\nPlease check permissions.",  errorText);
        break;
    case KIO::ERR_CANNOT_SYMLINK:
        result = i18n("Could not create symlink %1.\nPlease check permissions.",  errorText);
        break;
    case KIO::ERR_NO_CONTENT:
        result = errorText;
        break;
    case KIO::ERR_DISK_FULL:
        result = i18n("There is not enough space on the disk to write %1.",  errorText);
        break;
    case KIO::ERR_IDENTICAL_FILES:
        result = i18n("The source and destination are the same file.\n%1",  errorText);
        break;
    case KIO::ERR_SLAVE_DEFINED:
        result = errorText;
        break;
    case KIO::ERR_UPGRADE_REQUIRED:
        result = i18n("%1 is required by the server, but is not available.", errorText);
        break;
    case KIO::ERR_POST_DENIED:
        result = i18n("Access to restricted port in POST denied.");
        break;
    case KIO::ERR_POST_NO_SIZE:
        result = i18n("The required content size information was not provided for a POST operation.");
        break;
    case KIO::ERR_DROP_ON_ITSELF:
        result = i18n("A file or folder cannot be dropped onto itself");
        break;
    case KIO::ERR_CANNOT_MOVE_INTO_ITSELF:
        result = i18n("A folder cannot be moved into itself");
        break;
    case KIO::ERR_PASSWD_SERVER:
        result = i18n("Communication with the local password server failed");
        break;
    case KIO::ERR_CANNOT_CREATE_SLAVE:
        result = i18n("Unable to create io-slave. %1", errorText);
        break;
    case KIO::ERR_FILE_TOO_LARGE_FOR_FAT32:
        result = xi18nc("@info", "Cannot transfer <filename>%1</filename> because it is too large. The destination filesystem only supports files up to 4GiB", errorText);
        break;
    case KIO::ERR_PRIVILEGE_NOT_REQUIRED:
        result = i18n("Privilege escalation is not necessary because \n'%1' is owned by the current user.\nPlease retry after changing permissions.", errorText);
        break;
    default:
        result = i18n("Unknown error code %1\n%2\nPlease send a full bug report at https://bugs.kde.org.",  errorCode,  errorText);
        break;
    }

    return result;
}

QStringList KIO::Job::detailedErrorStrings(const QUrl *reqUrl /*= 0*/,
        int method /*= -1*/) const
{
    QString errorName, techName, description, ret2;
    QStringList causes, solutions, ret;

    QByteArray raw = rawErrorDetail(error(), errorText(), reqUrl, method);
    QDataStream stream(raw);

    stream >> errorName >> techName >> description >> causes >> solutions;

    QString url, protocol, datetime;
    if (reqUrl) {
        QString prettyUrl;
        prettyUrl = reqUrl->toDisplayString();
        url = prettyUrl.toHtmlEscaped();
        protocol = reqUrl->scheme();
    } else {
        url = i18nc("@info url", "(unknown)");
    }

    datetime = QLocale().toString(QDateTime::currentDateTime() , QLocale::LongFormat);

    ret << errorName;
    ret << i18nc("@info %1 error name, %2 description",
                 "<qt><p><b>%1</b></p><p>%2</p></qt>", errorName, description);

    ret2 = QStringLiteral("<qt>");
    if (!techName.isEmpty())
        ret2 += QLatin1String("<p>") + i18n("<b>Technical reason</b>: ") +
                techName + QLatin1String("</p>");
    ret2 += QLatin1String("<p>") + i18n("<b>Details of the request</b>:") +
            QLatin1String("</p><ul>") + i18n("<li>URL: %1</li>", url);
    if (!protocol.isEmpty()) {
        ret2 += i18n("<li>Protocol: %1</li>", protocol);
    }
    ret2 += i18n("<li>Date and time: %1</li>", datetime) +
            i18n("<li>Additional information: %1</li>",  errorText()) +
            QLatin1String("</ul>");
    if (!causes.isEmpty()) {
        ret2 += QLatin1String("<p>") + i18n("<b>Possible causes</b>:") +
                QLatin1String("</p><ul><li>") + causes.join(QLatin1String("</li><li>")) +
                QLatin1String("</li></ul>");
    }
    if (!solutions.isEmpty()) {
        ret2 += QLatin1String("<p>") + i18n("<b>Possible solutions</b>:") +
                QLatin1String("</p><ul><li>") + solutions.join(QLatin1String("</li><li>")) +
                QLatin1String("</li></ul>");
    }
    ret2 += QLatin1String("</qt>");
    ret << ret2;

    return ret;
}

KIOCORE_EXPORT QByteArray KIO::rawErrorDetail(int errorCode, const QString &errorText,
        const QUrl *reqUrl /*= 0*/, int /*method = -1*/)
{
    QString url, host, protocol, datetime, domain, path, filename;
    bool isSlaveNetwork = false;
    if (reqUrl) {
        url = reqUrl->toDisplayString();
        host = reqUrl->host();
        protocol = reqUrl->scheme();

        if (host.startsWith(QLatin1String("www."))) {
            domain = host.mid(4);
        } else {
            domain = host;
        }

        filename = reqUrl->fileName();
        path = reqUrl->path();

        // detect if protocol is a network protocol...
        if (!protocol.isEmpty()) {
            isSlaveNetwork = KProtocolInfo::protocolClass(protocol) == QLatin1String(":internet");
        }
    } else {
        // assume that the errorText has the location we are interested in
        url = host = domain = path = filename = errorText;
    }

    if (protocol.isEmpty()) {
        protocol = i18nc("@info protocol", "(unknown)");
    }

    datetime = QLocale().toString(QDateTime::currentDateTime(), QLocale::LongFormat);

    QString errorName, techName, description;
    QStringList causes, solutions;

    // c == cause, s == solution
    QString sSysadmin = i18n("Contact your appropriate computer support system, "
                             "whether the system administrator, or technical support group for further "
                             "assistance.");
    QString sServeradmin = i18n("Contact the administrator of the server "
                                "for further assistance.");
    // FIXME active link to permissions dialog
    QString sAccess = i18n("Check your access permissions on this resource.");
    QString cAccess = i18n("Your access permissions may be inadequate to "
                           "perform the requested operation on this resource.");
    QString cLocked = i18n("The file may be in use (and thus locked) by "
                           "another user or application.");
    QString sQuerylock = i18n("Check to make sure that no other "
                              "application or user is using the file or has locked the file.");
    QString cHardware = i18n("Although unlikely, a hardware error may have "
                             "occurred.");
    QString cBug = i18n("You may have encountered a bug in the program.");
    QString cBuglikely = i18n("This is most likely to be caused by a bug in the "
                              "program. Please consider submitting a full bug report as detailed below.");
    QString sUpdate = i18n("Update your software to the latest version. "
                           "Your distribution should provide tools to update your software.");
    QString sBugreport = i18n("When all else fails, please consider helping the "
                              "KDE team or the third party maintainer of this software by submitting a "
                              "high quality bug report. If the software is provided by a third party, "
                              "please contact them directly. Otherwise, first look to see if "
                              "the same bug has been submitted by someone else by searching at the "
                              "<a href=\"https://bugs.kde.org/\">KDE bug reporting website</a>. If not, take "
                              "note of the details given above, and include them in your bug report, along "
                              "with as many other details as you think might help.");
    QString cNetwork = i18n("There may have been a problem with your network "
                            "connection.");
    // FIXME netconf kcontrol link
    QString cNetconf = i18n("There may have been a problem with your network "
                            "configuration. If you have been accessing the Internet with no problems "
                            "recently, this is unlikely.");
    QString cNetpath = i18n("There may have been a problem at some point along "
                            "the network path between the server and this computer.");
    QString sTryagain = i18n("Try again, either now or at a later time.");
    QString cProtocol = i18n("A protocol error or incompatibility may have occurred.");
    QString sExists = i18n("Ensure that the resource exists, and try again.");
    QString cExists = i18n("The specified resource may not exist.");
    QString sTypo = i18n("Double-check that you have entered the correct location "
                         "and try again.");
    QString sNetwork = i18n("Check your network connection status.");

    switch (errorCode) {
    case  KIO::ERR_CANNOT_OPEN_FOR_READING:
        errorName = i18n("Cannot Open Resource For Reading");
        description = i18n("This means that the contents of the requested file "
                           "or folder <strong>%1</strong> could not be retrieved, as read "
                           "access could not be obtained.", path);
        causes << i18n("You may not have permissions to read the file or open "
                       "the folder.") << cLocked << cHardware;
        solutions << sAccess << sQuerylock << sSysadmin;
        break;

    case  KIO::ERR_CANNOT_OPEN_FOR_WRITING:
        errorName = i18n("Cannot Open Resource For Writing");
        description = i18n("This means that the file, <strong>%1</strong>, could "
                           "not be written to as requested, because access with permission to "
                           "write could not be obtained.",  KStringHandler::csqueeze(filename, s_maxFilePathLength));
        causes << cAccess << cLocked << cHardware;
        solutions << sAccess << sQuerylock << sSysadmin;
        break;

    case  KIO::ERR_CANNOT_LAUNCH_PROCESS:
        errorName = i18n("Cannot Launch Process required by the %1 Protocol",  protocol);
        techName = i18n("Unable to Launch Process");
        description = i18n("The program on your computer which provides access "
                           "to the <strong>%1</strong> protocol could not be found or started. This is "
                           "usually due to technical reasons.",  protocol);
        causes << i18n("The program which provides compatibility with this "
                       "protocol may not have been updated with your last update of KDE. "
                       "This can cause the program to be incompatible with the current version "
                       "and thus not start.") << cBug;
        solutions << sUpdate << sSysadmin;
        break;

    case  KIO::ERR_INTERNAL:
        errorName = i18n("Internal Error");
        description = i18n("The program on your computer which provides access "
                           "to the <strong>%1</strong> protocol has reported an internal error.",
                           protocol);
        causes << cBuglikely;
        solutions << sUpdate << sBugreport;
        break;

    case  KIO::ERR_MALFORMED_URL:
        errorName = i18n("Improperly Formatted URL");
        description = i18n("The <strong>U</strong>niform <strong>R</strong>esource "
                           "<strong>L</strong>ocator (URL) that you entered was not properly "
                           "formatted. The format of a URL is generally as follows:"
                           "<blockquote><strong>protocol://user:password@www.example.org:port/folder/"
                           "filename.extension?query=value</strong></blockquote>");
        solutions << sTypo;
        break;

    case  KIO::ERR_UNSUPPORTED_PROTOCOL:
        errorName = i18n("Unsupported Protocol %1",  protocol);
        description = i18n("The protocol <strong>%1</strong> is not supported "
                           "by the KDE programs currently installed on this computer.",
                           protocol);
        causes << i18n("The requested protocol may not be supported.")
               << i18n("The versions of the %1 protocol supported by this computer and "
                       "the server may be incompatible.",  protocol);
        solutions << i18n("You may perform a search on the Internet for a KDE "
                          "program (called a kioslave or ioslave) which supports this protocol. "
                          "Places to search include <a href=\"https://kde-apps.org/\">"
                          "https://kde-apps.org/</a> and <a href=\"http://freshmeat.net/\">"
                          "http://freshmeat.net/</a>.")
                  << sUpdate << sSysadmin;
        break;

    case  KIO::ERR_NO_SOURCE_PROTOCOL:
        errorName = i18n("URL Does Not Refer to a Resource.");
        techName = i18n("Protocol is a Filter Protocol");
        description = i18n("The <strong>U</strong>niform <strong>R</strong>esource "
                           "<strong>L</strong>ocator (URL) that you entered did not refer to a "
                           "specific resource.");
        causes << i18n("KDE is able to communicate through a protocol within a "
                       "protocol; the protocol specified is only for use in such situations, "
                       "however this is not one of these situations. This is a rare event, and "
                       "is likely to indicate a programming error.");
        solutions << sTypo;
        break;

    case  KIO::ERR_UNSUPPORTED_ACTION:
        errorName = i18n("Unsupported Action: %1",  errorText);
        description = i18n("The requested action is not supported by the KDE "
                           "program which is implementing the <strong>%1</strong> protocol.",
                           protocol);
        causes << i18n("This error is very much dependent on the KDE program. The "
                       "additional information should give you more information than is available "
                       "to the KDE input/output architecture.");
        solutions << i18n("Attempt to find another way to accomplish the same "
                          "outcome.");
        break;

    case  KIO::ERR_IS_DIRECTORY:
        errorName = i18n("File Expected");
        description = i18n("The request expected a file, however the "
                           "folder <strong>%1</strong> was found instead.", path);
        causes << i18n("This may be an error on the server side.") << cBug;
        solutions << sUpdate << sSysadmin;
        break;

    case  KIO::ERR_IS_FILE:
        errorName = i18n("Folder Expected");
        description = i18n("The request expected a folder, however "
                           "the file <strong>%1</strong> was found instead.", filename);
        causes << cBug;
        solutions << sUpdate << sSysadmin;
        break;

    case  KIO::ERR_DOES_NOT_EXIST:
        errorName = i18n("File or Folder Does Not Exist");
        description = i18n("The specified file or folder <strong>%1</strong> "
                           "does not exist.", path);
        causes << cExists;
        solutions << sExists;
        break;

    case  KIO::ERR_FILE_ALREADY_EXIST:
        errorName = i18n("File Already Exists");
        description = i18n("The requested file could not be created because a "
                           "file with the same name already exists.");
        solutions << i18n("Try moving the current file out of the way first, "
                          "and then try again.")
                  << i18n("Delete the current file and try again.")
                  << i18n("Choose an alternate filename for the new file.");
        break;

    case  KIO::ERR_DIR_ALREADY_EXIST:
        errorName = i18n("Folder Already Exists");
        description = i18n("The requested folder could not be created because "
                           "a folder with the same name already exists.");
        solutions << i18n("Try moving the current folder out of the way first, "
                          "and then try again.")
                  << i18n("Delete the current folder and try again.")
                  << i18n("Choose an alternate name for the new folder.");
        break;

    case  KIO::ERR_UNKNOWN_HOST:
        errorName = i18n("Unknown Host");
        description = i18n("An unknown host error indicates that the server with "
                           "the requested name, <strong>%1</strong>, could not be "
                           "located on the Internet.",  host);
        causes << i18n("The name that you typed, %1, may not exist: it may be "
                       "incorrectly typed.",  host)
               << cNetwork << cNetconf;
        solutions << sNetwork << sSysadmin;
        break;

    case  KIO::ERR_ACCESS_DENIED:
        errorName = i18n("Access Denied");
        description = i18n("Access was denied to the specified resource, "
                           "<strong>%1</strong>.",  url);
        causes << i18n("You may have supplied incorrect authentication details or "
                       "none at all.")
               << i18n("Your account may not have permission to access the "
                       "specified resource.");
        solutions << i18n("Retry the request and ensure your authentication details "
                          "are entered correctly.") << sSysadmin;
        if (!isSlaveNetwork) {
            solutions << sServeradmin;
        }
        break;

    case  KIO::ERR_WRITE_ACCESS_DENIED:
        errorName = i18n("Write Access Denied");
        description = i18n("This means that an attempt to write to the file "
                           "<strong>%1</strong> was rejected.",  filename);
        causes << cAccess << cLocked << cHardware;
        solutions << sAccess << sQuerylock << sSysadmin;
        break;

    case  KIO::ERR_CANNOT_ENTER_DIRECTORY:
        errorName = i18n("Unable to Enter Folder");
        description = i18n("This means that an attempt to enter (in other words, "
                           "to open) the requested folder <strong>%1</strong> was rejected.",
                           path);
        causes << cAccess << cLocked;
        solutions << sAccess << sQuerylock << sSysadmin;
        break;

    case  KIO::ERR_PROTOCOL_IS_NOT_A_FILESYSTEM:
        errorName = i18n("Folder Listing Unavailable");
        techName = i18n("Protocol %1 is not a Filesystem",  protocol);
        description = i18n("This means that a request was made which requires "
                           "determining the contents of the folder, and the KDE program supporting "
                           "this protocol is unable to do so.");
        causes << cBug;
        solutions << sUpdate << sBugreport;
        break;

    case  KIO::ERR_CYCLIC_LINK:
        errorName = i18n("Cyclic Link Detected");
        description = i18n("UNIX environments are commonly able to link a file or "
                           "folder to a separate name and/or location. KDE detected a link or "
                           "series of links that results in an infinite loop - i.e. the file was "
                           "(perhaps in a roundabout way) linked to itself.");
        solutions << i18n("Delete one part of the loop in order that it does not "
                          "cause an infinite loop, and try again.") << sSysadmin;
        break;

    case  KIO::ERR_USER_CANCELED:
        // Do nothing in this case. The user doesn't need to be told what he just did.
        // rodda: However, if we have been called, an application is about to display
        // this information anyway. If we don't return sensible information, the
        // user sees a blank dialog (I have seen this myself)
        errorName = i18n("Request Aborted By User");
        description = i18n("The request was not completed because it was "
                           "aborted.");
        solutions << i18n("Retry the request.");
        break;

    case  KIO::ERR_CYCLIC_COPY:
        errorName = i18n("Cyclic Link Detected During Copy");
        description = i18n("UNIX environments are commonly able to link a file or "
                           "folder to a separate name and/or location. During the requested copy "
                           "operation, KDE detected a link or series of links that results in an "
                           "infinite loop - i.e. the file was (perhaps in a roundabout way) linked "
                           "to itself.");
        solutions << i18n("Delete one part of the loop in order that it does not "
                          "cause an infinite loop, and try again.") << sSysadmin;
        break;

    case  KIO::ERR_CANNOT_CREATE_SOCKET:
        errorName = i18n("Could Not Create Network Connection");
        techName = i18n("Could Not Create Socket");
        description = i18n("This is a fairly technical error in which a required "
                           "device for network communications (a socket) could not be created.");
        causes << i18n("The network connection may be incorrectly configured, or "
                       "the network interface may not be enabled.");
        solutions << sNetwork << sSysadmin;
        break;

    case  KIO::ERR_CANNOT_CONNECT:
        errorName = i18n("Connection to Server Refused");
        description = i18n("The server <strong>%1</strong> refused to allow this "
                           "computer to make a connection.",  host);
        causes << i18n("The server, while currently connected to the Internet, "
                       "may not be configured to allow requests.")
               << i18n("The server, while currently connected to the Internet, "
                       "may not be running the requested service (%1).",  protocol)
               << i18n("A network firewall (a device which restricts Internet "
                       "requests), either protecting your network or the network of the server, "
                       "may have intervened, preventing this request.");
        solutions << sTryagain << sServeradmin << sSysadmin;
        break;

    case  KIO::ERR_CONNECTION_BROKEN:
        errorName = i18n("Connection to Server Closed Unexpectedly");
        description = i18n("Although a connection was established to "
                           "<strong>%1</strong>, the connection was closed at an unexpected point "
                           "in the communication.",  host);
        causes << cNetwork << cNetpath << i18n("A protocol error may have occurred, "
                                               "causing the server to close the connection as a response to the error.");
        solutions << sTryagain << sServeradmin << sSysadmin;
        break;

    case  KIO::ERR_NOT_FILTER_PROTOCOL:
        errorName = i18n("URL Resource Invalid");
        techName = i18n("Protocol %1 is not a Filter Protocol",  protocol);
        description = i18n("The <strong>U</strong>niform <strong>R</strong>esource "
                           "<strong>L</strong>ocator (URL) that you entered did not refer to "
                           "a valid mechanism of accessing the specific resource, "
                           "<strong>%1%2</strong>.",
                           !host.isNull() ? host + QLatin1Char('/') : QString(), path);
        causes << i18n("KDE is able to communicate through a protocol within a "
                       "protocol. This request specified a protocol be used as such, however "
                       "this protocol is not capable of such an action. This is a rare event, "
                       "and is likely to indicate a programming error.");
        solutions << sTypo << sSysadmin;
        break;

    case  KIO::ERR_CANNOT_MOUNT:
        errorName = i18n("Unable to Initialize Input/Output Device");
        techName = i18n("Could Not Mount Device");
        description = i18n("The requested device could not be initialized "
                           "(\"mounted\"). The reported error was: <strong>%1</strong>",
                           errorText);
        causes << i18n("The device may not be ready, for example there may be "
                       "no media in a removable media device (i.e. no CD-ROM in a CD drive), "
                       "or in the case of a peripheral/portable device, the device may not "
                       "be correctly connected.")
               << i18n("You may not have permissions to initialize (\"mount\") the "
                       "device. On UNIX systems, often system administrator privileges are "
                       "required to initialize a device.")
               << cHardware;
        solutions << i18n("Check that the device is ready; removable drives "
                          "must contain media, and portable devices must be connected and powered "
                          "on.; and try again.") << sAccess << sSysadmin;
        break;

    case  KIO::ERR_CANNOT_UNMOUNT:
        errorName = i18n("Unable to Uninitialize Input/Output Device");
        techName = i18n("Could Not Unmount Device");
        description = i18n("The requested device could not be uninitialized "
                           "(\"unmounted\"). The reported error was: <strong>%1</strong>",
                           errorText);
        causes << i18n("The device may be busy, that is, still in use by "
                       "another application or user. Even such things as having an open "
                       "browser window on a location on this device may cause the device to "
                       "remain in use.")
               << i18n("You may not have permissions to uninitialize (\"unmount\") "
                       "the device. On UNIX systems, system administrator privileges are "
                       "often required to uninitialize a device.")
               << cHardware;
        solutions << i18n("Check that no applications are accessing the device, "
                          "and try again.") << sAccess << sSysadmin;
        break;

    case  KIO::ERR_CANNOT_READ:
        errorName = i18n("Cannot Read From Resource");
        description = i18n("This means that although the resource, "
                           "<strong>%1</strong>, was able to be opened, an error occurred while "
                           "reading the contents of the resource.",  url);
        causes << i18n("You may not have permissions to read from the resource.");
        if (!isSlaveNetwork) {
            causes << cNetwork;
        }
        causes << cHardware;
        solutions << sAccess;
        if (!isSlaveNetwork) {
            solutions << sNetwork;
        }
        solutions << sSysadmin;
        break;

    case  KIO::ERR_CANNOT_WRITE:
        errorName = i18n("Cannot Write to Resource");
        description = i18n("This means that although the resource, <strong>%1</strong>"
                           ", was able to be opened, an error occurred while writing to the resource.",
                           url);
        causes << i18n("You may not have permissions to write to the resource.");
        if (!isSlaveNetwork) {
            causes << cNetwork;
        }
        causes << cHardware;
        solutions << sAccess;
        if (!isSlaveNetwork) {
            solutions << sNetwork;
        }
        solutions << sSysadmin;
        break;

    case  KIO::ERR_CANNOT_BIND:
        errorName = i18n("Could Not Listen for Network Connections");
        techName = i18n("Could Not Bind");
        description = i18n("This is a fairly technical error in which a required "
                           "device for network communications (a socket) could not be established "
                           "to listen for incoming network connections.");
        causes << i18n("The network connection may be incorrectly configured, or "
                       "the network interface may not be enabled.");
        solutions << sNetwork << sSysadmin;
        break;

    case  KIO::ERR_CANNOT_LISTEN:
        errorName = i18n("Could Not Listen for Network Connections");
        techName = i18n("Could Not Listen");
        description = i18n("This is a fairly technical error in which a required "
                           "device for network communications (a socket) could not be established "
                           "to listen for incoming network connections.");
        causes << i18n("The network connection may be incorrectly configured, or "
                       "the network interface may not be enabled.");
        solutions << sNetwork << sSysadmin;
        break;

    case  KIO::ERR_CANNOT_ACCEPT:
        errorName = i18n("Could Not Accept Network Connection");
        description = i18n("This is a fairly technical error in which an error "
                           "occurred while attempting to accept an incoming network connection.");
        causes << i18n("The network connection may be incorrectly configured, or "
                       "the network interface may not be enabled.")
               << i18n("You may not have permissions to accept the connection.");
        solutions << sNetwork << sSysadmin;
        break;

    case  KIO::ERR_CANNOT_LOGIN:
        errorName = i18n("Could Not Login: %1",  errorText);
        description = i18n("An attempt to login to perform the requested "
                           "operation was unsuccessful.");
        causes << i18n("You may have supplied incorrect authentication details or "
                       "none at all.")
               << i18n("Your account may not have permission to access the "
                       "specified resource.") << cProtocol;
        solutions << i18n("Retry the request and ensure your authentication details "
                          "are entered correctly.") << sServeradmin << sSysadmin;
        break;

    case  KIO::ERR_CANNOT_STAT:
        errorName = i18n("Could Not Determine Resource Status");
        techName = i18n("Could Not Stat Resource");
        description = i18n("An attempt to determine information about the status "
                           "of the resource <strong>%1</strong>, such as the resource name, type, "
                           "size, etc., was unsuccessful.",  url);
        causes << i18n("The specified resource may not have existed or may "
                       "not be accessible.") << cProtocol << cHardware;
        solutions << i18n("Retry the request and ensure your authentication details "
                          "are entered correctly.") << sSysadmin;
        break;

    case  KIO::ERR_CANNOT_CLOSEDIR:
        //result = i18n( "Could not terminate listing %1" ).arg( errorText );
        errorName = i18n("Could Not Cancel Listing");
        techName = i18n("FIXME: Document this");
        break;

    case  KIO::ERR_CANNOT_MKDIR:
        errorName = i18n("Could Not Create Folder");
        description = i18n("An attempt to create the requested folder failed.");
        causes << cAccess << i18n("The location where the folder was to be created "
                                  "may not exist.");
        if (!isSlaveNetwork) {
            causes << cProtocol;
        }
        solutions << i18n("Retry the request.") << sAccess;
        break;

    case  KIO::ERR_CANNOT_RMDIR:
        errorName = i18n("Could Not Remove Folder");
        description = i18n("An attempt to remove the specified folder, "
                           "<strong>%1</strong>, failed.", path);
        causes << i18n("The specified folder may not exist.")
               << i18n("The specified folder may not be empty.")
               << cAccess;
        if (!isSlaveNetwork) {
            causes << cProtocol;
        }
        solutions << i18n("Ensure that the folder exists and is empty, and try "
                          "again.") << sAccess;
        break;

    case  KIO::ERR_CANNOT_RESUME:
        errorName = i18n("Could Not Resume File Transfer");
        description = i18n("The specified request asked that the transfer of "
                           "file <strong>%1</strong> be resumed at a certain point of the "
                           "transfer. This was not possible.",  filename);
        causes << i18n("The protocol, or the server, may not support file "
                       "resuming.");
        solutions << i18n("Retry the request without attempting to resume "
                          "transfer.");
        break;

    case  KIO::ERR_CANNOT_RENAME:
        errorName = i18n("Could Not Rename Resource");
        description = i18n("An attempt to rename the specified resource "
                           "<strong>%1</strong> failed.",  KStringHandler::csqueeze(url, s_maxFilePathLength));
        causes << cAccess << cExists;
        if (!isSlaveNetwork) {
            causes << cProtocol;
        }
        solutions << sAccess << sExists;
        break;

    case  KIO::ERR_CANNOT_CHMOD:
        errorName = i18n("Could Not Alter Permissions of Resource");
        description = i18n("An attempt to alter the permissions on the specified "
                           "resource <strong>%1</strong> failed.",  url);
        causes << cAccess << cExists;
        solutions << sAccess << sExists;
        break;

    case  KIO::ERR_CANNOT_CHOWN:
        errorName = i18n("Could Not Change Ownership of Resource");
        description = i18n("An attempt to change the ownership of the specified "
                           "resource <strong>%1</strong> failed.",  url);
        causes << cAccess << cExists;
        solutions << sAccess << sExists;
        break;

    case  KIO::ERR_CANNOT_DELETE:
        errorName = i18n("Could Not Delete Resource");
        description = i18n("An attempt to delete the specified resource "
                           "<strong>%1</strong> failed.",  url);
        causes << cAccess << cExists;
        solutions << sAccess << sExists;
        break;

    case  KIO::ERR_SLAVE_DIED:
        errorName = i18n("Unexpected Program Termination");
        description = i18n("The program on your computer which provides access "
                           "to the <strong>%1</strong> protocol has unexpectedly terminated.",
                           url);
        causes << cBuglikely;
        solutions << sUpdate << sBugreport;
        break;

    case  KIO::ERR_OUT_OF_MEMORY:
        errorName = i18n("Out of Memory");
        description = i18n("The program on your computer which provides access "
                           "to the <strong>%1</strong> protocol could not obtain the memory "
                           "required to continue.",  protocol);
        causes << cBuglikely;
        solutions << sUpdate << sBugreport;
        break;

    case  KIO::ERR_UNKNOWN_PROXY_HOST:
        errorName = i18n("Unknown Proxy Host");
        description = i18n("While retrieving information about the specified "
                           "proxy host, <strong>%1</strong>, an Unknown Host error was encountered. "
                           "An unknown host error indicates that the requested name could not be "
                           "located on the Internet.",  errorText);
        causes << i18n("There may have been a problem with your network "
                       "configuration, specifically your proxy's hostname. If you have been "
                       "accessing the Internet with no problems recently, this is unlikely.")
               << cNetwork;
        solutions << i18n("Double-check your proxy settings and try again.")
                  << sSysadmin;
        break;

    case  KIO::ERR_CANNOT_AUTHENTICATE:
        errorName = i18n("Authentication Failed: Method %1 Not Supported",
                         errorText);
        description = i18n("Although you may have supplied the correct "
                           "authentication details, the authentication failed because the "
                           "method that the server is using is not supported by the KDE "
                           "program implementing the protocol %1.",  protocol);
        solutions << i18n("Please file a bug at <a href=\"https://bugs.kde.org/\">"
                          "https://bugs.kde.org/</a> to inform the KDE team of the unsupported "
                          "authentication method.") << sSysadmin;
        break;

    case  KIO::ERR_ABORTED:
        errorName = i18n("Request Aborted");
        description = i18n("The request was not completed because it was "
                           "aborted.");
        solutions << i18n("Retry the request.");
        break;

    case  KIO::ERR_INTERNAL_SERVER:
        errorName = i18n("Internal Error in Server");
        description = i18n("The program on the server which provides access "
                           "to the <strong>%1</strong> protocol has reported an internal error: "
                           "%2.",  protocol, errorText);
        causes << i18n("This is most likely to be caused by a bug in the "
                       "server program. Please consider submitting a full bug report as "
                       "detailed below.");
        solutions << i18n("Contact the administrator of the server "
                          "to advise them of the problem.")
                  << i18n("If you know who the authors of the server software are, "
                          "submit the bug report directly to them.");
        break;

    case  KIO::ERR_SERVER_TIMEOUT:
        errorName = i18n("Timeout Error");
        description = i18n("Although contact was made with the server, a "
                           "response was not received within the amount of time allocated for "
                           "the request as follows:<ul>"
                           "<li>Timeout for establishing a connection: %1 seconds</li>"
                           "<li>Timeout for receiving a response: %2 seconds</li>"
                           "<li>Timeout for accessing proxy servers: %3 seconds</li></ul>"
                           "Please note that you can alter these timeout settings in the KDE "
                           "System Settings, by selecting Network Settings -> Connection Preferences.",
                           KProtocolManager::connectTimeout(),
                           KProtocolManager::responseTimeout(),
                           KProtocolManager::proxyConnectTimeout());
        causes << cNetpath << i18n("The server was too busy responding to other "
                                   "requests to respond.");
        solutions << sTryagain << sServeradmin;
        break;

    case  KIO::ERR_UNKNOWN:
        errorName = i18n("Unknown Error");
        description = i18n("The program on your computer which provides access "
                           "to the <strong>%1</strong> protocol has reported an unknown error: "
                           "%2.",  protocol,  errorText);
        causes << cBug;
        solutions << sUpdate << sBugreport;
        break;

    case  KIO::ERR_UNKNOWN_INTERRUPT:
        errorName = i18n("Unknown Interruption");
        description = i18n("The program on your computer which provides access "
                           "to the <strong>%1</strong> protocol has reported an interruption of "
                           "an unknown type: %2.",  protocol,  errorText);
        causes << cBug;
        solutions << sUpdate << sBugreport;
        break;

    case KIO::ERR_CANNOT_DELETE_ORIGINAL:
        errorName = i18n("Could Not Delete Original File");
        description = i18n("The requested operation required the deleting of "
                           "the original file, most likely at the end of a file move operation. "
                           "The original file <strong>%1</strong> could not be deleted.",
                           errorText);
        causes << cAccess;
        solutions << sAccess;
        break;

    case KIO::ERR_CANNOT_DELETE_PARTIAL:
        errorName = i18n("Could Not Delete Temporary File");
        description = i18n("The requested operation required the creation of "
                           "a temporary file in which to save the new file while being "
                           "downloaded. This temporary file <strong>%1</strong> could not be "
                           "deleted.",  errorText);
        causes << cAccess;
        solutions << sAccess;
        break;

    case KIO::ERR_CANNOT_RENAME_ORIGINAL:
        errorName = i18n("Could Not Rename Original File");
        description = i18n("The requested operation required the renaming of "
                           "the original file <strong>%1</strong>, however it could not be "
                           "renamed.",  errorText);
        causes << cAccess;
        solutions << sAccess;
        break;

    case KIO::ERR_CANNOT_RENAME_PARTIAL:
        errorName = i18n("Could Not Rename Temporary File");
        description = i18n("The requested operation required the creation of "
                           "a temporary file <strong>%1</strong>, however it could not be "
                           "created.",  errorText);
        causes << cAccess;
        solutions << sAccess;
        break;

    case KIO::ERR_CANNOT_SYMLINK:
        errorName = i18n("Could Not Create Link");
        techName = i18n("Could Not Create Symbolic Link");
        description = i18n("The requested symbolic link %1 could not be created.",
                           errorText);
        causes << cAccess;
        solutions << sAccess;
        break;

    case KIO::ERR_NO_CONTENT:
        errorName = i18n("No Content");
        description = errorText;
        break;

    case KIO::ERR_DISK_FULL:
        errorName = i18n("Disk Full");
        description = i18n("The requested file <strong>%1</strong> could not be "
                           "written to as there is inadequate disk space.",  errorText);
        solutions << i18n("Free up enough disk space by 1) deleting unwanted and "
                          "temporary files; 2) archiving files to removable media storage such as "
                          "CD-Recordable discs; or 3) obtain more storage capacity.")
                  << sSysadmin;
        break;

    case KIO::ERR_IDENTICAL_FILES:
        errorName = i18n("Source and Destination Files Identical");
        description = i18n("The operation could not be completed because the "
                           "source and destination files are the same file.");
        solutions << i18n("Choose a different filename for the destination file.");
        break;

    case KIO::ERR_DROP_ON_ITSELF:
        errorName = i18n("File or Folder dropped onto itself");
        description = i18n("The operation could not be completed because the "
                           "source and destination file or folder are the same.");
        solutions << i18n("Drop the item into a different file or folder.");
        break;

    // We assume that the slave has all the details
    case KIO::ERR_SLAVE_DEFINED:
        errorName.clear();
        description = errorText;
        break;

    case KIO::ERR_CANNOT_MOVE_INTO_ITSELF:
        errorName = i18n("Folder moved into itself");
        description = i18n("The operation could not be completed because the "
                           "source can not be moved into itself.");
        solutions << i18n("Move the item into a different folder.");
        break;

    case KIO::ERR_PASSWD_SERVER:
        errorName = i18n("Could not communicate with password server");
        description = i18n("The operation could not be completed because the "
                           "service for requesting passwords (kpasswdserver) couldn't be contacted");
        solutions << i18n("Try restarting your session, or look in the logs for errors from kiod.");
        break;

    case KIO::ERR_CANNOT_CREATE_SLAVE:
        errorName = i18n("Cannot Initiate the %1 Protocol",  protocol);
        techName = i18n("Unable to Create io-slave");
        description = i18n("The io-slave which provides access "
                           "to the <strong>%1</strong> protocol could not be started. This is "
                           "usually due to technical reasons.",  protocol);
        causes << i18n("klauncher could not find or start the plugin which provides the protocol."
                       "This means you may have an outdated version of the plugin.");
        solutions << sUpdate << sSysadmin;
        break;

    case KIO::ERR_FILE_TOO_LARGE_FOR_FAT32:
        errorName = xi18nc("@info", "Cannot transfer <filename>%1</filename>", errorText);
        description = xi18nc("@info", "The file <filename>%1</filename> cannot be transferred,"
                            " because the destination filesystem does not support files that large", errorText);
        solutions << i18n("Reformat the destination drive to use a filesystem that supports files that large.");
        break;

    default:
        // fall back to the plain error...
        errorName = i18n("Undocumented Error");
        description = buildErrorString(errorCode, errorText);
    }

    QByteArray ret;
    QDataStream stream(&ret, QIODevice::WriteOnly);
    stream << errorName << techName << description << causes << solutions;
    return ret;
}

QFile::Permissions KIO::convertPermissions(int permissions)
{
    QFile::Permissions qPermissions;

    if (permissions > 0) {
        if (permissions & S_IRUSR) {
            qPermissions |= QFile::ReadOwner;
        }
        if (permissions & S_IWUSR) {
            qPermissions |= QFile::WriteOwner;
        }
        if (permissions & S_IXUSR) {
            qPermissions |= QFile::ExeOwner;
        }

        if (permissions & S_IRGRP) {
            qPermissions |= QFile::ReadGroup;
        }
        if (permissions & S_IWGRP) {
            qPermissions |= QFile::WriteGroup;
        }
        if (permissions & S_IXGRP) {
            qPermissions |= QFile::ExeGroup;
        }

        if (permissions & S_IROTH) {
            qPermissions |= QFile::ReadOther;
        }
        if (permissions & S_IWOTH) {
            qPermissions |= QFile::WriteOther;
        }
        if (permissions & S_IXOTH) {
            qPermissions |= QFile::ExeOther;
        }
    }

    return qPermissions;
}

