/* This file is part of the KDE libraries
   Copyright (C) 2000 David Faure <faure@kde.org>

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
