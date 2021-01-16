/*
    This file is part of the KDE libraries
    SPDX-FileCopyrightText: 2007 Thiago Macieira <thiago@kde.org>
    SPDX-FileCopyrightText: 2007 Andreas Hartmetz <ahartmetz@gmail.com>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#ifndef KTCPSOCKET_H
#define KTCPSOCKET_H

#include "kiocore_export.h"

#if KIOCORE_ENABLE_DEPRECATED_SINCE(5, 65)
#include "ksslerroruidata.h"

#include <QSslSocket>
#include <QSslConfiguration>


/*
  Notes on QCA::TLS compatibility
  In order to check for all validation problems as far as possible we need to use:
  Validity QCA::TLS::peerCertificateValidity()
  TLS::IdentityResult QCA::TLS::peerIdentityResult()
  CertificateChain QCA::TLS::peerCertificateChain().validate() - to find the failing cert!
  TLS::Error QCA::TLS::errorCode() - for more generic (but stil SSL) errors
 */

class KSslKeyPrivate;

/** SSL Key
 *  @deprecated since 5.65, use QSslKey instead.
 */
class KIOCORE_DEPRECATED_VERSION(5, 65, "Use QSslKey") KIOCORE_EXPORT KSslKey
{
public:
    enum Algorithm {
        Rsa = 0,
        Dsa,
        Dh,
    };
    enum KeySecrecy {
        PublicKey,
        PrivateKey,
    };

    KSslKey();
    KSslKey(const KSslKey &other);
    KSslKey(const QSslKey &sslKey);
    ~KSslKey();
    KSslKey &operator=(const KSslKey &other);

    Algorithm algorithm() const;
    bool isExportable() const;
    KeySecrecy secrecy() const;
    QByteArray toDer() const;
private:
    KSslKeyPrivate *const d;
};

class KSslCipherPrivate;

/** SSL Cipher
 *  @deprecated since 5.65, use QSslCipher instead.
 */
class KIOCORE_DEPRECATED_VERSION(5, 65, "Use QSslCipher") KIOCORE_EXPORT KSslCipher
{
public:
    KSslCipher();
    KSslCipher(const KSslCipher &other);
    KSslCipher(const QSslCipher &);
    ~KSslCipher();
    KSslCipher &operator=(const KSslCipher &other);

    bool isNull() const;
    QString authenticationMethod() const;
    QString encryptionMethod() const;
    QString keyExchangeMethod() const;
    QString digestMethod() const;
    /* mainly for internal use */
    QString name() const;
    int supportedBits() const;
    int usedBits() const;

    static QList<KSslCipher> supportedCiphers();

private:
    KSslCipherPrivate *const d;
};

class KSslErrorPrivate;
class KTcpSocket;

/** To be replaced by QSslError.
 *  @deprecated since 5.65
 */
class KIOCORE_DEPRECATED_VERSION(5, 65, "Use QSslError") KIOCORE_EXPORT KSslError
{
public:
    enum Error {
        NoError = 0,
        UnknownError,
        InvalidCertificateAuthorityCertificate,
        InvalidCertificate,
        CertificateSignatureFailed,
        SelfSignedCertificate,
        ExpiredCertificate,
        RevokedCertificate,
        InvalidCertificatePurpose,
        RejectedCertificate,
        UntrustedCertificate,
        NoPeerCertificate,
        HostNameMismatch,
        PathLengthExceeded,
    };

#if KIOCORE_ENABLE_DEPRECATED_SINCE(5, 63)
    /** @deprecated since 5.63, use the QSslError ctor instead. */
    KIOCORE_DEPRECATED_VERSION(5, 63, "Use KSslError(const QSslError &)")
    KSslError(KSslError::Error error = NoError, const QSslCertificate &cert = QSslCertificate());
#endif
    KSslError(const QSslError &error);
    KSslError(const KSslError &other);
    ~KSslError();
    KSslError &operator=(const KSslError &other);

    Error error() const;
    QString errorString() const;
    QSslCertificate certificate() const;
    /**
     * Returns the QSslError wrapped by this KSslError.
     * @since 5.63
     */
    QSslError sslError() const;
private:
    KSslErrorPrivate *const d;
};

//consider killing more convenience functions with huge signatures
//### do we need setSession() / session() ?

//BIG FAT TODO: do we keep openMode() up to date everywhere it can change?

//other TODO: limit possible error strings?, SSL key stuff

//TODO protocol (or maybe even application?) dependent automatic proxy choice

class KTcpSocketPrivate;
class QHostAddress;

/** TCP socket.
 *  @deprecated since 5.65, use QSslSocket instead.
 */
