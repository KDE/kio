/*
    SPDX-FileCopyrightText: 2017 Chinmoy Ranjan Pradhan <chinmoyrp65@gmail.com>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/

#ifndef FILEHELPER_H
#define FILEHELPER_H

#include <KAuth>

using namespace KAuth;

/**
 * This KAuth helper is responsible for performing file operations with
 * root privileges.
 */
class FileHelper : public QObject
{
  Q_OBJECT

public Q_SLOTS:
    /**
     * Execute action with root privileges.
     **/
    KAuth::ActionReply exec(const QVariantMap &args);
};

#endif
