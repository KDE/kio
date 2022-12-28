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
    QString m_defaultFilter = i18nc("Default mime type filter that shows all file types", "*|All Files");

    QVector<KFileFilter> m_fileFilters;
    QStringList m_filters;
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

void KFileFilterCombo::setFilter(const QString &filter)
{
    clear();
    d->m_filters.clear();
    d->m_fileFilters.clear();
    d->m_hasAllSupportedFiles = false;

    if (!filter.isEmpty()) {
        QString tmp = filter;
        int index = tmp.indexOf(QLatin1Char('\n'));
        while (index > 0) {
            d->m_filters.append(tmp.left(index));
            tmp.remove(0, index + 1);
            index = tmp.indexOf(QLatin1Char('\n'));
        }
        d->m_filters.append(tmp);
    } else {
        d->m_filters.append(d->m_defaultFilter);
    }

    QStringList::ConstIterator it;
    QStringList::ConstIterator end(d->m_filters.constEnd());
    for (it = d->m_filters.constBegin(); it != end; ++it) {
        int tab = (*it).indexOf(QLatin1Char('|'));
        addItem((tab < 0) ? *it : (*it).mid(tab + 1));
    }

    d->m_lastFilter = currentText();
    d->m_isMimeFilter = false;
}

QString KFileFilterCombo::currentFilter() const
{
    QString f = currentText();
    if (f == itemText(currentIndex())) { // user didn't edit the text

        if (!d->m_filters.isEmpty()) {
            f = d->m_filters.value(currentIndex());
        } else {
            f = d->m_fileFilters.value(currentIndex()).toFilterString();
        }

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

bool KFileFilterCombo::showsAllTypes() const
{
    return d->m_allTypes;
}

QStringList KFileFilterCombo::filters() const
{
    if (!d->m_filters.isEmpty()) {
        return d->m_filters;
    }

    QStringList result;

    for (const KFileFilter &filter : std::as_const(d->m_fileFilters)) {
        result << filter.toFilterString();
    }

    return result;
}

void KFileFilterCombo::setCurrentFilter(const QString &filterString)
{
    if (!d->m_filters.isEmpty()) {
        setCurrentIndex(d->m_filters.indexOf(filterString));
        return;
    }

    auto it = std::find_if(d->m_fileFilters.cbegin(), d->m_fileFilters.cend(), [filterString](const KFileFilter &filter) {
        return filterString == filter.toFilterString();
    });

    if (it == d->m_fileFilters.cend()) {
        qCWarning(KIO_KFILEWIDGETS_KFILEFILTERCOMBO) << "Could not find filter" << filterString;
        setCurrentIndex(-1);
        Q_EMIT filterChanged();
        return;
    }

    setCurrentIndex(std::distance(d->m_fileFilters.cbegin(), it));
    Q_EMIT filterChanged();
}

void KFileFilterCombo::setMimeFilter(const QStringList &types, const QString &defaultType)
{
    clear();
    d->m_filters.clear();
    d->m_fileFilters.clear();
    QString delim = QStringLiteral(", ");
    d->m_hasAllSupportedFiles = false;
    bool hasAllFilesFilter = false;
    QMimeDatabase db;

    d->m_allTypes = defaultType.isEmpty() && (types.count() > 1);

    // If there's MIME types that have the same comment, we will show the extension
    // in addition to the MIME type comment
    QHash<QString, int> allTypeComments;
    for (QStringList::ConstIterator it = types.begin(); it != types.end(); ++it) {
        const QMimeType type = db.mimeTypeForName(*it);
        if (!type.isValid()) {
            qCWarning(KIO_KFILEWIDGETS_KFILEFILTERCOMBO) << *it << "is not a valid MIME type";
            continue;
        }

        allTypeComments[type.comment()] += 1;
    }

    for (QStringList::ConstIterator it = types.begin(); it != types.end(); ++it) {
        // qDebug() << *it;
        const QMimeType type = db.mimeTypeForName(*it);
        if (!type.isValid()) {
            qCWarning(KIO_KFILEWIDGETS_KFILEFILTERCOMBO) << *it << "is not a valid MIME type";
            continue;
        }

        if (type.name().startsWith(QLatin1String("all/")) || type.isDefault()) {
            hasAllFilesFilter = true;
            continue;
        }

        KFileFilter filter;

        if (allTypeComments.value(type.comment()) > 1) {
            const QString label = i18nc("%1 is the mimetype name, %2 is the extensions", "%1 (%2)", type.comment(), type.suffixes().join(QLatin1String(", ")));
            filter = KFileFilter(label, {}, {*it});
        } else {
            filter = KFileFilter::fromMimeType(*it);
        }

        d->m_fileFilters.append(filter);
        addItem(filter.label());

        if (type.name() == defaultType) {
            setCurrentIndex(count() - 1);
        }
    }

    if (count() == 1) {
        d->m_allTypes = false;
    }

    if (d->m_allTypes) {
        QStringList allTypes;
        for (const KFileFilter &filter : std::as_const(d->m_fileFilters)) {
            allTypes << filter.mimePatterns().join(QLatin1Char(' '));
        }

        KFileFilter allSupportedFilesFilter;

        if (count() <= 3) { // show the MIME type comments of at max 3 types
            QStringList allComments;
            for (const KFileFilter &filter : std::as_const(d->m_fileFilters)) {
                allComments << filter.label();
            }

            allSupportedFilesFilter = KFileFilter(allComments.join(delim), {}, allTypes);
        } else {
            allSupportedFilesFilter = KFileFilter(i18n("All Supported Files"), {}, allTypes);
            d->m_hasAllSupportedFiles = true;
        }

        insertItem(0, allSupportedFilesFilter.label());
        d->m_fileFilters.prepend(allSupportedFilesFilter);
        setCurrentIndex(0);
    }

    if (hasAllFilesFilter) {
        addItem(i18n("All Files"));
        d->m_fileFilters.append(KFileFilter(i18n("All Files"), {}, {QStringLiteral("application/octet-stream")}));
    }

    d->m_lastFilter = currentText();
    d->m_isMimeFilter = true;
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

void KFileFilterCombo::setDefaultFilter(const QString &filter)
{
    d->m_defaultFilter = filter;
}

QString KFileFilterCombo::defaultFilter() const
{
    return d->m_defaultFilter;
}

bool KFileFilterCombo::isMimeFilter() const
{
    return d->m_isMimeFilter;
}

#include "moc_kfilefiltercombo.cpp"
