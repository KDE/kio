/*
    This file is part of the KDE File Manager
    SPDX-FileCopyrightText: 1998-2000 Waldo Bastian <bastian@kde.org>
    SPDX-FileCopyrightText: 2000, 2001 Dawit Alemayehu <adawit@kde.org>

    SPDX-License-Identifier: MIT
*/

// KDE File Manager -- HTTP Cookies

//
// The cookie protocol is a mess. RFC2109 is a joke since nobody seems to
// use it. Apart from that it is badly written.
// We try to implement Netscape Cookies and try to behave us according to
// RFC2109 as much as we can.
//
// We assume cookies do not contain any spaces (Netscape spec.)
// According to RFC2109 this is allowed though.
//

#include "kcookiejar.h"

#include <KConfig>
#include <KConfigGroup>
#include <QSaveFile>
#include <QDebug>

#include <QString>
#include <QFile>
#include <QRegularExpression>
#include <QTextStream>
#include <QLocale>
#include <QUrl>

Q_LOGGING_CATEGORY(KIO_COOKIEJAR, "kf.kio.slaves.http.cookiejar")

// BR87227
// Waba: Should the number of cookies be limited?
// I am not convinced of the need of such limit
// Mozilla seems to limit to 20 cookies / domain
// but it is unclear which policy it uses to expire
// cookies when it exceeds that amount
#undef MAX_COOKIE_LIMIT

#define MAX_COOKIES_PER_HOST 25
#define READ_BUFFER_SIZE 8192
#define IP_ADDRESS_EXPRESSION "(?:(?:25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)\\.){3}(?:25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)"

// Note with respect to QLatin1String( )....
// Cookies are stored as 8 bit data and passed to kio_http as Latin1
// regardless of their actual encoding.
#define QL1S(x)   QLatin1String(x)
#define QL1C(x)   QLatin1Char(x)

static QString removeWeekday(const QString &value)
{
    const int index = value.indexOf(QL1C(' '));
    if (index > -1) {
        int pos = 0;
        const QStringRef weekday = value.leftRef(index);
        const QLocale cLocale = QLocale::c();
        for (int i = 1; i < 8; ++i) {
            // No need to check for long names since the short names are
            // prefixes of the long names
            if (weekday.startsWith(cLocale.dayName(i, QLocale::ShortFormat), Qt::CaseInsensitive)) {
                pos = index + 1;
                break;
            }
        }
        if (pos > 0) {
            return value.mid(pos);
        }
    }
    return value;
}

static QDateTime parseDate(const QString &_value)
{
    // Handle sites sending invalid weekday as part of the date. #298660
    const QString value(removeWeekday(_value));

    // Check if expiration date matches RFC dates as specified under
    // RFC 2616 sec 3.3.1 & RFC 6265 sec 4.1.1
    QDateTime dt = QDateTime::fromString(value, Qt::RFC2822Date);

    if (!dt.isValid()) {
        static const char *const date_formats[] = {
            // Other formats documented in RFC 2616 sec 3.3.1
            // Note: the RFC says timezone information MUST be "GMT", hence the hardcoded timezone string
            "MMM dd HH:mm:ss yyyy",       /* ANSI C's asctime() format (#145244): Jan 01 00:00:00 1970 GMT */
            "dd-MMM-yy HH:mm:ss 'GMT'",   /* RFC 850 date: 06-Dec-39 00:30:42 GMT */
            // Non-standard formats
            "MMM dd yyyy HH:mm:ss",       /* A variation on ANSI C format seen @ amazon.com: Jan 01 1970 00:00:00 GMT */
            "dd-MMM-yyyy HH:mm:ss 'GMT'", /* cookies.test: Y2K38 problem: 06-Dec-2039 00:30:42 GMT */
            "MMM dd HH:mm:ss yyyy 'GMT'", /* cookies.test: Non-standard expiration dates: Sep 12 07:00:00 2020 GMT */
            "MMM dd yyyy HH:mm:ss 'GMT'", /* cookies.test: Non-standard expiration dates: Sep 12 2020 07:00:00 GMT */
            nullptr
        };

        // Only English month names are allowed, thus use the C locale.
        const QLocale cLocale = QLocale::c();
        for (int i = 0; date_formats[i]; ++i) {
            dt = cLocale.toDateTime(value, QL1S(date_formats[i]));
            if (dt.isValid()) {
                dt.setTimeSpec(Qt::UTC);
                break;
            }
        }
    }

    return dt.toUTC();  // Per RFC 2616 sec 3.3.1 always convert to UTC.
}

static qint64 toEpochSecs(const QDateTime &dt)
{
    return (dt.toMSecsSinceEpoch() / 1000); // convert to seconds...
}

static qint64 epoch()
{
    return toEpochSecs(QDateTime::currentDateTimeUtc());
}

QString KCookieJar::adviceToStr(KCookieAdvice _advice)
{
    switch (_advice) {
    case KCookieAccept: return QStringLiteral("Accept");
    case KCookieAcceptForSession: return QStringLiteral("AcceptForSession");
    case KCookieReject: return QStringLiteral("Reject");
    case KCookieAsk: return QStringLiteral("Ask");
    default: return QStringLiteral("Dunno");
    }
}

KCookieAdvice KCookieJar::strToAdvice(const QString &_str)
{
    if (_str.isEmpty()) {
        return KCookieDunno;
    }

    QString advice = _str.toLower().remove(QLatin1Char(' '));

    if (advice == QL1S("accept")) {
        return KCookieAccept;
    } else if (advice == QL1S("acceptforsession")) {
        return KCookieAcceptForSession;
    } else if (advice == QL1S("reject")) {
        return KCookieReject;
    } else if (advice == QL1S("ask")) {
        return KCookieAsk;
    }

    return KCookieDunno;
}

// KHttpCookie
///////////////////////////////////////////////////////////////////////////

//
// Cookie constructor
//
KHttpCookie::KHttpCookie(const QString &_host,
                         const QString &_domain,
                         const QString &_path,
                         const QString &_name,
                         const QString &_value,
                         qint64 _expireDate,
                         int _protocolVersion,
                         bool _secure,
                         bool _httpOnly,
                         bool _explicitPath) :
    mHost(_host),
    mDomain(_domain),
    mPath(_path.isEmpty() ? QString() : _path),
    mName(_name),
    mValue(_value),
    mExpireDate(_expireDate),
    mProtocolVersion(_protocolVersion),
    mSecure(_secure),
    mCrossDomain(false),
    mHttpOnly(_httpOnly),
    mExplicitPath(_explicitPath),
    mUserSelectedAdvice(KCookieDunno)
{
}

