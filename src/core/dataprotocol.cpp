/*
    Implementation of the data protocol (rfc 2397)

    SPDX-FileCopyrightText: 2002, 2003 Leo Savernik <l.savernik@aon.at>

    SPDX-License-Identifier: LGPL-2.0-only
*/

#include "dataprotocol_p.h"

#include "global.h"
#include "metadata.h"

#include <QByteArray>
#include <QStringDecoder>

using namespace KIO;

/* structure containing header information */
struct DataHeader {
    QString mime_type; // MIME type of content (lowercase)
    MetaData attributes; // attribute/value pairs (attribute lowercase,
    //  value unchanged)
    bool is_base64; // true if data is base64 encoded
    QByteArray url; // reference to decoded url
    int data_offset; // zero-indexed position within url
    // where the real data begins. May point beyond
    // the end to indicate that there is no data
};

/* returns the position of the first occurrence of any of the given
 * characters c1 or comma (',') or semicolon (';') or buf.length()
 * if none is contained.
 *
 * \a buf buffer where to look for c
 * \a begin zero-indexed starting position
 * \a c1 character to find or '\0' to ignore
 */
static int find(const QByteArray &buf, int begin, const char c1)
{
    static const char comma = ',';
    static const char semicolon = ';';
    int pos = begin;
    int size = buf.length();
    while (pos < size) {
        const char ch = buf[pos];
        if (ch == comma || ch == semicolon || (c1 != '\0' && ch == c1)) {
            break;
        }
        pos++;
    } /*wend*/
    return pos;
}

/* extracts the string between the current position pos and the first
 * occurrence of either c1 or comma (',') or semicolon (';') exclusively
 * and updates pos to point at the found delimiter or at the end of the
 * buffer if neither character occurred.
 * \a buf buffer where to look for
 * \a pos zero-indexed position within buffer
 * \a c1 character to find or '\0' to ignore
 */
static inline QString extract(const QByteArray &buf, int &pos, const char c1 = '\0')
{
    int oldpos = pos;
    pos = find(buf, oldpos, c1);
    return QString::fromLatin1(buf.mid(oldpos, pos - oldpos));
}

/* ignores all whitespaces
 * \a buf buffer to operate on
 * \a pos position to shift to first non-whitespace character
 *  Upon return pos will either point to the first non-whitespace
 *  character or to the end of the buffer.
 */
static inline void ignoreWS(const QByteArray &buf, int &pos)
{
    int size = buf.length();
    while (pos < size && (buf[pos] == ' ' || buf[pos] == '\t')) {
        ++pos;
    }
}

/* parses a quoted string as per rfc 822.
 *
 * If trailing quote is missing, the whole rest of the buffer is returned.
 * \a buf buffer to operate on
 * \a pos position pointing to the leading quote
 * Returns the extracted string. pos will be updated to point to the
 *  character following the trailing quote.
 */
static QString parseQuotedString(const QByteArray &buf, int &pos)
{
    int size = buf.length();
    QString res;
    res.reserve(size); // can't be larger than buf
    pos++; // jump over leading quote
    bool escaped = false; // if true means next character is literal
    bool parsing = true; // true as long as end quote not found
    while (parsing && pos < size) {
        const QChar ch = QLatin1Char(buf[pos++]);
        if (escaped) {
            res += ch;
            escaped = false;
        } else {
            switch (ch.unicode()) {
            case '"':
                parsing = false;
                break;
            case '\\':
                escaped = true;
                break;
            default:
                res += ch;
                break;
            } /*end switch*/
        } /*end if*/
    } /*wend*/
    res.squeeze();
    return res;
}

/* parses the header of a data url
 * \a url the data url
 * \a mimeOnly if the only interesting information is the MIME type
 * Returns DataHeader structure with the header information
 */
