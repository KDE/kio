/*
    This file is part of the KDE libraries
    SPDX-FileCopyrightText: 2000 David Faure <faure@kde.org>

    SPDX-License-Identifier: LGPL-2.0-only
*/

#ifndef KIO_SKIPDIALOG_H
#define KIO_SKIPDIALOG_H

#include "kiowidgets_export.h"
#include <kio/jobuidelegateextension.h>
#include <QDialog>

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
    ~SkipDialog();

private Q_SLOTS:
    void cancelPressed();
    void skipPressed();
    void autoSkipPressed();
    void retryPressed();

Q_SIGNALS:
    void result(SkipDialog *_this, int _button);

private:
    SkipDialogPrivate *const d;
};

}
#endif
