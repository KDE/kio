/* This file is part of the KDE libraries
    Copyright (C) 2008, 2009 Andreas Hartmetz <ahartmetz@gmail.com>

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

#ifndef HTTPAUTHENTICATION_H
#define HTTPAUTHENTICATION_H

#include <config-gssapi.h>

#include <QtCore/QByteArray>
#include <QtCore/QString>
#include <QtCore/QList>
#include <QUrl>

namespace KIO {
class AuthInfo;
}

class KConfigGroup;

class KAbstractHttpAuthentication
{
public:
    KAbstractHttpAuthentication(KConfigGroup *config = 0);
    virtual ~KAbstractHttpAuthentication();

    /**
     * Choose the best authentication mechanism from the offered ones
     *
     * This will return the most secure mechanism from the list of
     * mechanisms retuned by the server.
     */
    static QByteArray bestOffer(const QList<QByteArray> &offers);

    /**
     * Returns authentication object instance appropriate for @p offer.
     *
     * @param offer   the header from which an authentication object is created.
     * @param config  the config object to read stored authentication information.
     */
    static KAbstractHttpAuthentication *newAuth(const QByteArray &offer, KConfigGroup *config = 0);

    /**
     * Split all headers containing multiple authentication offers.
     *
     * @param offers the offers from multiple HTTP authentication header lines.
     * @return a list where each entry contains only a single offer
     */
    static QList<QByteArray> splitOffers(const QList<QByteArray> &offers);

    /**
     * reset to state after default construction.
     */
    void reset();
    /**
     * the authentication scheme: "Negotiate", "Digest", "Basic", "NTLM"
     */
    virtual QByteArray scheme() const = 0;
    /**
     * initiate authentication with challenge string (from HTTP header)
     */
    virtual void setChallenge(const QByteArray &c, const QUrl &resource, const QByteArray &httpMethod);
    /**
     * return value updated by setChallenge()
     *
     * if this is false user and password passed to generateResponse
     * will be ignored and may be empty.
     */
    bool needCredentials() const { return m_needCredentials; }
    /**
     * KIO compatible data to find cached credentials.
     *
     * Note that username and/or password as well as UI text will NOT be filled in.
     */
    virtual void fillKioAuthInfo(KIO::AuthInfo *ai) const = 0;
    /**
     * what to do in response to challenge
     */
    virtual void generateResponse(const QString &user,
                                  const QString &password) = 0;

    /**
     * returns true when the final stage of authentication is reached.
     *
     * Unless the authentication scheme requires multiple stages like NTLM this
     * function will always return true.
     */
    bool wasFinalStage() const { return m_finalAuthStage; }
    /**
     * Returns true if the authentication scheme supports path matching to identify
     * resources that belong to the same protection space (realm).
     *
     * See RFC 2617.
     */
    virtual bool supportsPathMatching() const { return false; }

    // the following accessors return useful data after generateResponse() has been called.
    // clients process the following fields top to bottom: highest priority is on top

    // malformed challenge and similar problems - it is advisable to reconnect
    bool isError() const { return m_isError; }
    /**
     * force keep-alive connection because the authentication method requires it
     */
    bool forceKeepAlive() const { return m_forceKeepAlive; }
    /**
     * force disconnection because the authentication method requires it
     */
    bool forceDisconnect() const { return m_forceDisconnect; }

    /**
     * insert this into the next request header after "Authorization: "
     * or "Proxy-Authorization: "
     */
    QByteArray headerFragment() const { return m_headerFragment; }
    /**
     * Returns the realm sent by the server.
     *
     * This is mainly for GUI shown to the user. This is the identification of
     * the protected area on the server (e.g. "Konquis home directory" or
     * "KDE files").
     */
    QString realm() const;

    /**
     * Sets the cache password flag to @p enable.
     */
    void setCachePasswordEnabled(bool enable) { m_keepPassword = enable; }

