/*
    This file is part of the KDE libraries
    SPDX-FileCopyrightText: 2008, 2009 Andreas Hartmetz <ahartmetz@gmail.com>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#include "httpauthentication.h"

#if HAVE_LIBGSSAPI
#if GSSAPI_MIT
#include <gssapi/gssapi.h>
#else
#include <gssapi.h>
#endif /* GSSAPI_MIT */

// Catch uncompatible crap (BR86019)
#if defined(GSS_RFC_COMPLIANT_OIDS) && (GSS_RFC_COMPLIANT_OIDS == 0)
#include <gssapi/gssapi_generic.h>
#define GSS_C_NT_HOSTBASED_SERVICE gss_nt_service_name
#endif

#endif /* HAVE_LIBGSSAPI */

#include <KRandom>
#include <QDebug>
#include <KConfigGroup>
#include <kio/authinfo.h>
#include <kntlm.h>

#include <QTextCodec>
#include <QCryptographicHash>

Q_LOGGING_CATEGORY(KIO_HTTP_AUTH, "kf.kio.slaves.http.auth")

static bool isWhiteSpace(char ch)
{
    return (ch == ' ' || ch == '\t' || ch == '\v' || ch == '\f');
}

static bool isWhiteSpaceOrComma(char ch)
{
    return (ch == ',' || isWhiteSpace(ch));
}

static bool containsScheme(const char input[], int start, int end)
{
    // skip any comma or white space
    while (start < end && isWhiteSpaceOrComma(input[start])) {
        start++;
    }

    while (start < end) {
        if (isWhiteSpace(input[start])) {
            return true;
        }
        start++;
    }

    return false;
}

// keys on even indexes, values on odd indexes. Reduces code expansion for the templated
// alternatives.
// If "ba" starts with empty content it will be removed from ba to simplify later calls
static QList<QByteArray> parseChallenge(QByteArray &ba, QByteArray *scheme, QByteArray *nextAuth = nullptr)
{
    QList<QByteArray> values;
    const char *b = ba.constData();
    int len = ba.count();
    int start = 0, end = 0, pos = 0, pos2 = 0;

    // parse scheme
    while (start < len && isWhiteSpaceOrComma(b[start])) {
        start++;
    }
    end = start;
    while (end < len && !isWhiteSpace(b[end])) {
        end++;
    }

    // drop empty stuff from the given string, it would have to be skipped over and over again
    if (start != 0) {
        ba.remove(0, start);
        end -= start;
        len -= start;
        start = 0;
        b = ba.constData();
    }
    Q_ASSERT(scheme);
    *scheme = ba.left(end);

    while (end < len) {
        start = end;
        while (end < len && b[end] != '=') {
            end++;
        }
        pos = end; // save the end position
        while (end - 1 > start && isWhiteSpace(b[end - 1])) { // trim whitespace
            end--;
        }
        pos2 = start;
        while (pos2 < end && isWhiteSpace(b[pos2])) { // skip whitespace
            pos2++;
        }
        if (containsScheme(b, start, end) || (b[pos2] == ',' && b[pos] != '=' && pos == len)) {
            if (nextAuth) {
                *nextAuth = QByteArray(b + start);
            }
            break;  // break on start of next scheme.
        }
        while (start < len && isWhiteSpaceOrComma(b[start])) {
            start++;
        }
        values.append(QByteArray(b + start, end - start));
        end = pos; // restore the end position
        if (end == len) {
            break;
        }

        // parse value
        start = end + 1;    //skip '='
        while (start < len && isWhiteSpace(b[start])) {
            start++;
        }

        if (b[start] == '"') {
            //quoted string
            bool hasBs = false;
            bool hasErr = false;
            end = ++start;
            while (end < len) {
                if (b[end] == '\\') {
                    end++;
                    if (end + 1 >= len) {
                        hasErr = true;
                        break;
                    } else {
                        hasBs = true;
                        end++;
                    }
                } else if (b[end] == '"') {
                    break;
                } else {
                    end++;
                }
            }
            if (hasErr || (end == len)) {
                // remove the key we already inserted
                // qDebug() << "error in quoted text for key" << values.last();
                values.removeLast();
                break;
            }
            QByteArray value = QByteArray(b + start, end - start);
            if (hasBs) {
                // skip over the next character, it might be an escaped backslash
                int i = -1;
                while ((i = value.indexOf('\\', i + 1)) >= 0) {
                    value.remove(i, 1);
                }
            }
            values.append(value);
            end++;
        } else {
            //unquoted string
            end = start;
            while (end < len && b[end] != ',' && !isWhiteSpace(b[end])) {
                end++;
            }
            values.append(QByteArray(b + start, end - start));
        }

        //the quoted string has ended, but only a comma ends a key-value pair
        while (end < len && isWhiteSpace(b[end])) {
            end++;
        }

        // garbage, here should be end or field delimiter (comma)
        if (end < len && b[end] != ',') {
            // qDebug() << "unexpected character" << b[end] << "found in WWW-authentication header where token boundary (,) was expected";
            break;
        }
    }
    // ensure every key has a value
    // WARNING: Do not remove the > 1 check or parsing a Type 1 NTLM
    // authentication challenge will surely fail.
    if (values.count() > 1 && values.count() % 2) {
        values.removeLast();
    }
    return values;
}

