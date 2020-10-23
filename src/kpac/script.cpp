/*
    SPDX-FileCopyrightText: 2003 Malte Starostik <malte@kde.org>
    SPDX-FileCopyrightText: 2011 Dawit Alemayehu <adawit@kde.org>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#include "script.h"

#include <QRegularExpression>
#include <QDateTime>
#include <QUrl>

#include <QHostInfo>
#include <QHostAddress>
#include <QNetworkInterface>

#include <QJSEngine>
#include <QJSValue>
#include <QJSValueIterator>

#include <KLocalizedString>
#include <kio/hostinfo.h>

#define QL1S(x)    QLatin1String(x)

namespace
{
static int findString(const QString &s, const char *const *values)
{
    int index = 0;
    for (const char *const *p = values; *p; ++p, ++index) {
        if (s.compare(QLatin1String(*p), Qt::CaseInsensitive) == 0) {
            return index;
        }
    }
    return -1;
}

static const QDateTime getTime(QString tz)
{
    if (tz.compare(QLatin1String("gmt"), Qt::CaseInsensitive) == 0) {
        return QDateTime::currentDateTimeUtc();
    }
    return QDateTime::currentDateTime();
}

template <typename T>
static bool checkRange(T value, T min, T max)
{
    return ((min <= max && value >= min && value <= max) ||
            (min > max && (value <= min || value >= max)));
}

static bool isLocalHostAddress(const QHostAddress &address)
{
    if (address == QHostAddress::LocalHost) {
        return true;
    }

    if (address == QHostAddress::LocalHostIPv6) {
        return true;
    }

    return false;
}

static bool isIPv6Address(const QHostAddress &address)
{
    return address.protocol() == QAbstractSocket::IPv6Protocol;
}

static bool isIPv4Address(const QHostAddress &address)
{
    return (address.protocol() == QAbstractSocket::IPv4Protocol);
}

static bool isSpecialAddress(const QHostAddress &address)
{
    // Catch all the special addresses and return false.
    if (address == QHostAddress::Null) {
        return true;
    }

    if (address == QHostAddress::Any) {
        return true;
    }

    if (address == QHostAddress::AnyIPv6) {
        return true;
    }

    if (address == QHostAddress::Broadcast) {
        return true;
    }

    return false;
}

static bool addressLessThanComparison(const QHostAddress &addr1,  const QHostAddress &addr2)
{
    if (addr1.protocol() == QAbstractSocket::IPv4Protocol &&
            addr2.protocol() == QAbstractSocket::IPv4Protocol) {
        return addr1.toIPv4Address() < addr2.toIPv4Address();
    }

    if (addr1.protocol() == QAbstractSocket::IPv6Protocol &&
            addr2.protocol() == QAbstractSocket::IPv6Protocol) {
        const Q_IPV6ADDR ipv6addr1 = addr1.toIPv6Address();
        const Q_IPV6ADDR ipv6addr2 = addr2.toIPv6Address();
        for (int i = 0; i < 16; ++i) {
            if (ipv6addr1[i] != ipv6addr2[i]) {
                return ((ipv6addr1[i] & 0xff) - (ipv6addr2[i] & 0xff));
            }
        }
    }

    return false;
}

static QString addressListToString(const QList<QHostAddress> &addressList,
                                   const QHash<QString, QString> &actualEntryMap)
{
    QString result;
    for (const QHostAddress &address : addressList) {
        if (!result.isEmpty()) {
            result += QLatin1Char(';');
        }
        result += actualEntryMap.value(address.toString());
    }
    return result;
}

class Address
{
public:
    struct Error {};
    static Address resolve(const QString &host)
    {
        return Address(host);
    }

    const QList<QHostAddress> &addresses() const
    {
        return m_addressList;
    }

    QHostAddress address() const
    {
        if (m_addressList.isEmpty()) {
            return QHostAddress();
        }

        return m_addressList.first();
    }

private:
    Address(const QString &host)
    {
        // Always try to see if it's already an IP first, to avoid Qt doing a
        // needless reverse lookup
        QHostAddress address(host);
        if (address.isNull()) {
            QHostInfo hostInfo = KIO::HostInfo::lookupCachedHostInfoFor(host);
            if (hostInfo.hostName().isEmpty() || hostInfo.error() != QHostInfo::NoError) {
                hostInfo = QHostInfo::fromName(host);
                KIO::HostInfo::cacheLookup(hostInfo);
            }
            m_addressList = hostInfo.addresses();
        } else {
            m_addressList.clear();
            m_addressList.append(address);
        }
    }

    QList<QHostAddress> m_addressList;
};

class ScriptHelper : public QObject {
    Q_OBJECT
    QJSEngine *m_engine;
public:
    ScriptHelper(QJSEngine *engine, QObject *parent) : QObject(parent), m_engine(engine) { }

// isPlainHostName(host)
// @returns true if @p host doesn't contains a domain part
Q_INVOKABLE QJSValue IsPlainHostName(QString string)
{
    return QJSValue(string.indexOf(QLatin1Char('.')) == -1);
}

// dnsDomainIs(host, domain)
// @returns true if the domain part of @p host matches @p domain
Q_INVOKABLE QJSValue DNSDomainIs(QString host, QString domain)
{
    return QJSValue(host.endsWith(domain, Qt::CaseInsensitive));
}

// localHostOrDomainIs(host, fqdn)
// @returns true if @p host is unqualified or equals @p fqdn
Q_INVOKABLE QJSValue LocalHostOrDomainIs(QString host, QString fqdn)
{
    if (!host.contains(QLatin1Char('.'))) {
        return QJSValue(true);
    }
    return QJSValue(host.compare(fqdn, Qt::CaseInsensitive) == 0);
}

// isResolvable(host)
// @returns true if host is resolvable to a IPv4 address.
Q_INVOKABLE QJSValue IsResolvable(QString host)
{
    try {
        const Address info = Address::resolve(host);
        bool hasResolvableIPv4Address = false;
        for (const QHostAddress &address : info.addresses()) {
            if (!isSpecialAddress(address) && isIPv4Address(address)) {
                hasResolvableIPv4Address = true;
                break;
            }
        }

        return QJSValue(hasResolvableIPv4Address);
    } catch (const Address::Error &) {
        return QJSValue(false);
    }
}

// isInNet(host, subnet, mask)
// @returns true if the IPv4 address of host is within the specified subnet
// and mask, false otherwise.
Q_INVOKABLE QJSValue IsInNet(QString host, QString subnet, QString mask)
{
    try {
        const Address info = Address::resolve(host);
        bool isInSubNet = false;
        const QString subnetStr = subnet + QLatin1Char('/') + mask;
        const QPair<QHostAddress, int> subnet = QHostAddress::parseSubnet(subnetStr);
        for (const QHostAddress &address : info.addresses()) {
            if (!isSpecialAddress(address) && isIPv4Address(address) && address.isInSubnet(subnet)) {
                isInSubNet = true;
                break;
            }
        }
        return QJSValue(isInSubNet);
    } catch (const Address::Error &) {
        return QJSValue(false);
    }
}

// dnsResolve(host)
// @returns the IPv4 address for host or an empty string if host is not resolvable.
Q_INVOKABLE QJSValue DNSResolve(QString host)
{
    try {
        const Address info = Address::resolve(host);
        QString resolvedAddress(QLatin1String(""));
        for (const QHostAddress &address : info.addresses()) {
            if (!isSpecialAddress(address) && isIPv4Address(address)) {
                resolvedAddress = address.toString();
                break;
            }
        }
        return QJSValue(resolvedAddress);
    } catch (const Address::Error &) {
        return QJSValue(QString(QLatin1String("")));
    }
}

// myIpAddress()
// @returns the local machine's IPv4 address. Note that this will return
// the address for the first interfaces that match its criteria even if the
// machine has multiple interfaces.
Q_INVOKABLE QJSValue MyIpAddress()
{
    QString ipAddress;
    const QList<QHostAddress> addresses = QNetworkInterface::allAddresses();
    for (const QHostAddress& address : addresses) {
        if (isIPv4Address(address) && !isSpecialAddress(address) && !isLocalHostAddress(address)) {
            ipAddress = address.toString();
            break;
        }
    }

    return QJSValue(ipAddress);
}

// dnsDomainLevels(host)
// @returns the number of dots ('.') in @p host
Q_INVOKABLE QJSValue DNSDomainLevels(QString host)
{
    if (host.isNull()) {
        return QJSValue(0);
    }

    return QJSValue(host.count(QLatin1Char('.')));
}

// shExpMatch(str, pattern)
// @returns true if @p str matches the shell @p pattern
Q_INVOKABLE QJSValue ShExpMatch(QString str, QString patternStr)
{
    const QRegularExpression pattern(QRegularExpression::wildcardToRegularExpression(patternStr));
    return QJSValue(pattern.match(str).hasMatch());
}

// weekdayRange(day [, "GMT" ])
// weekdayRange(day1, day2 [, "GMT" ])
// @returns true if the current day equals day or between day1 and day2 resp.
// If the last argument is "GMT", GMT timezone is used, otherwise local time
Q_INVOKABLE QJSValue WeekdayRange(QString day1, QString arg2 = QString(), QString tz = QString())
{
    static const char *const days[] = { "sun", "mon", "tue", "wed", "thu", "fri", "sat", nullptr };

    const int d1 = findString(day1, days);
    if (d1 == -1) {
        return QJSValue::UndefinedValue;
    }

    int d2 = findString(arg2, days);
    if (d2 == -1) {
        d2 = d1;
        tz = arg2;
    }

    // Adjust the days of week coming from QDateTime since it starts
    // counting with Monday as 1 and ends with Sunday as day 7.
    int dayOfWeek = getTime(tz).date().dayOfWeek();
    if (dayOfWeek == 7) {
        dayOfWeek = 0;
    }
    return QJSValue(checkRange(dayOfWeek, d1, d2));
}

// dateRange(day [, "GMT" ])
// dateRange(day1, day2 [, "GMT" ])
// dateRange(month [, "GMT" ])
// dateRange(month1, month2 [, "GMT" ])
// dateRange(year [, "GMT" ])
// dateRange(year1, year2 [, "GMT" ])
// dateRange(day1, month1, day2, month2 [, "GMT" ])
// dateRange(month1, year1, month2, year2 [, "GMT" ])
// dateRange(day1, month1, year1, day2, month2, year2 [, "GMT" ])
// @returns true if the current date (GMT or local time according to
// presence of "GMT" as last argument) is within the given range
Q_INVOKABLE QJSValue DateRangeInternal(QJSValue args)
{
    static const char *const months[] = { "jan", "feb", "mar", "apr", "may", "jun",
                                          "jul", "aug", "sep", "oct", "nov", "dec", nullptr
                                        };
    QVector<int> values;
    QJSValueIterator it(args);
    QString tz;
    bool onlySeenNumbers = true;
    int initialNumbers = 0;
    while (it.next()) {
        int value = -1;
        if (it.value().isNumber()) {
            value = it.value().toInt();
            if (onlySeenNumbers)
                initialNumbers++;
        } else {
            onlySeenNumbers = false;
            // QDate starts counting months from 1, so we add 1 here.
            value = findString(it.value().toString(), months) + 1;
            if (value <= 0)
                tz = it.value().toString();
        }

        if (value > 0) {
            values.append(value);
        } else {
            break;
        }
    }
    // Our variable args calling indirection means argument.length was appended
    if (values.count() == initialNumbers)
        --initialNumbers;
    values.resize(values.size() - 1);

    if (values.count() < 1 || values.count() > 7)
        return QJSValue::UndefinedValue;

    const QDate now = getTime(tz).date();

    // day1, month1, year1, day2, month2, year2
    if (values.size() == 6) {
        const QDate d1(values[2], values[1], values[0]);
        const QDate d2(values[5], values[4], values[3]);
        return QJSValue(checkRange(now, d1, d2));
    }
    // day1, month1, day2, month2
    else if (values.size() == 4 && values[ 1 ] < 13 && values[ 3 ] < 13) {
        const QDate d1(now.year(), values[1], values[0]);
        const QDate d2(now.year(), values[3], values[2]);
        return QJSValue(checkRange(now, d1, d2));
    }
    // month1, year1, month2, year2
    else if (values.size() == 4) {
        const QDate d1(values[1], values[0], now.day());
        const QDate d2(values[3], values[2], now.day());
        return QJSValue(checkRange(now, d1, d2));
    }
    // year1, year2
    else if (values.size() == 2 && values[0] >= 1000 && values[1] >= 1000) {
        return QJSValue(checkRange(now.year(), values[0], values[1]));
    }
    // day1, day2
    else if (values.size() == 2 && initialNumbers == 2) {
        return QJSValue(checkRange(now.day(), values[0], values[1]));
    }
    // month1, month2
    else if (values.size() == 2) {
        return QJSValue(checkRange(now.month(), values[0], values[1]));
    }
    // year
    else if (values.size() == 1 && values[ 0 ] >= 1000) {
        return QJSValue(checkRange(now.year(), values[0], values[0]));
    }
    // day
    else if (values.size() == 1 && initialNumbers == 1) {
        return QJSValue(checkRange(now.day(), values[0], values[0]));
    }
    // month
    else if (values.size() == 1) {
        return QJSValue(checkRange(now.month(), values[0], values[0]));
    }

    return QJSValue::UndefinedValue;
}

// timeRange(hour [, "GMT" ])
// timeRange(hour1, hour2 [, "GMT" ])
// timeRange(hour1, min1, hour2, min2 [, "GMT" ])
// timeRange(hour1, min1, sec1, hour2, min2, sec2 [, "GMT" ])
// @returns true if the current time (GMT or local based on presence
// of "GMT" argument) is within the given range
Q_INVOKABLE QJSValue TimeRange(int hour, QString tz = QString())
{
    const QTime now = getTime(tz).time();
    return m_engine->toScriptValue(checkRange(now.hour(), hour, hour));
}

Q_INVOKABLE QJSValue TimeRange(int hour1, int hour2, QString tz = QString())
{
    const QTime now = getTime(tz).time();
    return m_engine->toScriptValue(checkRange(now.hour(), hour1, hour2));
}

Q_INVOKABLE QJSValue TimeRange(int hour1, int min1, int hour2, int min2, QString tz = QString())
{
    const QTime now = getTime(tz).time();
    const QTime t1(hour1, min1);
    const QTime t2(hour2, min2);
    return m_engine->toScriptValue(checkRange(now, t1, t2));
}

Q_INVOKABLE QJSValue TimeRange(int hour1, int min1, int sec1, int hour2, int min2, int sec2, QString tz = QString())
{
    const QTime now = getTime(tz).time();

    const QTime t1(hour1, min1, sec1);
    const QTime t2(hour2, min2, sec2);
    return m_engine->toScriptValue(checkRange(now, t1, t2));
}

/*
 * Implementation of Microsoft's IPv6 Extension for PAC
 *
 * Documentation:
 * http://msdn.microsoft.com/en-us/library/gg308477(v=vs.85).aspx
 * http://msdn.microsoft.com/en-us/library/gg308478(v=vs.85).aspx
 * http://msdn.microsoft.com/en-us/library/gg308474(v=vs.85).aspx
 * http://blogs.msdn.com/b/wndp/archive/2006/07/13/ipv6-pac-extensions-v0-9.aspx
 */

