/*
    This file is part of the KDE libraries
    SPDX-FileCopyrightText: 2006-2010 Peter Penz <peter.penz@gmx.at>
    SPDX-FileCopyrightText: 2020 Méven Car <meven.car@kdemail.net>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#ifndef RENAMEFILEDIALOG_H
#define RENAMEFILEDIALOG_H

#include <KFileItem>

#include "kiowidgets_export.h"

#include <QDialog>
#include <QString>

class QLineEdit;
class QSpinBox;
class QPushButton;
class KJob;

namespace KIO
{
class RenameFileDialogPrivate;

// TODO KF6  : rename the class RenameFileDialog to RenameDialog and the class RenameDialog to RenameFileOverwrittenDialog or similar.
/*!
 * \class KIO::RenameFileDialog
 * \inheaderfile KIO/RenameFileDialog
 * \inmodule KIOWidgets
 *
 * \brief Dialog for renaming a variable number of files.
 *
 * The dialog deletes itself when accepted or rejected.
 *
 * \since 5.67
 */
class KIOWIDGETS_EXPORT RenameFileDialog : public QDialog
{
    Q_OBJECT

public:
    /*!
     * Constructs the Dialog to rename file(s)
     *
     * \a parent the parent QWidget
     *
     * \a items a non-empty list of items to rename
     */
    explicit RenameFileDialog(const KFileItemList &items, QWidget *parent);
    ~RenameFileDialog() override;

Q_SIGNALS:
    /*!
     *
     */
    void renamingFinished(const QList<QUrl> &urls);

    /*!
     *
     */
    void error(KJob *error);

private Q_SLOTS:
    KIOWIDGETS_NO_EXPORT void slotAccepted();
    KIOWIDGETS_NO_EXPORT void slotOperationChanged(int index);
    KIOWIDGETS_NO_EXPORT void slotStateChanged();
    KIOWIDGETS_NO_EXPORT void slotFileRenamed(const QUrl &oldUrl, const QUrl &newUrl);
    KIOWIDGETS_NO_EXPORT void slotResult(KJob *job);

private:
    class RenameFileDialogPrivate;
    RenameFileDialogPrivate *const d;
};
} // namespace KIO

#endif
