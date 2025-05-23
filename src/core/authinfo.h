/*
    This file is part of the KDE libraries
    SPDX-FileCopyrightText: 2000-2001 Dawit Alemayehu <adawit@kde.org>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#ifndef KIO_AUTHINFO_H
#define KIO_AUTHINFO_H

#include "kiocore_export.h"

#include <QList>
#include <QMap>
#include <QStringList>
#include <QUrl>
#include <QVariant> // Q_DECLARE_METATYPE

#include <memory>

class QDBusArgument;

namespace KIO
{
class AuthInfoPrivate;

/*!
 * \class KIO::AuthInfo
 * \inheaderfile KIO/AuthInfo
 * \inmodule KIOCore
 *
 * \brief A two way messaging class for passing authentication information.
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
 * \note If you extend this class to add additional
 * parameters do not forget to overload the stream insertion and
 * extraction operators ("<<" and ">>") so that the added data can
 * be correctly serialized.
 *
 */
class KIOCORE_EXPORT AuthInfo
{
    /*!
     *
     */
    KIOCORE_EXPORT friend QDataStream &operator<<(QDataStream &s, const AuthInfo &a);

    /*!
     *
     */
    KIOCORE_EXPORT friend QDataStream &operator>>(QDataStream &s, AuthInfo &a);

    /*!
     *
     */
    KIOCORE_EXPORT friend QDBusArgument &operator<<(QDBusArgument &argument, const AuthInfo &a);

    /*!
     *
     */
    KIOCORE_EXPORT friend const QDBusArgument &operator>>(const QDBusArgument &argument, AuthInfo &a);

public:
    /*!
     * Default constructor.
     */
    AuthInfo();

    /*!
     * Copy constructor.
     */
    AuthInfo(const AuthInfo &info);

    ~AuthInfo();

    AuthInfo &operator=(const AuthInfo &info);

    /*!
     * Use this method to check if the object was modified.
     * Returns \c true if the object has been modified
     */
    bool isModified() const;

    /*!
     * Use this method to indicate that this object has been modified.
     *
     * \a flag true to mark the object as modified, false to clear
     */
    void setModified(bool flag);

    /*!
     * The URL for which authentication is to be stored.
     *
     * This field is required when attempting to cache authorization
     * and retrieve it.  However, it is not needed when prompting
     * the user for authorization info.
     *
     * This setting is \e required except when prompting the
     * user for password.
     */
    QUrl url;

    /*!
     * This is \e required for caching.
     */
    QString username;

    /*!
     * This is \e required for caching.
     */
    QString password;

    /*!
     * Information to be displayed when prompting
     * the user for authentication information.
     *
     * \note If this field is not set, the authentication
     *    dialog simply displays the preset default prompt.
     *
     * This setting is \e optional and empty by default.
     */
    QString prompt;

    /*!
     * The text to displayed in the title bar of
     * the password prompting dialog.
     *
     * \note If this field is not set, the authentication
     *    dialog simply displays the preset default caption.
     *
     * This setting is \e optional and empty by default.
     */
    QString caption;

    /*!
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
     * the \a commentLabel field as it will be placed properly
     * in the dialog rather than to include it within the actual
     * comment.
     *
     * This setting is \e optional and empty by default.
     */
    QString comment;

    /*!
     * Descriptive label to be displayed in front of the
     * comment when prompting the user for password.
     *
     * This setting is \e optional and only applicable when
     * the comment field is also set.
     */
    QString commentLabel;

    /*!
     * A unique identifier that allows caching of multiple
     * passwords for different resources in the same server.
     *
     * Mostly this setting is applicable to the HTTP protocol
     * whose authentication scheme explicitly defines the use
     * of such a unique key.  However, any protocol that can
     * generate or supply a unique id can effectively use it
     * to distinguish passwords.
     *
     * This setting is \e optional and not set by default.
     */
    QString realmValue;

    /*!
     * Field to store any extra authentication information for
     * protocols that need it.
     *
     * This setting is \e optional and mostly applicable for HTTP
     * protocol.  However, any protocol can make use of it to
     * store extra info.
     */
    QString digestInfo;

    /*!
     * Flag that, if set, indicates whether a path match should be
     * performed when requesting for cached authorization.
     *
     * A path is deemed to be a match if it is equal to or is a subset
     * of the cached path.  For example, if stored path is "/foo/bar"
     * and the request's path set to "/foo/bar/acme", then it is a match
     * whereas it would not if the request's path was set to "/foo".
     *
     * This setting is \e optional and false by default.
     */
    bool verifyPath;

    /*!
     * Flag which if set forces the username field to be read-only.
     *
     * This setting is \e optional and false by default.
     */
    bool readOnly;

    /*!
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

    /*!
     * Flags for extra fields
     *
     * \value ExtraFieldNoFlags
     * \value ExtraFieldReadOnly
     * \value ExtraFieldMandatory
     */
    enum FieldFlags {
        ExtraFieldNoFlags = 0,
        ExtraFieldReadOnly = 1 << 1,
        ExtraFieldMandatory = 1 << 2,
    };

    /*!
     * Set Extra Field Value.
     * Currently supported extra-fields:
     *    "domain" (QString),
     *    "anonymous" (bool)
     * Setting it to an invalid QVariant() will disable the field.
     * Extra Fields are disabled by default.
     */
    void setExtraField(const QString &fieldName, const QVariant &value);

    /*!
     * Set Extra Field Flags
     */
    void setExtraFieldFlags(const QString &fieldName, const FieldFlags flags);

    /*!
     * Get Extra Field Value
     * Check QVariant::isValid() to find out if the field exists.
     */
    QVariant getExtraField(const QString &fieldName) const;

    /*!
     * Get Extra Field Flags
     */
    AuthInfo::FieldFlags getExtraFieldFlags(const QString &fieldName) const;

    /*!
     * Register the meta-types for AuthInfo. This is called from
     * AuthInfo's constructor but needed by daemons on the D-Bus such
     * as kpasswdserver.
     */
    static void registerMetaTypes();

protected:
    bool modified;

private:
    friend class ::KIO::AuthInfoPrivate;
    std::unique_ptr<AuthInfoPrivate> const d;
};

KIOCORE_EXPORT QDataStream &operator<<(QDataStream &s, const AuthInfo &a);
KIOCORE_EXPORT QDataStream &operator>>(QDataStream &s, AuthInfo &a);

KIOCORE_EXPORT QDBusArgument &operator<<(QDBusArgument &argument, const AuthInfo &a);
KIOCORE_EXPORT const QDBusArgument &operator>>(const QDBusArgument &argument, AuthInfo &a);
}

Q_DECLARE_METATYPE(KIO::AuthInfo)

#endif
