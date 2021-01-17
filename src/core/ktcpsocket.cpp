/*
    This file is part of the KDE libraries
    SPDX-FileCopyrightText: 2007, 2008 Andreas Hartmetz <ahartmetz@gmail.com>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#include "ktcpsocket.h"
#include "ksslerror_p.h"
#include "kiocoredebug.h"

#include <ksslcertificatemanager.h>
#include <KLocalizedString>

#include <QAbstractSocket>
#include <QUrl>
#include <QSslKey>
#include <QSslCipher>
#include <QHostAddress>
#include <QNetworkProxy>
#include <QAuthenticator>

#if KIOCORE_BUILD_DEPRECATED_SINCE(5, 65)

static KTcpSocket::SslVersion kSslVersionFromQ(QSsl::SslProtocol protocol)
{
    switch (protocol) {
    case QSsl::SslV2:
        return KTcpSocket::SslV2;
    case QSsl::SslV3:
        return KTcpSocket::SslV3;
    case QSsl::TlsV1_0:
        return KTcpSocket::TlsV1;
    case QSsl::TlsV1_1:
        return KTcpSocket::TlsV1_1;
    case QSsl::TlsV1_2:
        return KTcpSocket::TlsV1_2;
    case QSsl::TlsV1_3:
        return KTcpSocket::TlsV1_3;
    case QSsl::AnyProtocol:
        return KTcpSocket::AnySslVersion;
    case QSsl::TlsV1SslV3:
        return KTcpSocket::TlsV1SslV3;
    case QSsl::SecureProtocols:
        return KTcpSocket::SecureProtocols;
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
    KTcpSocket::SslVersions validVersions(KTcpSocket::SslV2 | KTcpSocket::SslV3 | KTcpSocket::TlsV1);
    validVersions |= KTcpSocket::TlsV1_1;
    validVersions |= KTcpSocket::TlsV1_2;
    validVersions |= KTcpSocket::TlsV1_3;
    validVersions |= KTcpSocket::TlsV1SslV3;
    validVersions |= KTcpSocket::SecureProtocols;

    if (!(sslVersion & validVersions)) {
        return QSsl::UnknownProtocol;
    }

    switch (sslVersion) {
    case KTcpSocket::SslV2:
        return QSsl::SslV2;
    case KTcpSocket::SslV3:
        return QSsl::SslV3;
    case KTcpSocket::TlsV1_0:
        return QSsl::TlsV1_0;
    case KTcpSocket::TlsV1_1:
        return QSsl::TlsV1_1;
    case KTcpSocket::TlsV1_2:
        return QSsl::TlsV1_2;
    case KTcpSocket::TlsV1_3:
        return QSsl::TlsV1_3;
    case KTcpSocket::TlsV1SslV3:
        return QSsl::TlsV1SslV3;
    case KTcpSocket::SecureProtocols:
        return QSsl::SecureProtocols;

    default:
        //QSslSocket doesn't really take arbitrary combinations. It's one or all.
        return QSsl::AnyProtocol;
    }
}

static QString protocolString(QSsl::SslProtocol protocol)
{
    switch (protocol) {
    case QSsl::SslV2:
        return QStringLiteral("SSLv2");
    case QSsl::SslV3:
        return QStringLiteral("SSLv3");
    case QSsl::TlsV1_0:
        return QStringLiteral("TLSv1.0");
    case QSsl::TlsV1_1:
        return QStringLiteral("TLSv1.1");
    case QSsl::TlsV1_2:
        return QStringLiteral("TLSv1.2");
    case QSsl::TlsV1_3:
        return QStringLiteral("TLSv1.3");
    default:
        return QStringLiteral("Unknown");;
    }
}

//cipher class converter KSslCipher -> QSslCipher
class CipherCc
{
public:
    CipherCc()
    {
        const QList<QSslCipher> list = QSslConfiguration::supportedCiphers();
        for (const QSslCipher &c : list) {
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

#if KIOCORE_BUILD_DEPRECATED_SINCE(5, 65)
KSslError::Error KSslErrorPrivate::errorFromQSslError(QSslError::SslError e)
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

QSslError::SslError KSslErrorPrivate::errorFromKSslError(KSslError::Error e)
{
    switch (e) {
        case KSslError::NoError:
            return QSslError::NoError;
        case KSslError::InvalidCertificateAuthorityCertificate:
            return QSslError::InvalidCaCertificate;
        case KSslError::InvalidCertificate:
            return QSslError::UnableToDecodeIssuerPublicKey;
        case KSslError::CertificateSignatureFailed:
            return QSslError::CertificateSignatureFailed;
        case KSslError::SelfSignedCertificate:
            return QSslError::SelfSignedCertificate;
        case KSslError::ExpiredCertificate:
            return QSslError::CertificateExpired;
        case KSslError::RevokedCertificate:
            return QSslError::CertificateRevoked;
        case KSslError::InvalidCertificatePurpose:
            return QSslError::InvalidPurpose;
        case KSslError::RejectedCertificate:
            return QSslError::CertificateRejected;
        case KSslError::UntrustedCertificate:
            return QSslError::CertificateUntrusted;
        case KSslError::NoPeerCertificate:
            return QSslError::NoPeerCertificate;
        case KSslError::HostNameMismatch:
            return QSslError::HostNameMismatch;
        case KSslError::PathLengthExceeded:
            return QSslError::PathLengthExceeded;
        case KSslError::UnknownError:
        default:
            return QSslError::UnspecifiedError;
    }
}

KSslError::KSslError(Error errorCode, const QSslCertificate &certificate)
    : d(new KSslErrorPrivate())
{
    d->error = QSslError(d->errorFromKSslError(errorCode), certificate);
}

KSslError::KSslError(const QSslError &other)
    : d(new KSslErrorPrivate())
{
    d->error = other;
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
    return KSslErrorPrivate::errorFromQSslError(d->error.error());
}

QString KSslError::errorString() const
{
    return d->error.errorString();
}

QSslCertificate KSslError::certificate() const
{
    return d->error.certificate();
}

QSslError KSslError::sslError() const
{
    return d->error;
}
#endif

class KTcpSocketPrivate
{
public:
    explicit KTcpSocketPrivate(KTcpSocket *qq)
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
        q->setErrorString(sock.errorString());
        Q_EMIT q->error(errorFromAbsSocket(e));
    }

    void reemitSslErrors(const QList<QSslError> &errors)
    {
        q->setErrorString(sock.errorString());
        q->showSslErrors(); //H4X
        QList<KSslError> kErrors;
        kErrors.reserve(errors.size());
        for (const QSslError &e : errors) {
            kErrors.append(KSslError(e));
        }
        Q_EMIT q->sslErrors(kErrors);
    }

    void reemitStateChanged(QAbstractSocket::SocketState s)
    {
        Q_EMIT q->stateChanged(state(s));
    }

    void reemitModeChanged(QSslSocket::SslMode m)
    {
        Q_EMIT q->encryptionModeChanged(encryptionMode(m));
    }

    // This method is needed because we might emit readyRead() due to this QIODevice
    // having some data buffered, so we need to care about blocking, too.
    //### useless ATM as readyRead() now just calls d->sock.readyRead().
    void reemitReadyRead()
    {
        if (!emittedReadyRead) {
            emittedReadyRead = true;
            Q_EMIT q->readyRead();
            emittedReadyRead = false;
        }
    }

    void maybeLoadCertificates()
    {
        if (!certificatesLoaded) {
            q->setCaCertificates(KSslCertificateManager::self()->caCertificates());
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

    connect(&d->sock, &QIODevice::aboutToClose, this, &QIODevice::aboutToClose);
    connect(&d->sock, &QIODevice::bytesWritten, this, &QIODevice::bytesWritten);
    connect(&d->sock, &QSslSocket::encryptedBytesWritten, this, &KTcpSocket::encryptedBytesWritten);
    connect(&d->sock, &QSslSocket::readyRead, this, [this]() { d->reemitReadyRead(); });
    connect(&d->sock, &QAbstractSocket::connected, this, &KTcpSocket::connected);
    connect(&d->sock, &QSslSocket::encrypted, this, &KTcpSocket::encrypted);
    connect(&d->sock, &QAbstractSocket::disconnected, this, &KTcpSocket::disconnected);
#ifndef QT_NO_NETWORKPROXY
    connect(&d->sock, &QAbstractSocket::proxyAuthenticationRequired,
            this, &KTcpSocket::proxyAuthenticationRequired);
#endif

#if QT_VERSION < QT_VERSION_CHECK(5, 15, 0)
    connect(&d->sock, QOverload<QAbstractSocket::SocketError>::of(&QSslSocket::error),
            this, [this](QAbstractSocket::SocketError err) { d->reemitSocketError(err); });
#else
    connect(&d->sock, &QSslSocket::errorOccurred,
            this, [this](QAbstractSocket::SocketError err) { d->reemitSocketError(err); });
#endif

    connect(&d->sock, QOverload<const QList<QSslError> &>::of(&QSslSocket::sslErrors),
            this, [this](const QList<QSslError> &errorList) { d->reemitSslErrors(errorList); });
    connect(&d->sock, &QAbstractSocket::hostFound, this, &KTcpSocket::hostFound);
    connect(&d->sock, &QSslSocket::stateChanged,
            this, [this](QAbstractSocket::SocketState state) { d->reemitStateChanged(state); });
    connect(&d->sock, &QSslSocket::modeChanged,
            this, [this](QSslSocket::SslMode mode) { d->reemitModeChanged(mode); });
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
    const auto networkError = d->sock.error();
    return d->errorFromAbsSocket(networkError);
}

QList<KSslError> KTcpSocket::sslErrors() const
{
    //### pretty slow; also consider throwing out duplicate error codes. We may get
    //    duplicates even though there were none in the original list because KSslError
    //    has a smallest common denominator range of SSL error codes.
#if QT_VERSION >= QT_VERSION_CHECK(5, 15, 0)
    const auto qsslErrors = d->sock.sslHandshakeErrors();
#else
    const auto qsslErrors = d->sock.sslErrors();
#endif
    QList<KSslError> ret;
    ret.reserve(qsslErrors.size());
    for (const QSslError &e : qsslErrors) {
        ret.append(KSslError(e));
    }
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
    if (!ret) {
        setErrorString(d->sock.errorString());
    }
    setOpenMode(d->sock.openMode() | QIODevice::Unbuffered);
    return ret;
}

bool KTcpSocket::waitForDisconnected(int msecs)
{
    bool ret = d->sock.waitForDisconnected(msecs);
    if (!ret) {
        setErrorString(d->sock.errorString());
    }
    setOpenMode(d->sock.openMode() | QIODevice::Unbuffered);
    return ret;
}

////////////////////////////// public methods from QSslSocket

void KTcpSocket::addCaCertificate(const QSslCertificate &certificate)
{
    d->maybeLoadCertificates();
#if QT_VERSION < QT_VERSION_CHECK(5, 15, 0)
    d->sock.addCaCertificate(certificate);
#else
    d->sock.sslConfiguration().addCaCertificate(certificate);
#endif
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
#if QT_VERSION < QT_VERSION_CHECK(5, 15, 0)
    d->sock.addCaCertificates(certificates);
#else
    d->sock.sslConfiguration().addCaCertificates(certificates);
#endif
}

QList<QSslCertificate> KTcpSocket::caCertificates() const
{
    d->maybeLoadCertificates();
    return d->sock.sslConfiguration().caCertificates();
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
    QSslConfiguration configuration = d->sock.sslConfiguration();
    configuration.setCaCertificates(certificates);
    d->sock.setSslConfiguration(configuration);
    d->certificatesLoaded = true;
}

void KTcpSocket::setCiphers(const QList<KSslCipher> &ciphers)
{
    d->ciphers = ciphers;
    QList<QSslCipher> cl;
    cl.reserve(d->ciphers.size());
    for (const KSslCipher &c : ciphers) {
        cl.append(d->ccc.converted(c));
    }
    QSslConfiguration configuration = d->sock.sslConfiguration();
    configuration.setCiphers(cl);
    d->sock.setSslConfiguration(configuration);
}

void KTcpSocket::setLocalCertificate(const QSslCertificate &certificate)
{
    d->sock.setLocalCertificate(certificate);
}

void KTcpSocket::setLocalCertificate(const QString &fileName, QSsl::EncodingFormat format)
{
    d->sock.setLocalCertificate(fileName, format);
}

void KTcpSocket::setVerificationPeerName(const QString &hostName)
{
    d->sock.setPeerVerifyName(hostName);
}

void KTcpSocket::setPrivateKey(const KSslKey &key)
{
    // We cannot map KSslKey::Algorithm:Dh to anything in QSsl::KeyAlgorithm.
    if (key.algorithm() == KSslKey::Dh) {
        return;
    }

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
    if (algorithm == KSslKey::Dh) {
        return;
    }

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

void KTcpSocket::setSslConfiguration(const QSslConfiguration &configuration)
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
#if (QT_VERSION < QT_VERSION_CHECK(5, 15, 0))
    const QList<QSslError> list = d->sock.sslErrors();
#else
    const QList<QSslError> list = d->sock.sslHandshakeErrors();
#endif
    for (const QSslError &e : list) {
        qCDebug(KIO_CORE) << e.errorString();
    }
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

    return kSslVersionFromQ(d->sock.sessionProtocol());
}

QString KTcpSocket::negotiatedSslVersionName() const
{
    if (!d->sock.isEncrypted()) {
        return QString();
    }

    return protocolString(d->sock.sessionProtocol());
}

////////////////////////////// KSslKey

class KSslKeyPrivate
{
public:
    KSslKey::Algorithm convertAlgorithm(QSsl::KeyAlgorithm a)
    {
        switch (a) {
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
    if (parenIdx > 0) {
        d->encryptionMethod.truncate(parenIdx);
    }
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
    if (d->name.endsWith(QLatin1String("SHA"))) {
        return QStringLiteral("SHA-1");
    } else if (d->name.endsWith(QLatin1String("MD5"))) {
        return QStringLiteral("MD5");
    } else {
        return QString();
    }
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
    const QList<QSslCipher> candidates = QSslConfiguration::supportedCiphers();
    ret.reserve(candidates.size());
    for (const QSslCipher &c : candidates) {
        ret.append(KSslCipher(c));
    }
    return ret;
}

#include "moc_ktcpsocket.cpp"

#endif
