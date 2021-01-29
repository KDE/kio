/*
    SPDX-FileCopyrightText: 2006 Peter Penz <peter.penz@gmx.at>
    SPDX-FileCopyrightText: 2006 Aaron J. Seigo <aseigo@kde.org>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#ifndef KURLNAVIGATORBUTTON_P_H
#define KURLNAVIGATORBUTTON_P_H

#include "kurlnavigatorbuttonbase_p.h"
#include "kurlnavigatormenu_p.h"

#include <kio/global.h>
#include <kio/udsentry.h>

#include <QPointer>
#include <QUrl>

class KJob;
class QDropEvent;
class QPaintEvent;

namespace KIO
{
class ListJob;
class Job;
}

namespace KDEPrivate
{

/**
 * @brief Button of the URL navigator which contains one part of an URL.
 *
 * It is possible to drop a various number of items to an UrlNavigatorButton. In this case
 * a context menu is opened where the user must select whether he wants
 * to copy, move or link the dropped items to the URL part indicated by
 * the button.
 */
class KUrlNavigatorButton : public KUrlNavigatorButtonBase
{
    Q_OBJECT
    Q_PROPERTY(QString plainText READ plainText) // for the unittest

public:
    explicit KUrlNavigatorButton(const QUrl &url, KUrlNavigator *parent);
    virtual ~KUrlNavigatorButton();

    void setUrl(const QUrl &url);
    QUrl url() const;

    /* Implementation note: QAbstractButton::setText() is not virtual,
     * but KUrlNavigatorButton needs to adjust the minimum size when
     * the text has been changed. KUrlNavigatorButton::setText() hides
     * QAbstractButton::setText() which is not nice, but sufficient for
     * the usage in KUrlNavigator.
     */
    void setText(const QString &text);

    /**
     * Sets the name of the sub directory that should be marked when
     * opening the sub directories popup.
     */
    void setActiveSubDirectory(const QString &subDir);
    QString activeSubDirectory() const;

    /** @see QWidget::sizeHint() */
    QSize sizeHint() const override;

    void setShowMnemonic(bool show);
    bool showMnemonic() const;

Q_SIGNALS:
    /**
     * Emitted when URLs are dropped on the KUrlNavigatorButton associated with
     * the URL @p destination.
     */
    void urlsDroppedOnNavButton(const QUrl &destination, QDropEvent *event);

    void clicked(const QUrl &url, Qt::MouseButton button, Qt::KeyboardModifiers modifiers);

    /**
     * Is emitted, if KUrlNavigatorButton::setUrl() cannot resolve
     * the text synchronously and KUrlNavigator::text() will return
     * an empty string in this case. The signal finishedTextResolving() is
     * emitted, as soon as the text has been resolved.
     */
    void startedTextResolving();

    /**
     * Is emitted, if the asynchronous resolving of the text has
     * been finished (see startTextResolving()).
     * KUrlNavigatorButton::text() contains the resolved text.
     */
    void finishedTextResolving();

protected:
    void paintEvent(QPaintEvent *event) override;
    void enterEvent(QEvent *event) override;
    void leaveEvent(QEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void dropEvent(QDropEvent *event) override;
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dragMoveEvent(QDragMoveEvent *event) override;
    void dragLeaveEvent(QDragLeaveEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;

private Q_SLOTS:
    /**
     * Requests to load the sub-directories after a short delay.
     * startSubDirsJob() is invoked if the delay is exceeded.
     */
    void requestSubDirs();

    /**
     * Starts to load the sub directories asynchronously. The directories
     * are stored in m_subDirs by addEntriesToSubDirs().
     */
    void startSubDirsJob();

    /**
     * Adds the entries from the sub-directories job to m_subDirs. The entries
     * will be shown if the job has been finished in openSubDirsMenu() or
     * replaceButton().
     */
    void addEntriesToSubDirs(KIO::Job *job, const KIO::UDSEntryList &entries);

    /**
     * Is called after the sub-directories job has been finished and opens a menu
     * showing all sub directories.
     */
    void openSubDirsMenu(KJob *job);

    /**
     * Is called after the sub-directories job has been finished and replaces
     * the button content by the current sub directory (triggered by
     * the scroll wheel).
     */
    void replaceButton(KJob *job);

    void slotUrlsDropped(QAction *action, QDropEvent *event);

    /**
     * Is called, if an action of a sub-menu has been triggered by
     * a click.
     */
    void slotMenuActionClicked(QAction *action, Qt::MouseButton button);

    void statFinished(KJob *);

private:
    /**
     * Cancels any request done by requestSubDirs().
     */
    void cancelSubDirsRequest();

    /**
     * @return Text without mnemonic characters.
     */
    QString plainText() const;

    int arrowWidth() const;
    bool isAboveArrow(int x) const;
    bool isTextClipped() const;
    void updateMinimumWidth();
    void initMenu(KUrlNavigatorMenu *menu, int startIndex);

private:
    bool m_hoverArrow;
    bool m_pendingTextChange;
    bool m_replaceButton;
    bool m_showMnemonic;
    int m_wheelSteps;
    QUrl m_url;

    QString m_subDir;
    QTimer *m_openSubDirsTimer;
    KIO::ListJob *m_subDirsJob;

    /// pair of name and display name
    QList<QPair<QString, QString> > m_subDirs;

    static QPointer<KUrlNavigatorMenu> m_subDirsMenu;
};

} // namespace KDEPrivate

#endif