#ifdef ENABLE_HTTP_AUTH_NONCE_SETTER
    // NOTE: FOR USE in unit testing ONLY.
    virtual void setDigestNonceValue(const QByteArray&) {}
#endif

protected:
    void authInfoBoilerplate(KIO::AuthInfo *a) const;
    /**
     * Returns any authentication data that should be cached for future use.
     *
     * NOTE: Do not reimplement this function for connection based authentication
     * schemes such as NTLM.
     */
    virtual QByteArray authDataToCache() const { return QByteArray(); }
    void generateResponseCommon(const QString &user, const QString &password);

    KConfigGroup *m_config;
    QByteArray m_scheme;    ///< this is parsed from the header and not necessarily == scheme().
    QByteArray m_challengeText;
    QList<QByteArray> m_challenge;
    QUrl m_resource;
    QByteArray m_httpMethod;

    bool m_isError;
    bool m_needCredentials;
    bool m_forceKeepAlive;
    bool m_forceDisconnect;
    bool m_finalAuthStage;
    bool m_keepPassword;
    QByteArray m_headerFragment;

    QString m_username;
    QString m_password;
};


class KHttpBasicAuthentication : public KAbstractHttpAuthentication
{
public:
    virtual QByteArray scheme() const;
    virtual void fillKioAuthInfo(KIO::AuthInfo *ai) const;
    virtual void generateResponse(const QString &user, const QString &password);
    virtual bool supportsPathMatching() const { return true; }
protected:
    virtual QByteArray authDataToCache() const { return m_challengeText; }
private:
    friend class KAbstractHttpAuthentication;
    KHttpBasicAuthentication(KConfigGroup *config = 0)
     : KAbstractHttpAuthentication(config) {}
};


class KHttpDigestAuthentication : public KAbstractHttpAuthentication
{
public:
    virtual QByteArray scheme() const;
    virtual void setChallenge(const QByteArray &c, const QUrl &resource, const QByteArray &httpMethod);
    virtual void fillKioAuthInfo(KIO::AuthInfo *ai) const;
    virtual void generateResponse(const QString &user, const QString &password);
    virtual bool supportsPathMatching() const { return true; }
#ifdef ENABLE_HTTP_AUTH_NONCE_SETTER
    virtual void setDigestNonceValue(const QByteArray&);
#endif

protected:
    virtual QByteArray authDataToCache() const { return m_challengeText; }
private:
    friend class KAbstractHttpAuthentication;
    KHttpDigestAuthentication(KConfigGroup *config = 0)
     : KAbstractHttpAuthentication(config) {}
#ifdef ENABLE_HTTP_AUTH_NONCE_SETTER
     QByteArray m_nonce;
#endif
};


class KHttpNtlmAuthentication : public KAbstractHttpAuthentication
{
public:
    virtual QByteArray scheme() const;
    virtual void setChallenge(const QByteArray &c, const QUrl &resource, const QByteArray &httpMethod);
    virtual void fillKioAuthInfo(KIO::AuthInfo *ai) const;
    virtual void generateResponse(const QString &user, const QString &password);
private:
    friend class KAbstractHttpAuthentication;
    KHttpNtlmAuthentication(KConfigGroup *config = 0)
     : KAbstractHttpAuthentication(config) {}
};


#if HAVE_LIBGSSAPI
class KHttpNegotiateAuthentication : public KAbstractHttpAuthentication
{
public:
    virtual QByteArray scheme() const;
    virtual void setChallenge(const QByteArray &c, const QUrl &resource, const QByteArray &httpMethod);
    virtual void fillKioAuthInfo(KIO::AuthInfo *ai) const;
    virtual void generateResponse(const QString &user, const QString &password);
private:
    friend class KAbstractHttpAuthentication;
    KHttpNegotiateAuthentication(KConfigGroup *config = 0)
     : KAbstractHttpAuthentication(config) {}
};
#endif // HAVE_LIBGSSAPI

#endif // HTTPAUTHENTICATION_H
