/*
    SPDX-FileCopyrightText: 2006-2010 Peter Penz <peter.penz@gmx.at>
    SPDX-FileCopyrightText: 2006 Aaron J. Seigo <aseigo@kde.org>
    SPDX-FileCopyrightText: 2007 Kevin Ottens <ervin@kde.org>
    SPDX-FileCopyrightText: 2007 Urs Wolfer <uwolfer @ kde.org>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#include "kcoreurlnavigator.h"

#include "urlutil_p.h"

#include <KIO/StatJob>
#include <KLocalizedString>
#include <kfileitem.h>
#include <kprotocolinfo.h>

#include <QDir>
#include <QMimeData>
#include <QMimeDatabase>

#include <algorithm>
#include <numeric>

struct LocationData {
    QUrl url;
    QVariant state;
};

class KCoreUrlNavigatorPrivate
{
public:
    KCoreUrlNavigatorPrivate(KCoreUrlNavigator *qq);

    ~KCoreUrlNavigatorPrivate()
    {
    }

    /**
     * Returns true, if the MIME type of the path represents a
     * compressed file like TAR or ZIP, as listed in @p archiveMimetypes
     */
    bool isCompressedPath(const QUrl &path, const QStringList &archiveMimetypes) const;

    /**
     * Returns the current history index, if \a historyIndex is
     * smaller than 0. If \a historyIndex is greater or equal than
     * the number of available history items, the largest possible
     * history index is returned. For the other cases just \a historyIndex
     * is returned.
     */
    int adjustedHistoryIndex(int historyIndex) const;

    KCoreUrlNavigator *const q;

    QList<LocationData> m_history;
    int m_historyIndex = 0;
};

KCoreUrlNavigatorPrivate::KCoreUrlNavigatorPrivate(KCoreUrlNavigator *qq)
    : q(qq)
{
}

bool KCoreUrlNavigatorPrivate::isCompressedPath(const QUrl &url, const QStringList &archiveMimetypes) const
{
    QMimeDatabase db;
    const QMimeType mime = db.mimeTypeForUrl(QUrl(url.toString(QUrl::StripTrailingSlash)));
    return std::any_of(archiveMimetypes.begin(), archiveMimetypes.end(), [mime](const QString &archiveType) {
        return mime.inherits(archiveType);
    });
}

int KCoreUrlNavigatorPrivate::adjustedHistoryIndex(int historyIndex) const
{
    const int historySize = m_history.size();
    if (historyIndex < 0) {
        historyIndex = m_historyIndex;
    } else if (historyIndex >= historySize) {
        historyIndex = historySize - 1;
        Q_ASSERT(historyIndex >= 0); // m_history.size() must always be > 0
    }
    return historyIndex;
}

// ------------------------------------------------------------------------------------------------

KCoreUrlNavigator::KCoreUrlNavigator(const QUrl &url, QObject *parent)
    : QObject(parent)
    , d(new KCoreUrlNavigatorPrivate(this))
{
    d->m_history.prepend(LocationData{url.adjusted(QUrl::NormalizePathSegments), {}});
}

KCoreUrlNavigator::~KCoreUrlNavigator()
{
}

QUrl KCoreUrlNavigator::locationUrl(int historyIndex) const
{
    historyIndex = d->adjustedHistoryIndex(historyIndex);
    return d->m_history.at(historyIndex).url;
}

void KCoreUrlNavigator::saveLocationState(const QVariant &state)
{
    d->m_history[d->m_historyIndex].state = state;
}

QVariant KCoreUrlNavigator::locationState(int historyIndex) const
{
    historyIndex = d->adjustedHistoryIndex(historyIndex);
    return d->m_history.at(historyIndex).state;
}

bool KCoreUrlNavigator::goBack()
{
    const int count = d->m_history.size();
    if (d->m_historyIndex < count - 1) {
        const QUrl newUrl = locationUrl(d->m_historyIndex + 1);
        Q_EMIT currentUrlAboutToChange(newUrl);

        ++d->m_historyIndex;

        Q_EMIT historyIndexChanged();
        Q_EMIT historyChanged();
        Q_EMIT currentLocationUrlChanged();
        return true;
    }

    return false;
}