//
// Checks if a cookie has been expired
//
bool KHttpCookie::isExpired(qint64 currentDate) const
{
    if (currentDate == -1) {
        currentDate = epoch();
    }

    return (mExpireDate != 0) && (mExpireDate < currentDate);
}

//
// Returns a string for a HTTP-header
//
QString KHttpCookie::cookieStr(bool useDOMFormat) const
{
    QString result;

    if (useDOMFormat || (mProtocolVersion == 0)) {
        if (mName.isEmpty()) {
            result = mValue;
        } else {
            result = mName + QL1C('=') + mValue;
        }
    } else {
        result = mName + QL1C('=') + mValue;
        if (mExplicitPath) {
            result += QL1S("; $Path=\"") + mPath + QL1C('"');
        }
        if (!mDomain.isEmpty()) {
            result += QL1S("; $Domain=\"") + mDomain + QL1C('"');
        }
        if (!mPorts.isEmpty()) {
            if (mPorts.length() == 2 && mPorts.at(0) == -1) {
                result += QL1S("; $Port");
            } else {
                QString portNums;
                for (int port : qAsConst(mPorts)) {
                    portNums += QString::number(port) + QL1C(' ');
                }
                result += QL1S("; $Port=\"") + portNums.trimmed() + QL1C('"');
            }
        }
    }
    return result;
}

//
// Returns whether this cookie should be send to this location.
bool KHttpCookie::match(const QString &fqdn, const QStringList &domains,
                        const QString &path, int port) const
{
    // Cookie domain match check
    if (mDomain.isEmpty()) {
        if (fqdn != mHost) {
            return false;
        }
    } else if (!domains.contains(mDomain)) {
        if (mDomain[0] == QL1C('.')) {
            return false;
        }

        // Maybe the domain needs an extra dot.
        const QString domain = QL1C('.') + mDomain;
        if (!domains.contains(domain))
            if (fqdn != mDomain) {
                return false;
            }
    } else if (mProtocolVersion != 0 && port != -1 &&
               !mPorts.isEmpty() && !mPorts.contains(port)) {
        return false;
    }

    // Cookie path match check
    if (mPath.isEmpty()) {
        return true;
    }

    // According to the netscape spec http://www.acme.com/foobar,
    // http://www.acme.com/foo.bar and http://www.acme.com/foo/bar
    // should all match http://www.acme.com/foo...
    // We only match http://www.acme.com/foo/bar
    if (path.startsWith(mPath) &&
            (
                (path.length() == mPath.length()) ||   // Paths are exact match
                mPath.endsWith(QL1C('/')) ||          // mPath ended with a slash
                (path[mPath.length()] == QL1C('/'))        // A slash follows
            )) {
        return true;    // Path of URL starts with cookie-path
    }

    return false;
}

// KCookieJar
///////////////////////////////////////////////////////////////////////////

//
// Constructs a new cookie jar
//
// One jar should be enough for all cookies.
//
KCookieJar::KCookieJar()
{
    m_globalAdvice = KCookieDunno;
    m_configChanged = false;
    m_cookiesChanged = false;

    KConfig cfg(QStringLiteral("kf5/kcookiejar/domain_info"), KConfig::NoGlobals, QStandardPaths::GenericDataLocation);
    KConfigGroup group(&cfg, QString());

    const QStringList tldList = group.readEntry("gTLDs", QStringList());
    const QStringList twoLevelTldList = group.readEntry("twoLevelTLD", QStringList());
    m_gTLDs = QSet<QString>(tldList.begin(), tldList.end());
    m_twoLevelTLD = QSet<QString>(twoLevelTldList.begin(), twoLevelTldList.end());
}

//
// Destructs the cookie jar
//
// Poor little cookies, they will all be eaten by the cookie monster!
//
KCookieJar::~KCookieJar()
{
    qDeleteAll(m_cookieDomains);
    // Not much to do here
}

// cookiePtr is modified: the window ids of the existing cookie in the list are added to it
static void removeDuplicateFromList(KHttpCookieList *list, KHttpCookie &cookiePtr, bool nameMatchOnly = false, bool updateWindowId = false)
{
    QString domain1 = cookiePtr.domain();
    if (domain1.isEmpty()) {
        domain1 = cookiePtr.host();
    }

    QMutableListIterator<KHttpCookie> cookieIterator(*list);
    while (cookieIterator.hasNext()) {
        const KHttpCookie &cookie = cookieIterator.next();
        QString domain2 = cookie.domain();
        if (domain2.isEmpty()) {
            domain2 = cookie.host();
        }

        if (cookiePtr.name() == cookie.name() &&
                (nameMatchOnly || (domain1 == domain2 && cookiePtr.path() == cookie.path()))) {
            if (updateWindowId) {
                for (WId windowId : cookie.windowIds()) {
                    if (windowId && (!cookiePtr.windowIds().contains(windowId))) {
                        cookiePtr.windowIds().append(windowId);
                    }
                }
            }
            cookieIterator.remove();
            break;
        }
    }
}

