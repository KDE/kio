/*
   Copyright (c) 2003 Malte Starostik <malte@kde.org>
   Copyright (c) 2011 Dawit Alemayehu <adawit@kde.org>

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

#include "script.h"

#include <QtCore/QString>
#include <QtCore/QRegExp>
#include <QtCore/QDateTime>
#include <QtCore/QTimer>
#include <QtCore/QEventLoop>
#include <QUrl>

#include <QtNetwork/QHostInfo>
#include <QtNetwork/QHostAddress>
#include <QtNetwork/QNetworkInterface>

#include <QtScript/QScriptValue>
#include <QtScript/QScriptEngine>
#include <QtScript/QScriptProgram>
#include <QtScript/QScriptContextInfo>

#include <klocalizedstring.h>
#include <kio/hostinfo.h>

#define QL1S(x)    QLatin1String(x)

namespace
{
    static int findString (const QString& s, const char* const* values)
    {
        int index = 0;
        const QString lower = s.toLower();
        for (const char* const* p = values; *p; ++p, ++index) {
            if (s.compare(QLatin1String(*p), Qt::CaseInsensitive) == 0) {
              return index;
            }
        }
        return -1;
    }

    static const QDateTime getTime (QScriptContext* context)
    {
        const QString tz = context->argument(context->argumentCount() - 1).toString();
        if (tz.compare(QLatin1String("gmt"), Qt::CaseInsensitive) == 0) {
            return QDateTime::currentDateTimeUtc();
        }
        return QDateTime::currentDateTime();
    }

    template <typename T>
    static bool checkRange (T value, T min, T max)
    {
        return ((min <= max && value >= min && value <= max) ||
                (min > max && (value <= min || value >= max)));
    }

    static bool isLocalHostAddress (const QHostAddress& address)
    {
        if (address == QHostAddress::LocalHost)
            return true;

        if (address == QHostAddress::LocalHostIPv6)
            return true;

        return false;
    }

    static bool isIPv6Address (const QHostAddress& address)
    {
        return address.protocol() == QAbstractSocket::IPv6Protocol;
    }

    static bool isIPv4Address (const QHostAddress& address)
    {
        return (address.protocol() == QAbstractSocket::IPv4Protocol);
    }

    static bool isSpecialAddress(const QHostAddress& address)
    {
        // Catch all the special addresses and return false.
        if (address == QHostAddress::Null)
            return true;

        if (address == QHostAddress::Any)
            return true;

        if (address == QHostAddress::AnyIPv6)
            return true;

        if (address == QHostAddress::Broadcast)
            return true;

        return false;
    }

    static bool addressLessThanComparison(const QHostAddress& addr1,  const QHostAddress& addr2)
    {
        if (addr1.protocol() == QAbstractSocket::IPv4Protocol &&
            addr2.protocol() == QAbstractSocket::IPv4Protocol) {
            return addr1.toIPv4Address() < addr2.toIPv4Address();
        }

        if (addr1.protocol() == QAbstractSocket::IPv6Protocol &&
            addr2.protocol() == QAbstractSocket::IPv6Protocol) {
            const Q_IPV6ADDR ipv6addr1 = addr1.toIPv6Address();
            const Q_IPV6ADDR ipv6addr2 = addr2.toIPv6Address();
            for (int i=0; i < 16; ++i) {
                if (ipv6addr1[i] != ipv6addr2[i]) {
                    return ((ipv6addr1[i] & 0xff) - (ipv6addr2[i] & 0xff));
                }
            }
        }

        return false;
    }

    static QString addressListToString(const QList<QHostAddress>& addressList,
                                       const QHash<QString, QString>& actualEntryMap)
    {
        QString result;
        Q_FOREACH(const QHostAddress& address, addressList) {
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
        static Address resolve( const QString& host )
        {
            return Address( host );
        }

        QList<QHostAddress> addresses() const
        {
            return m_addressList;
        }

        QHostAddress address() const
        {
           if (m_addressList.isEmpty())
              return QHostAddress();

           return m_addressList.first();
        }

    private:
        Address( const QString& host )
        {
            // Always try to see if it's already an IP first, to avoid Qt doing a
            // needless reverse lookup
            QHostAddress address ( host );
            if ( address.isNull() ) {
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


    // isPlainHostName(host)
    // @returns true if @p host doesn't contains a domain part
    QScriptValue IsPlainHostName(QScriptContext* context, QScriptEngine* engine)
    {
        if (context->argumentCount() != 1) {
            return engine->undefinedValue();
        }
        return engine->toScriptValue(context->argument(0).toString().indexOf(QLatin1Char('.')) == -1);
    }

    // dnsDomainIs(host, domain)
    // @returns true if the domain part of @p host matches @p domain
    QScriptValue DNSDomainIs (QScriptContext* context, QScriptEngine* engine)
    {
        if (context->argumentCount() != 2) {
            return engine->undefinedValue();
        }

        const QString host = context->argument(0).toString();
        const QString domain = context->argument(1).toString();
        return engine->toScriptValue(host.endsWith(domain, Qt::CaseInsensitive));
    }

    // localHostOrDomainIs(host, fqdn)
    // @returns true if @p host is unqualified or equals @p fqdn
    QScriptValue LocalHostOrDomainIs (QScriptContext* context, QScriptEngine* engine)
    {
        if (context->argumentCount() != 2) {
            return engine->undefinedValue();
        }

        const QString host = context->argument(0).toString();
        if (!host.contains(QLatin1Char('.'))) {
            return engine->toScriptValue(true);
        }
        const QString fqdn = context->argument(1).toString();
        return engine->toScriptValue((host.compare(fqdn, Qt::CaseInsensitive) == 0));
    }

    // isResolvable(host)
    // @returns true if host is resolvable to a IPv4 address.
    QScriptValue IsResolvable (QScriptContext* context, QScriptEngine* engine)
    {
        if (context->argumentCount() != 1) {
            return engine->undefinedValue();
        }

        try {
            const Address info = Address::resolve(context->argument(0).toString());
            bool hasResolvableIPv4Address = false;

            Q_FOREACH(const QHostAddress& address, info.addresses()) {
                if (!isSpecialAddress(address) && isIPv4Address(address)) {
                    hasResolvableIPv4Address = true;
                    break;
                }
            }

            return engine->toScriptValue(hasResolvableIPv4Address);
        }
        catch (const Address::Error&) {
            return engine->toScriptValue(false);
        }
    }

    // isInNet(host, subnet, mask)
    // @returns true if the IPv4 address of host is within the specified subnet
    // and mask, false otherwise.
    QScriptValue IsInNet (QScriptContext* context, QScriptEngine* engine)
    {
        if (context->argumentCount() != 3) {
            return engine->undefinedValue();
        }

        try {
            const Address info = Address::resolve(context->argument(0).toString());
            bool isInSubNet = false;
            QString subnetStr = context->argument(1).toString();
            subnetStr += QLatin1Char('/');
            subnetStr += context->argument(2).toString();
            const QPair<QHostAddress, int> subnet = QHostAddress::parseSubnet(subnetStr);
            Q_FOREACH(const QHostAddress& address, info.addresses()) {
                if (!isSpecialAddress(address) && isIPv4Address(address) && address.isInSubnet(subnet)) {
                    isInSubNet = true;
                    break;
                }
            }
            return engine->toScriptValue(isInSubNet);
        }
        catch (const Address::Error&) {
            return engine->toScriptValue(false);
        }
    }

    // dnsResolve(host)
    // @returns the IPv4 address for host or an empty string if host is not resolvable.
    QScriptValue DNSResolve (QScriptContext* context, QScriptEngine* engine)
    {
        if (context->argumentCount() != 1) {
            return engine->undefinedValue();
        }

        try {
            const Address info = Address::resolve(context->argument(0).toString());
            QString resolvedAddress (QLatin1String(""));
            Q_FOREACH(const QHostAddress& address, info.addresses()) {
                if (!isSpecialAddress(address) && isIPv4Address(address)) {
                    resolvedAddress = address.toString();
                    break;
                }
            }
            return engine->toScriptValue(resolvedAddress);
        }
        catch (const Address::Error&) {
            return engine->toScriptValue(QString(QLatin1String("")));
        }
    }

    // myIpAddress()
    // @returns the local machine's IPv4 address. Note that this will return
    // the address for the first interfaces that match its criteria even if the
    // machine has multiple interfaces.
    QScriptValue MyIpAddress (QScriptContext* context, QScriptEngine* engine)
    {
        if (context->argumentCount()) {
            return engine->undefinedValue();
        }

        QString ipAddress;
        const QList<QHostAddress> addresses = QNetworkInterface::allAddresses();
        Q_FOREACH(const QHostAddress address, addresses) {
            if (isIPv4Address(address) && !isSpecialAddress(address) && !isLocalHostAddress(address)) {
                ipAddress = address.toString();
                break;
            }
        }

        return engine->toScriptValue(ipAddress);
    }

    // dnsDomainLevels(host)
    // @returns the number of dots ('.') in @p host
    QScriptValue DNSDomainLevels (QScriptContext* context, QScriptEngine* engine)
    {
        if (context->argumentCount() != 1) {
            return engine->undefinedValue();
        }

        const QString host = context->argument(0).toString();
        if (host.isNull()) {
            return engine->toScriptValue(0);
        }

        return engine->toScriptValue(host.count(QLatin1Char('.')));
    }

    // shExpMatch(str, pattern)
    // @returns true if @p str matches the shell @p pattern
    QScriptValue ShExpMatch (QScriptContext* context, QScriptEngine* engine)
    {
        if (context->argumentCount() != 2) {
            return engine->undefinedValue();
        }

        QRegExp pattern(context->argument(1).toString(), Qt::CaseSensitive, QRegExp::Wildcard);
        return engine->toScriptValue(pattern.exactMatch(context->argument(0).toString()));
    }

    // weekdayRange(day [, "GMT" ])
    // weekdayRange(day1, day2 [, "GMT" ])
    // @returns true if the current day equals day or between day1 and day2 resp.
    // If the last argument is "GMT", GMT timezone is used, otherwise local time
    QScriptValue WeekdayRange (QScriptContext* context, QScriptEngine* engine)
    {
        if (context->argumentCount() < 1 || context->argumentCount() > 3) {
            return engine->undefinedValue();
        }

        static const char* const days[] = { "sun", "mon", "tue", "wed", "thu", "fri", "sat", 0 };

        const int d1 = findString(context->argument(0).toString(), days);
        if (d1 == -1) {
            return engine->undefinedValue();
        }

        int d2 = findString(context->argument(1).toString(), days);
        if (d2 == -1) {
            d2 = d1;
        }

        // Adjust the days of week coming from QDateTime since it starts
        // counting with Monday as 1 and ends with Sunday as day 7.
        int dayOfWeek = getTime(context).date().dayOfWeek();
        if (dayOfWeek == 7) {
            dayOfWeek = 0;
        }
        return engine->toScriptValue(checkRange(dayOfWeek, d1, d2));
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
    QScriptValue DateRange (QScriptContext* context, QScriptEngine* engine)
    {
        if (context->argumentCount() < 1 || context->argumentCount() > 7) {
            return engine->undefinedValue();
        }

        static const char* const months[] = { "jan", "feb", "mar", "apr", "may", "jun",
                                              "jul", "aug", "sep", "oct", "nov", "dec", 0 };

        QVector<int> values;
        for (int i = 0; i < context->argumentCount(); ++i)
        {
            int value = -1;
            if (context->argument(i).isNumber()) {
                value = context->argument(i).toInt32();
            } else {
                // QDate starts counting months from 1, so we add 1 here.
                value = findString(context->argument(i).toString(), months) + 1;
            }

            if (value > 0) {
                values.append(value);
            } else {
                break;
            }
        }

        const QDate now = getTime(context).date();

        // day1, month1, year1, day2, month2, year2
        if (values.size() == 6) {
            const QDate d1 (values[2], values[1], values[0]);
            const QDate d2 (values[5], values[4], values[3]);
            return engine->toScriptValue(checkRange(now, d1, d2));
        }
        // day1, month1, day2, month2
        else if (values.size() == 4 && values[ 1 ] < 13 && values[ 3 ] < 13) {
            const QDate d1 (now.year(), values[1], values[0]);
            const QDate d2 (now.year(), values[3], values[2]);
            return engine->toScriptValue(checkRange(now, d1, d2));
        }
        // month1, year1, month2, year2
        else if (values.size() == 4) {
            const QDate d1 (values[1], values[0], now.day());
            const QDate d2 (values[3], values[2], now.day());
            return engine->toScriptValue(checkRange(now, d1, d2));
        }
        // year1, year2
        else if (values.size() == 2 && values[0] >= 1000 && values[1] >= 1000) {
            return engine->toScriptValue(checkRange(now.year(), values[0], values[1]));
        }
        // day1, day2
        else if (values.size() == 2 && context->argument(0).isNumber() && context->argument(1).isNumber()) {
            return engine->toScriptValue(checkRange(now.day(), values[0], values[1]));
        }
        // month1, month2
        else if (values.size() == 2) {
            return engine->toScriptValue(checkRange(now.month(), values[0], values[1]));
        }
        // year
        else if (values.size() == 1 && values[ 0 ] >= 1000) {
            return engine->toScriptValue(checkRange(now.year(), values[0], values[0]));
        }
        // day
        else if (values.size() == 1 && context->argument(0).isNumber()) {
            return engine->toScriptValue(checkRange(now.day(), values[0], values[0]));
        }
        // month
        else if (values.size() == 1) {
            return engine->toScriptValue(checkRange(now.month(), values[0], values[0]));
        }

        return engine->undefinedValue();
    }

    // timeRange(hour [, "GMT" ])
    // timeRange(hour1, hour2 [, "GMT" ])
    // timeRange(hour1, min1, hour2, min2 [, "GMT" ])
    // timeRange(hour1, min1, sec1, hour2, min2, sec2 [, "GMT" ])
    // @returns true if the current time (GMT or local based on presence
    // of "GMT" argument) is within the given range
    QScriptValue TimeRange (QScriptContext* context, QScriptEngine* engine)
    {
        if (context->argumentCount() < 1 || context->argumentCount() > 7) {
            return engine->undefinedValue();
        }

        QVector<int> values;
        for (int i = 0; i < context->argumentCount(); ++i) {
            if (!context->argument(i).isNumber()) {
                break;
            }
            values.append(context->argument(i).toNumber());
        }

        const QTime now = getTime(context).time();

        // hour1, min1, sec1, hour2, min2, sec2
        if (values.size() == 6) {
            const QTime t1 (values[0], values[1], values[2]);
            const QTime t2 (values[3], values[4], values[5]);
            return engine->toScriptValue(checkRange(now, t1, t2));
        }
        // hour1, min1, hour2, min2
        else if (values.size() == 4) {
            const QTime t1 (values[0], values[1]);
            const QTime t2 (values[2], values[3]);
            return engine->toScriptValue(checkRange(now, t1, t2));
        }
        // hour1, hour2
        else if (values.size() == 2) {
            return engine->toScriptValue(checkRange(now.hour(), values[0], values[1]));
        }
        // hour
        else if (values.size() == 1) {
            return engine->toScriptValue(checkRange(now.hour(), values[0], values[0]));
        }

        return engine->undefinedValue();
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
    QScriptValue IsResolvableEx (QScriptContext* context, QScriptEngine* engine)
    {
        if (context->argumentCount() != 1) {
            return engine->undefinedValue();
        }

        try {
            const Address info = Address::resolve(context->argument(0).toString());
            bool hasResolvableIPAddress = false;
            Q_FOREACH(const QHostAddress& address, info.addresses()) {
                if (isIPv4Address(address) || isIPv6Address(address)) {
                    hasResolvableIPAddress = true;
                    break;
                }
            }
            return engine->toScriptValue(hasResolvableIPAddress);
        }
        catch (const Address::Error&) {
            return engine->toScriptValue(false);
        }
    }

    // isInNetEx(ipAddress, ipPrefix )
    // @returns true if ipAddress is within the specified ipPrefix.
    QScriptValue IsInNetEx (QScriptContext* context, QScriptEngine* engine)
    {
        if (context->argumentCount() != 2) {
            return engine->undefinedValue();
        }

        try {
            const Address info = Address::resolve(context->argument(0).toString());
            bool isInSubNet = false;
            const QString subnetStr = context->argument(1).toString();
            const QPair<QHostAddress, int> subnet = QHostAddress::parseSubnet(subnetStr);

            Q_FOREACH(const QHostAddress& address, info.addresses()) {
                if (isSpecialAddress(address)) {
                    continue;
                }

                if (address.isInSubnet(subnet)) {
                    isInSubNet = true;
                    break;
                }
            }
            return engine->toScriptValue(isInSubNet);
        }
        catch (const Address::Error&) {
            return engine->toScriptValue(false);
        }
    }

    // dnsResolveEx(host)
    // @returns a semi-colon delimited string containing IPv6 and IPv4 addresses
    // for host or an empty string if host is not resolvable.
    QScriptValue DNSResolveEx (QScriptContext* context, QScriptEngine* engine)
    {
        if (context->argumentCount() != 1) {
            return engine->undefinedValue();
        }

        try {
            const Address info = Address::resolve (context->argument(0).toString());

            QStringList addressList;
            QString resolvedAddress (QLatin1String(""));

            Q_FOREACH(const QHostAddress& address, info.addresses()) {
                if (!isSpecialAddress(address)) {
                    addressList << address.toString();
                }
            }
            if (!addressList.isEmpty()) {
                resolvedAddress = addressList.join(QLatin1String(";"));
            }

            return engine->toScriptValue(resolvedAddress);
        }
        catch (const Address::Error&) {
            return engine->toScriptValue(QString(QLatin1String("")));
        }
    }

    // myIpAddressEx()
    // @returns a semi-colon delimited string containing all IP addresses for localhost (IPv6 and/or IPv4),
    // or an empty string if unable to resolve localhost to an IP address.
    QScriptValue MyIpAddressEx (QScriptContext* context, QScriptEngine* engine)
    {
        if (context->argumentCount()) {
            return engine->undefinedValue();
        }

        QStringList ipAddressList;
        const QList<QHostAddress> addresses = QNetworkInterface::allAddresses();
        Q_FOREACH(const QHostAddress address, addresses) {
            if (!isSpecialAddress(address) && !isLocalHostAddress(address)) {
                ipAddressList << address.toString();
            }
        }

        return engine->toScriptValue(ipAddressList.join(QLatin1String(";")));
    }

    // sortIpAddressList(ipAddressList)
    // @returns a sorted ipAddressList. If both IPv4 and IPv6 addresses are present in
    // the list. The sorted IPv6 addresses will precede the sorted IPv4 addresses.
    QScriptValue SortIpAddressList(QScriptContext* context, QScriptEngine* engine)
    {
        if (context->argumentCount() != 1) {
           return engine->undefinedValue();
        }

        QHash<QString, QString> actualEntryMap;
        QList<QHostAddress> ipV4List, ipV6List;
        const QStringList ipAddressList = context->argument(0).toString().split(QLatin1Char(';'));

        Q_FOREACH(const QString& ipAddress, ipAddressList) {
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

        QString sortedAddress (QLatin1String(""));

        if (!ipV6List.isEmpty()) {
            qSort(ipV6List.begin(), ipV6List.end(), addressLessThanComparison);
            sortedAddress += addressListToString(ipV6List, actualEntryMap);
        }

        if (!ipV4List.isEmpty()) {
            qSort(ipV4List.begin(), ipV4List.end(), addressLessThanComparison);
            if (!sortedAddress.isEmpty()) {
                sortedAddress += QLatin1Char(';');
            }
            sortedAddress += addressListToString(ipV4List, actualEntryMap);
        }

        return engine->toScriptValue(sortedAddress);

    }

    // getClientVersion
    // @return the version number of this engine for future extension. We too start
    // this at version 1.0.
    QScriptValue GetClientVersion (QScriptContext* context, QScriptEngine* engine)
    {
        if (context->argumentCount()) {
            return engine->undefinedValue();
        }

        const QString version (QLatin1String("1.0"));
        return engine->toScriptValue(version);
    }

    void registerFunctions(QScriptEngine* engine)
    {
        QScriptValue value = engine->globalObject();
        value.setProperty(QL1S("isPlainHostName"), engine->newFunction(IsPlainHostName));
        value.setProperty(QL1S("dnsDomainIs"), engine->newFunction(DNSDomainIs));
        value.setProperty(QL1S("localHostOrDomainIs"), engine->newFunction(LocalHostOrDomainIs));
        value.setProperty(QL1S("isResolvable"), engine->newFunction(IsResolvable));
        value.setProperty(QL1S("isInNet"), engine->newFunction(IsInNet));
        value.setProperty(QL1S("dnsResolve"), engine->newFunction(DNSResolve));
        value.setProperty(QL1S("myIpAddress"), engine->newFunction(MyIpAddress));
        value.setProperty(QL1S("dnsDomainLevels"), engine->newFunction(DNSDomainLevels));
        value.setProperty(QL1S("shExpMatch"), engine->newFunction(ShExpMatch));
        value.setProperty(QL1S("weekdayRange"), engine->newFunction(WeekdayRange));
        value.setProperty(QL1S("dateRange"), engine->newFunction(DateRange));
        value.setProperty(QL1S("timeRange"), engine->newFunction(TimeRange));

        // Microsoft's IPv6 PAC Extensions...
        value.setProperty(QL1S("isResolvableEx"), engine->newFunction(IsResolvableEx));
        value.setProperty(QL1S("isInNetEx"), engine->newFunction(IsInNetEx));
        value.setProperty(QL1S("dnsResolveEx"), engine->newFunction(DNSResolveEx));
        value.setProperty(QL1S("myIpAddressEx"), engine->newFunction(MyIpAddressEx));
        value.setProperty(QL1S("sortIpAddressList"), engine->newFunction(SortIpAddressList));
        value.setProperty(QL1S("getClientVersion"), engine->newFunction(GetClientVersion));
    }
}

namespace KPAC
{
    Script::Script(const QString& code)
    {
        m_engine = new QScriptEngine;
        registerFunctions(m_engine);

        QScriptProgram program (code);
        const QScriptValue result = m_engine->evaluate(program);
        if (m_engine->hasUncaughtException() || result.isError())
            throw Error(m_engine->uncaughtException().toString());
    }

    Script::~Script()
    {
        delete m_engine;
    }

    QString Script::evaluate(const QUrl& url)
    {
        QScriptValue func = m_engine->globalObject().property(QL1S("FindProxyForURL"));

        if (!func.isValid()) {
            func = m_engine->globalObject().property(QL1S("FindProxyForURLEx"));
            if (!func.isValid()) {
                throw Error(i18n("Could not find 'FindProxyForURL' or 'FindProxyForURLEx'"));
                return QString();
            }
        }

        QScriptValueList args;
        args << url.url();
        args << url.host();

        QScriptValue result = func.call(QScriptValue(), args);
        if (result.isError()) {
            throw Error(i18n("Got an invalid reply when calling %1", func.toString()));
        }

        return result.toString();
    }
}

// vim: ts=4 sw=4 et
