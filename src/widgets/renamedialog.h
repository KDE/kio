/*
    This file is part of the KDE libraries
    SPDX-FileCopyrightText: 2000 Stephan Kulow <coolo@kde.org>
    SPDX-FileCopyrightText: 1999-2008 David Faure <faure@kde.org>
    SPDX-FileCopyrightText: 2001 Holger Freyther <freyther@kde.org>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#ifndef KIO_RENAMEDIALOG_H
#define KIO_RENAMEDIALOG_H

#include <kio/jobuidelegateextension.h>
#include <QDialog>
#include <QString>
#include <QDateTime>

#include <kio/global.h>
#include "kiowidgets_export.h"

class QScrollArea;
class QLabel;
class QPixmap;
class KFileItem;
class KSqueezedTextLabel;

namespace KIO
{

/**
 * @class KIO::RenameDialog renamedialog.h <KIO/RenameDialog>
 *
 * The dialog shown when a CopyJob realizes that a destination file already exists,
 * and wants to offer the user with the choice to either Rename, Overwrite, Skip;
 * this dialog is also used when a .part file exists and the user can choose to
 * Resume a previous download.
 */
class KIOWIDGETS_EXPORT RenameDialog : public QDialog
{
    Q_OBJECT
public:
    /**
     * Construct a "rename" dialog to let the user know that @p src is about to overwrite @p dest.
     *
     * @param parent parent widget (often 0)
     * @param caption the caption for the dialog box
     * @param src the url to the file/dir we're trying to copy, as it's part of the text message
     * @param dest the path to destination file/dir, i.e. the one that already exists
     * @param options parameters for the dialog (which buttons to show...),
     * @param sizeSrc size of source file
     * @param sizeDest size of destination file
     * @param ctimeSrc creation time of source file
     * @param ctimeDest creation time of destination file
     * @param mtimeSrc modification time of source file
     * @param mtimeDest modification time of destination file
     */
    RenameDialog(QWidget *parent, const QString &caption,
                 const QUrl &src, const QUrl &dest,
                 RenameDialog_Options options,
                 KIO::filesize_t sizeSrc = KIO::filesize_t(-1),
                 KIO::filesize_t sizeDest = KIO::filesize_t(-1),
                 const QDateTime &ctimeSrc = QDateTime(),
                 const QDateTime &ctimeDest = QDateTime(),
                 const QDateTime &mtimeSrc = QDateTime(),
                 const QDateTime &mtimeDest = QDateTime());
    ~RenameDialog();

    /**
     * @return the new destination
     * valid only if RENAME was chosen
     */
    QUrl newDestUrl();

    /**
     * @return an automatically renamed destination
     * @since 4.5
     * valid always
     */
    QUrl autoDestUrl() const;

#if KIOWIDGETS_ENABLE_DEPRECATED_SINCE(5, 0)
    /**
     * Given a directory path and a filename (which usually exists already),
     * this function returns a suggested name for a file that doesn't exist
     * in that directory. The existence is only checked for local urls though.
     * The suggested file name is of the form "foo 1", "foo 2" etc.
     * @deprecated Since 5.0. Use KIO::suggestName, since 5.61, use KFileUtils::suggestName from KCoreAddons
     */
    KIOWIDGETS_DEPRECATED_VERSION(5, 0, "Use KFileUtils::suggestName(const QUrl &, const QString &) from KCoreAddons >= 5.61, or KIO::suggestName(const QUrl &, const QString &)")
    static QString suggestName(const QUrl &baseURL, const QString &oldName);
#endif

public Q_SLOTS:
    void cancelPressed();
    void renamePressed();
    void skipPressed();
    void autoSkipPressed();
    void overwritePressed();
    void overwriteAllPressed();
    void overwriteWhenOlderPressed();
    void resumePressed();
    void resumeAllPressed();
    void suggestNewNamePressed();

protected Q_SLOTS:
    void enableRenameButton(const QString &);
private Q_SLOTS:
    void applyAllPressed();
    void showSrcIcon(const KFileItem &);
    void showDestIcon(const KFileItem &);
    void showSrcPreview(const KFileItem &, const QPixmap &);
    void showDestPreview(const KFileItem &, const QPixmap &);
    void resizePanels();

private:
    QScrollArea *createContainerLayout(QWidget *parent, const KFileItem &item, QLabel *preview);
    class RenameDialogPrivate;
    RenameDialogPrivate *const d;
};

}

#endif
