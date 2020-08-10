/*
    This file is part of the KDE libraries
    SPDX-FileCopyrightText: 2008 Andreas Hartmetz <ahartmetz@gmail.com>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#ifndef PARSINGHELPERS_H
#define PARSINGHELPERS_H

#include <QList>
#include <QPair>
#include <QMap>

struct HeaderField {
    HeaderField(bool multiValued)
    {
        isMultiValued = multiValued;
    }
    // QHash requires a default constructor
    HeaderField()
    {
        isMultiValued = false;
    }

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
    explicit HeaderTokenizer(char *buffer);
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
