/*
    This file is part of the KDE File Manager

    Copyright (C) 1998 Waldo Bastian (bastian@kde.org)

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public License
    as published by the Free Software Foundation; either
    version 2, or (at your option) version 3.

    This software is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this library; see the file COPYING. If not, write to
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.
*/
//----------------------------------------------------------------------------
//
// KDE File Manager -- HTTP Cookies

#ifndef KCOOKIEJAR_H
#define KCOOKIEJAR_H

#include <QtCore/QString>
#include <QtCore/QStringList>
#include <QtCore/QHash>
#include <QtCore/QSet>

class KConfig;
class KCookieJar;
class KHttpCookie;
class KHttpCookieList;

typedef KHttpCookie *KHttpCookiePtr;

enum KCookieAdvice
{
    KCookieDunno=0,
    KCookieAccept,
    KCookieAcceptForSession,
    KCookieReject,
    KCookieAsk
};

class KHttpCookie
{
    friend class KCookieJar;
    friend class KHttpCookieList;
    friend QDebug operator<<(QDebug, const KHttpCookie&); // for cookieStr()

protected:
    QString mHost;
    QString mDomain;
    QString mPath;
    QString mName;
    QString mValue;
    qint64  mExpireDate;
    int     mProtocolVersion;
    bool    mSecure;
    bool    mCrossDomain;
    bool    mHttpOnly;
    bool    mExplicitPath;
    QList<long> mWindowIds;
    QList<int> mPorts;
    KCookieAdvice mUserSelectedAdvice;

    QString cookieStr(bool useDOMFormat) const;

public:
    explicit KHttpCookie(const QString &_host=QString(),
                         const QString &_domain=QString(),
                         const QString &_path=QString(),
                         const QString &_name=QString(),
                         const QString &_value=QString(),
                         qint64 _expireDate=0,
                         int _protocolVersion=0,
                         bool _secure = false,
                         bool _httpOnly = false,
                         bool _explicitPath = false);

    QString domain() const { return mDomain; }
    QString host() const { return mHost; }
    QString path() const { return mPath; }
    QString name() const { return mName; }
    QString value() const { return mValue; }
    QList<long> &windowIds() { return mWindowIds; }
    const QList<long> &windowIds() const { return mWindowIds; }
    const QList<int> &ports() const { return mPorts; }
    void fixDomain(const QString &domain) { mDomain = domain; }
    qint64 expireDate() const { return mExpireDate; }
    int protocolVersion() const { return mProtocolVersion; }
    bool isSecure() const { return mSecure; }
    /**
     *  If currentDate is -1, the default, then the current timestamp in UTC
     *  is used for comparison against this cookie's expiration date.
     */
    bool isExpired(qint64 currentDate = -1) const;
    bool isCrossDomain() const { return mCrossDomain; }
    bool isHttpOnly() const { return mHttpOnly; }
    bool hasExplicitPath() const { return mExplicitPath; }
    bool match(const QString &fqdn, const QStringList &domainList, const QString &path, int port=-1) const;

    KCookieAdvice getUserSelectedAdvice() const { return mUserSelectedAdvice; }
    void setUserSelectedAdvice(KCookieAdvice advice) { mUserSelectedAdvice = advice; }
};

QDebug operator<<(QDebug, const KHttpCookie&);

class KHttpCookieList : public QList<KHttpCookie>
{
public:
    KHttpCookieList() : QList<KHttpCookie>(), advice( KCookieDunno )
    { }
    virtual ~KHttpCookieList() { }

    KCookieAdvice getAdvice() const { return advice; }
    void setAdvice(KCookieAdvice _advice) { advice = _advice; }

private:
    KCookieAdvice advice;
};

QDebug operator<<(QDebug, const KHttpCookieList&);

class KCookieJar
{
public:
    /**
     * Constructs a new cookie jar
     *
     * One jar should be enough for all cookies.
     */
    KCookieJar();

    /**
     * Destructs the cookie jar
     *
     * Poor little cookies, they will all be eaten by the cookie monster!
     */
    ~KCookieJar();

    /**
     * Returns whether the cookiejar has been changed
     */
    bool changed() const { return m_cookiesChanged || m_configChanged; }

