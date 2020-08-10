/* -*- c++ -*-
    SPDX-FileCopyrightText: 2000 Waldo Bastian <bastian@kde.org>

    SPDX-License-Identifier: BSD-2-Clause
*/

#ifndef __KRECENTDIRS_H
#define __KRECENTDIRS_H

#include <QStringList>

#include "kiofilewidgets_export.h"

/**
 * The goal of this namespace is to make sure that, when the user needs to
 * specify a file via the file selection dialog, this dialog will start
 * in the directory most likely to contain the desired files.
 *
 * This works as follows: Each time the file selection dialog is
 * shown, the programmer can specify a "file-class". The file-dialog will
 * then start with the directory associated with this file-class. When
 * the dialog closes, the directory currently shown in the file-dialog
 * will be associated with the file-class.
 *
 * A file-class can either start with ':' or with '::'. If it starts with
 * a single ':' the file-class is specific to the current application.
 * If the file-class starts with '::' it is global to all applications.
 *
 * @since 4.6
 */
namespace KRecentDirs
{
/**
 * Returns a list of directories associated with this file-class.
 * The most recently used directory is at the front of the list.
 *
 * @since 4.6
 */
KIOFILEWIDGETS_EXPORT QStringList list(const QString &fileClass);

/**
 * Returns the most recently used directory associated with this file-class.
 *
 * @since 4.6
 */
KIOFILEWIDGETS_EXPORT QString dir(const QString &fileClass);

/**
 * Associates @p directory with @p fileClass
 *
 * @since 4.6
 */
KIOFILEWIDGETS_EXPORT void add(const QString &fileClass, const QString &directory);
}

#endif
