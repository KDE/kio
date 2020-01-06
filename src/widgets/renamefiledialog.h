/* This file is part of the KDE libraries
    Copyright (C) 2006-2010 by Peter Penz (peter.penz@gmx.at)
    Copyright (C) 2020 by Méven Car (meven.car@kdemail.net)

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.
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

namespace KIO {
class RenameFileDialogPrivate;

/**
 * @class KIO::RenameFileDialog renamefiledialog.h <KIO/RenameFileDialog>
 *
 * @brief Dialog for renaming a variable number of files.
 *
 * The dialog deletes itself when accepted or rejected.
 *
 * @since 5.67
 */
// TODO KF6  : rename the class RenameFileDialog to RenameDialog and the class RenameDialog to RenameFileOverwrittenDialog or similar.
class KIOWIDGETS_EXPORT RenameFileDialog : public QDialog
{
    Q_OBJECT

public:
    /**
     * Contructs the Dialog to rename file(s)
     *
     * @param parent the parent QWidget
     * @param items a non-empty list of items to rename
     */
    explicit RenameFileDialog(const KFileItemList &items, QWidget *parent);
    ~RenameFileDialog() override;

Q_SIGNALS:
    void renamingFinished(const QList<QUrl> &urls);
    void error(KJob *error);

private Q_SLOTS:
    void slotAccepted();
    void slotTextChanged(const QString &newName);
    void slotFileRenamed(const QUrl &oldUrl, const QUrl &newUrl);
    void slotResult(KJob *job);

private:
    class RenameFileDialogPrivate;
    RenameFileDialogPrivate *const d;
};
} // namespace KIO

#endif
