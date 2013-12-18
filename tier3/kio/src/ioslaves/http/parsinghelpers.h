/* This file is part of the KDE libraries
    Copyright (C) 2008 Andreas Hartmetz <ahartmetz@gmail.com>

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

#ifndef PARSINGHELPERS_H
#define PARSINGHELPERS_H

#include <QtCore/QList>
#include <QtCore/QPair>
#include <QtCore/QMap>

struct HeaderField {
    HeaderField(bool multiValued)
        { isMultiValued = multiValued; }
    // QHash requires a default constructor
    HeaderField()
        { isMultiValued = false; }

    bool isMultiValued;
    QList<QPair<int, int> > beginEnd;
};

class HeaderTokenizer;
class TokenIterator
{
public:
    inline bool hasNext() const
    {
        return m_currentToken < m_tokens.count();
    }

    QByteArray next();

    QByteArray current() const;

    QList<QByteArray> all() const;

private:
    friend class HeaderTokenizer;
    QList<QPair<int, int> > m_tokens;
    int m_currentToken;
    const char *m_buffer;
    TokenIterator(const QList<QPair<int, int> > &tokens, const char *buffer)
     : m_tokens(tokens),
       m_currentToken(0),
       m_buffer(buffer) {}
};

class HeaderTokenizer : public QHash<QByteArray, HeaderField>
{
public:
    HeaderTokenizer(char *buffer);
    // note that buffer is not const - in the parsed area CR/LF will be overwritten
    // with spaces if there is a line continuation.
    /// @return: index of first char after header or end
    int tokenize(int begin, int end);

    // after tokenize() has been called use the QHash part of this class to
    // ask for a list of begin-end indexes in buffer for header values.

    TokenIterator iterator(const char *key) const;
private:
    char *m_buffer;
    struct HeaderFieldTemplate {
        const char *name;
        bool isMultiValued;
    };
    QList<QPair<int, int> > m_nullTokens;   //long-lived, allows us to pass out references.
};

#endif //PARSINGHELPERS_H
