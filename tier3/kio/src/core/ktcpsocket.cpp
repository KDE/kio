/* This file is part of the KDE libraries
    Copyright (C) 2007, 2008 Andreas Hartmetz <ahartmetz@gmail.com>

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

#include "ktcpsocket.h"
#include "ktcpsocket_p.h"

#include <ksslcertificatemanager.h>
#include <klocalizedstring.h>

#include <QDebug>
#include <QUrl>
#include <QtCore/QStringList>
#include <QtNetwork/QSslKey>
#include <QtNetwork/QSslCipher>
#include <QtNetwork/QHostAddress>
#include <QtNetwork/QNetworkProxy>

static KTcpSocket::SslVersion kSslVersionFromQ(QSsl::SslProtocol protocol)
{
    switch (protocol) {
    case QSsl::SslV2:
        return KTcpSocket::SslV2;
    case QSsl::SslV3:
        return KTcpSocket::SslV3;
    case QSsl::TlsV1_0:
        return KTcpSocket::TlsV1;
    case QSsl::AnyProtocol:
        return KTcpSocket::AnySslVersion;
#if QT_VERSION >= 0x040800
    case QSsl::TlsV1SslV3:
        return KTcpSocket::TlsV1SslV3;
    case QSsl::SecureProtocols:
        return KTcpSocket::SecureProtocols;
#endif
    default:
        return KTcpSocket::UnknownSslVersion;
    }
}


static QSsl::SslProtocol qSslProtocolFromK(KTcpSocket::SslVersion sslVersion)
{
    //### this lowlevel bit-banging is a little dangerous and a likely source of bugs
    if (sslVersion == KTcpSocket::AnySslVersion) {
        return QSsl::AnyProtocol;
    }
    //does it contain any valid protocol?
    KTcpSocket::SslVersions validVersions (KTcpSocket::SslV2 | KTcpSocket::SslV3 | KTcpSocket::TlsV1);
#if QT_VERSION >= 0x040800
    validVersions |= KTcpSocket::TlsV1SslV3;
    validVersions |= KTcpSocket::SecureProtocols;
#endif
    if (!(sslVersion & validVersions)) {
        return QSsl::UnknownProtocol;
    }

    switch (sslVersion) {
    case KTcpSocket::SslV2:
        return QSsl::SslV2;
    case KTcpSocket::SslV3:
        return QSsl::SslV3;
    case KTcpSocket::TlsV1:
        return QSsl::TlsV1_0;
#if QT_VERSION >= 0x040800
    case KTcpSocket::TlsV1SslV3:
        return QSsl::TlsV1SslV3;
    case KTcpSocket::SecureProtocols:
        return QSsl::SecureProtocols;
#endif

    default:
        //QSslSocket doesn't really take arbitrary combinations. It's one or all.
        return QSsl::AnyProtocol;
    }
}


//cipher class converter KSslCipher -> QSslCipher
class CipherCc
{
public:
    CipherCc()
    {
        foreach (const QSslCipher &c, QSslSocket::supportedCiphers()) {
            allCiphers.insert(c.name(), c);
        }
    }

    QSslCipher converted(const KSslCipher &ksc)
    {
        return allCiphers.value(ksc.name());
    }

private:
    QHash<QString, QSslCipher> allCiphers;
};


class KSslErrorPrivate
{
public:
    static KSslError::Error errorFromQSslError(QSslError::SslError e)
    {
        switch (e) {
        case QSslError::NoError:
            return KSslError::NoError;
        case QSslError::UnableToGetLocalIssuerCertificate:
        case QSslError::InvalidCaCertificate:
            return KSslError::InvalidCertificateAuthorityCertificate;
        case QSslError::InvalidNotBeforeField:
        case QSslError::InvalidNotAfterField:
        case QSslError::CertificateNotYetValid:
        case QSslError::CertificateExpired:
            return KSslError::ExpiredCertificate;
        case QSslError::UnableToDecodeIssuerPublicKey:
        case QSslError::SubjectIssuerMismatch:
        case QSslError::AuthorityIssuerSerialNumberMismatch:
            return KSslError::InvalidCertificate;
        case QSslError::SelfSignedCertificate:
        case QSslError::SelfSignedCertificateInChain:
            return KSslError::SelfSignedCertificate;
        case QSslError::CertificateRevoked:
            return KSslError::RevokedCertificate;
        case QSslError::InvalidPurpose:
            return KSslError::InvalidCertificatePurpose;
        case QSslError::CertificateUntrusted:
            return KSslError::UntrustedCertificate;
        case QSslError::CertificateRejected:
            return KSslError::RejectedCertificate;
        case QSslError::NoPeerCertificate:
            return KSslError::NoPeerCertificate;
        case QSslError::HostNameMismatch:
            return KSslError::HostNameMismatch;
        case QSslError::UnableToVerifyFirstCertificate:
        case QSslError::UnableToDecryptCertificateSignature:
        case QSslError::UnableToGetIssuerCertificate:
        case QSslError::CertificateSignatureFailed:
            return KSslError::CertificateSignatureFailed;
        case QSslError::PathLengthExceeded:
            return KSslError::PathLengthExceeded;
        case QSslError::UnspecifiedError:
        case QSslError::NoSslSupport:
        default:
            return KSslError::UnknownError;
        }
    }

    static QString errorString(KSslError::Error e)
    {
        switch (e) {
        case KSslError::NoError:
            return i18nc("SSL error","No error");
        case KSslError::InvalidCertificateAuthorityCertificate:
            return i18nc("SSL error","The certificate authority's certificate is invalid");
        case KSslError::ExpiredCertificate:
            return i18nc("SSL error","The certificate has expired");
        case KSslError::InvalidCertificate:
            return i18nc("SSL error","The certificate is invalid");
        case KSslError::SelfSignedCertificate:
            return i18nc("SSL error","The certificate is not signed by any trusted certificate authority");
        case KSslError::RevokedCertificate:
            return i18nc("SSL error","The certificate has been revoked");
        case KSslError::InvalidCertificatePurpose:
            return i18nc("SSL error","The certificate is unsuitable for this purpose");
        case KSslError::UntrustedCertificate:
            return i18nc("SSL error","The root certificate authority's certificate is not trusted for this purpose");
        case KSslError::RejectedCertificate:
            return i18nc("SSL error","The certificate authority's certificate is marked to reject this certificate's purpose");
        case KSslError::NoPeerCertificate:
            return i18nc("SSL error","The peer did not present any certificate");
        case KSslError::HostNameMismatch:
            return i18nc("SSL error","The certificate does not apply to the given host");
        case KSslError::CertificateSignatureFailed:
            return i18nc("SSL error","The certificate cannot be verified for internal reasons");
        case KSslError::PathLengthExceeded:
            return i18nc("SSL error","The certificate chain is too long");
        case KSslError::UnknownError:
        default:
            return i18nc("SSL error","Unknown error");
        }
    }

    KSslError::Error error;
    QSslCertificate certificate;
};


KSslError::KSslError(Error errorCode, const QSslCertificate &certificate)
 : d(new KSslErrorPrivate())
{
    d->error = errorCode;
    d->certificate = certificate;
}


KSslError::KSslError(const QSslError &other)
 : d(new KSslErrorPrivate())
{
    d->error = KSslErrorPrivate::errorFromQSslError(other.error());
    d->certificate = other.certificate();
}


KSslError::KSslError(const KSslError &other)
 : d(new KSslErrorPrivate())
{
    *d = *other.d;
}


KSslError::~KSslError()
{
    delete d;
}


KSslError &KSslError::operator=(const KSslError &other)
{
    *d = *other.d;
    return *this;
}


KSslError::Error KSslError::error() const
{
    return d->error;
}


QString KSslError::errorString() const
{
    return KSslErrorPrivate::errorString(d->error);
}


QSslCertificate KSslError::certificate() const
{
    return d->certificate;
}


class KTcpSocketPrivate
{
public:
    KTcpSocketPrivate(KTcpSocket *qq)
     : q(qq),
       certificatesLoaded(false),
       emittedReadyRead(false)
    {
        // create the instance, which sets Qt's static internal cert set to empty.
        KSslCertificateManager::self();
    }

    KTcpSocket::State state(QAbstractSocket::SocketState s)
    {
        switch (s) {
        case QAbstractSocket::UnconnectedState:
            return KTcpSocket::UnconnectedState;
        case QAbstractSocket::HostLookupState:
            return KTcpSocket::HostLookupState;
        case QAbstractSocket::ConnectingState:
            return KTcpSocket::ConnectingState;
        case QAbstractSocket::ConnectedState:
            return KTcpSocket::ConnectedState;
        case QAbstractSocket::ClosingState:
            return KTcpSocket::ClosingState;
        case QAbstractSocket::BoundState:
        case QAbstractSocket::ListeningState:
            //### these two are not relevant as long as this can't be a server socket
        default:
            return KTcpSocket::UnconnectedState; //the closest to "error"
        }
    }

    KTcpSocket::EncryptionMode encryptionMode(QSslSocket::SslMode mode)
    {
        switch (mode) {
        case QSslSocket::SslClientMode:
            return KTcpSocket::SslClientMode;
        case QSslSocket::SslServerMode:
            return KTcpSocket::SslServerMode;
        default:
            return KTcpSocket::UnencryptedMode;
        }
    }

    KTcpSocket::Error errorFromAbsSocket(QAbstractSocket::SocketError e)
    {
        switch (e) {
        case QAbstractSocket::ConnectionRefusedError:
            return KTcpSocket::ConnectionRefusedError;
        case QAbstractSocket::RemoteHostClosedError:
            return KTcpSocket::RemoteHostClosedError;
        case QAbstractSocket::HostNotFoundError:
            return KTcpSocket::HostNotFoundError;
        case QAbstractSocket::SocketAccessError:
            return KTcpSocket::SocketAccessError;
        case QAbstractSocket::SocketResourceError:
            return KTcpSocket::SocketResourceError;
        case QAbstractSocket::SocketTimeoutError:
            return KTcpSocket::SocketTimeoutError;
        case QAbstractSocket::NetworkError:
            return KTcpSocket::NetworkError;
        case QAbstractSocket::UnsupportedSocketOperationError:
            return KTcpSocket::UnsupportedSocketOperationError;
        case QAbstractSocket::SslHandshakeFailedError:
            return KTcpSocket::SslHandshakeFailedError;
        case QAbstractSocket::DatagramTooLargeError:
            //we don't do UDP
        case QAbstractSocket::AddressInUseError:
        case QAbstractSocket::SocketAddressNotAvailableError:
            //### own values if/when we ever get server socket support
        case QAbstractSocket::ProxyAuthenticationRequiredError:
            //### maybe we need an enum value for this
        case QAbstractSocket::UnknownSocketError:
        default:
            return KTcpSocket::UnknownError;
        }
    }

    //private slots
    void reemitSocketError(QAbstractSocket::SocketError e)
    {
        emit q->error(errorFromAbsSocket(e));
    }

    void reemitSslErrors(const QList<QSslError> &errors)
    {
        q->showSslErrors(); //H4X
        QList<KSslError> kErrors;
        foreach (const QSslError &e, errors) {
            kErrors.append(KSslError(e));
        }
        emit q->sslErrors(kErrors);
    }

    void reemitStateChanged(QAbstractSocket::SocketState s)
    {
        emit q->stateChanged(state(s));
    }

    void reemitModeChanged(QSslSocket::SslMode m)
    {
        emit q->encryptionModeChanged(encryptionMode(m));
    }

    // This method is needed because we might emit readyRead() due to this QIODevice
    // having some data buffered, so we need to care about blocking, too.
    //### useless ATM as readyRead() now just calls d->sock.readyRead().
    void reemitReadyRead()
    {
        if (!emittedReadyRead) {
            emittedReadyRead = true;
            emit q->readyRead();
            emittedReadyRead = false;
        }
    }

    void maybeLoadCertificates()
    {
        if (!certificatesLoaded) {
            sock.setCaCertificates(KSslCertificateManager::self()->caCertificates());
            certificatesLoaded = true;
        }
    }

    KTcpSocket *const q;
    bool certificatesLoaded;
    bool emittedReadyRead;
    QSslSocket sock;
    QList<KSslCipher> ciphers;
    KTcpSocket::SslVersion advertisedSslVersion;
    CipherCc ccc;
};


KTcpSocket::KTcpSocket(QObject *parent)
 : QIODevice(parent),
   d(new KTcpSocketPrivate(this))
{
    d->advertisedSslVersion = SslV3;

    connect(&d->sock, SIGNAL(aboutToClose()), this, SIGNAL(aboutToClose()));
    connect(&d->sock, SIGNAL(bytesWritten(qint64)), this, SIGNAL(bytesWritten(qint64)));
    connect(&d->sock, SIGNAL(encryptedBytesWritten(qint64)), this, SIGNAL(encryptedBytesWritten(qint64)));
    connect(&d->sock, SIGNAL(readyRead()), this, SLOT(reemitReadyRead()));
    connect(&d->sock, SIGNAL(connected()), this, SIGNAL(connected()));
    connect(&d->sock, SIGNAL(encrypted()), this, SIGNAL(encrypted()));
    connect(&d->sock, SIGNAL(disconnected()), this, SIGNAL(disconnected()));
#ifndef QT_NO_NETWORKPROXY
    connect(&d->sock, SIGNAL(proxyAuthenticationRequired(QNetworkProxy,QAuthenticator*)),
            this, SIGNAL(proxyAuthenticationRequired(QNetworkProxy,QAuthenticator*)));
#endif
    connect(&d->sock, SIGNAL(error(QAbstractSocket::SocketError)),
            this, SLOT(reemitSocketError(QAbstractSocket::SocketError)));
    connect(&d->sock, SIGNAL(sslErrors(QList<QSslError>)),
            this, SLOT(reemitSslErrors(QList<QSslError>)));
    connect(&d->sock, SIGNAL(hostFound()), this, SIGNAL(hostFound()));
    connect(&d->sock, SIGNAL(stateChanged(QAbstractSocket::SocketState)),
            this, SLOT(reemitStateChanged(QAbstractSocket::SocketState)));
    connect(&d->sock, SIGNAL(modeChanged(QSslSocket::SslMode)),
            this, SLOT(reemitModeChanged(QSslSocket::SslMode)));
}


KTcpSocket::~KTcpSocket()
{
    delete d;
}

////////////////////////////// (mostly) virtuals from QIODevice

bool KTcpSocket::atEnd() const
{
    return d->sock.atEnd() && QIODevice::atEnd();
}


qint64 KTcpSocket::bytesAvailable() const
{
    return d->sock.bytesAvailable() + QIODevice::bytesAvailable();
}


qint64 KTcpSocket::bytesToWrite() const
{
    return d->sock.bytesToWrite();
}


bool KTcpSocket::canReadLine() const
{
    return d->sock.canReadLine() || QIODevice::canReadLine();
}


void KTcpSocket::close()
{
    d->sock.close();
    QIODevice::close();
}


bool KTcpSocket::isSequential() const
{
    return true;
}


bool KTcpSocket::open(QIODevice::OpenMode open)
{
    bool ret = d->sock.open(open);
    setOpenMode(d->sock.openMode() | QIODevice::Unbuffered);
    return ret;
}


bool KTcpSocket::waitForBytesWritten(int msecs)
{
    return d->sock.waitForBytesWritten(msecs);
}


bool KTcpSocket::waitForReadyRead(int msecs)
{
    return d->sock.waitForReadyRead(msecs);
}


qint64 KTcpSocket::readData(char *data, qint64 maxSize)
{
    return d->sock.read(data, maxSize);
}


qint64 KTcpSocket::writeData(const char *data, qint64 maxSize)
{
    return d->sock.write(data, maxSize);
}

////////////////////////////// public methods from QAbstractSocket

void KTcpSocket::abort()
{
    d->sock.abort();
}


void KTcpSocket::connectToHost(const QString &hostName, quint16 port, ProxyPolicy policy)
{
    if (policy == AutoProxy) {
        //###
    }
    d->sock.connectToHost(hostName, port);
    // there are enough layers of buffers between us and the network, and there is a quirk
    // in QIODevice that can make it try to readData() twice per read() call if buffered and
    // reaData() does not deliver enough data the first time. like when the other side is
    // simply not sending any more data...
    // this can *apparently* lead to long delays sometimes which stalls applications.
    // do not want.
    setOpenMode(d->sock.openMode() | QIODevice::Unbuffered);
}


void KTcpSocket::connectToHost(const QHostAddress &hostAddress, quint16 port, ProxyPolicy policy)
{
    if (policy == AutoProxy) {
        //###
    }
    d->sock.connectToHost(hostAddress, port);
    setOpenMode(d->sock.openMode() | QIODevice::Unbuffered);
}


void KTcpSocket::connectToHost(const QUrl &url, ProxyPolicy policy)
{
    if (policy == AutoProxy) {
        //###
    }
    d->sock.connectToHost(url.host(), url.port());
    setOpenMode(d->sock.openMode() | QIODevice::Unbuffered);
}


void KTcpSocket::disconnectFromHost()
{
    d->sock.disconnectFromHost();
    setOpenMode(d->sock.openMode() | QIODevice::Unbuffered);
}


KTcpSocket::Error KTcpSocket::error() const
{
    return d->errorFromAbsSocket(d->sock.error());
}


QList<KSslError> KTcpSocket::sslErrors() const
{
    //### pretty slow; also consider throwing out duplicate error codes. We may get
    //    duplicates even though there were none in the original list because KSslError
    //    has a smallest common denominator range of SSL error codes.
    QList<KSslError> ret;
    foreach (const QSslError &e, d->sock.sslErrors())
        ret.append(KSslError(e));
    return ret;
}


bool KTcpSocket::flush()
{
    return d->sock.flush();
}


bool KTcpSocket::isValid() const
{
    return d->sock.isValid();
}


QHostAddress KTcpSocket::localAddress() const
{
    return d->sock.localAddress();
}


QHostAddress KTcpSocket::peerAddress() const
{
    return d->sock.peerAddress();
}


QString KTcpSocket::peerName() const
{
    return d->sock.peerName();
}


quint16 KTcpSocket::peerPort() const
{
    return d->sock.peerPort();
}


#ifndef QT_NO_NETWORKPROXY
QNetworkProxy KTcpSocket::proxy() const
{
    return d->sock.proxy();
}
#endif

qint64 KTcpSocket::readBufferSize() const
{
    return d->sock.readBufferSize();
}


#ifndef QT_NO_NETWORKPROXY
void KTcpSocket::setProxy(const QNetworkProxy &proxy)
{
    d->sock.setProxy(proxy);
}
#endif

void KTcpSocket::setReadBufferSize(qint64 size)
{
    d->sock.setReadBufferSize(size);
}


KTcpSocket::State KTcpSocket::state() const
{
    return d->state(d->sock.state());
}


bool KTcpSocket::waitForConnected(int msecs)
{
    bool ret = d->sock.waitForConnected(msecs);
    if (!ret)
        setErrorString(d->sock.errorString());
    setOpenMode(d->sock.openMode() | QIODevice::Unbuffered);
    return ret;
}


bool KTcpSocket::waitForDisconnected(int msecs)
{
    bool ret = d->sock.waitForDisconnected(msecs);
    if (!ret)
        setErrorString(d->sock.errorString());
    setOpenMode(d->sock.openMode() | QIODevice::Unbuffered);
    return ret;
}

////////////////////////////// public methods from QSslSocket

void KTcpSocket::addCaCertificate(const QSslCertificate &certificate)
{
    d->maybeLoadCertificates();
    d->sock.addCaCertificate(certificate);
}


/*
bool KTcpSocket::addCaCertificates(const QString &path, QSsl::EncodingFormat format,
                                   QRegExp::PatternSyntax syntax)
{
    d->maybeLoadCertificates();
    return d->sock.addCaCertificates(path, format, syntax);
}
*/