static DataHeader parseDataHeader(const QUrl &url, const bool mimeOnly)
{
    DataHeader header_info;

    // initialize header info members
    header_info.mime_type = QStringLiteral("text/plain");
    header_info.attributes.insert(QStringLiteral("charset"), QStringLiteral("us-ascii"));
    header_info.is_base64 = false;

    // decode url and save it
    const QByteArray &raw_url = header_info.url = QByteArray::fromPercentEncoding(url.path(QUrl::FullyEncoded).toLatin1());
    const int raw_url_len = raw_url.length();

    header_info.data_offset = 0;

    // read MIME type
    if (raw_url_len == 0) {
        return header_info;
    }
    const QString mime_type = extract(raw_url, header_info.data_offset).trimmed();
    if (!mime_type.isEmpty()) {
        header_info.mime_type = mime_type;
    }
    if (mimeOnly) {
        return header_info;
    }

    if (header_info.data_offset >= raw_url_len) {
        return header_info;
    }
    // jump over delimiter token and return if data reached
    if (raw_url[header_info.data_offset++] == ',') {
        return header_info;
    }

    // read all attributes and store them
    bool data_begin_reached = false;
    while (!data_begin_reached && header_info.data_offset < raw_url_len) {
        // read attribute
        const QString attribute = extract(raw_url, header_info.data_offset, '=').trimmed();
        if (header_info.data_offset >= raw_url_len || raw_url[header_info.data_offset] != '=') {
            // no assignment, must be base64 option
            if (attribute == QLatin1String("base64")) {
                header_info.is_base64 = true;
            }
        } else {
            header_info.data_offset++; // jump over '=' token

            // read value
            ignoreWS(raw_url, header_info.data_offset);
            if (header_info.data_offset >= raw_url_len) {
                return header_info;
            }

            QString value;
            if (raw_url[header_info.data_offset] == '"') {
                value = parseQuotedString(raw_url, header_info.data_offset);
                ignoreWS(raw_url, header_info.data_offset);
            } else {
                value = extract(raw_url, header_info.data_offset).trimmed();
            }

            // add attribute to map
            header_info.attributes[attribute.toLower()] = value;

        } /*end if*/
        if (header_info.data_offset < raw_url_len && raw_url[header_info.data_offset] == ',') {
            data_begin_reached = true;
        }
        header_info.data_offset++; // jump over separator token
    } /*wend*/

    return header_info;
}

DataProtocol::DataProtocol()
{
}

DataProtocol::~DataProtocol() = default;

void DataProtocol::get(const QUrl &url)
{
    ref();
    // qDebug() << this;

    const DataHeader hdr = parseDataHeader(url, false);

    const int size = hdr.url.length();
    const int data_ofs = qMin(hdr.data_offset, size);
    // FIXME: string is copied, would be nice if we could have a reference only
    const QByteArray url_data = hdr.url.mid(data_ofs);
    QByteArray outData;

    if (hdr.is_base64) {
        // base64 stuff is expected to contain the correct charset, so we just
        // decode it and pass it to the receiver
        outData = QByteArray::fromBase64(url_data);
    } else {
        QStringDecoder codec(hdr.attributes[QStringLiteral("charset")].toLatin1().constData());
        if (codec.isValid()) {
            outData = QString(codec.decode(url_data)).toUtf8();
        } else {
            outData = url_data;
        } /*end if*/
    } /*end if*/

    // qDebug() << "emit mimeType@"<<this;
    Q_EMIT mimeType(hdr.mime_type);
    // qDebug() << "emit totalSize@"<<this;
    Q_EMIT totalSize(outData.size());

    // qDebug() << "emit setMetaData@"<<this;
    setAllMetaData(hdr.attributes);

    // qDebug() << "emit sendMetaData@"<<this;
    sendMetaData();
    // qDebug() << "(1) queue size " << dispatchQueue.size();
    // empiric studies have shown that this shouldn't be queued & dispatched
    Q_EMIT data(outData);
    // qDebug() << "(2) queue size " << dispatchQueue.size();
    dispatch_data(QByteArray{});
    // qDebug() << "(3) queue size " << dispatchQueue.size();
    dispatch_finished();
    // qDebug() << "(4) queue size " << dispatchQueue.size();
    deref();
}

/* --------------------------------------------------------------------- */

void DataProtocol::mimetype(const QUrl &url)
{
    ref();
    Q_EMIT mimeType(parseDataHeader(url, true).mime_type);
    Q_EMIT finished();
    deref();
}

/* --------------------------------------------------------------------- */

#if !defined(TESTKIO)
#include "moc_dataprotocol_p.cpp"
#endif
