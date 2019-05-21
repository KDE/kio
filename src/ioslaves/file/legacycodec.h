/*
   Copyright (c) 2019 Christoph Feck <cfeck@kde.org>

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

#ifndef LEGACY_CODEC_H
#define LEGACY_CODEC_H

#include <QTextCodec>

class LegacyCodec : public QTextCodec
{
public:
    LegacyCodec() {
        if (codecForLocale()->mibEnum() == 106) {
            setCodecForLocale(this);
        }
    }

    ~LegacyCodec() override {
        setCodecForLocale(nullptr);
    }

    QList<QByteArray> aliases() const override {
        return QList<QByteArray>();
    }

    int mibEnum() const override {
        return 106;
    }

    QByteArray name() const override {
        return QByteArray("UTF-8");
    }

protected:
    QByteArray convertFromUnicode(const QChar *input, int number, QTextCodec::ConverterState *state) const override {
        Q_UNUSED(state);
        return encodeFileNameUTF8(QString::fromRawData(input, number));
    }

    QString convertToUnicode(const char *chars, int len, QTextCodec::ConverterState *state) const override {
        Q_UNUSED(state);
        return decodeFileNameUTF8(QByteArray::fromRawData(chars, len));
    }

private:
    static QByteArray encodeFileNameUTF8(const QString &fileName);
    static QString decodeFileNameUTF8(const QByteArray &localFileName);
};

#endif // define LEGACY_CODEC_H
