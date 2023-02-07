/*
    This file is part of the KDE libraries
    SPDX-FileCopyrightText: 2003 Thiago Macieira <thiago.macieira@kdemail.net>

    SPDX-License-Identifier: LGPL-2.0-only
*/

#include "kremoteencoding.h"

#include <QStringConverter>
#include <QUrl>

class KRemoteEncodingPrivate
{
public:
    QStringDecoder m_decoder;
    QStringEncoder m_encoder;
};

KRemoteEncoding::KRemoteEncoding(const char *name)
    : d(new KRemoteEncodingPrivate)
{
    setEncoding(name);
}

KRemoteEncoding::~KRemoteEncoding() = default;

QString KRemoteEncoding::decode(const QByteArray &name) const
{
    QString result = d->m_decoder.decode(name);
    if (d->m_encoder.encode(result) != name)
    // fallback in case of decoding failure
    {
        return QLatin1String(name);
    }

    return result;
}

QByteArray KRemoteEncoding::encode(const QString &name) const
{
    QByteArray result = d->m_encoder.encode(name);
    if (d->m_decoder.decode(result) != name) {
        return name.toLatin1();
    }

    return result;
}

QByteArray KRemoteEncoding::encode(const QUrl &url) const
{
    return encode(url.path());
}

QByteArray KRemoteEncoding::directory(const QUrl &url, bool ignore_trailing_slash) const
{
    QUrl dirUrl(url);
    if (ignore_trailing_slash && dirUrl.path().endsWith(QLatin1Char('/'))) {
        dirUrl = dirUrl.adjusted(QUrl::StripTrailingSlash);
    }
    const QString dir = dirUrl.adjusted(QUrl::RemoveFilename).path();
    return encode(dir);
}

QByteArray KRemoteEncoding::fileName(const QUrl &url) const
{
    return encode(url.fileName());
}

const char *KRemoteEncoding::encoding() const
{
    return d->m_decoder.name();
}

void KRemoteEncoding::setEncoding(const char *name)
{
    d->m_decoder = QStringDecoder(name);
    if (!d->m_decoder.isValid()) {
        d->m_decoder = QStringDecoder(QStringDecoder::Utf8);
    }
    d->m_encoder = QStringEncoder(name);
    if (!d->m_encoder.isValid()) {
        d->m_encoder = QStringEncoder(QStringEncoder::Utf8);
    }

    Q_ASSERT(d->m_decoder.isValid());
    Q_ASSERT(d->m_encoder.isValid());

    /*qDebug() << "setting encoding" << d->m_decoder.name()
        << "for name=" << name;*/
}

void KRemoteEncoding::virtual_hook(int, void *)
{
}