//
// Looks for cookies in the cookie jar which are appropriate for _url.
// Returned is a string containing all appropriate cookies in a format
// which can be added to a HTTP-header without any additional processing.
//
QString KCookieJar::findCookies(const QString &_url, bool useDOMFormat, WId windowId, KHttpCookieList *pendingCookies)
{
    QString cookieStr, fqdn, path;
    QStringList domains;
    int port = -1;

    if (!parseUrl(_url, fqdn, path, &port)) {
        return cookieStr;
    }

    const bool secureRequest = (_url.startsWith(QL1S("https://"), Qt::CaseInsensitive) ||
                                _url.startsWith(QL1S("webdavs://"), Qt::CaseInsensitive));
    if (port == -1) {
        port = (secureRequest ? 443 : 80);
    }

    extractDomains(fqdn, domains);

    KHttpCookieList allCookies;
    for (QStringList::ConstIterator it = domains.constBegin(), itEnd = domains.constEnd();; ++it) {
        KHttpCookieList *cookieList = nullptr;
        if (it == itEnd) {
            cookieList = pendingCookies; // Add pending cookies
            pendingCookies = nullptr;
            if (!cookieList) {
                break;
            }
        } else {
            if ((*it).isNull()) {
                cookieList = m_cookieDomains.value(QL1S(""));
            } else {
                cookieList = m_cookieDomains.value(*it);
            }

            if (!cookieList) {
                continue;    // No cookies for this domain
            }
        }

        QMutableListIterator<KHttpCookie> cookieIt(*cookieList);
        while (cookieIt.hasNext()) {
            KHttpCookie &cookie = cookieIt.next();
            if (cookieAdvice(cookie) == KCookieReject) {
                continue;
            }

            if (!cookie.match(fqdn, domains, path, port)) {
                continue;
            }

            if (cookie.isSecure() && !secureRequest) {
                continue;
            }

            if (cookie.isHttpOnly() && useDOMFormat) {
                continue;
            }

            // Do not send expired cookies.
            if (cookie.isExpired()) {
                // NOTE: there is no need to delete the cookie here because the
                // cookieserver will invoke its saveCookieJar function as a result
                // of the state change below. This will then result in the cookie
                // being deleting at that point.
                m_cookiesChanged = true;
                continue;
            }

            if (windowId && (cookie.windowIds().indexOf(windowId) == -1)) {
                cookie.windowIds().append(windowId);
            }

            if (it == itEnd) { // Only needed when processing pending cookies
                removeDuplicateFromList(&allCookies, cookie);
            }

            allCookies.append(cookie);
        }

        if (it == itEnd) {
            break;    // Finished.
        }
    }

    int protVersion = 0;
    for (const KHttpCookie &cookie : qAsConst(allCookies)) {
        if (cookie.protocolVersion() > protVersion) {
            protVersion = cookie.protocolVersion();
        }
    }

    if (!allCookies.isEmpty()) {
        if (!useDOMFormat) {
            cookieStr = QStringLiteral("Cookie: ");
        }

        if (protVersion > 0) {
            cookieStr = cookieStr + QLatin1String("$Version=") + QString::number(protVersion) + QLatin1String("; ");
        }

        for (const KHttpCookie &cookie : qAsConst(allCookies)) {
            cookieStr = cookieStr + cookie.cookieStr(useDOMFormat) + QStringLiteral("; ");
        }

        cookieStr.chop(2); // Remove the trailing '; '
    }

    return cookieStr;
}

//
// This function parses a string like 'my_name="my_value";' and returns
// 'my_name' in Name and 'my_value' in Value.
//
// A pointer to the end of the parsed part is returned.
// This pointer points either to:
// '\0' - The end of the string has reached.
// ';'  - Another my_name="my_value" pair follows
// ','  - Another cookie follows
// '\n' - Another header follows
static const char *parseNameValue(const char *header,
                                  QString &Name,
                                  QString &Value,
                                  bool keepQuotes = false,
                                  bool rfcQuotes = false)
{
    const char *s = header;
    // Parse 'my_name' part
    for (; (*s != '='); s++) {
        if ((*s == '\0') || (*s == ';') || (*s == '\n')) {
            // No '=' sign -> use string as the value, name is empty
            // (behavior found in Mozilla and IE)
            Name = QL1S("");
            Value = QL1S(header);
            Value.truncate(s - header);
            Value = Value.trimmed();
            return s;
        }
    }

    Name = QL1S(header);
    Name.truncate(s - header);
    Name = Name.trimmed();

    // *s == '='
    s++;

    // Skip any whitespace
    for (; (*s == ' ') || (*s == '\t'); s++) {
        if ((*s == '\0') || (*s == ';') || (*s == '\n')) {
            // End of Name
            Value = QLatin1String("");
            return s;
        }
    }

    if ((rfcQuotes || !keepQuotes) && (*s == '\"')) {
        // Parse '"my_value"' part (quoted value)
        if (keepQuotes) {
            header = s++;
        } else {
            header = ++s;    // skip "
        }
        for (; (*s != '\"'); s++) {
            if ((*s == '\0') || (*s == '\n')) {
                // End of Name
                Value = QL1S(header);
                Value.truncate(s - header);
                return s;
            }
        }
        Value = QL1S(header);
        // *s == '\"';
        if (keepQuotes) {
            Value.truncate(++s - header);
        } else {
            Value.truncate(s++ - header);
        }

        // Skip any remaining garbage
        for (;; s++) {
            if ((*s == '\0') || (*s == ';') || (*s == '\n')) {
                break;
            }
        }
    } else {
        // Parse 'my_value' part (unquoted value)
        header = s;
        while ((*s != '\0') && (*s != ';') && (*s != '\n')) {
            s++;
        }
        // End of Name
        Value = QL1S(header);
        Value.truncate(s - header);
        Value = Value.trimmed();
    }
    return s;

}

void KCookieJar::stripDomain(const QString &_fqdn, QString &_domain) const
{
    QStringList domains;
    extractDomains(_fqdn, domains);
    if (domains.count() > 3) {
        _domain = domains[3];
    } else if (!domains.isEmpty()) {
        _domain = domains[0];
    } else {
        _domain = QL1S("");
    }
}

QString KCookieJar::stripDomain(const KHttpCookie &cookie) const
{
    QString domain; // We file the cookie under this domain.
    if (cookie.domain().isEmpty()) {
        stripDomain(cookie.host(), domain);
    } else {
        domain = cookie.domain();
    }
    return domain;
}

bool KCookieJar::parseUrl(const QString &_url, QString &_fqdn, QString &_path, int *port)
{
    QUrl kurl(_url);
    if (!kurl.isValid() || kurl.scheme().isEmpty()) {
        return false;
    }

    _fqdn = kurl.host().toLower();
    // Cookie spoofing protection.  Since there is no way a path separator,
    // a space or the escape encoding character is allowed in the hostname
    // according to RFC 2396, reject attempts to include such things there!
    if (_fqdn.contains(QL1C('/')) || _fqdn.contains(QL1C('%'))) {
        return false;    // deny everything!!
    }

    // Set the port number from the protocol when one is found...
    if (port) {
        *port = kurl.port();
    }

    _path = kurl.path();
    if (_path.isEmpty()) {
        _path = QStringLiteral("/");
    }

    return true;
}

