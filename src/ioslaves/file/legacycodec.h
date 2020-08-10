/*
    SPDX-FileCopyrightText: 2019 Christoph Feck <cfeck@kde.org>

    SPDX-License-Identifier: LGPL-2.0-or-later
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
