/*
    SPDX-FileCopyrightText: 2010-2016 Klaralvdalens Datakonsult AB, a KDAB Group company <info@kdab.com>
    SPDX-FileContributor: David Faure <david.faure@kdab.com>

    This file initially comes from the KD Soap library.

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only
*/

#include "httpserver_p.h"
#include <KIO/Job>
#include <QBuffer>
#include <QSslSocket>

static bool splitHeadersAndData(const QByteArray &request, QByteArray &header, QByteArray &data)
{
    const int sep = request.indexOf("\r\n\r\n");
    if (sep <= 0) {
        return false;
    }
    header = request.left(sep);
    data = request.mid(sep + 4);
    return true;
}

typedef QMap<QByteArray, QByteArray> HeadersMap;
static HeadersMap parseHeaders(const QByteArray &headerData)
{
    HeadersMap headersMap;
    QBuffer sourceBuffer;
    sourceBuffer.setData(headerData);
    sourceBuffer.open(QIODevice::ReadOnly);
    // The first line is special, it's the GET or POST line
    const QList<QByteArray> firstLine = sourceBuffer.readLine().split(' ');
    if (firstLine.count() < 3) {
        qDebug() << "Malformed HTTP request:" << firstLine;
        return headersMap;
    }
    const QByteArray request = firstLine[0];
    const QByteArray path = firstLine[1];
    const QByteArray httpVersion = firstLine[2];
    if (request != "GET" && request != "POST") {
        qDebug() << "Unknown HTTP request:" << firstLine;
        return headersMap;
    }
    headersMap.insert("_path", path);
    headersMap.insert("_httpVersion", httpVersion);

    while (!sourceBuffer.atEnd()) {
        const QByteArray line = sourceBuffer.readLine();
        const int pos = line.indexOf(':');
        if (pos == -1) {
            qDebug() << "Malformed HTTP header:" << line;
        }
        const QByteArray header = line.left(pos);
        const QByteArray value = line.mid(pos + 1).trimmed(); // remove space before and \r\n after
        //qDebug() << "HEADER" << header << "VALUE" << value;
        headersMap.insert(header, value);
    }
    return headersMap;
}

enum Method { None, Basic, Plain, Login, Ntlm, CramMd5, DigestMd5 };

static void parseAuthLine(const QString &str, Method *method, QString *headerVal)
{
    *method = None;
    // The code below (from QAuthenticatorPrivate::parseHttpResponse)
    // is supposed to be run in a loop, apparently
    // (multiple WWW-Authenticate lines? multiple values in the line?)

    //qDebug() << "parseAuthLine() " << str;
    if (*method < Basic && str.startsWith(QLatin1String("Basic"), Qt::CaseInsensitive)) {
        *method = Basic;
        *headerVal = str.mid(6);
    } else if (*method < Ntlm && str.startsWith(QLatin1String("NTLM"), Qt::CaseInsensitive)) {
        *method = Ntlm;
        *headerVal = str.mid(5);
    } else if (*method < DigestMd5 && str.startsWith(QLatin1String("Digest"), Qt::CaseInsensitive)) {
        *method = DigestMd5;
        *headerVal = str.mid(7);
    }
}

QByteArray HttpServerThread::makeHttpResponse(const QByteArray &responseData) const
{
    QByteArray httpResponse;
    if (m_features & Error404) {
        httpResponse += "HTTP/1.1 404 Not Found\r\n";
    } else {
        httpResponse += "HTTP/1.1 200 OK\r\n";
    }
    if (!m_contentType.isEmpty()) {
        httpResponse += "Content-Type: " + m_contentType + "\r\n";
    }
    httpResponse += "Mozilla/5.0 (X11; Linux x86_64) KHTML/5.20.0 (like Gecko) Konqueror/5.20\r\n";
    httpResponse += "Content-Length: ";
    httpResponse += QByteArray::number(responseData.size());
    httpResponse += "\r\n";

    // We don't support multiple connections so let's ask the client
    // to close the connection every time.
    httpResponse += "Connection: close\r\n";
    httpResponse += "\r\n";
    httpResponse += responseData;
    return httpResponse;
}

void HttpServerThread::disableSsl()
{
    m_server->disableSsl();
}

void HttpServerThread::finish()
{
    KIO::Job *job = KIO::get(QUrl(endPoint() + QLatin1String("/terminateThread")));
    job->exec();
}