// not static because it uses m_twoLevelTLD
void KCookieJar::extractDomains(const QString &_fqdn,
                                QStringList &_domains) const
{
    if (_fqdn.isEmpty()) {
        _domains.append(QStringLiteral("localhost"));
        return;
    }

    // Return numeric IPv6 addresses as is...
    if (_fqdn[0] == QL1C('[')) {
        _domains.append(_fqdn);
        return;
    }
    // Return numeric IPv4 addresses as is...
    if (_fqdn[0] >= QL1C('0') && _fqdn[0] <= QL1C('9')
        && _fqdn.indexOf(QRegularExpression(QStringLiteral(IP_ADDRESS_EXPRESSION))) > -1) {
        _domains.append(_fqdn);
        return;
    }

    // Always add the FQDN at the start of the list for
    // hostname == cookie-domainname checks!
    _domains.append(_fqdn);
    _domains.append(QL1C('.') + _fqdn);

    QStringList partList = _fqdn.split(QL1C('.'), Qt::SkipEmptyParts);

    if (!partList.isEmpty()) {
        partList.erase(partList.begin());    // Remove hostname
    }

    while (partList.count()) {

        if (partList.count() == 1) {
            break;    // We only have a TLD left.
        }

        if ((partList.count() == 2) && m_twoLevelTLD.contains(partList[1].toLower())) {
            // This domain uses two-level TLDs in the form xxxx.yy
            break;
        }

        if ((partList.count() == 2) && (partList[1].length() == 2)) {
            // If this is a TLD, we should stop. (e.g. co.uk)
            // We assume this is a TLD if it ends with .xx.yy or .x.yy
            if (partList[0].length() <= 2) {
                break;    // This is a TLD.
            }

            // Catch some TLDs that we miss with the previous check
            // e.g. com.au, org.uk, mil.co
            if (m_gTLDs.contains(partList[0].toLower())) {
                break;
            }
        }

        QString domain = partList.join(QLatin1Char('.'));
        _domains.append(domain);
        _domains.append(QL1C('.') + domain);
        partList.erase(partList.begin()); // Remove part
    }
}

//
// This function parses cookie_headers and returns a linked list of
// KHttpCookie objects for all cookies found in cookie_headers.
// If no cookies could be found 0 is returned.
//
// cookie_headers should be a concatenation of all lines of a HTTP-header
// which start with "Set-Cookie". The lines should be separated by '\n's.
//
KHttpCookieList KCookieJar::makeCookies(const QString &_url,
                                        const QByteArray &cookie_headers,
                                        WId windowId)
{
    QString fqdn, path;

    if (!parseUrl(_url, fqdn, path)) {
        return KHttpCookieList();    // Error parsing _url
    }

    QString Name, Value;
    KHttpCookieList cookieList, cookieList2;

    bool isRFC2965 = false;
    bool crossDomain = false;
    const char *cookieStr = cookie_headers.constData();

    QString defaultPath;
    const int index = path.lastIndexOf(QL1C('/'));
    if (index > 0) {
        defaultPath = path.left(index);
    }

    // Check for cross-domain flag from kio_http
    if (qstrncmp(cookieStr, "Cross-Domain\n", 13) == 0) {
        cookieStr += 13;
        crossDomain = true;
    }

    //  The hard stuff :)
    for (;;) {
        // check for "Set-Cookie"
        if (qstrnicmp(cookieStr, "Set-Cookie:", 11) == 0) {
            cookieStr = parseNameValue(cookieStr + 11, Name, Value, true);

            // Host = FQDN
            // Default domain = ""
            // Default path according to rfc2109

            KHttpCookie cookie(fqdn, QL1S(""), defaultPath, Name, Value);
            if (windowId) {
                cookie.mWindowIds.append(windowId);
            }
            cookie.mCrossDomain = crossDomain;

            // Insert cookie in chain
            cookieList.append(cookie);
        } else if (qstrnicmp(cookieStr, "Set-Cookie2:", 12) == 0) {
            // Attempt to follow rfc2965
            isRFC2965 = true;
            cookieStr = parseNameValue(cookieStr + 12, Name, Value, true, true);

            // Host = FQDN
            // Default domain = ""
            // Default path according to rfc2965

            KHttpCookie cookie(fqdn, QL1S(""), defaultPath, Name, Value);
            if (windowId) {
                cookie.mWindowIds.append(windowId);
            }
            cookie.mCrossDomain = crossDomain;

            // Insert cookie in chain
            cookieList2.append(cookie);
        } else {
            // This is not the start of a cookie header, skip till next line.
            while (*cookieStr && *cookieStr != '\n') {
                cookieStr++;
            }

            if (*cookieStr == '\n') {
                cookieStr++;
            }

            if (!*cookieStr) {
                break;    // End of cookie_headers
            } else {
                continue;    // end of this header, continue with next.
            }
        }

        while ((*cookieStr == ';') || (*cookieStr == ' ')) {
            cookieStr++;

            // Name-Value pair follows
            cookieStr = parseNameValue(cookieStr, Name, Value);
            KHttpCookie &lastCookie = (isRFC2965 ? cookieList2.last() : cookieList.last());

            if (Name.compare(QL1S("domain"), Qt::CaseInsensitive) == 0) {
                QString dom = Value.toLower();
                // RFC2965 3.2.2: If an explicitly specified value does not
                // start with a dot, the user agent supplies a leading dot
                if (dom.length() > 0 && dom[0] != QL1C('.')) {
                    dom.prepend(QL1C('.'));
                }
                // remove a trailing dot
                if (dom.length() > 2 && dom[dom.length() - 1] == QL1C('.')) {
                    dom.chop(1);
                }

                if (dom.count(QL1C('.')) > 1 || dom == QLatin1String(".local")) {
                    lastCookie.mDomain = dom;
                }
            } else if (Name.compare(QL1S("max-age"), Qt::CaseInsensitive) == 0) {
                int max_age = Value.toInt();
                if (max_age == 0) {
                    lastCookie.mExpireDate = 1;
                } else {
                    lastCookie.mExpireDate = toEpochSecs(QDateTime::currentDateTimeUtc().addSecs(max_age));
                }
            } else if (Name.compare(QL1S("expires"), Qt::CaseInsensitive) == 0) {
                const QDateTime dt = parseDate(Value);

                if (dt.isValid()) {
                    lastCookie.mExpireDate = toEpochSecs(dt);
                    if (lastCookie.mExpireDate == 0) {
                        lastCookie.mExpireDate = 1;
                    }
                }
            } else if (Name.compare(QL1S("path"), Qt::CaseInsensitive) == 0) {
                if (Value.isEmpty()) {
                    lastCookie.mPath.clear();    // Catch "" <> QString()
                } else {
                    lastCookie.mPath = QUrl::fromPercentEncoding(Value.toLatin1());
                }
                lastCookie.mExplicitPath = true;
            } else if (Name.compare(QL1S("version"), Qt::CaseInsensitive) == 0) {
                lastCookie.mProtocolVersion = Value.toInt();
            } else if (Name.compare(QL1S("secure"), Qt::CaseInsensitive) == 0 ||
                       (Name.isEmpty() && Value.compare(QL1S("secure"), Qt::CaseInsensitive) == 0)) {
                lastCookie.mSecure = true;
            } else if (Name.compare(QL1S("httponly"), Qt::CaseInsensitive) == 0 ||
                       (Name.isEmpty() && Value.compare(QL1S("httponly"), Qt::CaseInsensitive) == 0)) {
                lastCookie.mHttpOnly = true;
            } else if (isRFC2965 && (Name.compare(QL1S("port"), Qt::CaseInsensitive) == 0 ||
                                     (Name.isEmpty() && Value.compare(QL1S("port"), Qt::CaseInsensitive) == 0))) {
                // Based on the port selection rule of RFC 2965 section 3.3.4...
                if (Name.isEmpty()) {
                    // We intentionally append a -1 first in order to distinguish
                    // between only a 'Port' vs a 'Port="80 443"' in the sent cookie.
                    lastCookie.mPorts.append(-1);
                    const bool secureRequest = (_url.startsWith(QL1S("https://"), Qt::CaseInsensitive) ||
                                                _url.startsWith(QL1S("webdavs://"), Qt::CaseInsensitive));
                    if (secureRequest) {
                        lastCookie.mPorts.append(443);
                    } else {
                        lastCookie.mPorts.append(80);
                    }
                } else {
                    bool ok;
                    const QStringList portNums = Value.split(QL1C(' '), Qt::SkipEmptyParts);
                    for (const QString &portNum : portNums) {
                        const int port = portNum.toInt(&ok);
                        if (ok) {
                            lastCookie.mPorts.append(port);
                        }
                    }
                }
            }
        }

        if (*cookieStr == '\0') {
            break;    // End of header
        }

        // Skip ';' or '\n'
        cookieStr++;
    }

    // RFC2965 cookies come last so that they override netscape cookies.
    while (!cookieList2.isEmpty()) {
        KHttpCookie &lastCookie = cookieList2.first();
        removeDuplicateFromList(&cookieList, lastCookie, true);
        cookieList.append(lastCookie);
        cookieList2.removeFirst();
    }

    return cookieList;
}