static QByteArray valueForKey(const QList<QByteArray> &ba, const QByteArray &key)
{
    for (int i = 0, count = ba.count(); (i + 1) < count; i += 2) {
        if (ba[i] == key) {
            return ba[i + 1];
        }
    }
    return QByteArray();
}

KAbstractHttpAuthentication::KAbstractHttpAuthentication(KConfigGroup *config)
    : m_config(config), m_finalAuthStage(false)
{
    reset();
}

KAbstractHttpAuthentication::~KAbstractHttpAuthentication()
{
}

QByteArray KAbstractHttpAuthentication::bestOffer(const QList<QByteArray> &offers)
{
    // choose the most secure auth scheme offered
    QByteArray negotiateOffer;
    QByteArray digestOffer;
    QByteArray ntlmOffer;
    QByteArray basicOffer;
    for (const QByteArray &offer : offers) {
        const QByteArray scheme = offer.mid(0, offer.indexOf(' ')).toLower();
#if HAVE_LIBGSSAPI
        if (scheme == "negotiate") { // krazy:exclude=strings
            negotiateOffer = offer;
        } else
#endif
            if (scheme == "digest") { // krazy:exclude=strings
                digestOffer = offer;
            } else if (scheme == "ntlm") { // krazy:exclude=strings
                ntlmOffer = offer;
            } else if (scheme == "basic") { // krazy:exclude=strings
                basicOffer = offer;
            }
    }

    if (!negotiateOffer.isEmpty()) {
        return negotiateOffer;
    }

    if (!digestOffer.isEmpty()) {
        return digestOffer;
    }

    if (!ntlmOffer.isEmpty()) {
        return ntlmOffer;
    }

    return basicOffer;  //empty or not...
}

KAbstractHttpAuthentication *KAbstractHttpAuthentication::newAuth(const QByteArray &offer, KConfigGroup *config)
{
    const QByteArray scheme = offer.mid(0, offer.indexOf(' ')).toLower();
#if HAVE_LIBGSSAPI
    if (scheme == "negotiate") { // krazy:exclude=strings
        return new KHttpNegotiateAuthentication(config);
    } else
#endif
        if (scheme == "digest") { // krazy:exclude=strings
            return new KHttpDigestAuthentication();
        } else if (scheme == "ntlm") { // krazy:exclude=strings
            return new KHttpNtlmAuthentication(config);
        } else if (scheme == "basic") { // krazy:exclude=strings
            return new KHttpBasicAuthentication();
        }
    return nullptr;
}

QList< QByteArray > KAbstractHttpAuthentication::splitOffers(const QList< QByteArray > &offers)
{
    // first detect if one entry may contain multiple offers
    QList<QByteArray> alloffers;
    for (QByteArray offer : offers) {
        QByteArray scheme, cont;

        parseChallenge(offer, &scheme, &cont);

        while (!cont.isEmpty()) {
            offer.chop(cont.length());
            alloffers << offer;
            offer = cont;
            cont.clear();
            parseChallenge(offer, &scheme, &cont);
        }
        alloffers << offer;
    }
    return alloffers;
}

