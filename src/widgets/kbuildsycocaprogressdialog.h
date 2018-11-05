/* This file is part of the KDE project
   Copyright (C) 2003 Waldo Bastian <bastian@kde.org>

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public
   License version 2 as published by the Free Software Foundation.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public License
   along with this library; see the file COPYING.LIB.  If not, write to
   the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
   Boston, MA 02110-1301, USA.
*/

#ifndef KBUILDSYCOCAPROGRESSDIALOG_H
#define KBUILDSYCOCAPROGRESSDIALOG_H

#include <QTimer>
#include <QProgressDialog>
#include "kiowidgets_export.h"

class KBuildSycocaProgressDialogPrivate;
/**
 * @class KBuildSycocaProgressDialog kbuildsycocaprogressdialog.h <KBuildSycocaProgressDialog>
 *
 * Progress dialog while ksycoca is being rebuilt (by kbuildsycoca).
 * Usage: KBuildSycocaProgressDialog::rebuildKSycoca(parentWidget)
 */
class KIOWIDGETS_EXPORT KBuildSycocaProgressDialog : public QProgressDialog
{
    Q_OBJECT
public:

    /**
     * Rebuild KSycoca and show a progress dialog while doing so.
     * @param parent Parent widget for the progress dialog
     */
    static void rebuildKSycoca(QWidget *parent);

private:
    KBuildSycocaProgressDialog(QWidget *parent,
                               const QString &caption, const QString &text);
    ~KBuildSycocaProgressDialog();

private:
    KBuildSycocaProgressDialogPrivate *const d;
};

#endif
