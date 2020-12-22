/*
    This file is part of the KDE libraries
    SPDX-FileCopyrightText: Stephan Kulow <coolo@kde.org>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#include "kfilefiltercombo.h"
#include "kfilefiltercombo_debug.h"

#include <QDebug>
#include <KLocalizedString>
#include <QMimeDatabase>
#include <config-kiofilewidgets.h>
#include <QEvent>
#include <QLineEdit>

class Q_DECL_HIDDEN KFileFilterCombo::Private
{
public:
    explicit Private(KFileFilterCombo *_parent)
        : parent(_parent),
          hasAllSupportedFiles(false),
          isMimeFilter(false),
          defaultFilter(i18n("*|All Files"))
    {
    }

    void _k_slotFilterChanged();

    KFileFilterCombo * const parent;
    // when we have more than 3 mimefilters and no default-filter,
    // we don't show the comments of all mimefilters in one line,
    // instead we show "All supported files". We have to translate
    // that back to the list of mimefilters in currentFilter() tho.
    bool hasAllSupportedFiles;
    // true when setMimeFilter was called
    bool isMimeFilter;
    QString lastFilter;
    QString defaultFilter;

    QStringList m_filters;
    bool m_allTypes;
};

KFileFilterCombo::KFileFilterCombo(QWidget *parent)
    : KComboBox(true, parent), d(new Private(this))
{
    setTrapReturnKey(true);
    setInsertPolicy(QComboBox::NoInsert);
    connect(this, QOverload<int>::of(&QComboBox::activated), this, &KFileFilterCombo::filterChanged);
    connect(this, QOverload<>::of(&KComboBox::returnPressed), this, &KFileFilterCombo::filterChanged);
    connect(this, SIGNAL(filterChanged()), SLOT(_k_slotFilterChanged()));
    d->m_allTypes = false;
}

KFileFilterCombo::~KFileFilterCombo()
{
    delete d;
}

void KFileFilterCombo::setFilter(const QString &filter)
{
    clear();
    d->m_filters.clear();
    d->hasAllSupportedFiles = false;

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
        d->m_filters.append(d->defaultFilter);
    }

    QStringList::ConstIterator it;
    QStringList::ConstIterator end(d->m_filters.constEnd());
    for (it = d->m_filters.constBegin(); it != end; ++it) {
        int tab = (*it).indexOf(QLatin1Char('|'));
        addItem((tab < 0) ? *it :
                (*it).mid(tab + 1));
    }

    d->lastFilter = currentText();
    d->isMimeFilter = false;
}

QString KFileFilterCombo::currentFilter() const
{
    QString f = currentText();
    if (f == itemText(currentIndex())) { // user didn't edit the text
        f = d->m_filters.value(currentIndex());
        if (d->isMimeFilter || (currentIndex() == 0 && d->hasAllSupportedFiles)) {
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
    return d->m_filters;
}

void KFileFilterCombo::setCurrentFilter(const QString &filter)
{
    setCurrentIndex(d->m_filters.indexOf(filter));
    emit filterChanged();
}

void KFileFilterCombo::setMimeFilter(const QStringList &types,
                                     const QString &defaultType)
{
    clear();
    d->m_filters.clear();
    QString delim = QStringLiteral(", ");
    d->hasAllSupportedFiles = false;
    bool hasAllFilesFilter = false;
    QMimeDatabase db;

    d->m_allTypes = defaultType.isEmpty() && (types.count() > 1);

    QString allComments, allTypes;

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

        if (d->m_allTypes && it != types.begin()) {
            allComments += delim;
            allTypes += QLatin1Char(' ');
        }

        d->m_filters.append(type.name());
        if (d->m_allTypes) {
            allTypes += type.name();
            allComments += type.comment();
        }
        if (allTypeComments.value(type.comment()) > 1) {
            addItem(i18nc("%1 is the mimetype name, %2 is the extensions", "%1 (%2)", type.comment(), type.suffixes().join(QStringLiteral(", "))));
        } else {
            addItem(type.comment());
        }
        if (type.name() == defaultType) {
            setCurrentIndex(count() - 1);
        }
    }

    if (count() == 1) {
        d->m_allTypes = false;
    }

    if (d->m_allTypes) {
        if (count() <= 3) { // show the MIME type comments of at max 3 types
            insertItem(0, allComments);
        } else {
            insertItem(0, i18n("All Supported Files"));
            d->hasAllSupportedFiles = true;
        }
        setCurrentIndex(0);

        d->m_filters.prepend(allTypes);
    }

    if (hasAllFilesFilter) {
        addItem(i18n("All Files"));
        d->m_filters.append(QStringLiteral("application/octet-stream"));
    }

    d->lastFilter = currentText();
    d->isMimeFilter = true;
}

void KFileFilterCombo::Private::_k_slotFilterChanged()
{
    lastFilter = parent->currentText();
}

bool KFileFilterCombo::eventFilter(QObject *o, QEvent *e)
{
    if (o == lineEdit() && e->type() == QEvent::FocusOut) {
        if (currentText() != d->lastFilter) {
            emit filterChanged();
        }
    }

    return KComboBox::eventFilter(o, e);
}

void KFileFilterCombo::setDefaultFilter(const QString &filter)
{
    d->defaultFilter = filter;
}

QString KFileFilterCombo::defaultFilter() const
{
    return d->defaultFilter;
}

bool KFileFilterCombo::isMimeFilter() const
{
    return d->isMimeFilter;
}

#include "moc_kfilefiltercombo.cpp"