void KTcpSocket::addCaCertificates(const QList<QSslCertificate> &certificates)
{
    d->maybeLoadCertificates();
    d->sock.addCaCertificates(certificates);
}


QList<QSslCertificate> KTcpSocket::caCertificates() const
{
    d->maybeLoadCertificates();
    return d->sock.caCertificates();
}


QList<KSslCipher> KTcpSocket::ciphers() const
{
    return d->ciphers;
}


void KTcpSocket::connectToHostEncrypted(const QString &hostName, quint16 port, OpenMode openMode)
{
    d->maybeLoadCertificates();
    d->sock.setProtocol(qSslProtocolFromK(d->advertisedSslVersion));
    d->sock.connectToHostEncrypted(hostName, port, openMode);
    setOpenMode(d->sock.openMode() | QIODevice::Unbuffered);
}


QSslCertificate KTcpSocket::localCertificate() const
{
    return d->sock.localCertificate();
}


QList<QSslCertificate> KTcpSocket::peerCertificateChain() const
{
    return d->sock.peerCertificateChain();
}


KSslKey KTcpSocket::privateKey() const
{
    return KSslKey(d->sock.privateKey());
}


KSslCipher KTcpSocket::sessionCipher() const
{
    return KSslCipher(d->sock.sessionCipher());
}