    /**
     * Store all the cookies in a safe(?) place
     */
    bool saveCookies(const QString &_filename);

    /**
     * Load all the cookies from file and add them to the cookie jar.
     */
    bool loadCookies(const QString &_filename);

    /**
     * Save the cookie configuration
     */
    void saveConfig(KConfig *_config);

    /**
     * Load the cookie configuration
     */
    void loadConfig(KConfig *_config, bool reparse = false);

    /**
     * Looks for cookies in the cookie jar which are appropriate for _url.
     * Returned is a string containing all appropriate cookies in a format
     * which can be added to a HTTP-header without any additional processing.
     *
     * If @p useDOMFormat is true, the string is formatted in a format
     * in compliance with the DOM standard.
     * @p pendingCookies contains a list of cookies that have not been
     * approved yet by the user but that will be included in the result
     * none the less.
     */
    QString findCookies(const QString &_url, bool useDOMFormat, long windowId, KHttpCookieList *pendingCookies=0);

    /**
     * This function parses cookie_headers and returns a linked list of
     * valid KHttpCookie objects for all cookies found in cookie_headers.
     * If no cookies could be found 0 is returned.
     *
     * cookie_headers should be a concatenation of all lines of a HTTP-header
     * which start with "Set-Cookie". The lines should be separated by '\n's.
     */
    KHttpCookieList makeCookies(const QString &_url, const QByteArray &cookie_headers, long windowId);

    /**
     * This function parses cookie_headers and returns a linked list of
     * valid KHttpCookie objects for all cookies found in cookie_headers.
     * If no cookies could be found 0 is returned.
     *
     * cookie_domstr should be a concatenation of "name=value" pairs, separated
     * by a semicolon ';'.
     */
    KHttpCookieList makeDOMCookies(const QString &_url, const QByteArray &cookie_domstr, long windowId);

    /**
     * This function hands a KHttpCookie object over to the cookie jar.
     */
    void addCookie(KHttpCookie &cookie);

    /**
     * This function tells whether a single KHttpCookie object should
     * be considered persistent. Persistent cookies do not get deleted
     * at the end of the session and are saved on disk.
     */
    bool cookieIsPersistent(const KHttpCookie& cookie) const;

    /**
     * This function advices whether a single KHttpCookie object should
     * be added to the cookie jar.
     *
     * Possible return values are:
     *     - KCookieAccept, the cookie should be added
     *     - KCookieAcceptForSession, the cookie should be added as session cookie
     *     - KCookieReject, the cookie should not be added
     *     - KCookieAsk, the user should decide what to do
     *
     * Before sending cookies back to a server this function is consulted,
     * so that cookies having advice KCookieReject are not sent back.
     */
    KCookieAdvice cookieAdvice(const KHttpCookie& cookie) const;

    /**
     * This function gets the advice for all cookies originating from
     * _domain.
     *
     *     - KCookieDunno, no specific advice for _domain
     *     - KCookieAccept, accept all cookies for _domain
     *     - KCookieAcceptForSession, accept all cookies for _domain as session cookies
     *     - KCookieReject, reject all cookies for _domain
     *     - KCookieAsk, the user decides what to do with cookies for _domain
     */
    KCookieAdvice getDomainAdvice(const QString &_domain) const;

    /**
     * This function sets the advice for all cookies originating from
     * _domain.
     *
     * _advice can have the following values:
     *     - KCookieDunno, no specific advice for _domain
     *     - KCookieAccept, accept all cookies for _domain
     *     - KCookieAcceptForSession, accept all cookies for _domain as session cookies
     *     - KCookieReject, reject all cookies for _domain
     *     - KCookieAsk, the user decides what to do with cookies for _domain
     */
    void setDomainAdvice(const QString &_domain, KCookieAdvice _advice);

    /**
     * This function sets the advice for all cookies originating from
     * the same domain as _cookie
     *
     * _advice can have the following values:
     *     - KCookieDunno, no specific advice for _domain
     *     - KCookieAccept, accept all cookies for _domain
     *     - KCookieAcceptForSession, accept all cookies for _domain as session cookies
     *     - KCookieReject, reject all cookies for _domain
     *     - KCookieAsk, the user decides what to do with cookies for _domain
     */
    void setDomainAdvice(const KHttpCookie& _cookie, KCookieAdvice _advice);