// isResolvableEx(host)
// @returns true if host is resolvable to an IPv4 or IPv6 address.
Q_INVOKABLE QJSValue IsResolvableEx(QString host)
{
    try {
        const Address info = Address::resolve(host);
        bool hasResolvableIPAddress = false;
        for (const QHostAddress &address : info.addresses()) {
            if (isIPv4Address(address) || isIPv6Address(address)) {
                hasResolvableIPAddress = true;
                break;
            }
        }
        return QJSValue(hasResolvableIPAddress);
    } catch (const Address::Error &) {
        return QJSValue(false);
    }
}

// isInNetEx(ipAddress, ipPrefix )
// @returns true if ipAddress is within the specified ipPrefix.
Q_INVOKABLE QJSValue IsInNetEx(QString ipAddress, QString ipPrefix)
{
    try {
        const Address info = Address::resolve(ipAddress);
        bool isInSubNet = false;
        const QString subnetStr = ipPrefix;
        const QPair<QHostAddress, int> subnet = QHostAddress::parseSubnet(subnetStr);
        for (const QHostAddress &address : info.addresses()) {
            if (isSpecialAddress(address)) {
                continue;
            }

            if (address.isInSubnet(subnet)) {
                isInSubNet = true;
                break;
            }
        }
        return QJSValue(isInSubNet);
    } catch (const Address::Error &) {
        return QJSValue(false);
    }
}