void KTcpSocket::setCaCertificates(const QList<QSslCertificate> &certificates)
{
    d->sock.setCaCertificates(certificates);
    d->certificatesLoaded = true;
}


void KTcpSocket::setCiphers(const QList<KSslCipher> &ciphers)
{
    d->ciphers = ciphers;
    QList<QSslCipher> cl;
    foreach (const KSslCipher &c, d->ciphers) {
        cl.append(d->ccc.converted(c));
    }
    d->sock.setCiphers(cl);
}


void KTcpSocket::setLocalCertificate(const QSslCertificate &certificate)
{
    d->sock.setLocalCertificate(certificate);
}


void KTcpSocket::setLocalCertificate(const QString &fileName, QSsl::EncodingFormat format)
{
    d->sock.setLocalCertificate(fileName, format);
}


void KTcpSocket::setVerificationPeerName(const QString& hostName)
{
#if QT_VERSION >= 0x040800
    d->sock.setPeerVerifyName(hostName);
#else
    Q_UNUSED(hostName);
#endif
}


void KTcpSocket::setPrivateKey(const KSslKey &key)
{
    // We cannot map KSslKey::Algorithm:Dh to anything in QSsl::KeyAlgorithm.
    if (key.algorithm() == KSslKey::Dh)
        return;

    QSslKey _key(key.toDer(),
        (key.algorithm() == KSslKey::Rsa) ? QSsl::Rsa : QSsl::Dsa,
        QSsl::Der,
        (key.secrecy() == KSslKey::PrivateKey) ? QSsl::PrivateKey : QSsl::PublicKey);

    d->sock.setPrivateKey(_key);
}


