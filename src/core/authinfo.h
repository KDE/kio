/*
    This file is part of the KDE libraries
    SPDX-FileCopyrightText: 2000-2001 Dawit Alemayehu <adawit@kde.org>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#ifndef KIO_AUTHINFO_H
#define KIO_AUTHINFO_H

#include "kiocore_export.h"

#include <QMap>
#include <QList>
#include <QStringList>
#include <QUrl>
#include <QVariant> // Q_DECLARE_METATYPE

class QDBusArgument;

namespace KIO
{

class AuthInfoPrivate;

/**
 * @class KIO::AuthInfo authinfo.h <KIO/AuthInfo>
 *
 * This class is intended to make it easier to prompt for, cache
 * and retrieve authorization information.
 *
 * When using this class to cache, retrieve or prompt authentication
 * information, you only need to set the necessary attributes. For
 * example, to check whether a password is already cached, the only
 * required information is the URL of the resource and optionally
 * whether or not a path match should be performed.  Similarly, to
 * prompt for password you only need to optionally set the prompt,
 * username (if already supplied), comment and commentLabel fields.
 *
 * <em>SPECIAL NOTE:</em> If you extend this class to add additional
 * parameters do not forget to overload the stream insertion and
 * extraction operators ("<<" and ">>") so that the added data can
 * be correctly serialized.
 *
 * @short A two way messaging class for passing authentication information.
 * @author Dawit Alemayehu <adawit@kde.org>
 */
class KIOCORE_EXPORT AuthInfo
{
    KIOCORE_EXPORT friend QDataStream &operator<< (QDataStream &s, const AuthInfo &a);
    KIOCORE_EXPORT friend QDataStream &operator>> (QDataStream &s, AuthInfo &a);

    KIOCORE_EXPORT friend QDBusArgument &operator<<(QDBusArgument &argument, const AuthInfo &a);
    KIOCORE_EXPORT friend const QDBusArgument &operator>>(const QDBusArgument &argument, AuthInfo &a);

public:

    /**
     * Default constructor.
     */
    AuthInfo();

    /**
     * Copy constructor.
     */
    AuthInfo(const AuthInfo &info);

    /**
     * Destructor
     * @since 4.1
     */
    ~AuthInfo();

    /**
     * Custom assignment operator.
     */
    AuthInfo &operator=(const AuthInfo &info);

    /**
     * Use this method to check if the object was modified.
     * @return true if the object has been modified
     */
    bool isModified() const;

    /**
     * Use this method to indicate that this object has been modified.
     * @param flag true to mark the object as modified, false to clear
     */
    void setModified(bool flag);

    /**
     * The URL for which authentication is to be stored.
     *
     * This field is required when attempting to cache authorization
     * and retrieve it.  However, it is not needed when prompting
     * the user for authorization info.
     *
     * This setting is @em required except when prompting the
     * user for password.
     */
    QUrl url;

    /**
     * This is @em required for caching.
     */
    QString username;

    /**
     * This is @em required for caching.
     */
    QString password;

    /**
     * Information to be displayed when prompting
     * the user for authentication information.
     *
     * @note If this field is not set, the authentication
     *    dialog simply displays the preset default prompt.
     *
     * This setting is @em optional and empty by default.
     */
    QString prompt;

    /**
     * The text to displayed in the title bar of
     * the password prompting dialog.
     *
     * @note If this field is not set, the authentication
     *    dialog simply displays the preset default caption.
     *
     * This setting is @em optional and empty by default.
     */
    QString caption;

    /**
     * Additional comment to be displayed when prompting
     * the user for authentication information.
     *
     * This field allows you to display a short (no more than
     * 80 characters) extra description in the password prompt
     * dialog.  For example, this field along with the
     * commentLabel can be used to describe the server that
     * requested the authentication:
     *
     *  \code
     *  Server:   Squid Proxy @ foo.com
     *  \endcode
     *
     * where "Server:" is the commentLabel and the rest is the
     * actual comment.  Note that it is always better to use
     * the @p commentLabel field as it will be placed properly
     * in the dialog rather than to include it within the actual
     * comment.
     *
     * This setting is @em optional and empty by default.
     */
    QString comment;

    /**
     * Descriptive label to be displayed in front of the
     * comment when prompting the user for password.
     *
     * This setting is @em optional and only applicable when
     * the comment field is also set.
     */
    QString commentLabel;

    /**
     * A unique identifier that allows caching of multiple
     * passwords for different resources in the same server.
     *
     * Mostly this setting is applicable to the HTTP protocol
     * whose authentication scheme explicitly defines the use
     * of such a unique key.  However, any protocol that can
     * generate or supply a unique id can effectively use it
     * to distinguish passwords.
     *
     * This setting is @em optional and not set by default.
     */
    QString realmValue;

    /**
     * Field to store any extra authentication information for
     * protocols that need it.
     *
     * This setting is @em optional and mostly applicable for HTTP
     * protocol.  However, any protocol can make use of it to
     * store extra info.
     */
    QString digestInfo;