/**
* Parses cookie_domstr and returns a linked list of KHttpCookie objects.
* cookie_domstr should be a semicolon-delimited list of "name=value"
* pairs. Any whitespace before "name" or around '=' is discarded.
* If no cookies are found, 0 is returned.
*/
KHttpCookieList KCookieJar::makeDOMCookies(const QString &_url,
        const QByteArray &cookie_domstring,
        WId windowId)
{
    // A lot copied from above
    KHttpCookieList cookieList;

    const char *cookieStr = cookie_domstring.data();
    QString fqdn;
    QString path;

    if (!parseUrl(_url, fqdn, path)) {
        // Error parsing _url
        return KHttpCookieList();
    }

    QString Name;
    QString Value;
    //  This time it's easy
    while (*cookieStr) {
        cookieStr = parseNameValue(cookieStr, Name, Value);

        // Host = FQDN
        // Default domain = ""
        // Default path = ""
        KHttpCookie cookie(fqdn, QString(), QString(),
                           Name, Value);
        if (windowId) {
            cookie.mWindowIds.append(windowId);
        }

        cookieList.append(cookie);

        if (*cookieStr != '\0') {
            cookieStr++;    // Skip ';' or '\n'
        }
    }

    return cookieList;
}

// KHttpCookieList sorting
///////////////////////////////////////////////////////////////////////////

// We want the longest path first
static bool compareCookies(const KHttpCookie &item1, const KHttpCookie &item2)
{
    return item1.path().length() > item2.path().length();
}

#ifdef MAX_COOKIE_LIMIT
static void makeRoom(KHttpCookieList *cookieList, KHttpCookiePtr &cookiePtr)
{
    // Too many cookies: throw one away, try to be somewhat clever
    KHttpCookiePtr lastCookie = 0;
    for (KHttpCookiePtr cookie = cookieList->first(); cookie; cookie = cookieList->next()) {
        if (compareCookies(cookie, cookiePtr)) {
            break;
        }
        lastCookie = cookie;
    }
    if (!lastCookie) {
        lastCookie = cookieList->first();
    }
    cookieList->removeRef(lastCookie);
}
#endif