void KTcpSocket::setPrivateKey(const QString &fileName, KSslKey::Algorithm algorithm,
                               QSsl::EncodingFormat format, const QByteArray &passPhrase)
{
    // We cannot map KSslKey::Algorithm:Dh to anything in QSsl::KeyAlgorithm.
    if (algorithm == KSslKey::Dh)
        return;

    d->sock.setPrivateKey(fileName,
        (algorithm == KSslKey::Rsa) ? QSsl::Rsa : QSsl::Dsa,
        format,
        passPhrase);
}


bool KTcpSocket::waitForEncrypted(int msecs)
{
    return d->sock.waitForEncrypted(msecs);
}


KTcpSocket::EncryptionMode KTcpSocket::encryptionMode() const
{
    return d->encryptionMode(d->sock.mode());
}

QVariant KTcpSocket::socketOption(QAbstractSocket::SocketOption options) const
{
    return d->sock.socketOption(options);
}

void KTcpSocket::setSocketOption(QAbstractSocket::SocketOption options, const QVariant &value)
{
    d->sock.setSocketOption(options, value);
}

QSslConfiguration KTcpSocket::sslConfiguration() const
{
    return d->sock.sslConfiguration();
}

void KTcpSocket::setSslConfiguration (const QSslConfiguration& configuration)
{
    d->sock.setSslConfiguration(configuration);
}

