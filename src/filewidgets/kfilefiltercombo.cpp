/*
    This file is part of the KDE libraries
    SPDX-FileCopyrightText: Stephan Kulow <coolo@kde.org>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#include "kfilefiltercombo.h"
#include "kfilefilter.h"
#include "kfilefiltercombo_debug.h"

#include <KLocalizedString>
#include <QDebug>
#include <QEvent>
#include <QLineEdit>
#include <QMimeDatabase>

#include <config-kiofilewidgets.h>

#include <algorithm>
#include <utility>

class KFileFilterComboPrivate
{
public:
    explicit KFileFilterComboPrivate(KFileFilterCombo *qq)
        : q(qq)
    {
    }

    void slotFilterChanged();

    KFileFilterCombo *const q;
    // when we have more than 3 mimefilters and no default-filter,
    // we don't show the comments of all mimefilters in one line,
    // instead we show "All supported files". We have to translate
    // that back to the list of mimefilters in currentFilter() tho.
    bool m_hasAllSupportedFiles = false;
    // true when setMimeFilter was called
    bool m_isMimeFilter = false;
    QString m_lastFilter;
    KFileFilter m_defaultFilter = KFileFilter::fromFilterString(i18nc("Default mime type filter that shows all file types", "*|All Files")).first();

    QList<KFileFilter> m_filters;
    bool m_allTypes;
};

KFileFilterCombo::KFileFilterCombo(QWidget *parent)
    : KComboBox(true, parent)
    , d(new KFileFilterComboPrivate(this))
{
    setTrapReturnKey(true);
    setInsertPolicy(QComboBox::NoInsert);
    connect(this, &QComboBox::activated, this, &KFileFilterCombo::filterChanged);
    connect(this, &KComboBox::returnPressed, this, &KFileFilterCombo::filterChanged);
    connect(this, &KFileFilterCombo::filterChanged, this, [this]() {
        d->slotFilterChanged();
    });
    d->m_allTypes = false;
}

KFileFilterCombo::~KFileFilterCombo() = default;

void KFileFilterCombo::setFilters(const QList<KFileFilter> &types, const KFileFilter &defaultFilter)
{
    clear();
    d->m_filters.clear();
    QString delim = QStringLiteral(", ");
    d->m_hasAllSupportedFiles = false;
    bool hasAllFilesFilter = false;
    QMimeDatabase db;

    if (types.isEmpty()) {
        d->m_filters = {d->m_defaultFilter};
        addItem(d->m_defaultFilter.label());

        d->m_lastFilter = currentText();
        return;
    }

    d->m_allTypes = defaultFilter.isEmpty() && (types.count() > 1);

    if (!types.isEmpty() && types.first().mimePatterns().isEmpty()) {
        d->m_allTypes = false;
    }

    // If there's MIME types that have the same comment, we will show the extension
    // in addition to the MIME type comment
    QHash<QString, int> allTypeComments;
    for (const KFileFilter &filter : types) {
        allTypeComments[filter.label()] += 1;
    }

    for (const KFileFilter &filter : types) {
        if (!filter.isValid()) {
            continue;
        }

        const QStringList mimeTypes = filter.mimePatterns();

        const bool isAllFileFilters = std::any_of(mimeTypes.cbegin(), mimeTypes.cend(), [&db](const QString &mimeTypeName) {
            const QMimeType type = db.mimeTypeForName(mimeTypeName);

            if (!type.isValid()) {
                qCWarning(KIO_KFILEWIDGETS_KFILEFILTERCOMBO) << mimeTypeName << "is not a valid MIME type";
                return false;
            }

            return type.name().startsWith(QLatin1String("all/")) || type.isDefault();
        });

        if (isAllFileFilters) {
            hasAllFilesFilter = true;
            continue;
        }

        if (allTypeComments.value(filter.label()) > 1) {
            QStringList mimeSuffixes;

            for (const QString &mimeTypeName : filter.mimePatterns()) {
                const QMimeType type = db.mimeTypeForName(mimeTypeName);
                mimeSuffixes << type.suffixes();
            }

            const QString label = i18nc("%1 is the mimetype name, %2 is the extensions", "%1 (%2)", filter.label(), mimeSuffixes.join(QLatin1String(", ")));
            KFileFilter newFilter(label, filter.filePatterns(), filter.mimePatterns());

            d->m_filters.append(newFilter);
            addItem(newFilter.label());
        } else {
            d->m_filters.append(filter);
            addItem(filter.label());
        }

        if (filter == defaultFilter) {
            setCurrentIndex(count() - 1);
        }
    }

    if (count() == 1) {
        d->m_allTypes = false;
    }

    if (d->m_allTypes) {
        QStringList allMimePatterns;
        QStringList allFilePatterns;
        for (const KFileFilter &filter : std::as_const(d->m_filters)) {
            allMimePatterns << filter.mimePatterns();
            allFilePatterns << filter.filePatterns();
        }

        KFileFilter allSupportedFilesFilter;

        if (count() <= 3) { // show the MIME type comments of at max 3 types
            QStringList allComments;
            for (const KFileFilter &filter : std::as_const(d->m_filters)) {
                allComments << filter.label();
            }

            allSupportedFilesFilter = KFileFilter(allComments.join(delim), allFilePatterns, allMimePatterns);
        } else {
            allSupportedFilesFilter = KFileFilter(i18n("All Supported Files"), allMimePatterns, allMimePatterns);
            d->m_hasAllSupportedFiles = true;
        }

        insertItem(0, allSupportedFilesFilter.label());
        d->m_filters.prepend(allSupportedFilesFilter);
        setCurrentIndex(0);
    }

    if (hasAllFilesFilter) {
        addItem(i18n("All Files"));

        KFileFilter allFilter(i18n("All Files"), {}, {QStringLiteral("application/octet-stream")});

        d->m_filters.append(allFilter);

        if (defaultFilter == allFilter) {
            setCurrentIndex(count() - 1);
        }
    }

    d->m_lastFilter = currentText();
}

KFileFilter KFileFilterCombo::currentFilter() const
{
    if (currentText() != itemText(currentIndex())) {
        // The user edited the text

        const QList<KFileFilter> filter = KFileFilter::fromFilterString(currentText());

        if (!filter.isEmpty()) {
            return filter.first();
        } else {
            return KFileFilter();
        }
    } else {
        if (currentIndex() == -1) {
            return KFileFilter();
        }

        return d->m_filters[currentIndex()];
    }
}

bool KFileFilterCombo::showsAllTypes() const
{
    return d->m_allTypes;
}

QList<KFileFilter> KFileFilterCombo::filters() const
{
    return d->m_filters;
}

void KFileFilterCombo::setCurrentFilter(const KFileFilter &filter)
{
    auto it = std::find(d->m_filters.cbegin(), d->m_filters.cend(), filter);

    if (it == d->m_filters.cend()) {
        qCWarning(KIO_KFILEWIDGETS_KFILEFILTERCOMBO) << "KFileFilterCombo::setCurrentFilter: Could not find file filter" << filter;
        setCurrentIndex(-1);
        Q_EMIT filterChanged();
        return;
    }

    setCurrentIndex(std::distance(d->m_filters.cbegin(), it));
    Q_EMIT filterChanged();
}

void KFileFilterComboPrivate::slotFilterChanged()
{
    m_lastFilter = q->currentText();
}

bool KFileFilterCombo::eventFilter(QObject *o, QEvent *e)
{
    if (o == lineEdit() && e->type() == QEvent::FocusOut) {
        if (currentText() != d->m_lastFilter) {
            Q_EMIT filterChanged();
        }
    }

    return KComboBox::eventFilter(o, e);
}

void KFileFilterCombo::setDefaultFilter(const KFileFilter &filter)
{
    d->m_defaultFilter = filter;
}

KFileFilter KFileFilterCombo::defaultFilter() const
{
    return d->m_defaultFilter;
}

#include "moc_kfilefiltercombo.cpp"
