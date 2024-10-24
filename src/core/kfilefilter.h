/*
    This file is part of the KDE libraries
    SPDX-FileCopyrightText: 2022 Nicolas Fella <nicolas.fella@gmx.de>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#ifndef KFILEFILTER_H
#define KFILEFILTER_H

#include "kiocore_export.h"

#include <QSharedDataPointer>
#include <QString>
#include <QStringList>

class KFileFilterPrivate;

/**
 * @class KFileFilter kfilefilter.h <KFileFilter>
 *
 * Encapsulates rules to filter a list of files.
 * Files can be filtered based on name patterns (e.g. *.cpp), MIME types, or both.
 * Filters also optionally have a user-facing label.
 *
 * @since 5.101
 */
class KIOCORE_EXPORT KFileFilter
{
public:
    /**
     * Creates an empty filter.
     */
    explicit KFileFilter();

    /**
     * Creates a filter with a given label, name patterns, and MIME types.
     *
     * @param label The user-facing label for this filter.
     * @param filePatterns A list of file name patterns that should be included, e.g. ("*.cpp", "*.cxx").
     * @param mimePatterns A list of MIME types that should be included, e.g. ("text/plain", "image/png").
     *
     */
    explicit KFileFilter(const QString &label, const QStringList &filePatterns, const QStringList &mimePatterns);

    KFileFilter(const KFileFilter &other);
    KFileFilter &operator=(const KFileFilter &other);
    ~KFileFilter();

    /**
     * Checks whether two filters are equal.
     *
     * Filters are considered equal if their file and name patters match.
     * The label is ignored here.
     */
    bool operator==(const KFileFilter &other) const;

    /**
     * The user-facing label for this filter.
     *
     * If no label is passed on creation one is created based on the patterns.
     */
    QString label() const;

    /**
     * List of file name patterns that are included by this filter.
     */
    QStringList filePatterns() const;

    /**
     * List of MIME types that are included by this filter;
     */
    QStringList mimePatterns() const;

    /**
     * Converts this filter to a string representation
     */
    QString toFilterString() const;

    /**
     * Whether the filer is empty, i.e.\ matches all files.
     */
    bool isEmpty() const;

    /**
     * Whether the filter is valid.
     *
     * Creating a filter from an invalid/unkown MIME type will result in an invalid filter.
     *
     * @since 6.0
     */
    bool isValid() const;

    /*
     * Creates a filter for one MIME type.
     * The user-facing label is automatically determined from the MIME type.
     */
    static KFileFilter fromMimeType(const QString &mimeType);

    /**
     * Creates filters from a list of MIME types.
     * The user-facing label is automatically determined from the MIME type.
     *
     * @since 6.0
     */
    static QList<KFileFilter> fromMimeTypes(const QStringList &mimeTypes);

private:
    /**
     * Convert a filter string understood by KFileWidget to a list of KFileFilters.
     */
    static QList<KFileFilter> fromFilterString(const QString &filterString);
    friend class KFileFilterCombo;
    friend class KFileFilterTest;
    friend class KFileFilterComboPrivate;
    friend class KFileWidgetTest;
    friend class KFileFilterComboTest;
    friend class KDEPlatformFileDialog;
    friend class KDEPlatformFileDialogHelper;
    friend class KEncodingFileDialog;

    QSharedDataPointer<KFileFilterPrivate> d;
};

KIOCORE_EXPORT QDebug operator<<(QDebug dbg, const KFileFilter &filter);

#endif