//slot
void KTcpSocket::ignoreSslErrors()
{
    d->sock.ignoreSslErrors();
}


//slot
void KTcpSocket::startClientEncryption()
{
    d->maybeLoadCertificates();
    d->sock.setProtocol(qSslProtocolFromK(d->advertisedSslVersion));
    d->sock.startClientEncryption();
}


//debugging H4X
void KTcpSocket::showSslErrors()
{
	foreach (const QSslError &e, d->sock.sslErrors())
		qDebug() << e.errorString();
}


void KTcpSocket::setAdvertisedSslVersion(KTcpSocket::SslVersion version)
{
    d->advertisedSslVersion = version;
}


KTcpSocket::SslVersion KTcpSocket::advertisedSslVersion() const
{
    return d->advertisedSslVersion;
}


KTcpSocket::SslVersion KTcpSocket::negotiatedSslVersion() const
{
    if (!d->sock.isEncrypted()) {
        return UnknownSslVersion;
    }
    return kSslVersionFromQ(d->sock.protocol());
}


QString KTcpSocket::negotiatedSslVersionName() const
{
    if (!d->sock.isEncrypted()) {
        return QString();
    }
    return d->sock.sessionCipher().protocolString();
}


////////////////////////////// KSslKey

class KSslKeyPrivate
{
public:
    KSslKey::Algorithm convertAlgorithm(QSsl::KeyAlgorithm a)
    {
        switch(a) {
        case QSsl::Dsa:
            return KSslKey::Dsa;
        default:
            return KSslKey::Rsa;
        }
    }