//
// This function hands a KHttpCookie object over to the cookie jar.
//
void KCookieJar::addCookie(KHttpCookie &cookie)
{
    QStringList domains;
    // We always need to do this to make sure that the
    // that cookies of type hostname == cookie-domainname
    // are properly removed and/or updated as necessary!
    extractDomains(cookie.host(), domains);

    // If the cookie specifies a domain, check whether it is valid. Otherwise,
    // accept the cookie anyways but removes the domain="" value to prevent
    // cross-site cookie injection.
    if (!cookie.domain().isEmpty()) {
        if (!domains.contains(cookie.domain()) &&
                !cookie.domain().endsWith(QL1C('.') + cookie.host())) {
            cookie.fixDomain(QString());
        }
    }

    for (const QString &key : qAsConst(domains)) {
        KHttpCookieList *list;

        if (key.isNull()) {
            list = m_cookieDomains.value(QL1S(""));
        } else {
            list = m_cookieDomains.value(key);
        }

        if (list) {
            removeDuplicateFromList(list, cookie, false, true);
        }
    }

    const QString domain = stripDomain(cookie);
    KHttpCookieList *cookieList;
    if (domain.isNull()) {
        cookieList = m_cookieDomains.value(QL1S(""));
    } else {
        cookieList = m_cookieDomains.value(domain);
    }

    if (!cookieList) {
        // Make a new cookie list
        cookieList = new KHttpCookieList();

        // All cookies whose domain is not already
        // known to us should be added with KCookieDunno.
        // KCookieDunno means that we use the global policy.
        cookieList->setAdvice(KCookieDunno);

        m_cookieDomains.insert(domain, cookieList);

        // Update the list of domains
        m_domainList.append(domain);
    }

    // Add the cookie to the cookie list
    // The cookie list is sorted 'longest path first'
    if (!cookie.isExpired()) {
#ifdef MAX_COOKIE_LIMIT
        if (cookieList->count() >= MAX_COOKIES_PER_HOST) {
            makeRoom(cookieList, cookie);    // Delete a cookie
        }
#endif
        cookieList->push_back(cookie);
        // Use a stable sort so that unit tests are reliable.
        // In practice it doesn't matter though.
        std::stable_sort(cookieList->begin(), cookieList->end(), compareCookies);

        m_cookiesChanged = true;
    }
}

//
// This function advises whether a single KHttpCookie object should
// be added to the cookie jar.
//
KCookieAdvice KCookieJar::cookieAdvice(const KHttpCookie &cookie) const
{
    if (m_rejectCrossDomainCookies && cookie.isCrossDomain()) {
        return KCookieReject;
    }

    if (cookie.getUserSelectedAdvice() != KCookieDunno) {
        return cookie.getUserSelectedAdvice();
    }

    if (m_autoAcceptSessionCookies && cookie.expireDate() == 0) {
        return KCookieAccept;
    }

    QStringList domains;
    extractDomains(cookie.host(), domains);

    KCookieAdvice advice = KCookieDunno;
    QStringListIterator it(domains);
    while (advice == KCookieDunno && it.hasNext()) {
        const QString &domain = it.next();
        if (domain.startsWith(QL1C('.')) || cookie.host() == domain) {
            KHttpCookieList *cookieList = m_cookieDomains.value(domain);
            if (cookieList) {
                advice = cookieList->getAdvice();
            }
        }
    }

    if (advice == KCookieDunno) {
        advice = m_globalAdvice;
    }

    return advice;
}

//
// This function tells whether a single KHttpCookie object should
// be considered persistent. Persistent cookies do not get deleted
// at the end of the session and are saved on disk.
//
bool KCookieJar::cookieIsPersistent(const KHttpCookie &cookie) const
{
    if (cookie.expireDate() == 0) {
        return false;
    }

    KCookieAdvice advice = cookieAdvice(cookie);

    if (advice == KCookieReject || advice == KCookieAcceptForSession) {
        return false;
    }

    return true;
}

//
// This function gets the advice for all cookies originating from
// _domain.
//
KCookieAdvice KCookieJar::getDomainAdvice(const QString &_domain) const
{
    KHttpCookieList *cookieList = m_cookieDomains.value(_domain);
    KCookieAdvice advice;

    if (cookieList) {
        advice = cookieList->getAdvice();
    } else {
        advice = KCookieDunno;
    }

    return advice;
}

//
// This function sets the advice for all cookies originating from
// _domain.
//
void KCookieJar::setDomainAdvice(const QString &domain, KCookieAdvice _advice)
{
    KHttpCookieList *cookieList = m_cookieDomains.value(domain);

    if (cookieList) {
        if (cookieList->getAdvice() != _advice) {
            m_configChanged = true;
            // domain is already known
            cookieList->setAdvice(_advice);
        }

        if ((cookieList->isEmpty()) && (_advice == KCookieDunno)) {
            // This deletes cookieList!
            delete m_cookieDomains.take(domain);
            m_domainList.removeAll(domain);
        }
    } else {
        // domain is not yet known
        if (_advice != KCookieDunno) {
            // We should create a domain entry
            m_configChanged = true;
            // Make a new cookie list
            cookieList = new KHttpCookieList();
            cookieList->setAdvice(_advice);
            m_cookieDomains.insert(domain, cookieList);
            // Update the list of domains
            m_domainList.append(domain);
        }
    }
}

//
// This function sets the advice for all cookies originating from
// the same domain as _cookie
//
void KCookieJar::setDomainAdvice(const KHttpCookie &cookie, KCookieAdvice _advice)
{
    QString domain;
    stripDomain(cookie.host(), domain); // We file the cookie under this domain.
    setDomainAdvice(domain, _advice);
}

//
// This function sets the global advice for cookies
//
void KCookieJar::setGlobalAdvice(KCookieAdvice _advice)
{
    if (m_globalAdvice != _advice) {
        m_configChanged = true;
    }
    m_globalAdvice = _advice;
}

//
// Get a list of all domains known to the cookie jar.
//
const QStringList &KCookieJar::getDomainList()
{
    return m_domainList;
}

//
// Get a list of all cookies in the cookie jar originating from _domain.
//
KHttpCookieList *KCookieJar::getCookieList(const QString &_domain,
        const QString &_fqdn)
{
    QString domain;

    if (_domain.isEmpty()) {
        stripDomain(_fqdn, domain);
    } else {
        domain = _domain;
    }

    return m_cookieDomains.value(domain);
}

//
// Eat a cookie out of the jar.
// cookieIterator should be one of the cookies returned by getCookieList()
//
void KCookieJar::eatCookie(const KHttpCookieList::iterator &cookieIterator)
{
    const KHttpCookie &cookie = *cookieIterator;
    const QString domain = stripDomain(cookie); // We file the cookie under this domain.
    KHttpCookieList *cookieList = m_cookieDomains.value(domain);

    if (cookieList) {
        // This deletes cookie!
        cookieList->erase(cookieIterator);

        if ((cookieList->isEmpty()) &&
                (cookieList->getAdvice() == KCookieDunno)) {
            // This deletes cookieList!
            delete m_cookieDomains.take(domain);
            m_domainList.removeAll(domain);
        }
    }
}

