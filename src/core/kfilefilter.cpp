/*
    This file is part of the KDE libraries
    SPDX-FileCopyrightText: 2022 Nicolas Fella <nicolas.fella@gmx.de>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#include "kfilefilter.h"

#include <QDebug>
#include <QMetaType>
#include <QMimeDatabase>
#include <algorithm>
#include <qchar.h>

#include "kiocoredebug.h"

class KFileFilterPrivate : public QSharedData
{
public:
    KFileFilterPrivate()
    {
    }

    KFileFilterPrivate(const KFileFilterPrivate &other)
        : QSharedData(other)
        , m_label(other.m_label)
        , m_filePatterns(other.m_filePatterns)
        , m_mimePatterns(other.m_mimePatterns)
    {
    }

    QString m_label;
    QStringList m_filePatterns;
    QStringList m_mimePatterns;
};

QVector<KFileFilter> KFileFilter::fromFilterString(const QString &filterString)
{
    int pos = filterString.indexOf(QLatin1Char('/'));

    // Check for an un-escaped '/', if found
    // interpret as a MIME filter.

    if (pos > 0 && filterString[pos - 1] != QLatin1Char('\\')) {
        const QStringList filters = filterString.split(QLatin1Char(' '), Qt::SkipEmptyParts);

        QVector<KFileFilter> result;
        result.reserve(filters.size());

        std::transform(filters.begin(), filters.end(), std::back_inserter(result), [](const QString &mimeType) {
            return KFileFilter::fromMimeType(mimeType);
        });

        return result;
    }

    // Strip the escape characters from
    // escaped '/' characters.

    QString escapeRemoved(filterString);
    for (pos = 0; (pos = escapeRemoved.indexOf(QLatin1String("\\/"), pos)) != -1; ++pos) {
        escapeRemoved.remove(pos, 1);
    }

    const QStringList filters = escapeRemoved.split(QLatin1Char('\n'), Qt::SkipEmptyParts);

    QVector<KFileFilter> result;

    for (const QString &filter : filters) {
        int separatorPos = filter.indexOf(QLatin1Char('|'));

        QString label;
        QStringList patterns;

        if (separatorPos != -1) {
            label = filter.mid(separatorPos + 1);
            patterns = filter.left(separatorPos).split(QLatin1Char(' '));
        } else {
            patterns = filter.split(QLatin1Char(' '));
            label = patterns.join(QLatin1Char(' '));
        }

        result << KFileFilter(label, patterns, {});
    }

    return result;
}

KFileFilter::KFileFilter()
    : d(new KFileFilterPrivate)
{
}

KFileFilter::KFileFilter(const QString &label, const QStringList &filePatterns, const QStringList &mimePatterns)
    : d(new KFileFilterPrivate)
{
    d->m_filePatterns = filePatterns;
    d->m_mimePatterns = mimePatterns;
    d->m_label = label;
}

KFileFilter::~KFileFilter() = default;

KFileFilter::KFileFilter(const KFileFilter &other)
    : d(other.d)
{
}

KFileFilter &KFileFilter::operator=(const KFileFilter &other)
{
    if (this != &other) {
        d = other.d;
    }

    return *this;
}

QString KFileFilter::label() const
{
    return d->m_label;
}

QStringList KFileFilter::filePatterns() const
{
    return d->m_filePatterns;
}

QStringList KFileFilter::mimePatterns() const
{
    return d->m_mimePatterns;
}

bool KFileFilter::operator==(const KFileFilter &other) const
{
    return d->m_label == other.d->m_label && d->m_filePatterns == other.d->m_filePatterns && d->m_mimePatterns == other.d->m_mimePatterns;
}

bool KFileFilter::isEmpty() const
{
    return d->m_filePatterns.isEmpty() && d->m_mimePatterns.isEmpty();
}

QString KFileFilter::toFilterString() const
{
    if (!d->m_filePatterns.isEmpty() && !d->m_mimePatterns.isEmpty()) {
        qCWarning(KIO_CORE) << "KFileFilters with both mime and file patterns cannot be converted to filter strings";
        return QString();
    }

    if (!d->m_mimePatterns.isEmpty()) {
        return d->m_mimePatterns.join(QLatin1Char(' '));
    }

    if (!d->m_label.isEmpty()) {
        const QString patterns = d->m_filePatterns.join(QLatin1Char(' '));
        const QString escapedLabel = QString(d->m_label).replace(QLatin1String("/"), QLatin1String("\\/"));

        if (patterns != d->m_label) {
            return patterns + QLatin1Char('|') + escapedLabel;
        } else {
            return patterns;
        }
    } else {
        return d->m_filePatterns.join(QLatin1Char(' '));
    }
}

KFileFilter KFileFilter::fromMimeType(const QString &mimeType)
{
    if (mimeType.isEmpty()) {
        return KFileFilter();
    }

    static QMimeDatabase db;
    const QMimeType type = db.mimeTypeForName(mimeType);

    KFileFilter filter(type.comment(), {}, {mimeType});
    return filter;
}

Q_DECLARE_METATYPE(KFileFilter);