bool KCoreUrlNavigator::goForward()
{
    if (d->m_historyIndex > 0) {
        const QUrl newUrl = locationUrl(d->m_historyIndex - 1);
        Q_EMIT currentUrlAboutToChange(newUrl);

        --d->m_historyIndex;

        Q_EMIT historyIndexChanged();
        Q_EMIT historyChanged();
        Q_EMIT currentLocationUrlChanged();
        return true;
    }

    return false;
}

bool KCoreUrlNavigator::goUp()
{
    const QUrl currentUrl = locationUrl();
    const QUrl upUrl = KIO::upUrl(currentUrl);
    if (!currentUrl.matches(upUrl, QUrl::StripTrailingSlash)) {
        setCurrentLocationUrl(upUrl);
        return true;
    }

    return false;
}

QUrl KCoreUrlNavigator::currentLocationUrl() const
{
    return locationUrl();
}

void KCoreUrlNavigator::setCurrentLocationUrl(const QUrl &newUrl)
{
    if (newUrl == locationUrl()) {
        return;
    }

    QUrl url = newUrl.adjusted(QUrl::NormalizePathSegments);

    // This will be used below; we define it here because in the lower part of the
    // code locationUrl() and url become the same URLs
    QUrl firstChildUrl = KIO::UrlUtil::firstChildUrl(locationUrl(), url);

    const QString scheme = url.scheme();
    if (!scheme.isEmpty()) {
        // Check if the URL represents a tar-, zip- or 7z-file, or an archive file supported by krarc.
        const QStringList archiveMimetypes = KProtocolInfo::archiveMimetypes(scheme);

        if (!archiveMimetypes.isEmpty()) {
            // Check whether the URL is really part of the archive file, otherwise
            // replace it by the local path again.
            bool insideCompressedPath = d->isCompressedPath(url, archiveMimetypes);
            if (!insideCompressedPath) {
                QUrl prevUrl = url;
                QUrl parentUrl = KIO::upUrl(url);
                while (parentUrl != prevUrl) {
                    if (d->isCompressedPath(parentUrl, archiveMimetypes)) {
                        insideCompressedPath = true;
                        break;
                    }
                    prevUrl = parentUrl;
                    parentUrl = KIO::upUrl(parentUrl);
                }
            }
            if (!insideCompressedPath) {
                // drop the tar:, zip:, sevenz: or krarc: protocol since we are not
                // inside the compressed path
                url.setScheme(QStringLiteral("file"));
                firstChildUrl.setScheme(QStringLiteral("file"));
            }
        }
    }

    // Check whether current history element has the same URL.
    // If this is the case, just ignore setting the URL.
    const LocationData &data = d->m_history.at(d->m_historyIndex);
    const bool isUrlEqual = url.matches(locationUrl(), QUrl::StripTrailingSlash) || (!url.isValid() && url.matches(data.url, QUrl::StripTrailingSlash));
    if (isUrlEqual) {
        return;
    }

    Q_EMIT currentUrlAboutToChange(url);

    if (d->m_historyIndex > 0) {
        // If an URL is set when the history index is not at the end (= 0),
        // then clear all previous history elements so that a new history
        // tree is started from the current position.
        auto begin = d->m_history.begin();
        auto end = begin + d->m_historyIndex;
        d->m_history.erase(begin, end);
        d->m_historyIndex = 0;
    }

    Q_ASSERT(d->m_historyIndex == 0);
    d->m_history.insert(0, LocationData{url, {}});

    // Prevent an endless growing of the history: remembering
    // the last 100 Urls should be enough...
    const int historyMax = 100;
    if (d->m_history.size() > historyMax) {
        auto begin = d->m_history.begin() + historyMax;
        auto end = d->m_history.end();
        d->m_history.erase(begin, end);
    }

    Q_EMIT historyIndexChanged();
    Q_EMIT historySizeChanged();
    Q_EMIT historyChanged();
    Q_EMIT currentLocationUrlChanged();
    if (firstChildUrl.isValid()) {
        Q_EMIT urlSelectionRequested(firstChildUrl);
    }
}

int KCoreUrlNavigator::historySize() const
{
    return d->m_history.count();
}

int KCoreUrlNavigator::historyIndex() const
{
    return d->m_historyIndex;
}