// dnsResolveEx(host)
// @returns a semi-colon delimited string containing IPv6 and IPv4 addresses
// for host or an empty string if host is not resolvable.
Q_INVOKABLE QJSValue DNSResolveEx(QString host)
{
    try {
        const Address info = Address::resolve(host);

        QStringList addressList;
        QString resolvedAddress(QLatin1String(""));
        for (const QHostAddress &address : info.addresses()) {
            if (!isSpecialAddress(address)) {
                addressList << address.toString();
            }
        }
        if (!addressList.isEmpty()) {
            resolvedAddress = addressList.join(QLatin1Char(';'));
        }

        return QJSValue(resolvedAddress);
    } catch (const Address::Error &) {
        return QJSValue(QString(QLatin1String("")));
    }
}

// myIpAddressEx()
// @returns a semi-colon delimited string containing all IP addresses for localhost (IPv6 and/or IPv4),
// or an empty string if unable to resolve localhost to an IP address.
Q_INVOKABLE QJSValue MyIpAddressEx()
{
    QStringList ipAddressList;
    const QList<QHostAddress> addresses = QNetworkInterface::allAddresses();
    for (const QHostAddress& address : addresses) {
        if (!isSpecialAddress(address) && !isLocalHostAddress(address)) {
            ipAddressList << address.toString();
        }
    }

    return m_engine->toScriptValue(ipAddressList.join(QLatin1Char(';')));
}

