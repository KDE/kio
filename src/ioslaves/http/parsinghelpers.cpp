/*
    This file is part of the KDE libraries
    SPDX-FileCopyrightText: 2008 Andreas Hartmetz <ahartmetz@gmail.com>
    SPDX-FileCopyrightText: 2010, 2011 Rolf Eike Beer <kde@opensource.sf-tec.de>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#include <ctype.h>

#include <QDir>
#include <QTextCodec>

#include <QDebug>

// Advance *pos beyond spaces / tabs
static void skipSpace(const char input[], int *pos, int end)
{
    int idx = *pos;
    while (idx < end && (input[idx] == ' ' || input[idx] == '\t')) {
        idx++;
    }
    *pos = idx;
    return;
}

// Advance *pos to start of next line while being forgiving about line endings.
// Return false if the end of the header has been reached, true otherwise.
static bool nextLine(const char input[], int *pos, int end)
{
    int idx = *pos;
    while (idx < end && input[idx] != '\r' && input[idx] != '\n') {
        idx++;
    }
    int rCount = 0;
    int nCount = 0;
    while (idx < end && qMax(rCount, nCount) < 2 && (input[idx] == '\r' || input[idx] == '\n')) {
        input[idx] == '\r' ? rCount++ : nCount++;
        idx++;
    }
    if (idx < end && qMax(rCount, nCount) == 2 && qMin(rCount, nCount) == 1) {
        // if just one of the others is missing eat it too.
        // this ensures that conforming headers using the proper
        // \r\n sequence (and also \n\r) will be parsed correctly.
        if ((rCount == 1 && input[idx] == '\r') || (nCount == 1 && input[idx] == '\n')) {
            idx++;
        }
    }

    *pos = idx;
    return idx < end && rCount < 2 && nCount < 2;
}

// QByteArray::fromPercentEncoding() does not notify us about encoding errors so we need
// to check here if this is valid at all.
static bool isValidPercentEncoding(const QByteArray &data)
{
    int i = 0;
    const int last = data.length() - 1;
    const char *d = data.constData();

    while ((i = data.indexOf('%', i)) != -1) {
        if (i >= last - 2) {
            return false;
        }
        if (! isxdigit(d[i + 1])) {
            return false;
        }
        if (! isxdigit(d[i + 2])) {
            return false;
        }
        i++;
    }

    return true;
}

QByteArray TokenIterator::next()
{
    QPair<int, int> token = m_tokens[m_currentToken++];
    //fromRawData brings some speed advantage but also the requirement to keep the text buffer
    //around. this together with implicit sharing (you don't know where copies end up)
    //is dangerous!
    //return QByteArray::fromRawData(&m_buffer[token.first], token.second - token.first);
    return QByteArray(&m_buffer[token.first], token.second - token.first);
}

QByteArray TokenIterator::current() const
{
    QPair<int, int> token = m_tokens[m_currentToken - 1];
    //return QByteArray::fromRawData(&m_buffer[token.first], token.second - token.first);
    return QByteArray(&m_buffer[token.first], token.second - token.first);
}

QList<QByteArray> TokenIterator::all() const
{
    QList<QByteArray> ret;
    ret.reserve(m_tokens.count());
    for (int i = 0; i < m_tokens.count(); i++) {
        QPair<int, int> token = m_tokens[i];
        ret.append(QByteArray(&m_buffer[token.first], token.second - token.first));
    }
    return ret;
}

HeaderTokenizer::HeaderTokenizer(char *buffer)
    : m_buffer(buffer)
{
    // add information about available headers and whether they have one or multiple,
    // comma-separated values.

    //The following response header fields are from RFC 2616 unless otherwise specified.
    //Hint: search the web for e.g. 'http "accept-ranges header"' to find information about
    //a header field.
    static const HeaderFieldTemplate headerFieldTemplates[] = {
        {"accept-ranges", false},
        {"age", false},
        {"cache-control", true},
        {"connection", true},
        {"content-disposition", false}, //is multi-valued in a way, but with ";" separator!
        {"content-encoding", true},
        {"content-language", true},
        {"content-length", false},
        {"content-location", false},
        {"content-md5", false},
        {"content-type", false},
        {"date", false},
        {"dav", true}, //RFC 2518
        {"etag", false},
        {"expires", false},
        {"keep-alive", true}, //RFC 2068
        {"last-modified", false},
        {"link", false}, //RFC 2068, multi-valued with ";" separator
        {"location", false},
        {"p3p", true}, // http://www.w3.org/TR/P3P/
        {"pragma", true},
        {"proxy-authenticate", false}, //complicated multi-valuedness: quoted commas don't separate
        //multiple values. we handle this at a higher level.
        {"proxy-connection", true}, //inofficial but well-known; to avoid misunderstandings
        //when using "connection" when talking to a proxy.
        {"refresh", false}, //not sure, only found some mailing list posts mentioning it
        {"set-cookie", false}, //RFC 2109; the multi-valuedness seems to be usually achieved
        //by sending several instances of this field as opposed to
        //usually comma-separated lists with maybe multiple instances.
        {"transfer-encoding", true},
        {"upgrade", true},
        {"warning", true},
        {"www-authenticate", false} //see proxy-authenticate
    };

    for (const HeaderFieldTemplate &ft : headerFieldTemplates) {
        insert(QByteArray(ft.name), HeaderField(ft.isMultiValued));
    }
}

int HeaderTokenizer::tokenize(int begin, int end)
{
    char *buf = m_buffer;  //keep line length in check :/
    int idx = begin;
    int startIdx = begin; //multi-purpose start of current token
    bool multiValuedEndedWithComma = false; //did the last multi-valued line end with a comma?
    QByteArray headerKey;
    do {

        if (buf[idx] == ' ' || buf [idx] == '\t') {
            // line continuation; preserve startIdx except (see below)
            if (headerKey.isEmpty()) {
                continue;
            }
            // turn CR/LF into spaces for later parsing convenience
            int backIdx = idx - 1;
            while (backIdx >= begin && (buf[backIdx] == '\r' || buf[backIdx] == '\n')) {
                buf[backIdx--] = ' ';
            }

            // multiple values, comma-separated: add new value or continue previous?
            if (operator[](headerKey).isMultiValued) {
                if (multiValuedEndedWithComma) {
                    // start new value; this is almost like no line continuation
                    skipSpace(buf, &idx, end);
                    startIdx = idx;
                } else {
                    // continue previous value; this is tricky. unit tests to the rescue!
                    if (operator[](headerKey).beginEnd.last().first == startIdx) {
                        // remove entry, it will be re-added because already idx != startIdx
                        operator[](headerKey).beginEnd.removeLast();
                    } else {
                        // no comma, no entry: the prev line was whitespace only - start new value
                        skipSpace(buf, &idx, end);
                        startIdx = idx;
                    }
                }
            }

        } else {
            // new field
            startIdx = idx;
            // also make sure that there is at least one char after the colon
            while (idx < (end - 1) && buf[idx] != ':' && buf[idx] != '\r' && buf[idx] != '\n') {
                buf[idx] = tolower(buf[idx]);
                idx++;
            }
            if (buf[idx] != ':') {
                //malformed line: no colon
                headerKey.clear();
                continue;
            }
            headerKey = QByteArray(&buf[startIdx], idx - startIdx);
            if (!contains(headerKey)) {
                //we don't recognize this header line
                headerKey.clear();
                continue;
            }
            // skip colon & leading whitespace
            idx++;
            skipSpace(buf, &idx, end);
            startIdx = idx;
        }

        // we have the name/key of the field, now parse the value
        if (!operator[](headerKey).isMultiValued) {

            // scan to end of line
            while (idx < end && buf[idx] != '\r' && buf[idx] != '\n') {
                idx++;
            }
            if (!operator[](headerKey).beginEnd.isEmpty()) {
                // there already is an entry; are we just in a line continuation?
                if (operator[](headerKey).beginEnd.last().first == startIdx) {
                    // line continuation: delete previous entry and later insert a new, longer one.
                    operator[](headerKey).beginEnd.removeLast();
                }
            }
            operator[](headerKey).beginEnd.append(QPair<int, int>(startIdx, idx));

        } else {

            // comma-separated list
            while (true) {
                //skip one value
                while (idx < end && buf[idx] != '\r' && buf[idx] != '\n' && buf[idx] != ',') {
                    idx++;
                }
                if (idx != startIdx) {
                    operator[](headerKey).beginEnd.append(QPair<int, int>(startIdx, idx));
                }
                multiValuedEndedWithComma = buf[idx] == ',';
                //skip comma(s) and leading whitespace, if any respectively
                while (idx < end && buf[idx] == ',') {
                    idx++;
                }
                skipSpace(buf, &idx, end);
                //next value or end-of-line / end of header?
                if (buf[idx] >= end || buf[idx] == '\r' || buf[idx] == '\n') {
                    break;
                }
                //next value
                startIdx = idx;
            }
        }
    } while (nextLine(buf, &idx, end));
    return idx;
}

TokenIterator HeaderTokenizer::iterator(const char *key) const
{
    QByteArray keyBa = QByteArray::fromRawData(key, strlen(key));
    if (contains(keyBa)) {
        return TokenIterator(value(keyBa).beginEnd, m_buffer);
    } else {
        return TokenIterator(m_nullTokens, m_buffer);
    }
}

static void skipLWS(const QString &str, int &pos)
{
    while (pos < str.length() && (str[pos] == QLatin1Char(' ') || str[pos] == QLatin1Char('\t'))) {
        ++pos;
    }
}

// keep the common ending, this allows the compiler to join them
static const char typeSpecials[] =  "{}*'%()<>@,;:\\\"/[]?=";
static const char attrSpecials[] =     "'%()<>@,;:\\\"/[]?=";
static const char valueSpecials[] =      "()<>@,;:\\\"/[]?=";

static bool specialChar(const QChar &ch, const char *specials)
{
    // WORKAROUND: According to RFC 2616, any character other than ascii
    // characters should NOT be allowed in unquoted content-disposition file
    // names. However, since none of the major browsers follow this rule, we do
    // the same thing here and allow all printable unicode characters. See
    // https://bugs.kde.org/show_bug.cgi?id=261223 for the details.
    if (!ch.isPrint()) {
        return true;
    }

    for (int i = qstrlen(specials) - 1; i >= 0; i--) {
        if (ch == QLatin1Char(specials[i])) {
            return true;
        }
    }

    return false;
}

/**
 * read and parse the input until the given terminator
 * @param str input string to parse
 * @param term terminator
 * @param pos position marker in the input string
 * @param specials characters forbidden in this section
 * @return the next section or an empty string if it was invalid
 *
 * Extracts token-like input until terminator char or EOL.
 * Also skips over the terminator.
 *
 * pos is correctly incremented even if this functions returns
 * an empty string so this can be used to skip over invalid
 * parts and continue.
 */