class KIOCORE_DEPRECATED_VERSION(5, 65, "Use QSslSocket") KIOCORE_EXPORT KTcpSocket: public QIODevice
{
    Q_OBJECT
public:
    enum State {
        UnconnectedState = 0,
        HostLookupState,
        ConnectingState,
        ConnectedState,
        BoundState,
        ListeningState,
        ClosingState,
        //hmmm, do we need an SslNegotiatingState?
    };
    enum SslVersion {
        UnknownSslVersion = 0x01,
        SslV2 = 0x02,
        SslV3 = 0x04,
        TlsV1 = 0x08,
        SslV3_1 = 0x08,
        TlsV1SslV3 = 0x10,
        SecureProtocols = 0x20,
        TlsV1_0 = TlsV1,
        TlsV1_1 = 0x40,
        TlsV1_2 = 0x80,
        TlsV1_3 = 0x100,
        AnySslVersion = SslV2 | SslV3 | TlsV1,
    };
    Q_DECLARE_FLAGS(SslVersions, SslVersion)

    enum Error {
        UnknownError = 0,
        ConnectionRefusedError,
        RemoteHostClosedError,
        HostNotFoundError,
        SocketAccessError,
        SocketResourceError,
        SocketTimeoutError,
        NetworkError,
        UnsupportedSocketOperationError,
        SslHandshakeFailedError,                 ///< @since 4.10.5
    };
    /*
    The following is based on reading the OpenSSL interface code of both QSslSocket
    and QCA::TLS. Barring oversights it should be accurate. The two cases with the
    question marks apparently will never be emitted by QSslSocket so there is nothing
    to compare.

    QSslError::NoError                                  KTcpSocket::NoError
    QSslError::UnableToGetIssuerCertificate             QCA::ErrorSignatureFailed
    QSslError::UnableToDecryptCertificateSignature      QCA::ErrorSignatureFailed
    QSslError::UnableToDecodeIssuerPublicKey            QCA::ErrorInvalidCA
    QSslError::CertificateSignatureFailed               QCA::ErrorSignatureFailed
    QSslError::CertificateNotYetValid                   QCA::ErrorExpired
    QSslError::CertificateExpired                       QCA::ErrorExpired
    QSslError::InvalidNotBeforeField                    QCA::ErrorExpired
    QSslError::InvalidNotAfterField                     QCA::ErrorExpired
    QSslError::SelfSignedCertificate                    QCA::ErrorSelfSigned
    QSslError::SelfSignedCertificateInChain             QCA::ErrorSelfSigned
    QSslError::UnableToGetLocalIssuerCertificate        QCA::ErrorInvalidCA
    QSslError::UnableToVerifyFirstCertificate           QCA::ErrorSignatureFailed
    QSslError::CertificateRevoked                       QCA::ErrorRevoked
    QSslError::InvalidCaCertificate                     QCA::ErrorInvalidCA
    QSslError::PathLengthExceeded                       QCA::ErrorPathLengthExceeded
    QSslError::InvalidPurpose                           QCA::ErrorInvalidPurpose
    QSslError::CertificateUntrusted                     QCA::ErrorUntrusted
    QSslError::CertificateRejected                      QCA::ErrorRejected
    QSslError::SubjectIssuerMismatch                    QCA::TLS::InvalidCertificate ?
    QSslError::AuthorityIssuerSerialNumberMismatch      QCA::TLS::InvalidCertificate ?
    QSslError::NoPeerCertificate                        QCA::TLS::NoCertificate
    QSslError::HostNameMismatch                         QCA::TLS::HostMismatch
    QSslError::UnspecifiedError                         KTcpSocket::UnknownError
    QSslError::NoSslSupport                             Never happens :)
     */
    enum EncryptionMode {
        UnencryptedMode = 0,
        SslClientMode,
        SslServerMode, //### not implemented
    };
    enum ProxyPolicy {
        /// Use the proxy that KProtocolManager suggests for the connection parameters given.
        AutoProxy = 0,
        /// Use the proxy set by setProxy(), if any; otherwise use no proxy.
        ManualProxy,
    };

    KTcpSocket(QObject *parent = nullptr);
    ~KTcpSocket();

    //from QIODevice
    //reimplemented virtuals - the ones not reimplemented are OK for us
    bool atEnd() const override;
    qint64 bytesAvailable() const override;
    qint64 bytesToWrite() const override;
    bool canReadLine() const override;
    void close() override;
    bool isSequential() const override;
    bool open(QIODevice::OpenMode open) override;
    bool waitForBytesWritten(int msecs) override;
    //### Document that this actually tries to read *more* data
    bool waitForReadyRead(int msecs = 30000) override;
protected:
    qint64 readData(char *data, qint64 maxSize) override;
    qint64 writeData(const char *data, qint64 maxSize) override;
Q_SIGNALS:
    /// @since 4.8.1
    /// Forwarded from QSslSocket
    void encryptedBytesWritten(qint64 written);
public:
    //from QAbstractSocket
    void abort();
    void connectToHost(const QString &hostName, quint16 port, ProxyPolicy policy = AutoProxy);
    void connectToHost(const QHostAddress &hostAddress, quint16 port, ProxyPolicy policy = AutoProxy);

