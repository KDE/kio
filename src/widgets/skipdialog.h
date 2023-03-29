/*
    This file is part of the KDE libraries
    SPDX-FileCopyrightText: 2000 David Faure <faure@kde.org>

    SPDX-License-Identifier: LGPL-2.0-only
*/

#ifndef KIO_SKIPDIALOG_H
#define KIO_SKIPDIALOG_H

#include "kiowidgets_export.h"
#include <QDialog>
#include <kio/jobuidelegateextension.h>

class QWidget;

namespace KIO
{
class SkipDialogPrivate;
/**
 * @internal
 */
class KIOWIDGETS_EXPORT SkipDialog : public QDialog
{
    Q_OBJECT
public:
    SkipDialog(QWidget *parent, KIO::SkipDialog_Options options, const QString &_error_text);
    ~SkipDialog() override;

private Q_SLOTS:
    KIOWIDGETS_NO_EXPORT void cancelPressed();
    KIOWIDGETS_NO_EXPORT void skipPressed();
    KIOWIDGETS_NO_EXPORT void autoSkipPressed();
    KIOWIDGETS_NO_EXPORT void retryPressed();

private:
    SkipDialogPrivate *const d;
};

}
#endif
