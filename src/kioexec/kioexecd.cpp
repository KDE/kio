/*  This file is part of the KDE libraries
    Copyright (C) 2017 Elvis Angelaccio <elvis.angelaccio@kde.org>

    This library is free software; you can redistribute it and/or modify
    it under the terms of the GNU Lesser General Public License as published by
    the Free Software Foundation; either version 2 of the License or ( at
    your option ) version 3 or, at the discretion of KDE e.V. ( which shall
    act as a proxy as in section 14 of the GPLv3 ), any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Lesser General Public License
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.
*/

#include "kioexecd.h"
#include "kioexecdadaptor.h"
#include "kioexecdebug.h"

#include <KDirWatch>
#include <KIO/CopyJob>
#include <KLocalizedString>
#include <KPluginFactory>
#include <KMessageBox>

#include <QDir>
#include <QFileInfo>
#include <QStandardPaths>
#include <QUrl>

K_PLUGIN_FACTORY_WITH_JSON(KIOExecdFactory,
                           "kioexecd.json",
                           registerPlugin<KIOExecd>();)

KIOExecd::KIOExecd(QObject *parent, const QList<QVariant> &)
    : KDEDModule(parent)
{
    qCDebug(KIOEXEC) << "kioexecd started";

    new KIOExecdAdaptor(this);
    m_watcher = new KDirWatch(this);

    connect(m_watcher, &KDirWatch::dirty, this, &KIOExecd::slotDirty);
    connect(m_watcher, &KDirWatch::deleted, this, &KIOExecd::slotDeleted);
}

KIOExecd::~KIOExecd()
{
    for (auto it = m_watched.constBegin(); it != m_watched.constEnd(); ++it) {
        QFileInfo info(it.key());
        const auto parentDir = info.path();
        qCDebug(KIOEXEC) << "About to delete" << parentDir << "containing" << info.fileName();
        QFile::remove(it.key());
        QDir().rmdir(parentDir);
    }
}

void KIOExecd::watch(const QString &path, const QString &destUrl)
{
    if (m_watched.contains(path)) {
        qCDebug(KIOEXEC) << "Already watching" << path;
        return;
    }

    qCDebug(KIOEXEC) << "Going to watch" << path << "for changes, remote destination is" << destUrl;

    m_watcher->addFile(path);
    m_watched.insert(path, QUrl(destUrl));
}

void KIOExecd::slotDirty(const QString &path)
{
    if (!m_watched.contains(path)) {
        return;
    }

    const auto dest = m_watched.value(path);

    if (KMessageBox::questionYesNo(nullptr,
                                   i18n("The file %1\nhas been modified. Do you want to upload the changes?" , dest.toDisplayString()),
                                   i18n("File Changed"), KGuiItem(i18n("Upload")), KGuiItem(i18n("Do Not Upload"))) != KMessageBox::Yes) {
        return;
    }

    qCDebug(KIOEXEC) << "Uploading" << path << "to" << dest;
    auto job = KIO::copy(QUrl::fromLocalFile(path), dest);
    connect(job, &KJob::result, this, [](KJob *job) {
        if (job->error()) {
            KMessageBox::error(nullptr, job->errorString());
        }
    });
}

void KIOExecd::slotDeleted(const QString &path)
{
    if (!m_watched.contains(path)) {
        return;
    }

    qCDebug(KIOEXEC) << "Going to forget" << path;
    m_watcher->removeFile(path);
    m_watched.remove(path);
}

#include "kioexecd.moc"