    KSslKey::Algorithm algorithm;
    KSslKey::KeySecrecy secrecy;
    bool isExportable;
    QByteArray der;
};


KSslKey::KSslKey()
 : d(new KSslKeyPrivate)
{
    d->algorithm = Rsa;
    d->secrecy = PublicKey;
    d->isExportable = true;
}


KSslKey::KSslKey(const KSslKey &other)
 : d(new KSslKeyPrivate)
{
    *d = *other.d;
}


KSslKey::KSslKey(const QSslKey &qsk)
 : d(new KSslKeyPrivate)
{
    d->algorithm = d->convertAlgorithm(qsk.algorithm());
    d->secrecy = (qsk.type() == QSsl::PrivateKey) ? PrivateKey : PublicKey;
    d->isExportable = true;
    d->der = qsk.toDer();
}


KSslKey::~KSslKey()
{
    delete d;
}


KSslKey &KSslKey::operator=(const KSslKey &other)
{
    *d = *other.d;
    return *this;
}


KSslKey::Algorithm KSslKey::algorithm() const
{
    return d->algorithm;
}


bool KSslKey::isExportable() const
{
    return d->isExportable;
}


KSslKey::KeySecrecy KSslKey::secrecy() const
{
    return d->secrecy;
}


QByteArray KSslKey::toDer() const
{
    return d->der;
}