void KAbstractHttpAuthentication::reset()
{
    m_scheme.clear();
    m_challenge.clear();
    m_challengeText.clear();
    m_resource.clear();
    m_httpMethod.clear();
    m_isError = false;
    m_needCredentials = true;
    m_forceKeepAlive = false;
    m_forceDisconnect = false;
    m_keepPassword = false;
    m_headerFragment.clear();
    m_username.clear();
    m_password.clear();
}

void KAbstractHttpAuthentication::setChallenge(const QByteArray &c, const QUrl &resource,
        const QByteArray &httpMethod)
{
    reset();
    m_challengeText = c.trimmed();
    m_challenge = parseChallenge(m_challengeText, &m_scheme);
    Q_ASSERT(m_scheme.toLower() == scheme().toLower());
    m_resource = resource;
    m_httpMethod = httpMethod;
}

QString KAbstractHttpAuthentication::realm() const
{
    const QByteArray realm = valueForKey(m_challenge, "realm");
    // TODO: Find out what this is supposed to address. The site mentioned below does not exist.
    if (QLocale().uiLanguages().contains(QLatin1String("ru"))) {
        //for sites like lib.homelinux.org
        return QTextCodec::codecForName("CP1251")->toUnicode(realm);
    }
    return QString::fromLatin1(realm.constData(), realm.length());
}

void KAbstractHttpAuthentication::authInfoBoilerplate(KIO::AuthInfo *a) const
{
    a->url = m_resource;
    a->username = m_username;
    a->password = m_password;
    a->verifyPath = supportsPathMatching();
    a->realmValue = realm();
    a->digestInfo = QLatin1String(authDataToCache());
    a->keepPassword = m_keepPassword;
}

void KAbstractHttpAuthentication::generateResponseCommon(const QString &user, const QString &password)
{
    if (m_scheme.isEmpty() || m_httpMethod.isEmpty()) {
        m_isError = true;
        return;
    }

    if (m_needCredentials) {
        m_username = user;
        m_password = password;
    }

    m_isError = false;
    m_forceKeepAlive = false;
    m_forceDisconnect = false;
    m_finalAuthStage = true;
}

QByteArray KHttpBasicAuthentication::scheme() const
{
    return "Basic";
}

void KHttpBasicAuthentication::fillKioAuthInfo(KIO::AuthInfo *ai) const
{
    authInfoBoilerplate(ai);
}

void KHttpBasicAuthentication::generateResponse(const QString &user, const QString &password)
{
    generateResponseCommon(user, password);
    if (m_isError) {
        return;
    }

    m_headerFragment = "Basic ";
    m_headerFragment += QByteArray(m_username.toLatin1() + ':' + m_password.toLatin1()).toBase64();
    m_headerFragment += "\r\n";
}

QByteArray KHttpDigestAuthentication::scheme() const
{
    return "Digest";
}

void KHttpDigestAuthentication::setChallenge(const QByteArray &c, const QUrl &resource,
        const QByteArray &httpMethod)
{
    QString oldUsername;
    QString oldPassword;
    if (valueForKey(m_challenge, "stale").toLower() == "true") {
        // stale nonce: the auth failure that triggered this round of authentication is an artifact
        // of digest authentication. the credentials are probably still good, so keep them.
        oldUsername = m_username;
        oldPassword = m_password;
    }
    KAbstractHttpAuthentication::setChallenge(c, resource, httpMethod);
    if (!oldUsername.isEmpty() && !oldPassword.isEmpty()) {
        // keep credentials *and* don't ask for new ones
        m_needCredentials = false;
        m_username = oldUsername;
        m_password = oldPassword;
    }
}

void KHttpDigestAuthentication::fillKioAuthInfo(KIO::AuthInfo *ai) const
{
    authInfoBoilerplate(ai);
}

struct DigestAuthInfo {
    QByteArray nc;
    QByteArray qop;
    QByteArray realm;
    QByteArray nonce;
    QByteArray method;
    QByteArray cnonce;
    QByteArray username;
    QByteArray password;
    QList<QUrl> digestURIs;
    QByteArray algorithm;
    QByteArray entityBody;
};