static QString extractUntil(const QString &str, QChar term, int &pos, const char *specials)
{
    QString out;
    skipLWS(str, pos);
    bool valid = true;

    while (pos < str.length() && (str[pos] != term)) {
        out += str[pos];
        valid = (valid && !specialChar(str[pos], specials));
        ++pos;
    }

    if (pos < str.length()) { // Stopped due to finding term
        ++pos;
    }

    if (!valid) {
        return QString();
    }

    // Remove trailing linear whitespace...
    while (out.endsWith(QLatin1Char(' ')) || out.endsWith(QLatin1Char('\t'))) {
        out.chop(1);
    }

    if (out.contains(QLatin1Char(' '))) {
        out.clear();
    }

    return out;
}

// As above, but also handles quotes..
// pos is set to -1 on parse error
static QString extractMaybeQuotedUntil(const QString &str, int &pos)
{
    const QChar term = QLatin1Char(';');

    skipLWS(str, pos);

    // Are we quoted?
    if (pos < str.length() && str[pos] == QLatin1Char('"')) {
        QString out;

        // Skip the quote...
        ++pos;

        // when quoted we also need an end-quote
        bool endquote = false;

        // Parse until trailing quote...
        while (pos < str.length()) {
            if (str[pos] == QLatin1Char('\\') && pos + 1 < str.length()) {
                // quoted-pair = "\" CHAR
                out += str[pos + 1];
                pos += 2; // Skip both...
            } else if (str[pos] == QLatin1Char('"')) {
                ++pos;
                endquote = true;
                break;
            } else if (!str[pos].isPrint()) { // Don't allow CTL's RFC 2616 sec 2.2
                break;
            } else {
                out += str[pos];
                ++pos;
            }
        }

        if (!endquote) {
            pos = -1;
            return QString();
        }

        // Skip until term..
        while (pos < str.length() && (str[pos] != term)) {
            if ((str[pos] != QLatin1Char(' ')) && (str[pos] != QLatin1Char('\t'))) {
                pos = -1;
                return QString();
            }
            ++pos;
        }

        if (pos < str.length()) {  // Stopped due to finding term
            ++pos;
        }

        return out;
    } else {
        return extractUntil(str, term, pos, valueSpecials);
    }
}

