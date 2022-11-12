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

#include "kfilefilter.h"

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
     * @since 5.101
     *
     */
    void setFileFilters(const QVector<KFileFilter> &filters, const KFileFilter &defaultFilter);

    /**
     * The current filters.
     *
     * This is not necessarily the same as the list set by setFileFilters() since
     * entries for "All files" and "All supported files" are added automatically as needed.
     *
     * @since 5.101
     */
    QVector<KFileFilter> fileFilters() const;

#if KIOFILEWIDGETS_ENABLE_DEPRECATED_SINCE(5, 101)
    /**
     * Sets the @p filter string.
     *
     * @deprecated since 5.101, use setFileFilters() instead
     */
    KIOFILEWIDGETS_DEPRECATED_VERSION(5, 101, "Use setFileFilters() instead")
    void setFilter(const QString &filter);
#endif

#if KIOFILEWIDGETS_ENABLE_DEPRECATED_SINCE(5, 101)
    /**
     * @returns the current filter, either something like "*.cpp *.h"
     * or the current MIME type, like "text/html", or a list of those, like
     " "text/html text/plain image/png", all separated with one space.
     *
     * @deprecated since 5.101, use currentFileFilter() instead.
     */
    KIOFILEWIDGETS_DEPRECATED_VERSION(5, 101, "Use currentFileFilter() instead")
    QString currentFilter() const;
#endif

    /**
     * The currently selected/active filter.
     *
     * @since 5.101
     */
    KFileFilter currentFileFilter() const;

#if KIOFILEWIDGETS_ENABLE_DEPRECATED_SINCE(5, 101)
    /**
     * Sets the current filter. Filter must match one of the filter items
     * passed before to this widget.
     *
     * @deprecated since 5.101, use setCurrentFileFilter() instead.
     */
    KIOFILEWIDGETS_DEPRECATED_VERSION(5, 101, "Use setCurrentFileFilter() instead")
    void setCurrentFilter(const QString &filter);
#endif

    /**
     * Sets the current filter. Filter must match one of the filter items
     * passed before to this widget.
     *
     * @since 5.101
     */
    void setCurrentFileFilter(const KFileFilter &filter);

#if KIOFILEWIDGETS_ENABLE_DEPRECATED_SINCE(5, 101)
    /**
     * Sets a list of MIME types.
     * If @p defaultType is set, it will be set as the current item.
     * Otherwise, a first item showing all the MIME types will be created.
     *
     * @deprecated since 5.101, use setFileFilters() instead.
     */
    KIOFILEWIDGETS_DEPRECATED_VERSION(5, 101, "Use setFileFilters() instead")
    void setMimeFilter(const QStringList &types, const QString &defaultType);
#endif

    /**
     * @return true if the filter's first item is the list of all MIME types
     */
    bool showsAllTypes() const;

#if KIOFILEWIDGETS_ENABLE_DEPRECATED_SINCE(5, 101)
    /**
     * This method allows you to set a default-filter, that is used when an
     * empty filter is set. Make sure you call this before calling
     * setFilter().
     *
     * By default, this is set to i18n("*|All Files")
     * @see defaultFilter
     *
     * @deprecated since 5.101, use setDefaultFileFilter() instead.
     */
    KIOFILEWIDGETS_DEPRECATED_VERSION(5, 101, "Use setDefaultFileFilter() instead")
    void setDefaultFilter(const QString &filter);
#endif

#if KIOFILEWIDGETS_ENABLE_DEPRECATED_SINCE(5, 101)
    /**
     * @return the default filter, used when an empty filter is set.
     * @see setDefaultFilter
     *
     * @deprecated since 5.101, use defaultFileFilter() instead.
     */
    KIOFILEWIDGETS_DEPRECATED_VERSION(5, 101, "Use defaultFileFilters instead")
    QString defaultFilter() const;
#endif

    /**
     * This method allows to set a default-filter, that is used when an
     * empty filter is set. Make sure you call this before calling
     * setFileFilter().
     *
     * By default, this is set to match all files.
     * @see defaultFileFilter
     *
     * @since 5.101
     */
    void setDefaultFileFilter(const KFileFilter &filter);

    /**
     * @return the default filter, used when an empty filter is set.
     * @see setDefaultFileFilter
     *
     * @since 5.101
     */
    KFileFilter defaultFileFilter() const;

#if KIOFILEWIDGETS_ENABLE_DEPRECATED_SINCE(5, 101)
    /**
     * @return all filters (this can be a list of patterns or a list of MIME types)
     *
     * @deprecated since 5.101, use fileFilters() instead.
     */
    KIOFILEWIDGETS_DEPRECATED_VERSION(5, 101, "Use fileFilters instead")
    QStringList filters() const;
#endif

#if KIOFILEWIDGETS_ENABLE_DEPRECATED_SINCE(5, 101)
    /**
     * Returns true if the filter has been set using setMimeFilter().
     * @since 4.6.1
     */
    bool isMimeFilter() const;
#endif

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
