/*
    This file is part of the KDE project
    SPDX-FileCopyrightText: 2000 George Staikos <staikos@kde.org>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#include "ksslsettings.h"

#include <KConfigGroup>

class KSSLSettingsPrivate
{
public:
    KSSLSettingsPrivate()
    {
    }
    ~KSSLSettingsPrivate()
    {
    }

    KConfig *m_cfg;
    bool m_bWarnOnEnter, m_bWarnOnLeave;
};

KSSLSettings::KSSLSettings(bool readConfig)
    : d(new KSSLSettingsPrivate)
{
    d->m_cfg = new KConfig(QStringLiteral("cryptodefaults"), KConfig::NoGlobals);

    if (readConfig) {
        load();
    }
}

// we don't save settings in case it was a temporary object
KSSLSettings::~KSSLSettings()
{
    delete d->m_cfg;
}

// FIXME - sync these up so that we can use them with the control module!!
void KSSLSettings::load()
{
    d->m_cfg->reparseConfiguration();

    KConfigGroup cfg(d->m_cfg, "Warnings");
    d->m_bWarnOnEnter = cfg.readEntry("OnEnter", false);
    d->m_bWarnOnLeave = cfg.readEntry("OnLeave", true);
}
bool KSSLSettings::warnOnEnter() const
{
    return d->m_bWarnOnEnter;
}
bool KSSLSettings::warnOnLeave() const
{
    return d->m_bWarnOnLeave;
}
