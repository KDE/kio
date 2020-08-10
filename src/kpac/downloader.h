/*
    SPDX-FileCopyrightText: 2003 Malte Starostik <malte@kde.org>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#ifndef KPAC_DOWNLOADER_H
#define KPAC_DOWNLOADER_H

#include <QObject>

#include <QUrl>

class KJob;
namespace KIO
{
class Job;
}

namespace KPAC
{
class Downloader : public QObject
{
    Q_OBJECT
public:
    explicit Downloader(QObject *);

    void download(const QUrl &);
    const QUrl &scriptUrl()
    {
        return m_scriptURL;
    }
    const QString &script()
    {
        return m_script;
    }
    const QString &error()
    {
        return m_error;
    }

Q_SIGNALS:
    void result(bool);

protected:
    virtual void failed();
    void setError(const QString &);

private Q_SLOTS:
    void redirection(KIO::Job *, const QUrl &);
    void data(KIO::Job *, const QByteArray &);
    void result(KJob *);

private:
    QByteArray m_data;
    QUrl m_scriptURL;
    QString m_script;
    QString m_error;
};
}

#endif // KPAC_DOWNLOADER_H

