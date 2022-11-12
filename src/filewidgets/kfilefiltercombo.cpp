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
#include <QtGlobal>

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

    QVector<KFileFilter> m_filters;
    bool m_allTypes;
};

KFileFilterCombo::KFileFilterCombo(QWidget *parent)
    : KComboBox(true, parent)
    , d(new KFileFilterComboPrivate(this))
{
    setTrapReturnKey(true);
    setInsertPolicy(QComboBox::NoInsert);
    connect(this, qOverload<int>(&QComboBox::activated), this, &KFileFilterCombo::filterChanged);
    // TODO KF6: remove this QOverload, only KUrlComboBox::returnPressed(const QString &) will remain
    connect(this, qOverload<const QString &>(&KComboBox::returnPressed), this, &KFileFilterCombo::filterChanged);
    connect(this, &KFileFilterCombo::filterChanged, this, [this]() {
        d->slotFilterChanged();
    });
    d->m_allTypes = false;
}

KFileFilterCombo::~KFileFilterCombo() = default;

void KFileFilterCombo::setFileFilters(const QVector<KFileFilter> &types, const KFileFilter &defaultFilter)
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
        QStringList allTypes;
        for (const KFileFilter &filter : std::as_const(d->m_filters)) {
            allTypes << filter.mimePatterns().join(QLatin1Char(' '));
        }

        KFileFilter allSupportedFilesFilter;

        if (count() <= 3) { // show the MIME type comments of at max 3 types
            QStringList allComments;
            for (const KFileFilter &filter : std::as_const(d->m_filters)) {
                allComments << filter.label();
            }

            allSupportedFilesFilter = KFileFilter(allComments.join(delim), {}, allTypes);
        } else {
            allSupportedFilesFilter = KFileFilter(i18n("All Supported Files"), {}, allTypes);
            d->m_hasAllSupportedFiles = true;
        }

        insertItem(0, allSupportedFilesFilter.label());
        d->m_filters.prepend(allSupportedFilesFilter);
        setCurrentIndex(0);
    }

    if (hasAllFilesFilter) {
        addItem(i18n("All Files"));
        d->m_filters.append(KFileFilter(i18n("All Files"), {}, {QStringLiteral("application/octet-stream")}));
    }

    d->m_lastFilter = currentText();
}

QVector<KFileFilter> KFileFilterCombo::fileFilters() const
{
    return d->m_filters;
}

#if KIOFILEWIDGETS_BUILD_DEPRECATED_SINCE(5, 101)
void KFileFilterCombo::setFilter(const QString &filterString)
{
    d->m_hasAllSupportedFiles = false;

    const QVector<KFileFilter> filters = KFileFilter::fromFilterString(filterString);

    setFileFilters(filters, KFileFilter{});

    d->m_lastFilter = currentText();
    d->m_isMimeFilter = false;
}
#endif

#if KIOFILEWIDGETS_BUILD_DEPRECATED_SINCE(5, 101)
QString KFileFilterCombo::currentFilter() const
{
    QString f = currentText();
    if (f == itemText(currentIndex())) { // user didn't edit the text
        f = d->m_filters.value(currentIndex()).toFilterString();
        if (d->m_isMimeFilter || (currentIndex() == 0 && d->m_hasAllSupportedFiles)) {
            return f; // we have a MIME type as filter
        }
    }

    int tab = f.indexOf(QLatin1Char('|'));
    if (tab < 0) {
        return f;
    } else {
        return f.left(tab);
    }
}
#endif

KFileFilter KFileFilterCombo::currentFileFilter() const
{
    if (currentText() != itemText(currentIndex())) {
        // The user edited the text

        const QVector<KFileFilter> filter = KFileFilter::fromFilterString(currentText());

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

#if KIOFILEWIDGETS_BUILD_DEPRECATED_SINCE(5, 101)
QStringList KFileFilterCombo::filters() const
{
    QStringList result;

    for (const KFileFilter &filter : std::as_const(d->m_filters)) {
        result << filter.toFilterString();
    }

    return result;
}
#endif

#if KIOFILEWIDGETS_BUILD_DEPRECATED_SINCE(5, 101)
void KFileFilterCombo::setCurrentFilter(const QString &filterString)
{
    auto it = std::find_if(d->m_filters.cbegin(), d->m_filters.cend(), [filterString](const KFileFilter &filter) {
        return filterString == filter.toFilterString();
    });

    if (it == d->m_filters.cend()) {
        qCWarning(KIO_KFILEWIDGETS_KFILEFILTERCOMBO) << "Could not find filter" << filterString;
        setCurrentIndex(-1);
        Q_EMIT filterChanged();
        return;
    }

    setCurrentIndex(std::distance(d->m_filters.cbegin(), it));
    Q_EMIT filterChanged();
}
#endif

void KFileFilterCombo::setCurrentFileFilter(const KFileFilter &filter)
{
    auto it = std::find(d->m_filters.cbegin(), d->m_filters.cend(), filter);

    if (it == d->m_filters.cend()) {
        qCWarning(KIO_KFILEWIDGETS_KFILEFILTERCOMBO) << "Could not find file filter";
        setCurrentIndex(-1);
        Q_EMIT filterChanged();
        return;
    }

    setCurrentIndex(std::distance(d->m_filters.cbegin(), it));
    Q_EMIT filterChanged();
}

#if KIOFILEWIDGETS_BUILD_DEPRECATED_SINCE(5, 101)
void KFileFilterCombo::setMimeFilter(const QStringList &types, const QString &defaultType)
{
    QVector<KFileFilter> filters;

    for (const QString &type : types) {
        filters << KFileFilter::fromMimeType(type);
    }

    setFileFilters(filters, KFileFilter::fromMimeType(defaultType));

    d->m_isMimeFilter = true;
}
#endif

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

#if KIOFILEWIDGETS_BUILD_DEPRECATED_SINCE(5, 101)
void KFileFilterCombo::setDefaultFilter(const QString &filter)
{
    const auto filters = KFileFilter::fromFilterString(filter);

    Q_ASSERT_X(filters.size() == 1, "setDefaultFilter", "Default filter must contain exactly one filter");

    d->m_defaultFilter = filters.first();
}
#endif

#if KIOFILEWIDGETS_BUILD_DEPRECATED_SINCE(5, 101)
QString KFileFilterCombo::defaultFilter() const
{
    return d->m_defaultFilter.toFilterString();
}
#endif

void KFileFilterCombo::setDefaultFileFilter(const KFileFilter &filter)
{
    d->m_defaultFilter = filter;
}

KFileFilter KFileFilterCombo::defaultFileFilter() const
{
    return d->m_defaultFilter;
}

#if KIOFILEWIDGETS_BUILD_DEPRECATED_SINCE(5, 101)
bool KFileFilterCombo::isMimeFilter() const
{
    return d->m_isMimeFilter;
}
#endif

#include "moc_kfilefiltercombo.cpp"