void HttpServerThread::run()
{
    m_server = new BlockingHttpServer(m_features & Ssl);
    m_server->listen();
    QMutexLocker lock(&m_mutex);
    m_port = m_server->serverPort();
    lock.unlock();
    m_ready.release();

    const bool doDebug = qEnvironmentVariableIsSet("HTTP_TEST_DEBUG");

    if (doDebug) {
        qDebug() << "HttpServerThread listening on port" << m_port;
    }

    // Wait for first connection (we'll wait for further ones inside the loop)
    QTcpSocket *clientSocket = m_server->waitForNextConnectionSocket();
    Q_ASSERT(clientSocket);

    Q_FOREVER {
        // get the "request" packet
        if (doDebug) {
            qDebug() << "HttpServerThread: waiting for read";
        }
        if (clientSocket->state() == QAbstractSocket::UnconnectedState ||
                !clientSocket->waitForReadyRead(2000)) {
            if (clientSocket->state() == QAbstractSocket::UnconnectedState) {
                delete clientSocket;
                if (doDebug) {
                    qDebug() << "Waiting for next connection...";
                }
                clientSocket = m_server->waitForNextConnectionSocket();
                Q_ASSERT(clientSocket);
                continue; // go to "waitForReadyRead"
            } else {
                const auto clientSocketError = clientSocket->error();
                qDebug() << "HttpServerThread:" << clientSocketError << "waiting for \"request\" packet";
                break;
            }
        }
        const QByteArray request = m_partialRequest + clientSocket->readAll();
        if (doDebug) {
            qDebug() << "HttpServerThread: request:" << request;
        }

        // Split headers and request xml
        lock.relock();
        const bool splitOK = splitHeadersAndData(request, m_receivedHeaders, m_receivedData);
        if (!splitOK) {
            //if (doDebug)
            //    qDebug() << "Storing partial request" << request;
            m_partialRequest = request;
            continue;
        }

        m_headers = parseHeaders(m_receivedHeaders);

        if (m_headers.value("Content-Length").toInt() > m_receivedData.size()) {
            //if (doDebug)
            //    qDebug() << "Storing partial request" << request;
            m_partialRequest = request;
            continue;
        }

        m_partialRequest.clear();

        if (m_headers.value("_path").endsWith("terminateThread")) { // we're asked to exit
            break;    // normal exit
        }

        lock.unlock();

        //qDebug() << "headers received:" << m_receivedHeaders;
        //qDebug() << headers;
        //qDebug() << "data received:" << m_receivedData;

        if (m_features & BasicAuth) {
            QByteArray authValue = m_headers.value("Authorization");
            if (authValue.isEmpty()) {
                authValue = m_headers.value("authorization");    // as sent by Qt-4.5
            }
            bool authOk = false;
            if (!authValue.isEmpty()) {
                //qDebug() << "got authValue=" << authValue; // looks like "Basic <base64 of user:pass>"
                Method method;
                QString headerVal;
                parseAuthLine(QString::fromLatin1(authValue.data(), authValue.size()), &method, &headerVal);
                //qDebug() << "method=" << method << "headerVal=" << headerVal;
                switch (method) {
                case None: // we want auth, so reject "None"
                    break;
                case Basic: {
                    const QByteArray userPass = QByteArray::fromBase64(headerVal.toLatin1());
                    //qDebug() << userPass;
                    // TODO if (validateAuth(userPass)) {
                    if (userPass == ("kdab:testpass")) {
                        authOk = true;
                    }
                    break;
                }
                default:
                    qWarning("Unsupported authentication mechanism %s", authValue.constData());
                }
            }

            if (!authOk) {
                // send auth request (Qt supports basic, ntlm and digest)
                const QByteArray unauthorized = "HTTP/1.1 401 Authorization Required\r\nWWW-Authenticate: Basic realm=\"example\"\r\nContent-Length: 0\r\n\r\n";
                clientSocket->write(unauthorized);
                if (!clientSocket->waitForBytesWritten(2000)) {
                    const auto clientSocketError = clientSocket->error();
                    qDebug() << "HttpServerThread:" << clientSocketError << "writing auth request";
                    break;
                }
                continue;
            }
        }

        // send response
        const QByteArray response = makeHttpResponse(m_dataToSend);
        if (doDebug) {
            qDebug() << "HttpServerThread: writing" << response;
        }
        clientSocket->write(response);

        clientSocket->flush();
    }
    // all done...
    delete clientSocket;
    delete m_server;
    if (doDebug) {
        qDebug() << "HttpServerThread terminated";
    }
}


void BlockingHttpServer::incomingConnection(qintptr socketDescriptor)
{
    if (doSsl) {
        QSslSocket *serverSocket = new QSslSocket;
        serverSocket->setParent(this);
        serverSocket->setSocketDescriptor(socketDescriptor);
        connect(serverSocket, QOverload<const QList<QSslError>&>::of(&QSslSocket::sslErrors),
                this, &BlockingHttpServer::slotSslErrors);
        // TODO setupSslServer(serverSocket);
        //qDebug() << "Created QSslSocket, starting server encryption";
        serverSocket->startServerEncryption();
        sslSocket = serverSocket;
        // If startServerEncryption fails internally [and waitForEncrypted hangs],
        // then this is how to debug it.
        // A way to catch such errors is really missing in Qt..
        //qDebug() << "startServerEncryption said:" << sslSocket->errorString();
        bool ok = serverSocket->waitForEncrypted();
        Q_ASSERT(ok);
        Q_UNUSED(ok);
    } else {
        QTcpServer::incomingConnection(socketDescriptor);
    }
}
