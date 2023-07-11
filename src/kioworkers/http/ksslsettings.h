/*
    This file is part of the KDE project
    SPDX-FileCopyrightText: 2000-2003 George Staikos <staikos@kde.org>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#ifndef _KSSLSETTINGS_H
#define _KSSLSETTINGS_H

#include <QString>

#include <KConfig>

#include <memory>

class KSSLSettingsPrivate;

/**
 * KDE SSL Settings
 *
 * This class contains some of the SSL settings for easy use.
 *
 * @author George Staikos <staikos@kde.org>
 * @see KSSL
 * @short KDE SSL Settings
 */
class KSSLSettings
{
public:
    /**
     *  Construct a KSSL Settings object
     *
     *  @param readConfig read in the configuration immediately if true
     */
    KSSLSettings(bool readConfig = true);

    /**
     *  Destroy this KSSL Settings object
     */
    ~KSSLSettings();

    KSSLSettings(const KSSLSettings &) = delete;
    KSSLSettings &operator=(const KSSLSettings &) = delete;

    /**
     *  Does the user want to be warned on entering SSL mode
     *  @return true if the user wants to be warned
     */
    bool warnOnEnter() const;

    /**
     *  Does the user want to be warned on leaving SSL mode
     *  @return true if the user wants to be warned
     */
    bool warnOnLeave() const;

    /**
     *  Load the user's settings.
     */
    void load();

private:
    std::unique_ptr<KSSLSettingsPrivate> const d;
};

#endif
