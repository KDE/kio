/*
    SPDX-FileCopyrightText: 2013 Szókovács Róbert <szo@szo.hu>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#include "legacycodec.h"

QByteArray LegacyCodec::encodeFileNameUTF8(const QString &fileName)
{
    int len = fileName.length();
    const QChar *uc = fileName.constData();

    uchar replacement = '?';
    int rlen = 3 * len;
    int surrogate_high = -1;

    QByteArray rstr;
    rstr.resize(rlen);
    uchar *cursor = (uchar *)rstr.data();
    const QChar *ch = uc;
    int invalid = 0;

    const QChar *end = ch + len;
    while (ch < end) {
        uint u = ch->unicode();
        if (surrogate_high >= 0) {
            if (ch->isLowSurrogate()) {
                u = QChar::surrogateToUcs4(surrogate_high, u);
                surrogate_high = -1;
            } else {
                // high surrogate without low
                *cursor = replacement;
                ++ch;
                ++invalid;
                surrogate_high = -1;
                continue;
            }
        } else if (ch->isLowSurrogate()) {
            // low surrogate without high
            *cursor = replacement;
            ++ch;
            ++invalid;
            continue;
        } else if (ch->isHighSurrogate()) {
            surrogate_high = u;
            ++ch;
            continue;
        }

        if (u >= 0x10FE00 && u <= 0x10FE7F) {
            *cursor++ = uchar(u - 0x10FE00 + 128);
        } else if (u < 0x80) {
            *cursor++ = uchar(u);
        } else {
            if (u < 0x0800) {
                *cursor++ = 0xc0 | uchar(u >> 6);
            } else {
                // is it one of the Unicode non-characters?
                if (QChar::isNonCharacter(u)) {
                    *cursor++ = replacement;
                    ++ch;
                    ++invalid;
                    continue;
                }

                if (u > 0xffff) {
                    *cursor++ = 0xf0 | uchar(u >> 18);
                    *cursor++ = 0x80 | uchar((u >> 12) & 0x3f);
                } else {
                    *cursor++ = 0xe0 | uchar((u >> 12) & 0x3f);
                }
                *cursor++ = 0x80 | uchar((u >> 6) & 0x3f);
            }
            *cursor++ = 0x80 | uchar(u & 0x3f);
        }
        ++ch;
    }

    rstr.resize(cursor - (const uchar *)rstr.constData());
    return rstr;
}

QString LegacyCodec::decodeFileNameUTF8(const QByteArray &localFileName)
{
    const char *chars = localFileName.constData();
    int len = qstrlen(chars);
    int need = 0;
    uint uc = 0;
    uint min_uc = 0;

    QString result(need + 2 * len + 1, Qt::Uninitialized); // worst case
    ushort *qch = (ushort *)result.unicode();
    uchar ch;

    for (int i = 0; i < len; ++i) {
        ch = chars[i];
        if (need) {
            if ((ch & 0xc0) == 0x80) {
                uc = (uc << 6) | (ch & 0x3f);
                --need;
                if (!need) {
                    bool nonCharacter = QChar::isNonCharacter(uc);
                    if (!nonCharacter && uc > 0xffff && uc < 0x110000) {
                        // surrogate pair
                        Q_ASSERT((qch - (ushort *)result.unicode()) + 2 < result.length());
                        *qch++ = QChar::highSurrogate(uc);
                        *qch++ = QChar::lowSurrogate(uc);
                    } else if ((uc < min_uc) || (uc >= 0xd800 && uc <= 0xdfff) || nonCharacter || uc >= 0x110000) {
                        // error: overlong sequence, UTF16 surrogate or non-character
                        goto error;
                    } else {
                        *qch++ = uc;
                    }
                }
            } else {
                goto error;
            }
        } else {
            if (ch < 128) {
                *qch++ = ushort(ch);
            } else if ((ch & 0xe0) == 0xc0) {
                uc = ch & 0x1f;
                need = 1;
                min_uc = 0x80;
            } else if ((ch & 0xf0) == 0xe0) {
                uc = ch & 0x0f;
                need = 2;
                min_uc = 0x800;
            } else if ((ch & 0xf8) == 0xf0) {
                uc = ch & 0x07;
                need = 3;
                min_uc = 0x10000;
            } else {
                goto error;
            }
        }
    }
    if (need > 0) {
        // unterminated UTF sequence
        goto error;
    }
    result.truncate(qch - (ushort *)result.unicode());
    return result;

error:

    qch = (ushort *)result.unicode();
    for (int i = 0; i < len; ++i) {
        ch = chars[i];
        if (ch < 128) {
            *qch++ = ushort(ch);
        } else {
            uint uc = ch - 128 + 0x10FE00; //U+10FE00-U+10FE7F
            *qch++ = QChar::highSurrogate(uc);
            *qch++ = QChar::lowSurrogate(uc);
        }
    }
    result.truncate(qch - (ushort *)result.unicode());
    return result;
}