    /**
     * Take the hostname and port from @p url and connect to them. The information from a
     * full URL enables the most accurate choice of proxy in case of proxy rules that
     * depend on high-level information like protocol or username.
     * @see KProtocolManager::proxyForUrl()
     */
    void connectToHost(const QUrl &url, ProxyPolicy policy = AutoProxy);
    void disconnectFromHost();
    Error error() const; //### QAbstractSocket's model is strange. error() should be related to the
    //current state and *NOT* just report the last error if there was one.
    QList<KSslError> sslErrors() const; //### the errors returned can only have a subset of all
    //possible QSslError::SslError enum values depending on backend
    bool flush();
    bool isValid() const;
    QHostAddress localAddress() const;
    QHostAddress peerAddress() const;
    QString peerName() const;
    quint16 peerPort() const;
    void setVerificationPeerName(const QString &hostName);

#ifndef QT_NO_NETWORKPROXY
    /**
     * @see: connectToHost()
     */
    QNetworkProxy proxy() const;
#endif
    qint64 readBufferSize() const; //probably hard to implement correctly

#ifndef QT_NO_NETWORKPROXY
    /**
     * @see: connectToHost()
     */
    void setProxy(const QNetworkProxy &proxy); //people actually seem to need it
#endif
    void setReadBufferSize(qint64 size);
    State state() const;
    bool waitForConnected(int msecs = 30000);
    bool waitForDisconnected(int msecs = 30000);

    //from QSslSocket
    void addCaCertificate(const QSslCertificate &certificate);
//    bool addCaCertificates(const QString &path, QSsl::EncodingFormat format = QSsl::Pem,
//                           QRegExp::PatternSyntax syntax = QRegExp::FixedString);
    void addCaCertificates(const QList<QSslCertificate> &certificates);
    QList<QSslCertificate> caCertificates() const;
    QList<KSslCipher> ciphers() const;
    void connectToHostEncrypted(const QString &hostName, quint16 port, OpenMode openMode = ReadWrite);
    // bool isEncrypted() const { return encryptionMode() != UnencryptedMode }
    QSslCertificate localCertificate() const;
    QList<QSslCertificate> peerCertificateChain() const;
    KSslKey privateKey() const;
    KSslCipher sessionCipher() const;
    void setCaCertificates(const QList<QSslCertificate> &certificates);
    void setCiphers(const QList<KSslCipher> &ciphers);
    //### void setCiphers(const QString &ciphers); //what about i18n?
    void setLocalCertificate(const QSslCertificate &certificate);
    void setLocalCertificate(const QString &fileName, QSsl::EncodingFormat format = QSsl::Pem);
    void setPrivateKey(const KSslKey &key);
    void setPrivateKey(const QString &fileName, KSslKey::Algorithm algorithm = KSslKey::Rsa,
                       QSsl::EncodingFormat format = QSsl::Pem,
                       const QByteArray &passPhrase = QByteArray());
    void setAdvertisedSslVersion(SslVersion version);
    SslVersion advertisedSslVersion() const;    //always equal to last setSslAdvertisedVersion
    SslVersion negotiatedSslVersion() const;     //negotiated version; downgrades are possible.
    QString negotiatedSslVersionName() const;
    bool waitForEncrypted(int msecs = 30000);

    EncryptionMode encryptionMode() const;

    /**
     * Returns the state of the socket @p option.
     *
     * @see QAbstractSocket::socketOption
     *
     * @since 4.5.0
     */
    QVariant socketOption(QAbstractSocket::SocketOption options) const;

    /**
     * Sets the socket @p option to @p value.
     *
     * @see QAbstractSocket::setSocketOption
     *
     * @since 4.5.0
     */
    void setSocketOption(QAbstractSocket::SocketOption options, const QVariant &value);

    /**
     * Returns the socket's SSL configuration.
     *
     * @since 4.8.4
     */
    QSslConfiguration sslConfiguration() const;

    /**
     * Sets the socket's SSL configuration.
     *
     * @since 4.8.4
     */
    void setSslConfiguration(const QSslConfiguration &configuration);

Q_SIGNALS:
    //from QAbstractSocket
    void connected();
    void disconnected();
    void error(KTcpSocket::Error);
    void hostFound();
#ifndef QT_NO_NETWORKPROXY
    void proxyAuthenticationRequired(const QNetworkProxy &proxy, QAuthenticator *authenticator);
#endif
    // only for raw socket state, SSL is separate
    void stateChanged(KTcpSocket::State);

    //from QSslSocket
    void encrypted();
    void encryptionModeChanged(EncryptionMode);
    void sslErrors(const QList<KSslError> &errors);

public Q_SLOTS:
    void ignoreSslErrors();
    void startClientEncryption();
    // void startServerEncryption(); //not implemented
private:
    Q_PRIVATE_SLOT(d, void reemitReadyRead())
    Q_PRIVATE_SLOT(d, void reemitSocketError(QAbstractSocket::SocketError))
    Q_PRIVATE_SLOT(d, void reemitSslErrors(const QList<QSslError> &))
    Q_PRIVATE_SLOT(d, void reemitStateChanged(QAbstractSocket::SocketState))
    Q_PRIVATE_SLOT(d, void reemitModeChanged(QSslSocket::SslMode))

//debugging H4X
    void showSslErrors();

    friend class KTcpSocketPrivate;
    KTcpSocketPrivate *const d;
};

#endif // deprecated since 5.65

#endif // KTCPSOCKET_H