//calculateResponse() from the original HTTPProtocol
static QByteArray calculateResponse(const DigestAuthInfo &info, const QUrl &resource)
{
    QCryptographicHash md(QCryptographicHash::Md5);
    QByteArray HA1;
    QByteArray HA2;

    // Calculate H(A1)
    QByteArray authStr = info.username;
    authStr += ':';
    authStr += info.realm;
    authStr += ':';
    authStr += info.password;
    md.addData(authStr);

    if (info.algorithm.toLower() == "md5-sess") {
        authStr = md.result().toHex();
        authStr += ':';
        authStr += info.nonce;
        authStr += ':';
        authStr += info.cnonce;
        md.reset();
        md.addData(authStr);
    }
    HA1 = md.result().toHex();

    // qDebug() << "A1 => " << HA1;

    // Calculate H(A2)
    authStr = info.method;
    authStr += ':';
    authStr += resource.path(QUrl::FullyEncoded).toLatin1();
    if (resource.hasQuery()) {
        authStr += '?' + resource.query(QUrl::FullyEncoded).toLatin1();
    }
    if (info.qop == "auth-int") {
        authStr += ':';
        md.reset();
        md.addData(info.entityBody);
        authStr += md.result().toHex();
    }
    md.reset();
    md.addData(authStr);
    HA2 = md.result().toHex();

    // qDebug() << "A2 => " << HA2;

    // Calculate the response.
    authStr = HA1;
    authStr += ':';
    authStr += info.nonce;
    authStr += ':';
    if (!info.qop.isEmpty()) {
        authStr += info.nc;
        authStr += ':';
        authStr += info.cnonce;
        authStr += ':';
        authStr += info.qop;
        authStr += ':';
    }
    authStr += HA2;
    md.reset();
    md.addData(authStr);

    const QByteArray response = md.result().toHex();
    // qDebug() << "Response =>" << response;
    return response;
}

