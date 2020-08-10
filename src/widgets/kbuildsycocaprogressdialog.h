/*
    This file is part of the KDE project
    SPDX-FileCopyrightText: 2003 Waldo Bastian <bastian@kde.org>

    SPDX-License-Identifier: LGPL-2.0-only
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
