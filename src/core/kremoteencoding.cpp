/*
    This file is part of the KDE libraries
    SPDX-FileCopyrightText: 2003 Thiago Macieira <thiago.macieira@kdemail.net>

    SPDX-License-Identifier: LGPL-2.0-only
*/

#include "kremoteencoding.h"

#include <KStringHandler>
#include <QTextCodec>
#include <QUrl>

class KRemoteEncodingPrivate
{
public:
    KRemoteEncodingPrivate()
        : m_codec(nullptr)
    {
    }

    QTextCodec *m_codec;
    QByteArray m_codeName;
};

KRemoteEncoding::KRemoteEncoding(const char *name)
    : d(new KRemoteEncodingPrivate)
{
    setEncoding(name);
}

KRemoteEncoding::~KRemoteEncoding()
{
    delete d;
}

QString KRemoteEncoding::decode(const QByteArray &name) const
{
#ifdef CHECK_UTF8
    if (d->m_codec->mibEnum() == 106 && !KStringHandler::isUtf8(name)) {
        return QLatin1String(name);
    }
#endif

    QString result = d->m_codec->toUnicode(name);
    if (d->m_codec->fromUnicode(result) != name)
        // fallback in case of decoding failure
    {
        return QLatin1String(name);
    }

    return result;
}

QByteArray KRemoteEncoding::encode(const QString &name) const
{
    QByteArray result = d->m_codec->fromUnicode(name);
    if (d->m_codec->toUnicode(result) != name) {
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
    // KF6 TODO: return QByteArray
    d->m_codeName = d->m_codec->name();
    return d->m_codeName.constData();
}

int KRemoteEncoding::encodingMib() const
{
    return d->m_codec->mibEnum();
}

void KRemoteEncoding::setEncoding(const char *name)
{
    // don't delete codecs

    if (name) {
        d->m_codec = QTextCodec::codecForName(name);
    }

    if (d->m_codec == nullptr) {
        d->m_codec = QTextCodec::codecForMib(106);    // fallback to UTF-8
    }

    if (d->m_codec == nullptr) {
        d->m_codec = QTextCodec::codecForMib(4 /* latin-1 */);
    }

    Q_ASSERT(d->m_codec);

    /*qDebug() << "setting encoding" << d->m_codec->name()
        << "for name=" << name;*/
}

void KRemoteEncoding::virtual_hook(int, void *)
{
}