    /**
     * Flag that, if set, indicates whether a path match should be
     * performed when requesting for cached authorization.
     *
     * A path is deemed to be a match if it is equal to or is a subset
     * of the cached path.  For example, if stored path is "/foo/bar"
     * and the request's path set to "/foo/bar/acme", then it is a match
     * whereas it would not if the request's path was set to "/foo".
     *
     * This setting is @em optional and false by default.
     */
    bool verifyPath;

    /**
     * Flag which if set forces the username field to be read-only.
     *
     * This setting is @em optional and false by default.
     */
    bool readOnly;

    /**
     * Flag to indicate the persistence of the given password.
     *
     * This is a two-way flag, when set before calling openPasswordDialog
     * it makes the "keep Password" check box visible to the user.
     * In return the flag will indicate the state of the check box.
     * By default if the flag is checked the password will be cached
     * for the entire life of the current KDE session otherwise the
     * cached password is deleted right after the application using
     * it has been closed.
     */
    bool keepPassword;

    /**
     * Flags for extra fields
     * @since 4.1
     */
    enum FieldFlags {
        ExtraFieldNoFlags = 0,
        ExtraFieldReadOnly = 1 << 1,
        ExtraFieldMandatory = 1 << 2,
    };

    /**
     * Set Extra Field Value.
     * Currently supported extra-fields:
     *    "domain" (QString),
     *    "anonymous" (bool)
     * Setting it to an invalid QVariant() will disable the field.
     * Extra Fields are disabled by default.
     * @since 4.1
     */
    void setExtraField(const QString &fieldName, const QVariant &value);

    /**
     * Set Extra Field Flags
     * @since 4.1
     */
    void setExtraFieldFlags(const QString &fieldName, const FieldFlags flags);

    /**
     * Get Extra Field Value
     * Check QVariant::isValid() to find out if the field exists.
     * @since 4.1
     */
    QVariant getExtraField(const QString &fieldName) const;

    /**
     * Get Extra Field Flags
     * @since 4.1
     */
    AuthInfo::FieldFlags getExtraFieldFlags(const QString &fieldName) const;

    /**
     * Register the meta-types for AuthInfo. This is called from
     * AuthInfo's constructor but needed by daemons on the D-Bus such
     * as kpasswdserver.
     * @since 4.3
     */
    static void registerMetaTypes();

protected:
    bool modified;

private:
    friend class ::KIO::AuthInfoPrivate;
    AuthInfoPrivate *const d;
};

KIOCORE_EXPORT QDataStream &operator<< (QDataStream &s, const AuthInfo &a);
KIOCORE_EXPORT QDataStream &operator>> (QDataStream &s, AuthInfo &a);

KIOCORE_EXPORT QDBusArgument &operator<<(QDBusArgument &argument, const AuthInfo &a);
KIOCORE_EXPORT const QDBusArgument &operator>>(const QDBusArgument &argument, AuthInfo &a);

/**
 * A Singleton class that provides access to passwords
 * stored in .netrc files for automatic login purposes.
 * This is only meant to address backward compatibility
 * with old automated ftp client style logins...
 *
 * @short An interface to the ftp .netrc files
 * @author Dawit Alemayehu <adawit@kde.org>
 */
class KIOCORE_EXPORT NetRC
{
public:

    /**
     * Specifies the mode to be used when searching for a
     * matching automatic login info for a given site :
     *
     * @li exactOnly        search entries with exact host name matches.
     * @li defaultOnly      search entries that are specified as "default".
     * @li presetOnly       search entries that are specified as "preset".
     *
     * @see lookup
     * @see LookUpMode
     */
    enum LookUpModeFlag {
        exactOnly = 0x0002,
        defaultOnly = 0x0004,
        presetOnly = 0x0008,
    };
    /**
     * Stores a combination of #LookUpModeFlag values.
     */
    Q_DECLARE_FLAGS(LookUpMode, LookUpModeFlag)

    /**
     * Contains auto login information.
     * @see lookup()
     */
    struct AutoLogin {
        QString type;
        QString machine;
        QString login;
        QString password;
        QMap<QString, QStringList> macdef;
    };

    /**
     * A reference to the instance of the class.
     * @return the class
     */
    static NetRC *self();

    /**
     * Looks up the @p login information for the given @p url.
     *
     * @param url the url whose login information will be checked
     * @param login the login information will be writte here
     * @param userealnetrc if true, use $HOME/.netrc file
     * @param type the type of the login. If null, the @p url's protocol
     *        will be taken
     * @param mode the LookUpMode flags (ORed) for the query
     */
    bool lookup(const QUrl &url, AutoLogin &login,
                bool userealnetrc = false,
                const QString &type = QString(),
                LookUpMode mode = LookUpMode(exactOnly) | defaultOnly);
    /**
     * Reloads the auto login information.
     */
    void reload();

protected:
    bool parse(const QString &fileName);

private:
    NetRC();
    ~NetRC();

    NetRC(const NetRC &) = delete;
    NetRC& operator=(const NetRC &) = delete;

private:
    static NetRC *instance;

    class NetRCPrivate;
    NetRCPrivate *const d;
};

Q_DECLARE_OPERATORS_FOR_FLAGS(NetRC::LookUpMode)

}

Q_DECLARE_METATYPE(KIO::AuthInfo)

#endif