void KCookieJar::eatCookiesForDomain(const QString &domain)
{
    KHttpCookieList *cookieList = m_cookieDomains.value(domain);
    if (!cookieList || cookieList->isEmpty()) {
        return;
    }

    cookieList->clear();
    if (cookieList->getAdvice() == KCookieDunno) {
        // This deletes cookieList!
        delete m_cookieDomains.take(domain);
        m_domainList.removeAll(domain);
    }
    m_cookiesChanged = true;
}

void KCookieJar::eatSessionCookies(long windowId)
{
    if (!windowId) {
        return;
    }

    for (const QString &domain : qAsConst(m_domainList)) {
        eatSessionCookies(domain, windowId, false);
    }
}

void KCookieJar::eatAllCookies()
{
    // we need a copy as eatCookiesForDomain() might remove domain from m_domainList
    const QStringList list = m_domainList;
    for (const QString &domain : list) {
        eatCookiesForDomain(domain);
    }
}

void KCookieJar::eatSessionCookies(const QString &fqdn, WId windowId,
                                   bool isFQDN)
{
    KHttpCookieList *cookieList;
    if (!isFQDN) {
        cookieList = m_cookieDomains.value(fqdn);
    } else {
        QString domain;
        stripDomain(fqdn, domain);
        cookieList = m_cookieDomains.value(domain);
    }

    if (cookieList) {
        QMutableListIterator<KHttpCookie> cookieIterator(*cookieList);
        while (cookieIterator.hasNext()) {
            KHttpCookie &cookie = cookieIterator.next();

            if (cookieIsPersistent(cookie)) {
                continue;
            }

            QList<WId> &ids = cookie.windowIds();

#ifndef NDEBUG
            if (ids.contains(windowId)) {
                if (ids.count() > 1) {
                    qCDebug(KIO_COOKIEJAR) << "removing window id" << windowId << "from session cookie";
                } else {
                    qCDebug(KIO_COOKIEJAR) << "deleting session cookie";
                }
            }
#endif
            if (!ids.removeAll(windowId) || !ids.isEmpty()) {
                continue;
            }
            cookieIterator.remove();
        }
    }
}

static QString hostWithPort(const KHttpCookie *cookie)
{
    const QList<int> &ports = cookie->ports();

    if (ports.isEmpty()) {
        return cookie->host();
    }

    QStringList portList;
    for (int port : ports) {
        portList << QString::number(port);
    }

    return (cookie->host() + QL1C(':') + portList.join(QLatin1Char(',')));
}

//
// Saves all cookies to the file '_filename'.
// On success 'true' is returned.
// On failure 'false' is returned.
bool KCookieJar::saveCookies(const QString &_filename)
{
    QSaveFile cookieFile(_filename);

    if (!cookieFile.open(QIODevice::WriteOnly)) {
        return false;
    }

    QTextStream ts(&cookieFile);

    ts << "# KDE Cookie File v2\n#\n";

    const QString str =
        QString::asprintf("%-20s %-20s %-12s %-10s %-4s %-20s %-4s %s\n",
              "# Host", "Domain", "Path", "Exp.date", "Prot",
              "Name", "Sec", "Value");
    ts << str;


    for (const QString &domainName : qAsConst(m_domainList)) {
        bool domainPrinted = false;

        KHttpCookieList *cookieList = m_cookieDomains.value(domainName);
        if (!cookieList) {
            continue;
        }

        QMutableListIterator<KHttpCookie> cookieIterator(*cookieList);
        while (cookieIterator.hasNext()) {
            const KHttpCookie &cookie = cookieIterator.next();

            if (cookie.isExpired()) {
                // Delete expired cookies
                cookieIterator.remove();
                continue;
            }
            if (cookieIsPersistent(cookie)) {
                // Only save cookies that are not "session-only cookies"
                if (!domainPrinted) {
                    domainPrinted = true;
                    ts << '[' << domainName.toLocal8Bit().data() << "]\n";
                }
                // Store persistent cookies
                const QString path = QL1C('"') + cookie.path() + QL1C('"');
                const QString domain = QL1C('"') + cookie.domain() + QL1C('"');
                const QString host = hostWithPort(&cookie);

                // TODO: replace with direct QTextStream output ?
                const QString str = QString::asprintf("%-20s %-20s %-12s %10lld  %3d %-20s %-4i %s\n",
                          host.toLatin1().constData(), domain.toLatin1().constData(),
                          path.toLatin1().constData(), cookie.expireDate(),
                          cookie.protocolVersion(),
                          cookie.name().isEmpty() ? cookie.value().toLatin1().constData() : cookie.name().toLatin1().constData(),
                          (cookie.isSecure() ? 1 : 0) + (cookie.isHttpOnly() ? 2 : 0) +
                          (cookie.hasExplicitPath() ? 4 : 0) + (cookie.name().isEmpty() ? 8 : 0),
                          cookie.value().toLatin1().constData());
                ts << str.toLatin1();
            }
        }
    }

    if (cookieFile.commit()) {
        QFile::setPermissions(_filename, QFile::ReadUser | QFile::WriteUser);
        return true;
    }
    return false;
}

static const char *parseField(char *&buffer, bool keepQuotes = false)
{
    char *result;
    if (!keepQuotes && (*buffer == '\"')) {
        // Find terminating "
        buffer++;
        result = buffer;
        while ((*buffer != '\"') && (*buffer)) {
            buffer++;
        }
    } else {
        // Find first white space
        result = buffer;
        while ((*buffer != ' ') && (*buffer != '\t') && (*buffer != '\n') && (*buffer)) {
            buffer++;
        }
    }

    if (!*buffer) {
        return result;    //
    }
    *buffer++ = '\0';

    // Skip white-space
    while ((*buffer == ' ') || (*buffer == '\t') || (*buffer == '\n')) {
        buffer++;
    }

    return result;
}

static QString extractHostAndPorts(const QString &str, QList<int> *ports = nullptr)
{
    if (str.isEmpty()) {
        return str;
    }

    const int index = str.indexOf(QL1C(':'));
    if (index == -1) {
        return str;
    }

    const QString host = str.left(index);
    if (ports) {
        bool ok;
        const QStringList list = str.mid(index + 1).split(QL1C(','));
        for (const QString &portStr : list) {
            const int portNum = portStr.toInt(&ok);
            if (ok) {
                ports->append(portNum);
            }
        }
    }

    return host;
}

