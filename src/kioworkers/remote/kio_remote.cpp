/*
    This file is part of the KDE project
    SPDX-FileCopyrightText: 2004 Kevin Ottens <ervin ipsquad net>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#include "kio_remote.h"
#include "debug.h"
#include <stdlib.h>

#include <QCoreApplication>

// Pseudo plugin class to embed meta data
class KIOPluginForMetaData : public QObject
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID "org.kde.kio.worker.remote" FILE "remote.json")
};

extern "C" {
int Q_DECL_EXPORT kdemain(int argc, char **argv)
{
    // necessary to use other kio workers
    QCoreApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("kio_remote"));

    // start the worker
    RemoteProtocol worker(argv[1], argv[2], argv[3]);
    worker.dispatchLoop();
    return 0;
}
}

RemoteProtocol::RemoteProtocol(const QByteArray &protocol, const QByteArray &pool, const QByteArray &app)
    : WorkerBase(protocol, pool, app)
{
}

RemoteProtocol::~RemoteProtocol()
{
}

KIO::WorkerResult RemoteProtocol::listDir(const QUrl &url)
{
    qCDebug(KIOREMOTE_LOG) << "RemoteProtocol::listDir: " << url;

    if (url.path().length() <= 1) {
        return listRoot();
    }

    int second_slash_idx = url.path().indexOf(QLatin1Char('/'), 1);
    const QString root_dirname = url.path().mid(1, second_slash_idx - 1);

    QUrl target = m_impl.findBaseURL(root_dirname);
    qCDebug(KIOREMOTE_LOG) << "possible redirection target : " << target;
    if (target.isValid()) {
        if (second_slash_idx < 0) {
            second_slash_idx = url.path().size();
        }
        const QString urlPath = url.path().remove(0, second_slash_idx);
        if (!urlPath.isEmpty()) {
            target.setPath(QStringLiteral("%1/%2").arg(target.path(), urlPath));
        }
        qCDebug(KIOREMOTE_LOG) << "complete redirection target : " << target;
        redirection(target);
        return KIO::WorkerResult::pass();
    }

    return KIO::WorkerResult::fail(KIO::ERR_MALFORMED_URL, url.toDisplayString());
}

KIO::WorkerResult RemoteProtocol::listRoot()
{
    KIO::UDSEntry entry;

    KIO::UDSEntryList remote_entries;
    m_impl.listRoot(remote_entries);

    totalSize(remote_entries.count() + 2);

    m_impl.createTopLevelEntry(entry);
    listEntry(entry);

    KIO::UDSEntryList::ConstIterator it = remote_entries.constBegin();
    const KIO::UDSEntryList::ConstIterator end = remote_entries.constEnd();
    for (; it != end; ++it) {
        listEntry(*it);
    }

    entry.clear();
    return KIO::WorkerResult::pass();
}

KIO::WorkerResult RemoteProtocol::stat(const QUrl &url)
{
    qCDebug(KIOREMOTE_LOG) << "RemoteProtocol::stat: " << url;

    QString path = url.path();
    if (path.isEmpty() || path == QLatin1String("/")) {
        // The root is "virtual" - it's not a single physical directory
        KIO::UDSEntry entry;
        m_impl.createTopLevelEntry(entry);
        statEntry(entry);
        return KIO::WorkerResult::pass();
    }

    int second_slash_idx = url.path().indexOf(QLatin1Char('/'), 1);
    const QString root_dirname = url.path().mid(1, second_slash_idx - 1);

    if (second_slash_idx == -1 || ((int)url.path().length()) == second_slash_idx + 1) {
        KIO::UDSEntry entry;
        if (m_impl.statNetworkFolder(entry, root_dirname)) {
            statEntry(entry);
            return KIO::WorkerResult::pass();
        }
    } else {
        QUrl target = m_impl.findBaseURL(root_dirname);
        qCDebug(KIOREMOTE_LOG) << "possible redirection target : " << target;
        if (target.isValid()) {
            if (second_slash_idx < 0) {
                second_slash_idx = url.path().size();
            }
            const QString urlPath = url.path().remove(0, second_slash_idx);
            if (!urlPath.isEmpty()) {
                target.setPath(QStringLiteral("%1/%2").arg(target.path(), urlPath));
            }
            qCDebug(KIOREMOTE_LOG) << "complete redirection target : " << target;
            redirection(target);
            return KIO::WorkerResult::pass();
        }
    }

    return KIO::WorkerResult::fail(KIO::ERR_MALFORMED_URL, url.toDisplayString());
}

KIO::WorkerResult RemoteProtocol::del(const QUrl &url, bool /*isFile*/)
{
    qCDebug(KIOREMOTE_LOG) << "RemoteProtocol::del: " << url;

    if (m_impl.deleteNetworkFolder(url.fileName())) {
        return KIO::WorkerResult::pass();
    }

    return KIO::WorkerResult::fail(KIO::ERR_CANNOT_DELETE, url.toDisplayString());
}

KIO::WorkerResult RemoteProtocol::get(const QUrl &url)
{
    qCDebug(KIOREMOTE_LOG) << "RemoteProtocol::get: " << url;

    const QString file = m_impl.findDesktopFile(url.fileName());
    qCDebug(KIOREMOTE_LOG) << "desktop file : " << file;

    if (!file.isEmpty()) {
        redirection(QUrl::fromLocalFile(file));
        return KIO::WorkerResult::pass();
    }

    return KIO::WorkerResult::fail(KIO::ERR_MALFORMED_URL, url.toDisplayString());
}

KIO::WorkerResult RemoteProtocol::rename(const QUrl &src, const QUrl &dest, KIO::JobFlags flags)
{
    if (src.scheme() != QLatin1String("remote") || dest.scheme() != QLatin1String("remote")) {
        return KIO::WorkerResult::fail(KIO::ERR_UNSUPPORTED_ACTION, src.toDisplayString());
    }

    if (m_impl.renameFolders(src.fileName(), dest.fileName(), flags & KIO::Overwrite)) {
        return KIO::WorkerResult::pass();
    }

    return KIO::WorkerResult::fail(KIO::ERR_CANNOT_RENAME, src.toDisplayString());
}

KIO::WorkerResult RemoteProtocol::symlink(const QString &target, const QUrl &dest, KIO::JobFlags flags)
{
    if (m_impl.changeFolderTarget(dest.fileName(), target, flags & KIO::Overwrite)) {
        return KIO::WorkerResult::pass();
    }

    return KIO::WorkerResult::fail(KIO::ERR_CANNOT_SYMLINK, dest.toDisplayString());
}

#include "kio_remote.moc"