    /**
     * Get the global advice for cookies
     *
     * The returned advice can have the following values:
     *     - KCookieAccept, accept cookies
     *     - KCookieAcceptForSession, accept cookies as session cookies
     *     - KCookieReject, reject cookies
     *     - KCookieAsk, the user decides what to do with cookies
     *
     * The global advice is used if the domain has no advice set.
     */
    KCookieAdvice getGlobalAdvice() const { return m_globalAdvice; }

    /**
     * This function sets the global advice for cookies
     *
     * _advice can have the following values:
     *     - KCookieAccept, accept cookies
     *     - KCookieAcceptForSession, accept cookies as session cookies
     *     - KCookieReject, reject cookies
     *     - KCookieAsk, the user decides what to do with cookies
     *
     * The global advice is used if the domain has no advice set.
     */
    void setGlobalAdvice(KCookieAdvice _advice);

    /**
     * Get a list of all domains known to the cookie jar.
     * A domain is known to the cookie jar if:
     *     - It has a cookie originating from the domain
     *     - It has a specific advice set for the domain
     */
    const QStringList& getDomainList();

    /**
     * Get a list of all cookies in the cookie jar originating from _domain.
     */
    KHttpCookieList *getCookieList(const QString & _domain,
                                   const QString& _fqdn );

    /**
     * Remove & delete a cookie from the jar.
     *
     * cookieIterator should be one of the entries in a KHttpCookieList.
     * Update your KHttpCookieList by calling getCookieList after
     * calling this function.
     */
    void eatCookie(KHttpCookieList::iterator cookieIterator);

    /**
     * Remove & delete all cookies for @p domain.
     */
    void eatCookiesForDomain(const QString &domain);

    /**
     * Remove & delete all cookies
     */
    void eatAllCookies();

    /**
     * Removes all end of session cookies set by the
     * session @p windId.
     */
    void eatSessionCookies( long windowId );

    /**
     * Removes all end of session cookies set by the
     * session @p windId.
     */
    void eatSessionCookies( const QString& fqdn, long windowId, bool isFQDN = true );

    /**
     * Parses _url and returns the FQDN (_fqdn) and path (_path).
     */
    static bool parseUrl(const QString &_url, QString &_fqdn, QString &_path, int *port = 0);

    /**
     * Returns a list of domains in @p _domainList relevant for this host.
     * The list is sorted with the FQDN listed first and the top-most
     * domain listed last
     */
    void extractDomains(const QString &_fqdn,
                        QStringList &_domainList) const;

    static QString adviceToStr(KCookieAdvice _advice);
    static KCookieAdvice strToAdvice(const QString &_str);

    enum KCookieDefaultPolicy {
        ApplyToShownCookiesOnly = 0,
        ApplyToCookiesFromDomain = 1,
        ApplyToAllCookies = 2
    };
    /** Returns the user's choice in the cookie window */
    KCookieDefaultPolicy preferredDefaultPolicy() const { return m_preferredPolicy; }

    /** Returns the */
    bool showCookieDetails () const { return m_showCookieDetails; }

     /**
      * Sets the user's default preference cookie policy.
      */
     void setPreferredDefaultPolicy (KCookieDefaultPolicy value) { m_preferredPolicy = value; }

     /**
      * Sets the user's preference of level of detail displayed
      * by the cookie dialog.
      */
     void setShowCookieDetails (bool value) { m_showCookieDetails = value; }

protected:
     void stripDomain(const QString &_fqdn, QString &_domain) const;
     QString stripDomain(const KHttpCookie& cookie) const;

protected:
    QStringList m_domainList;    
    KCookieAdvice m_globalAdvice;
    QHash<QString, KHttpCookieList*> m_cookieDomains;
    QSet<QString> m_twoLevelTLD;
    QSet<QString> m_gTLDs;

    bool m_configChanged;
    bool m_cookiesChanged;
    bool m_showCookieDetails;
    bool m_rejectCrossDomainCookies;
    bool m_autoAcceptSessionCookies;

    KCookieDefaultPolicy m_preferredPolicy;
};
#endif