////////////////////////////// KSslCipher

//nice-to-have: make implicitly shared
class KSslCipherPrivate
{
public:

    QString authenticationMethod;
    QString encryptionMethod;
    QString keyExchangeMethod;
    QString name;
    bool isNull;
    int supportedBits;
    int usedBits;
};


KSslCipher::KSslCipher()
 : d(new KSslCipherPrivate)
{
    d->isNull = true;
    d->supportedBits = 0;
    d->usedBits = 0;
}


KSslCipher::KSslCipher(const KSslCipher &other)
 : d(new KSslCipherPrivate)
{
    *d = *other.d;
}


KSslCipher::KSslCipher(const QSslCipher &qsc)
 : d(new KSslCipherPrivate)
{
    d->authenticationMethod = qsc.authenticationMethod();
    d->encryptionMethod = qsc.encryptionMethod();
    //Qt likes to append the number of bits (usedBits?) to the algorithm,
    //for example "AES(256)". We only want the pure algorithm name, though.
    int parenIdx = d->encryptionMethod.indexOf(QLatin1Char('('));
    if (parenIdx > 0)
        d->encryptionMethod.truncate(parenIdx);
    d->keyExchangeMethod = qsc.keyExchangeMethod();
    d->name = qsc.name();
    d->isNull = qsc.isNull();
    d->supportedBits = qsc.supportedBits();
    d->usedBits = qsc.usedBits();
}