void KHttpDigestAuthentication::generateResponse(const QString &user, const QString &password)
{
    generateResponseCommon(user, password);
    if (m_isError) {
        return;
    }

    // magic starts here (this part is slightly modified from the original in HTTPProtocol)

    DigestAuthInfo info;

    info.username = m_username.toLatin1();  //### charset breakage
    info.password = m_password.toLatin1();  //###

    // info.entityBody = p;  // FIXME: send digest of data for POST action ??
    info.realm = "";
    info.nonce = "";
    info.qop = "";

    // cnonce is recommended to contain about 64 bits of entropy
#ifdef ENABLE_HTTP_AUTH_NONCE_SETTER
    info.cnonce = m_nonce;
#else
    info.cnonce = KRandom::randomString(16).toLatin1();
#endif

    // HACK: Should be fixed according to RFC 2617 section 3.2.2
    info.nc = "00000001";

    // Set the method used...
    info.method = m_httpMethod;

    // Parse the Digest response....
    info.realm = valueForKey(m_challenge, "realm");

    info.algorithm = valueForKey(m_challenge, "algorithm");
    if (info.algorithm.isEmpty()) {
        info.algorithm = valueForKey(m_challenge, "algorith");
    }
    if (info.algorithm.isEmpty()) {
        info.algorithm = "MD5";
    }

    const QList<QByteArray> list = valueForKey(m_challenge, "domain").split(' ');
    for (const QByteArray &path : list) {
        QUrl u = m_resource.resolved(QUrl(QString::fromUtf8(path)));
        if (u.isValid()) {
            info.digestURIs.append(u);
        }
    }

    info.nonce = valueForKey(m_challenge, "nonce");
    QByteArray opaque = valueForKey(m_challenge, "opaque");
    info.qop = valueForKey(m_challenge, "qop");

    // NOTE: Since we do not have access to the entity body, we cannot support
    // the "auth-int" qop value ; so if the server returns a comma separated
    // list of qop values, prefer "auth".See RFC 2617 sec 3.2.2 for the details.
    // If "auth" is not present or it is set to "auth-int" only, then we simply
    // print a warning message and disregard the qop option altogether.
    if (info.qop.contains(',')) {
        const QList<QByteArray> values = info.qop.split(',');
        if (info.qop.contains("auth")) {
            info.qop = "auth";
        } else {
            qCWarning(KIO_HTTP_AUTH) << "Unsupported digest authentication qop parameters:" << values;
            info.qop.clear();
        }
    } else if (info.qop == "auth-int") {
        qCWarning(KIO_HTTP_AUTH) << "Unsupported digest authentication qop parameter:" << info.qop;
        info.qop.clear();
    }

    if (info.realm.isEmpty() || info.nonce.isEmpty()) {
        // ### proper error return
        m_isError = true;
        return;
    }

    // If the "domain" attribute was not specified and the current response code
    // is authentication needed, add the current request url to the list over which
    // this credential can be automatically applied.
    if (info.digestURIs.isEmpty() /*###&& (m_request.responseCode == 401 || m_request.responseCode == 407)*/) {
        info.digestURIs.append(m_resource);
    } else {
        // Verify whether or not we should send a cached credential to the
        // server based on the stored "domain" attribute...
        bool send = true;

        // Determine the path of the request url...
        QString requestPath = m_resource.adjusted(QUrl::RemoveFilename).path();
        if (requestPath.isEmpty()) {
            requestPath = QLatin1Char('/');
        }

        for (const QUrl &u : qAsConst(info.digestURIs)) {
            send &= (m_resource.scheme().toLower() == u.scheme().toLower());
            send &= (m_resource.host().toLower() == u.host().toLower());

            if (m_resource.port() > 0 && u.port() > 0) {
                send &= (m_resource.port() == u.port());
            }

            QString digestPath = u.adjusted(QUrl::RemoveFilename).path();
            if (digestPath.isEmpty()) {
                digestPath = QLatin1Char('/');
            }

            send &= (requestPath.startsWith(digestPath));

            if (send) {
                break;
            }
        }

        if (!send) {
            m_isError = true;
            return;
        }
    }

    // qDebug() << "RESULT OF PARSING:";
    // qDebug() << "  algorithm: " << info.algorithm;
    // qDebug() << "  realm:     " << info.realm;
    // qDebug() << "  nonce:     " << info.nonce;
    // qDebug() << "  opaque:    " << opaque;
    // qDebug() << "  qop:       " << info.qop;

    // Calculate the response...
    const QByteArray response = calculateResponse(info, m_resource);

    QByteArray auth = "Digest username=\"";
    auth += info.username;

    auth += "\", realm=\"";
    auth += info.realm;
    auth += "\"";

    auth += ", nonce=\"";
    auth += info.nonce;

    auth += "\", uri=\"";
    auth += m_resource.path(QUrl::FullyEncoded).toLatin1();
    if (m_resource.hasQuery()) {
        auth += '?' + m_resource.query(QUrl::FullyEncoded).toLatin1();
    }

    if (!info.algorithm.isEmpty()) {
        auth += "\", algorithm=";
        auth += info.algorithm;
    }

    if (!info.qop.isEmpty()) {
        auth += ", qop=";
        auth += info.qop;
        auth += ", cnonce=\"";
        auth += info.cnonce;
        auth += "\", nc=";
        auth += info.nc;
    }

    auth += ", response=\"";
    auth += response;
    if (!opaque.isEmpty()) {
        auth += "\", opaque=\"";
        auth += opaque;
    }
    auth += "\"\r\n";

    // magic ends here
    // note that auth already contains \r\n
    m_headerFragment = auth;
}

#ifdef ENABLE_HTTP_AUTH_NONCE_SETTER
void KHttpDigestAuthentication::setDigestNonceValue(const QByteArray &nonce)
{
    m_nonce = nonce;
}
#endif

QByteArray KHttpNtlmAuthentication::scheme() const
{
    return "NTLM";
}

void KHttpNtlmAuthentication::setChallenge(const QByteArray &c, const QUrl &resource,
        const QByteArray &httpMethod)
{
    QString oldUsername, oldPassword;
    if (!m_finalAuthStage && !m_username.isEmpty() && !m_password.isEmpty()) {
        oldUsername = m_username;
        oldPassword = m_password;
    }
    KAbstractHttpAuthentication::setChallenge(c, resource, httpMethod);
    if (!oldUsername.isEmpty() && !oldPassword.isEmpty()) {
        m_username = oldUsername;
        m_password = oldPassword;
    }
    // The type 1 message we're going to send needs no credentials;
    // they come later in the type 3 message.
    m_needCredentials = !m_challenge.isEmpty();
}

void KHttpNtlmAuthentication::fillKioAuthInfo(KIO::AuthInfo *ai) const
{
    authInfoBoilerplate(ai);
    // Every auth scheme is supposed to supply a realm according to the RFCs. Of course this doesn't
    // prevent Microsoft from not doing it... Dummy value!
    // we don't have the username yet which may (may!) contain a domain, so we really have no choice
    ai->realmValue = QStringLiteral("NTLM");
}

