/*
    SPDX-FileCopyrightText: 2003 Malte Starostik <malte@kde.org>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#include <config-kpac.h>
//#include <config-prefix.h>
#include <netdb.h>
#include <unistd.h>

#if HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#if HAVE_NETINET_IN_H
#include <netinet/in.h>
#endif
#include <arpa/nameser.h>
#if HAVE_ARPA_NAMESER8_COMPAT_H
#include <arpa/nameser8_compat.h>
#else
#if HAVE_ARPA_NAMESER_COMPAT_H
#include <arpa/nameser_compat.h>
#endif
#endif
#if HAVE_SYS_PARAM_H
// Basically, the BSDs need this before resolv.h
#include <sys/param.h>
#endif

#include <resolv.h>
#include <sys/utsname.h>

#include <QTimer>
#include <QProcess>
#include <QHostInfo>
#include <QStandardPaths>
#include <QUrl>

#include <KLocalizedString>
#include "moc_discovery.cpp"

namespace KPAC
{
Discovery::Discovery(QObject *parent)
    : Downloader(parent),
      m_helper(new QProcess(this))
{
    m_helper->setProcessChannelMode(QProcess::SeparateChannels);
    connect(m_helper, &QProcess::readyReadStandardOutput, this, &Discovery::helperOutput);
    connect(m_helper, QOverload<int,QProcess::ExitStatus>::of(&QProcess::finished), this, &Discovery::failed);
    m_helper->start(QStringLiteral(KDE_INSTALL_FULL_LIBEXECDIR_KF5 "/kpac_dhcp_helper"), QStringList());
    if (!m_helper->waitForStarted()) {
        QTimer::singleShot(0, this, &Discovery::failed);
    }
}

bool Discovery::initDomainName()
{
    m_domainName = QHostInfo::localDomainName();
    return !m_domainName.isEmpty();
}

bool Discovery::checkDomain() const
{
    // If a domain has a SOA record, don't traverse any higher.
    // Returns true if no SOA can be found (domain is "ok" to use)
    // Stick to old resolver interface for portability reasons.
    union {
        HEADER header;
        unsigned char buf[ PACKETSZ ];
    } response;

    int len = res_query(m_domainName.toLocal8Bit().constData(), C_IN, T_SOA,
                        response.buf, sizeof(response.buf));
    if (len <= int(sizeof(response.header)) ||
            ntohs(response.header.ancount) != 1) {
        return true;
    }

    unsigned char *pos = response.buf + sizeof(response.header);
    unsigned char *end = response.buf + len;

    // skip query section
    pos += dn_skipname(pos, end) + QFIXEDSZ;
    if (pos >= end) {
        return true;
    }

    // skip answer domain
    pos += dn_skipname(pos, end);
    short type;
    GETSHORT(type, pos);
    return type != T_SOA;
}

void Discovery::failed()
{
    setError(i18n("Could not find a usable proxy configuration script"));

    // If this is the first DNS query, initialize our host name or abort
    // on failure. Otherwise abort if the current domain (which was already
    // queried for a host called "wpad" contains a SOA record)
    const bool firstQuery = m_domainName.isEmpty();
    if ((firstQuery && !initDomainName()) ||
            (!firstQuery && !checkDomain())) {
        Q_EMIT result(false);
        return;
    }

    const int dot = m_domainName.indexOf(QLatin1Char('.'));
    if (dot > -1 || firstQuery) {
        const QString address = QLatin1String("http://wpad.") + m_domainName + QLatin1String("/wpad.dat");
        if (dot > -1) {
            m_domainName.remove(0, dot + 1);    // remove one domain level
        }
        download(QUrl(address));
        return;
    }

    Q_EMIT result(false);
}

void Discovery::helperOutput()
{
    m_helper->disconnect(this);
    const QByteArray line = m_helper->readLine();
    const QUrl url(QString::fromLocal8Bit(line.constData(), line.length()).trimmed());
    download(url);
}
}

