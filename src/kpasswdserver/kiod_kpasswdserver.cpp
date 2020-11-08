/*
    This file is part of the KDE libraries
    SPDX-FileCopyrightText: 2015 Alex Richardson <arichardson.kde@gmail.com>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/

#include "kpasswdserver.h"

#include <KPluginFactory>

K_PLUGIN_CLASS_WITH_JSON(KPasswdServer, "kpasswdserver.json")

#include "kiod_kpasswdserver.moc"