void KHttpNtlmAuthentication::generateResponse(const QString &_user, const QString &password)
{
    generateResponseCommon(_user, password);
    if (m_isError) {
        return;
    }

    QByteArray buf;

    if (m_challenge.isEmpty()) {
        m_finalAuthStage = false;
        // first, send type 1 message (with empty domain, workstation..., but it still works)
        switch (m_stage1State) {
        case Init:
            if (!KNTLM::getNegotiate(buf)) {
                qCWarning(KIO_HTTP_AUTH) << "Error while constructing Type 1 NTLMv1 authentication request";
                m_isError = true;
                return;
            }
            m_stage1State = SentNTLMv1;
            break;
        case SentNTLMv1:
            if (!KNTLM::getNegotiate(buf, QString(), QString(), KNTLM::Negotiate_NTLM2_Key
                                     | KNTLM::Negotiate_Always_Sign | KNTLM::Negotiate_Unicode
                                     | KNTLM::Request_Target | KNTLM::Negotiate_NTLM)) {
                qCWarning(KIO_HTTP_AUTH) << "Error while constructing Type 1 NTLMv2 authentication request";
                m_isError = true;
                return;
            }
            m_stage1State = SentNTLMv2;
            break;
        default:
            qCWarning(KIO_HTTP_AUTH) << "Error - Type 1 NTLM already sent - no Type 2 response received.";
            m_isError = true;
            return;
        }
    } else {
        m_finalAuthStage = true;
        // we've (hopefully) received a valid type 2 message: send type 3 message as last step
        QString user, domain;
        if (m_username.contains(QLatin1Char('\\'))) {
            domain = m_username.section(QLatin1Char('\\'), 0, 0);
            user = m_username.section(QLatin1Char('\\'), 1);
        } else {
            user = m_username;
        }

        m_forceKeepAlive = true;
        const QByteArray challenge = QByteArray::fromBase64(m_challenge[0]);

        KNTLM::AuthFlags flags = KNTLM::Add_LM;
        if ((!m_config || !m_config->readEntry("EnableNTLMv2Auth", false)) && (m_stage1State != SentNTLMv2)) {
            flags |= KNTLM::Force_V1;
        }

        if (!KNTLM::getAuth(buf, challenge, user, m_password, domain, QStringLiteral("WORKSTATION"), flags)) {
            qCWarning(KIO_HTTP_AUTH) << "Error while constructing Type 3 NTLM authentication request";
            m_isError = true;
            return;
        }
    }

    m_headerFragment = "NTLM " + buf.toBase64() + "\r\n";

    return;
}

//////////////////////////
#if HAVE_LIBGSSAPI

// just an error message formatter
static QByteArray gssError(int major_status, int minor_status)
{
    OM_uint32 new_status;
    OM_uint32 msg_ctx = 0;
    gss_buffer_desc major_string;
    gss_buffer_desc minor_string;
    OM_uint32 ret;
    QByteArray errorstr;

    do {
        ret = gss_display_status(&new_status, major_status, GSS_C_GSS_CODE, GSS_C_NULL_OID, &msg_ctx, &major_string);
        errorstr += (const char *)major_string.value;
        errorstr += ' ';
        ret = gss_display_status(&new_status, minor_status, GSS_C_MECH_CODE, GSS_C_NULL_OID, &msg_ctx, &minor_string);
        errorstr += (const char *)minor_string.value;
        errorstr += ' ';
    } while (!GSS_ERROR(ret) && msg_ctx != 0);

    return errorstr;
}

QByteArray KHttpNegotiateAuthentication::scheme() const
{
    return "Negotiate";
}

void KHttpNegotiateAuthentication::setChallenge(const QByteArray &c, const QUrl &resource,
        const QByteArray &httpMethod)
{
    KAbstractHttpAuthentication::setChallenge(c, resource, httpMethod);
    // GSSAPI knows how to get the credentials on its own
    m_needCredentials = false;
}

void KHttpNegotiateAuthentication::fillKioAuthInfo(KIO::AuthInfo *ai) const
{
    authInfoBoilerplate(ai);
    //### does GSSAPI supply anything realm-like? dummy value for now.
    ai->realmValue = QStringLiteral("Negotiate");
}

