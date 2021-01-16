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
     * Sets the @p filter string.
     */
    void setFilter(const QString &filter);

    /**
     * @returns the current filter, either something like "*.cpp *.h"
     * or the current MIME type, like "text/html", or a list of those, like
     " "text/html text/plain image/png", all separated with one space.
     */
    QString currentFilter() const;

    /**
     * Sets the current filter. Filter must match one of the filter items
     * passed before to this widget.
     */
    void setCurrentFilter(const QString &filter);

    /**
     * Sets a list of MIME types.
     * If @p defaultType is set, it will be set as the current item.
     * Otherwise, a first item showing all the MIME types will be created.
     */
    void setMimeFilter(const QStringList &types, const QString &defaultType);

    /**
     * @return true if the filter's first item is the list of all MIME types
     */
    bool showsAllTypes() const;

    /**
     * This method allows you to set a default-filter, that is used when an
     * empty filter is set. Make sure you call this before calling
     * setFilter().
     *
     * By default, this is set to i18n("*|All Files")
     * @see defaultFilter
     */
    void setDefaultFilter(const QString &filter);

    /**
     * @return the default filter, used when an empty filter is set.
     * @see setDefaultFilter
     */
    QString defaultFilter() const;

    /**
     * @return all filters (this can be a list of patterns or a list of MIME types)
     */
    QStringList filters() const;

    /**
     * Returns true if the filter has been set using setMimeFilter().
     * @since 4.6.1
     */
    bool isMimeFilter() const;

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