// sortIpAddressList(ipAddressList)
// @returns a sorted ipAddressList. If both IPv4 and IPv6 addresses are present in
// the list. The sorted IPv6 addresses will precede the sorted IPv4 addresses.
Q_INVOKABLE QJSValue SortIpAddressList(QString ipAddressListStr)
{
    QHash<QString, QString> actualEntryMap;
    QList<QHostAddress> ipV4List, ipV6List;
    const QStringList ipAddressList = ipAddressListStr.split(QLatin1Char(';'));

    for (const QString &ipAddress : ipAddressList) {
        QHostAddress address(ipAddress);
        switch (address.protocol()) {
        case QAbstractSocket::IPv4Protocol:
            ipV4List << address;
            actualEntryMap.insert(address.toString(), ipAddress);
            break;
        case QAbstractSocket::IPv6Protocol:
            ipV6List << address;
            actualEntryMap.insert(address.toString(), ipAddress);
            break;
        default:
            break;
        }
    }

    QString sortedAddress(QLatin1String(""));

    if (!ipV6List.isEmpty()) {
        std::sort(ipV6List.begin(), ipV6List.end(), addressLessThanComparison);
        sortedAddress += addressListToString(ipV6List, actualEntryMap);
    }

    if (!ipV4List.isEmpty()) {
        std::sort(ipV4List.begin(), ipV4List.end(), addressLessThanComparison);
        if (!sortedAddress.isEmpty()) {
            sortedAddress += QLatin1Char(';');
        }
        sortedAddress += addressListToString(ipV4List, actualEntryMap);
    }

    return QJSValue(sortedAddress);

}