void KHttpNegotiateAuthentication::generateResponse(const QString &user, const QString &password)
{
    generateResponseCommon(user, password);
    if (m_isError) {
        return;
    }

    OM_uint32 major_status, minor_status;
    gss_buffer_desc input_token = GSS_C_EMPTY_BUFFER;
    gss_buffer_desc output_token = GSS_C_EMPTY_BUFFER;
    gss_name_t server;
    gss_ctx_id_t ctx;
    gss_OID mech_oid;
    static gss_OID_desc krb5_oid_desc = {9, (void *) "\x2a\x86\x48\x86\xf7\x12\x01\x02\x02"};
    static gss_OID_desc spnego_oid_desc = {6, (void *) "\x2b\x06\x01\x05\x05\x02"};
    gss_OID_set mech_set;
    gss_OID tmp_oid;

    ctx = GSS_C_NO_CONTEXT;
    mech_oid = &krb5_oid_desc;

    // see whether we can use the SPNEGO mechanism
    major_status = gss_indicate_mechs(&minor_status, &mech_set);
    if (GSS_ERROR(major_status)) {
        qCDebug(KIO_HTTP_AUTH) << "gss_indicate_mechs failed:" << gssError(major_status, minor_status);
    } else {
        for (uint i = 0; i < mech_set->count; i++) {
            tmp_oid = &mech_set->elements[i];
            if (tmp_oid->length == spnego_oid_desc.length &&
                    !memcmp(tmp_oid->elements, spnego_oid_desc.elements, tmp_oid->length)) {
                // qDebug() << "found SPNEGO mech";
                mech_oid = &spnego_oid_desc;
                break;
            }
        }
        gss_release_oid_set(&minor_status, &mech_set);
    }

    // the service name is "HTTP/f.q.d.n"
    QByteArray servicename = "HTTP@";
    servicename += m_resource.host().toLatin1();

    input_token.value = (void *)servicename.data();
    input_token.length = servicename.length() + 1;

    major_status = gss_import_name(&minor_status, &input_token,
                                   GSS_C_NT_HOSTBASED_SERVICE, &server);

    input_token.value = nullptr;
    input_token.length = 0;

    if (GSS_ERROR(major_status)) {
        qCDebug(KIO_HTTP_AUTH) << "gss_import_name failed:" << gssError(major_status, minor_status);
        m_isError = true;
        return;
    }

    OM_uint32 req_flags;
    if (m_config && m_config->readEntry("DelegateCredentialsOn", false)) {
        req_flags = GSS_C_DELEG_FLAG;
    } else {
        req_flags = 0;
    }

    // GSSAPI knows how to get the credentials its own way, so don't ask for any
    major_status = gss_init_sec_context(&minor_status, GSS_C_NO_CREDENTIAL,
                                        &ctx, server, mech_oid,
                                        req_flags, GSS_C_INDEFINITE,
                                        GSS_C_NO_CHANNEL_BINDINGS,
                                        GSS_C_NO_BUFFER, nullptr, &output_token,
                                        nullptr, nullptr);

    if (GSS_ERROR(major_status) || (output_token.length == 0)) {
        qCDebug(KIO_HTTP_AUTH) << "gss_init_sec_context failed:" << gssError(major_status, minor_status);
        gss_release_name(&minor_status, &server);
        if (ctx != GSS_C_NO_CONTEXT) {
            gss_delete_sec_context(&minor_status, &ctx, GSS_C_NO_BUFFER);
            ctx = GSS_C_NO_CONTEXT;
        }
        m_isError = true;
        return;
    }

    m_headerFragment = "Negotiate ";
    m_headerFragment += QByteArray::fromRawData(static_cast<const char *>(output_token.value),
                        output_token.length).toBase64();
    m_headerFragment += "\r\n";

    // free everything
    gss_release_name(&minor_status, &server);
    if (ctx != GSS_C_NO_CONTEXT) {
        gss_delete_sec_context(&minor_status, &ctx, GSS_C_NO_BUFFER);
        ctx = GSS_C_NO_CONTEXT;
    }
    gss_release_buffer(&minor_status, &output_token);
}

#endif // HAVE_LIBGSSAPI
