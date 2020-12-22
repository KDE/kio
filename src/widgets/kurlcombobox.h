/*
    This file is part of the KDE libraries
    SPDX-FileCopyrightText: 2000 Carsten Pfeiffer <pfeiffer@kde.org>

    SPDX-License-Identifier: LGPL-2.0-only
*/

#ifndef KURLCOMBOBOX_H
#define KURLCOMBOBOX_H

#include "kiowidgets_export.h"

#include <QList>
#include <QMap>
#include <QStringList>
#include <QIcon>

#include <KComboBox>
class QUrl;
class KUrlComboBoxPrivate;

/**
 * @class KUrlComboBox kurlcombobox.h <KUrlComboBox>
 *
 * This combobox shows a number of recent URLs/directories, as well as some
 * default directories.
 * It will manage the default dirs root-directory, home-directory and
 * Desktop-directory, as well as a number of URLs set via setUrls()
 * and one additional entry to be set via setUrl().
 *
 * This widget forces the layout direction to be Qt::LeftToRight instead
 * of inheriting the layout direction like a normal widget. This means
 * that even in RTL desktops the widget will be displayed in LTR mode,
 * as generally URLs are LTR by nature.
 *
 * @short A combo box showing a number of recent URLs/directories
 * @author Carsten Pfeiffer <pfeiffer@kde.org>
 */
class KIOWIDGETS_EXPORT KUrlComboBox : public KComboBox
{
    Q_OBJECT
    Q_PROPERTY(QStringList urls READ urls WRITE setUrls DESIGNABLE true)
    Q_PROPERTY(int maxItems READ maxItems WRITE setMaxItems DESIGNABLE true)

public:
    /**
     * This enum describes which kind of items is shown in the combo box.
     */
    enum Mode { Files = -1, Directories = 1, Both = 0 };
    /**
     * This Enumeration is used in setUrl() to determine which items
     * will be removed when the given list is larger than maxItems().
     *
     * @li RemoveTop means that items will be removed from top
     * @li RemoveBottom means, that items will be removed from the bottom
     */
    enum OverLoadResolving { RemoveTop, RemoveBottom };

    /**
     * Constructs a KUrlComboBox.
     * @param mode is either Files, Directories or Both and controls the
     * following behavior:
     * @li Files  all inserted URLs will be treated as files, therefore the
     *            url shown in the combo will never show a trailing /
     *            the icon will be the one associated with the file's MIME type.
     * @li Directories  all inserted URLs will be treated as directories, will
     *                  have a trailing slash in the combobox. The current
     *                  directory will show the "open folder" icon, other
     *                  directories the "folder" icon.
     * @li Both  Don't mess with anything, just show the url as given.
     * @param parent The parent object of this widget.
     */
    explicit KUrlComboBox(Mode mode, QWidget *parent = nullptr);
    KUrlComboBox(Mode mode, bool rw, QWidget *parent = nullptr);
    /**
     * Destructs the combo box.
     */
    ~KUrlComboBox();

    /**
     * Sets the current url. This combo handles exactly one url additionally
     * to the default items and those set via setUrls(). So you can call
     * setUrl() as often as you want, it will always replace the previous one
     * set via setUrl().
     * If @p url is already in the combo, the last item will stay there
     * and the existing item becomes the current item.
     * The current item will always have the open-directory-pixmap as icon.
     *
     * Note that you won't receive any signals, e.g. textChanged(),
     * returnPressed() or activated() upon calling this method.
     */
    void setUrl(const QUrl &url);

    /**
     * Inserts @p urls into the combobox below the "default urls" (see
     * addDefaultUrl).
     *
     * If the list of urls contains more items than maxItems, the first items
     * will be stripped.
     */
    void setUrls(const QStringList &urls);

    /**
     * Inserts @p urls into the combobox below the "default urls" (see
     * addDefaultUrl).
     *
     * If the list of urls contains more items than maxItems, the @p remove
     * parameter determines whether the first or last items will be stripped.
     */
    void setUrls(const QStringList &urls, OverLoadResolving remove);

    /**
     * @returns a list of all urls currently handled. The list contains at most
     * maxItems() items.
     * Use this to save the list of urls in a config-file and reinsert them
     * via setUrls() next time.
     * Note that all default urls set via addDefaultUrl() are not
     * returned, they will automatically be set via setUrls() or setUrl().
     * You will always get fully qualified urls, i.e. with protocol like
     * file:/
     */
    QStringList urls() const;

    /**
     * Sets how many items should be handled and displayed by the combobox.
     * @see maxItems
     */
    void setMaxItems(int);

    /**
     * @returns the maximum of items the combobox handles.
     * @see setMaxItems
     */
    int maxItems() const;

    /**
     * Adds a url that will always be shown in the combobox, it can't be
     * "rotated away". Default urls won't be returned in urls() and don't
     * have to be set via setUrls().
     * If you want to specify a special pixmap, use the overloaded method with
     * the pixmap parameter.
     * Default URLs will be inserted into the combobox by setDefaults()
     */
    void addDefaultUrl(const QUrl &url, const QString &text = QString());

    /**
     * Adds a url that will always be shown in the combobox, it can't be
     * "rotated away". Default urls won't be returned in urls() and don't
     * have to be set via setUrls().
     * If you don't need to specify a pixmap, use the overloaded method without
     * the pixmap parameter.
     * Default URLs will be inserted into the combobox by setDefaults()
     */
    void addDefaultUrl(const QUrl &url, const QIcon &icon,
                       const QString &text = QString());

    /**
     * Clears all items and inserts the default urls into the combo. Will be
     * called implicitly upon the first call to setUrls() or setUrl()
     * @see addDefaultUrl
     */
    void setDefaults();

    /**
     * Removes any occurrence of @p url. If @p checkDefaultUrls is false
     * default-urls won't be removed.
     */
    void removeUrl(const QUrl &url, bool checkDefaultURLs = true);

    /**
     * Reimplemented from KComboBox (from KCompletion)
     * @internal
     */
    void setCompletionObject(KCompletion *compObj, bool hsig = true) override;

Q_SIGNALS:
    /**
     * Emitted when an item was clicked at.
     * @param url is the url of the now current item.
     */
    void urlActivated(const QUrl &url);

protected:
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;

private:
    friend class KUrlComboBoxPrivate;
    KUrlComboBoxPrivate *const d;

    Q_DISABLE_COPY(KUrlComboBox)

    Q_PRIVATE_SLOT(d, void _k_slotActivated(int))
};

#endif // KURLCOMBOBOX_H