KSslCipher::~KSslCipher()
{
    delete d;
}


KSslCipher &KSslCipher::operator=(const KSslCipher &other)
{
    *d = *other.d;
    return *this;
}


bool KSslCipher::isNull() const
{
    return d->isNull;
}


QString KSslCipher::authenticationMethod() const
{
    return d->authenticationMethod;
}


QString KSslCipher::encryptionMethod() const
{
    return d->encryptionMethod;
}


QString KSslCipher::keyExchangeMethod() const
{
    return d->keyExchangeMethod;
}


QString KSslCipher::digestMethod() const
{
    //### This is not really backend neutral. It works for OpenSSL and
    //    for RFC compliant names, though.
    if (d->name.endsWith(QLatin1String("SHA")))
        return QString::fromLatin1("SHA-1");
    else if (d->name.endsWith(QLatin1String("MD5")))
        return QString::fromLatin1("MD5");
    else
        return QString::fromLatin1(""); // ## probably QString() is enough
}


QString KSslCipher::name() const
{
    return d->name;
}


int KSslCipher::supportedBits() const
{
    return d->supportedBits;
}


int KSslCipher::usedBits() const
{
    return d->usedBits;
}


//static
QList<KSslCipher> KSslCipher::supportedCiphers()
{
    QList<KSslCipher> ret;
    QList<QSslCipher> candidates = QSslSocket::supportedCiphers();
    foreach(const QSslCipher &c, candidates) {
        ret.append(KSslCipher(c));
    }
    return ret;
}


KSslErrorUiData::KSslErrorUiData()
 : d(new Private())
{
    d->usedBits = 0;
    d->bits = 0;
}


KSslErrorUiData::KSslErrorUiData(const KTcpSocket *socket)
 : d(new Private())
{
    d->certificateChain = socket->peerCertificateChain();
    d->sslErrors = socket->sslErrors();
    d->ip = socket->peerAddress().toString();
    d->host = socket->peerName();
    d->sslProtocol = socket->negotiatedSslVersionName();
    d->cipher = socket->sessionCipher().name();
    d->usedBits = socket->sessionCipher().usedBits();
    d->bits = socket->sessionCipher().supportedBits();
}

KSslErrorUiData::KSslErrorUiData(const QSslSocket *socket)
 : d(new Private())
{
    d->certificateChain = socket->peerCertificateChain();

    // See KTcpSocket::sslErrors()
    foreach (const QSslError &e, socket->sslErrors())
        d->sslErrors.append(KSslError(e));

    d->ip = socket->peerAddress().toString();
    d->host = socket->peerName();
    if (socket->isEncrypted()) {
        d->sslProtocol = socket->sessionCipher().protocolString();
    }
    d->cipher = socket->sessionCipher().name();
    d->usedBits = socket->sessionCipher().usedBits();
    d->bits = socket->sessionCipher().supportedBits();
}


KSslErrorUiData::KSslErrorUiData(const KSslErrorUiData &other)
 : d(new Private(*other.d))
{}

KSslErrorUiData::~KSslErrorUiData()
{
    delete d;
}

KSslErrorUiData &KSslErrorUiData::operator=(const KSslErrorUiData &other)
{
    *d = *other.d;
    return *this;
}


#include "moc_ktcpsocket.cpp"
