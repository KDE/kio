/*
    This file is part of the KDE project
    SPDX-FileCopyrightText: 2004 Jan Schaefer <j_schaef@informatik.uni-kl.de>

    SPDX-License-Identifier: LGPL-2.0-only
*/

#include "knfsshare.h"

#include "../utils_p.h"

#include <QDebug>
#include <QFile>
#include <QFileInfo>
#include <QSet>
#include <QTextStream>

#include "kiocoredebug.h"
#include <KConfig>
#include <KConfigGroup>
#include <KDirWatch>

class Q_DECL_HIDDEN KNFSShare::KNFSSharePrivate
{
public:
    explicit KNFSSharePrivate(KNFSShare *parent);

    void slotFileChange(const QString &);

    bool readExportsFile();
    bool findExportsFile();

    KNFSShare *const q;
    QSet<QString> sharedPaths;
    QString exportsFile;
};

KNFSShare::KNFSSharePrivate::KNFSSharePrivate(KNFSShare *parent)
    : q(parent)
{
    if (findExportsFile()) {
        readExportsFile();
    }
}

/**
 * Try to find the nfs config file path
 * First tries the kconfig, then checks
 * several well-known paths
 * @return whether an 'exports' file was found.
 **/
bool KNFSShare::KNFSSharePrivate::findExportsFile()
{
    KConfig knfsshare(QStringLiteral("knfsshare"));
    KConfigGroup config(&knfsshare, "General");
    exportsFile = config.readPathEntry("exportsFile", QString());

    if (!exportsFile.isEmpty() && QFileInfo::exists(exportsFile)) {
        return true;
    }

    if (QFile::exists(QStringLiteral("/etc/exports"))) {
        exportsFile = QStringLiteral("/etc/exports");
    } else {
        // qDebug() << "Could not find exports file! /etc/exports doesn't exist. Configure it in share/config/knfsshare, [General], exportsFile=....";
        return false;
    }

    config.writeEntry("exportsFile", exportsFile);
    return true;
}

/**
 * Reads all paths from the exports file
 * and fills the sharedPaths dict with the values
 */
bool KNFSShare::KNFSSharePrivate::readExportsFile()
{
    QFile f(exportsFile);

    // qDebug() << exportsFile;

    if (!f.open(QIODevice::ReadOnly)) {
        qCWarning(KIO_CORE) << "KNFSShare: Could not open" << exportsFile;
        return false;
    }

    sharedPaths.clear();

    QTextStream s(&f);

    bool continuedLine = false; // is true if the line before ended with a backslash
    QString completeLine;

    while (!s.atEnd()) {
        QString currentLine = s.readLine().trimmed();

        if (continuedLine) {
            completeLine += currentLine;
            continuedLine = false;
        } else {
            completeLine = currentLine;
        }

        // is the line continued in the next line ?
        if (completeLine.endsWith(QLatin1Char('\\'))) {
            continuedLine = true;
            // remove the ending backslash
            completeLine.chop(1);
            continue;
        }

        // comments or empty lines
        if (completeLine.startsWith(QLatin1Char('#')) || completeLine.isEmpty()) {
            continue;
        }

        QString path;

        // Handle quotation marks
        if (completeLine[0] == QLatin1Char('\"')) {
            int i = completeLine.indexOf(QLatin1Char('"'), 1);
            if (i == -1) {
                qCWarning(KIO_CORE) << "KNFSShare: Parse error: Missing quotation mark:" << completeLine;
                continue;
            }
            path = completeLine.mid(1, i - 1);

        } else { // no quotation marks
            int i = completeLine.indexOf(QLatin1Char(' '));
            if (i == -1) {
                i = completeLine.indexOf(QLatin1Char('\t'));
            }

            if (i == -1) {
                path = completeLine;
            } else {
                path = completeLine.left(i);
            }
        }

        // qDebug() << "KNFSShare: Found path: " << path;

        if (!path.isEmpty()) {
            // Append a '/' to normalize path
            Utils::appendSlash(path);
            sharedPaths.insert(path);
        }
    }

    return true;
}

KNFSShare::KNFSShare()
    : d(new KNFSSharePrivate(this))
{
    if (!d->exportsFile.isEmpty() && QFileInfo::exists(d->exportsFile)) {
        KDirWatch::self()->addFile(d->exportsFile);
        connect(KDirWatch::self(), &KDirWatch::dirty, this, [this](const QString &path) {
            d->slotFileChange(path);
        });
    }
}

KNFSShare::~KNFSShare() = default;

bool KNFSShare::isDirectoryShared(const QString &path) const
{
    if (path.isEmpty()) {
        return false;
    }

    return d->sharedPaths.contains(Utils::slashAppended(path));
}

QStringList KNFSShare::sharedDirectories() const
{
    return d->sharedPaths.values();
}

QString KNFSShare::exportsPath() const
{
    return d->exportsFile;
}

void KNFSShare::KNFSSharePrivate::slotFileChange(const QString &path)
{
    if (path == exportsFile) {
        readExportsFile();
    }

    Q_EMIT q->changed();
}

class KNFSShareSingleton
{
public:
    KNFSShare instance;
};

Q_GLOBAL_STATIC(KNFSShareSingleton, _instance)

KNFSShare *KNFSShare::instance()
{
    return &_instance()->instance;
}

#include "moc_knfsshare.cpp"
