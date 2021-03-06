/*
    main.cpp for creating the konqueror kio kcm modules
    SPDX-FileCopyrightText: 2000, 2001, 2009 Alexander Neundorf <neundorf@kde.org>
    SPDX-FileCopyrightText: Torben Weis 1998
    SPDX-FileCopyrightText: David Faure 1998

    SPDX-License-Identifier: GPL-2.0-or-later
*/

// Qt

// KDE
#include <KPluginFactory>

// Local
#include "kcookiesmain.h"
#include "kproxydlg.h"
#include "netpref.h"
#include "smbrodlg.h"

K_PLUGIN_FACTORY(KioConfigFactory, registerPlugin<SMBRoOptions>(QStringLiteral("smb")); registerPlugin<KIOPreferences>(QStringLiteral("netpref"));
                 registerPlugin<KProxyDialog>(QStringLiteral("proxy"));
                 registerPlugin<KCookiesMain>(QStringLiteral("cookie"));)

#include "main.moc"
