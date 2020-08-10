/*
    This file is part of the KDE project
    SPDX-FileCopyrightText: 2000-2003 George Staikos <staikos@kde.org>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#ifndef _KSSLSETTINGS_H
#define _KSSLSETTINGS_H

#include "kiocore_export.h"

#include <QString>

#include <KConfig>

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
class KIOCORE_EXPORT KSSLSettings
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
    KSSLSettings& operator=(const KSSLSettings &) = delete;

    /**
     *  Does the user want to be warned on entering SSL mode
     *  @return true if the user wants to be warned
     */
    bool warnOnEnter() const;

    /**
     *  Change the user's warnOnEnter() setting
     *  @param x true if the user is to be warned
     *  @see warnOnEnter
     */
    void setWarnOnEnter(bool x);

    /**
     *  Does the user want to be warned on sending unencrypted data
     *  @return true if the user wants to be warned
     *  @see setWarnOnUnencrypted
     */
    bool warnOnUnencrypted() const;

    /**
     *  Change the user's warnOnUnencrypted() setting
     *  @param x true if the user is to be warned
     *  @see warnOnUnencrypted
     */
    void setWarnOnUnencrypted(bool x);

    /**
     *  Does the user want to be warned on leaving SSL mode
     *  @return true if the user wants to be warned
     */
    bool warnOnLeave() const;

    /**
     *  Change the user's warnOnLeave() setting
     *  @param x true if the user is to be warned
     *  @see warnOnLeave
     */
    void setWarnOnLeave(bool x);

    /**
     *  Does the user want to be warned during mixed SSL/non-SSL mode
     *  @return true if the user wants to be warned
     */
    bool warnOnMixed() const;

    /**
     *  Does the user want to use the Entropy Gathering Daemon?
     *  @return true if the user wants to use EGD
     */
    bool useEGD() const;

    /**
     *  Does the user want to use an entropy file?
     *  @return true if the user wants to use an entropy file
     */
    bool useEFile() const;

    /**
     *  Does the user want X.509 client certificates to always be sent when
     *  possible?
     *  @return true if the user always wants a certificate sent
     */
    bool autoSendX509() const;

    /**
     *  Does the user want to be prompted to send X.509 client certificates
     *  when possible?
     *  @return true if the user wants to be prompted
     */
    bool promptSendX509() const;

    /**
     *  Get the OpenSSL cipher list for selecting the list of ciphers to
     *  use in a connection.
     *  @return the cipher list
     */
    QString getCipherList();

    /**
     *  Get the configured path to the entropy gathering daemon or entropy
     *  file.
     *  @return the path
     */
    QString &getEGDPath();

    /**
     *  Load the user's settings.
     */
    void load();

    /**
     *  Revert to default settings.
     */
    void defaults();

    /**
     *  Save the current settings.
     */
    void save();

private:
    KSSLSettingsPrivate *const d;
};

#endif

