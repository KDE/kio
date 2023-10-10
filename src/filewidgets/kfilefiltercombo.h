/*
    This file is part of the KDE libraries
    SPDX-FileCopyrightText: Stephan Kulow <coolo@kde.org>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#ifndef KFILEFILTERCOMBO_H
#define KFILEFILTERCOMBO_H

#include "kiofilewidgets_export.h"

#include <QStringList>

#include <KComboBox>
#include <KFileFilter>

class KFileFilterComboPrivate;

/**
 * @class KFileFilterCombo kfilefiltercombo.h <KFileFilterCombo>
 *
 * File filter combo box.
 */
class KIOFILEWIDGETS_EXPORT KFileFilterCombo : public KComboBox
{
    Q_OBJECT

public:
    /**
     * Creates a new filter combo box.
     *
     * @param parent The parent widget.
     */
    explicit KFileFilterCombo(QWidget *parent = nullptr);

    /**
     * Destroys the filter combo box.
     */
    ~KFileFilterCombo() override;

    /**
     * Sets the filters to be used.
     *
     * @param filters each item in the list corresponds to one item in the combobox.
     * Entries for "All files" and "All supported files" are added automatically as needed.
     *
     * @param defaultFilter if not empty this will be the by default active filter
     *
     * @since 6.0
     *
     */
    void setFilters(const QList<KFileFilter> &filters, const KFileFilter &defaultFilter = KFileFilter());

    /**
     * The currently selected/active filter.
     *
     * @since 6.0
     */
    KFileFilter currentFilter() const;

    /**
     * The current filters.
     *
     * This is not necessarily the same as the list set by setFileFilters() since
     * entries for "All files" and "All supported files" are added automatically as needed.
     *
     * @since 6.0
     */
    QList<KFileFilter> filters() const;

    /**
     * This method allows to set a default-filter, that is used when an
     * empty filter is set. Make sure you call this before calling
     * setFileFilter().
     *
     * By default, this is set to match all files.
     * @see defaultFileFilter
     *
     * @since 6.0
     */
    void setDefaultFilter(const KFileFilter &filter);

    /**
     * @return the default filter, used when an empty filter is set.
     * @see setDefaultFileFilter
     *
     * @since 6.0
     */
    KFileFilter defaultFilter() const;

    /**
     * Sets the current filter. Filter must match one of the filter items
     * passed before to this widget.
     *
     * @since 6.0
     */
    void setCurrentFilter(const KFileFilter &filter);

    /**
     * @return true if the filter's first item is the list of all MIME types
     */
    bool showsAllTypes() const;

protected:
    bool eventFilter(QObject *, QEvent *) override;

Q_SIGNALS:
    /**
     * This signal is emitted whenever the filter has been changed.
     */
    void filterChanged();

private:
    std::unique_ptr<KFileFilterComboPrivate> const d;
};

#endif