// getClientVersion
// @return the version number of this engine for future extension. We too start
// this at version 1.0.
Q_INVOKABLE QJSValue GetClientVersion()
{
    const QString version(QStringLiteral("1.0"));
    return QJSValue(version);
}
}; // class ScriptHelper

void registerFunctions(QJSEngine *engine)
{
    QJSValue value = engine->globalObject();
    auto scriptHelper = new ScriptHelper(engine, engine);
    QJSValue functions = engine->newQObject(scriptHelper);

    value.setProperty(QStringLiteral("isPlainHostName"), functions.property(QStringLiteral("IsPlainHostName")));
    value.setProperty(QStringLiteral("dnsDomainIs"), functions.property(QStringLiteral("DNSDomainIs")));
    value.setProperty(QStringLiteral("localHostOrDomainIs"), functions.property(QStringLiteral("LocalHostOrDomainIs")));
    value.setProperty(QStringLiteral("isResolvable"), functions.property(QStringLiteral("IsResolvable")));
    value.setProperty(QStringLiteral("isInNet"), functions.property(QStringLiteral("IsInNet")));
    value.setProperty(QStringLiteral("dnsResolve"), functions.property(QStringLiteral("DNSResolve")));
    value.setProperty(QStringLiteral("myIpAddress"), functions.property(QStringLiteral("MyIpAddress")));
    value.setProperty(QStringLiteral("dnsDomainLevels"), functions.property(QStringLiteral("DNSDomainLevels")));
    value.setProperty(QStringLiteral("shExpMatch"), functions.property(QStringLiteral("ShExpMatch")));
    value.setProperty(QStringLiteral("weekdayRange"), functions.property(QStringLiteral("WeekdayRange")));
    value.setProperty(QStringLiteral("timeRange"), functions.property(QStringLiteral("TimeRange")));
    value.setProperty(QStringLiteral("dateRangeInternal"), functions.property(QStringLiteral("DateRangeInternal")));
    engine->evaluate(QStringLiteral("dateRange = function() { return dateRangeInternal(Array.prototype.slice.call(arguments)); };"));

    // Microsoft's IPv6 PAC Extensions...
    value.setProperty(QStringLiteral("isResolvableEx"), functions.property(QStringLiteral("IsResolvableEx")));
    value.setProperty(QStringLiteral("isInNetEx"), functions.property(QStringLiteral("IsInNetEx")));
    value.setProperty(QStringLiteral("dnsResolveEx"), functions.property(QStringLiteral("DNSResolveEx")));
    value.setProperty(QStringLiteral("myIpAddressEx"), functions.property(QStringLiteral("MyIpAddressEx")));
    value.setProperty(QStringLiteral("sortIpAddressList"), functions.property(QStringLiteral("SortIpAddressList")));
    value.setProperty(QStringLiteral("getClientVersion"), functions.property(QStringLiteral("GetClientVersion")));
}
} // namespace

