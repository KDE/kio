/*
    SPDX-FileCopyrightText: 2003 Malte Starostik <malte@kde.org>
    SPDX-FileCopyrightText: 2011 Dawit Alemayehu <adawit@kde.org>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#ifndef KPAC_SCRIPT_H
#define KPAC_SCRIPT_H

#include <QString>

class QUrl;
class QJSEngine;

namespace KPAC
{
class Script
{
public:
    class Error
    {
    public:
        explicit Error(const QString &message)
            : m_message(message) {}
        const QString &message() const
        {
            return m_message;
        }

    private:
        QString m_message;
    };

    explicit Script(const QString &code);
    ~Script();
    Script(const Script &) = delete;
    Script &operator=(const Script &) = delete;
    QString evaluate(const QUrl &);

private:
    QJSEngine *m_engine;
};
}

#endif // KPAC_SCRIPT_H

