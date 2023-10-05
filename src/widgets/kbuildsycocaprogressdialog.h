/*
    This file is part of the KDE project
    SPDX-FileCopyrightText: 2003 Waldo Bastian <bastian@kde.org>

    SPDX-License-Identifier: LGPL-2.0-only
*/

#ifndef KBUILDSYCOCAPROGRESSDIALOG_H
#define KBUILDSYCOCAPROGRESSDIALOG_H

#include "kiowidgets_export.h"
#include <QProgressDialog>
#include <QTimer>

#include <memory>

namespace KIO
{
class WidgetsBuildSycocaHandler;
} // namespace KIO

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
    friend class KIO::WidgetsBuildSycocaHandler;

public:
    /**
     * Rebuild KSycoca and show a progress dialog while doing so.
     * @param parent Parent widget for the progress dialog
     */
    KIOWIDGETS_DEPRECATED_VERSION(5, 240, "Use KIO::BuildSycocaJob (don't forget KJobWidgets::setWindow(&job, parent) if you want a UI with parent)")
    static void rebuildKSycoca(QWidget *parent);

protected:
    KIOWIDGETS_NO_EXPORT KBuildSycocaProgressDialog(QWidget *parent, const QString &title, const QString &text);
    KIOWIDGETS_NO_EXPORT ~KBuildSycocaProgressDialog() override;

private:
    std::unique_ptr<KBuildSycocaProgressDialogPrivate> const d;
};

#endif