//
// Reloads all cookies from the file '_filename'.
// On success 'true' is returned.
// On failure 'false' is returned.
bool KCookieJar::loadCookies(const QString &_filename)
{
    QFile cookieFile(_filename);

    if (!cookieFile.open(QIODevice::ReadOnly)) {
        return false;
    }

    int version = 1;
    bool success = false;
    char *buffer = new char[READ_BUFFER_SIZE];
    qint64 len = cookieFile.readLine(buffer, READ_BUFFER_SIZE - 1);

    if (len != -1) {
        if (qstrcmp(buffer, "# KDE Cookie File\n") == 0) {
            success = true;
        } else if (qstrcmp(buffer, "# KDE Cookie File v") > 0) {
            bool ok = false;
            const int verNum = QByteArray(buffer + 19, len - 19).trimmed().toInt(&ok);
            if (ok) {
                version = verNum;
                success = true;
            }
        }
    }

    if (success) {
        const qint64 currentTime = epoch();
        QList<int> ports;

        while (cookieFile.readLine(buffer, READ_BUFFER_SIZE - 1) != -1) {
            char *line = buffer;
            // Skip lines which begin with '#' or '['
            if ((line[0] == '#') || (line[0] == '[')) {
                continue;
            }

            const QString host = extractHostAndPorts(QL1S(parseField(line)), &ports);
            const QString domain = QL1S(parseField(line));
            if (host.isEmpty() && domain.isEmpty()) {
                continue;
            }
            const QString path = QL1S(parseField(line));
            const QString expStr = QL1S(parseField(line));
            if (expStr.isEmpty()) {
                continue;
            }
            const qint64 expDate = expStr.toLongLong();
            const QString verStr = QL1S(parseField(line));
            if (verStr.isEmpty()) {
                continue;
            }
            int protVer  = verStr.toInt();
            QString name = QL1S(parseField(line));
            bool keepQuotes = false;
            bool secure = false;
            bool httpOnly = false;
            bool explicitPath = false;
            const char *value = nullptr;
            if ((version == 2) || (protVer >= 200)) {
                if (protVer >= 200) {
                    protVer -= 200;
                }
                int i = atoi(parseField(line));
                secure = i & 1;
                httpOnly = i & 2;
                explicitPath = i & 4;
                if (i & 8) {
                    name = QLatin1String("");
                }
                line[strlen(line) - 1] = '\0'; // Strip LF.
                value = line;
            } else {
                if (protVer >= 100) {
                    protVer -= 100;
                    keepQuotes = true;
                }
                value = parseField(line, keepQuotes);
                secure = QByteArray(parseField(line)).toShort();
            }

            // Expired or parse error
            if (!value || expDate == 0 || expDate < currentTime) {
                continue;
            }

            KHttpCookie cookie(host, domain, path, name, QString::fromUtf8(value), expDate,
                               protVer, secure, httpOnly, explicitPath);
            if (!ports.isEmpty()) {
                cookie.mPorts = ports;
            }
            addCookie(cookie);
        }
    }

    delete [] buffer;
    m_cookiesChanged = false;
    return success;
}

//
// Save the cookie configuration
//

void KCookieJar::saveConfig(KConfig *_config)
{
    if (!m_configChanged) {
        return;
    }

    KConfigGroup dlgGroup(_config, "Cookie Dialog");
    dlgGroup.writeEntry("PreferredPolicy", static_cast<int>(m_preferredPolicy));
    dlgGroup.writeEntry("ShowCookieDetails", m_showCookieDetails);
    KConfigGroup policyGroup(_config, "Cookie Policy");
    policyGroup.writeEntry("CookieGlobalAdvice", adviceToStr(m_globalAdvice));

    QStringList domainSettings;
    for (const QString &domain : qAsConst(m_domainList)) {
        KCookieAdvice advice = getDomainAdvice(domain);
        if (advice != KCookieDunno) {
            const QString value = domain + QL1C(':') + adviceToStr(advice);
            domainSettings.append(value);
        }
    }
    policyGroup.writeEntry("CookieDomainAdvice", domainSettings);
    _config->sync();
    m_configChanged = false;
}

//
// Load the cookie configuration
//

void KCookieJar::loadConfig(KConfig *_config, bool reparse)
{
    if (reparse) {
        _config->reparseConfiguration();
    }

    KConfigGroup dlgGroup(_config, "Cookie Dialog");
    m_showCookieDetails = dlgGroup.readEntry("ShowCookieDetails", false);
    m_preferredPolicy = static_cast<KCookieDefaultPolicy>(dlgGroup.readEntry("PreferredPolicy", 0));

    KConfigGroup policyGroup(_config, "Cookie Policy");
    const QStringList domainSettings = policyGroup.readEntry("CookieDomainAdvice", QStringList());
    // Warning: those default values are duplicated in the kcm (kio/kcookiespolicies.cpp)
    m_rejectCrossDomainCookies = policyGroup.readEntry("RejectCrossDomainCookies", true);
    m_autoAcceptSessionCookies = policyGroup.readEntry("AcceptSessionCookies", true);
    m_globalAdvice = strToAdvice(policyGroup.readEntry("CookieGlobalAdvice", QStringLiteral("Accept")));

    // Reset current domain settings first.
    // We need a copy as setDomainAdvice() may modify m_domainList
    const QStringList list = m_domainList;
    for (const QString &domain : list) {
        setDomainAdvice(domain, KCookieDunno);
    }

    // Now apply the domain settings read from config file...
    for (QStringList::ConstIterator it = domainSettings.constBegin(), itEnd = domainSettings.constEnd();
            it != itEnd; ++it) {
        const QString &value = *it;
        const int sepPos = value.lastIndexOf(QL1C(':'));
        if (sepPos <= 0) {
            continue;
        }

        const QString domain(value.left(sepPos));
        KCookieAdvice advice = strToAdvice(value.mid(sepPos + 1));
        setDomainAdvice(domain, advice);
    }
}

QDebug operator<<(QDebug dbg, const KHttpCookie &cookie)
{
    dbg.nospace() << cookie.cookieStr(false);
    return dbg.space();
}

QDebug operator<<(QDebug dbg, const KHttpCookieList &list)
{
    for (const KHttpCookie &cookie : list) {
        dbg << cookie;
    }
    return dbg;
}