namespace KPAC
{
Script::Script(const QString &code)
{
    m_engine = new QJSEngine;
    registerFunctions(m_engine);

    const QJSValue result = m_engine->evaluate(code);
    if (result.isError()) {
        throw Error(result.toString());
    }
}

Script::~Script()
{
    delete m_engine;
}

QString Script::evaluate(const QUrl &url)
{
    QJSValue func = m_engine->globalObject().property(QStringLiteral("FindProxyForURL"));

    if (!func.isCallable()) {
        func = m_engine->globalObject().property(QStringLiteral("FindProxyForURLEx"));
        if (!func.isCallable()) {
            throw Error(i18n("Could not find 'FindProxyForURL' or 'FindProxyForURLEx'"));
            return QString();
        }
    }

    QUrl cleanUrl = url;
    cleanUrl.setUserInfo(QString());
    if (cleanUrl.scheme() == QLatin1String("https")) {
        cleanUrl.setPath(QString());
        cleanUrl.setQuery(QString());
    }

    QJSValueList args;
    args << cleanUrl.url();
    args << cleanUrl.host();

    QJSValue result = func.call(args);
    if (result.isError()) {
        throw Error(i18n("Got an invalid reply when calling %1 -> %2", func.toString(), result.toString()));
    }

    return result.toString();
}
} // namespace KPAC

#include "script.moc"