static QMap<QString, QString> contentDispositionParserInternal(const QString &disposition)
{
    // qDebug() << "disposition: " << disposition;
    int pos = 0;
    const QString strDisposition = extractUntil(disposition, QLatin1Char(';'), pos, typeSpecials).toLower();

    QMap<QString, QString> parameters;
    QMap<QString, QString> contparams;   // all parameters that contain continuations
    QMap<QString, QString> encparams;    // all parameters that have character encoding

    // the type is invalid, the complete header is junk
    if (strDisposition.isEmpty()) {
        return parameters;
    }

    parameters.insert(QStringLiteral("type"), strDisposition);

    while (pos < disposition.length()) {
        QString key = extractUntil(disposition, QLatin1Char('='), pos, attrSpecials).toLower();

        if (key.isEmpty()) {
            // parse error in this key: do not parse more, but add up
            // everything we already got
            // qDebug() << "parse error in key, abort parsing";
            break;
        }

        QString val;
        if (key.endsWith(QLatin1Char('*'))) {
            val = extractUntil(disposition, QLatin1Char(';'), pos, valueSpecials);
        } else {
            val = extractMaybeQuotedUntil(disposition, pos);
        }

        if (val.isEmpty()) {
            if (pos == -1) {
                // qDebug() << "parse error in value, abort parsing";
                break;
            }
            continue;
        }

        const int spos = key.indexOf(QLatin1Char('*'));
        if (spos == key.length() - 1) {
            key.chop(1);
            encparams.insert(key, val);
        } else if (spos >= 0) {
            contparams.insert(key, val);
        } else if (parameters.contains(key)) {
            // qDebug() << "duplicate key" << key << "found, ignoring everything more";
            parameters.remove(key);
            return parameters;
        } else {
            parameters.insert(key, val);
        }
    }

    QMap<QString, QString>::iterator i = contparams.begin();
    while (i != contparams.end()) {
        QString key = i.key();
        int spos = key.indexOf(QLatin1Char('*'));
        bool hasencoding = false;

        if (key.at(spos + 1) != QLatin1Char('0')) {
            ++i;
            continue;
        }

        // no leading zeros allowed, so delete the junk
        int klen = key.length();
        if (klen > spos + 2) {
            // nothing but continuations and encodings may insert * into parameter name
            if ((klen > spos + 3) || ((klen == spos + 3) && (key.at(spos + 2) != QLatin1Char('*')))) {
                // qDebug() << "removing invalid key " << key << "with val" << i.value() << key.at(spos + 2);
                i = contparams.erase(i);
                continue;
            }
            hasencoding = true;
        }

        int seqnum = 1;
        QMap<QString, QString>::iterator partsi;
        // we do not need to care about encoding specifications: only the first
        // part is allowed to have one
        QString val = i.value();

        key.chop(hasencoding ? 2 : 1);

        while ((partsi = contparams.find(key + QString::number(seqnum))) != contparams.end()) {
            val += partsi.value();
            contparams.erase(partsi);
        }

        i = contparams.erase(i);

        key.chop(1);
        if (hasencoding) {
            encparams.insert(key, val);
        } else {
            if (parameters.contains(key)) {
                // qDebug() << "duplicate key" << key << "found, ignoring everything more";
                parameters.remove(key);
                return parameters;
            }

            parameters.insert(key, val);
        }
    }

    for (QMap<QString, QString>::iterator i = encparams.begin(); i != encparams.end(); ++i) {
        QString val = i.value();

        // RfC 2231 encoded character set in filename
        int spos = val.indexOf(QLatin1Char('\''));
        if (spos == -1) {
            continue;
        }
        int npos = val.indexOf(QLatin1Char('\''), spos + 1);
        if (npos == -1) {
            continue;
        }

        const QStringRef charset = val.leftRef(spos);
        const QByteArray encodedVal = val.midRef(npos + 1).toLatin1();

        if (! isValidPercentEncoding(encodedVal)) {
            continue;
        }

        const QByteArray rawval = QByteArray::fromPercentEncoding(encodedVal);

        if (charset.isEmpty() || (charset == QLatin1String("us-ascii"))) {
            bool valid = true;
            for (int j = rawval.length() - 1; (j >= 0) && valid; j--) {
                valid = (rawval.at(j) >= 32);
            }

            if (!valid) {
                continue;
            }
            val = QString::fromLatin1(rawval.constData());
        } else {
            QTextCodec *codec = QTextCodec::codecForName(charset.toLatin1());
            if (!codec) {
                continue;
            }
            val = codec->toUnicode(rawval);
        }

        parameters.insert(i.key(), val);
    }

    return parameters;
}

static QMap<QString, QString> contentDispositionParser(const QString &disposition)
{
    QMap<QString, QString> parameters = contentDispositionParserInternal(disposition);

    const QLatin1String fn("filename");
    if (parameters.contains(fn)) {
        // Content-Disposition is not allowed to dictate directory
        // path, thus we extract the filename only.
        const QString val = QDir::toNativeSeparators(parameters[fn]);
        int slpos = val.lastIndexOf(QDir::separator());

        if (slpos > -1) {
            parameters.insert(fn, val.mid(slpos + 1));
        }
    }

    return parameters;
}
