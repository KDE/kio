/*
    This file is part of the KDE project
    SPDX-FileCopyrightText: 2000 George Staikos <staikos@kde.org>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#include "ksslsettings.h"


#include <KConfigGroup>

#include <QStandardPaths>

class CipherNode
{
public:
    CipherNode(const char *_name, int _keylen) :
        name(QString::fromUtf8(_name)), keylen(_keylen) {}
    QString name;
    int keylen;
    inline int operator==(CipherNode &x)
    {
        return ((x.keylen == keylen) && (x.name == name));
    }
    inline int operator< (CipherNode &x)
    {
        return keylen < x.keylen;
    }
    inline int operator<=(CipherNode &x)
    {
        return keylen <= x.keylen;
    }
    inline int operator> (CipherNode &x)
    {
        return keylen > x.keylen;
    }
    inline int operator>=(CipherNode &x)
    {
        return keylen >= x.keylen;
    }
};

class KSSLSettingsPrivate
{
public:
    KSSLSettingsPrivate()
    {
    }
    ~KSSLSettingsPrivate()
    {

    }

    bool m_bUseEGD;
    bool m_bUseEFile;
    QString m_EGDPath;
    bool m_bSendX509;
    bool m_bPromptX509;

    KConfig *m_cfg;
    bool m_bWarnOnEnter, m_bWarnOnUnencrypted, m_bWarnOnLeave, m_bWarnOnMixed;
    bool m_bWarnSelfSigned, m_bWarnRevoked, m_bWarnExpired;

    QStringList m_v3ciphers;
    QStringList m_v3selectedciphers;
    QList<int>  m_v3bits;
};

//
// FIXME
// Implementation note: for now, we only read cipher settings from disk,
//                      and do not store them in memory.  This should change.
//

KSSLSettings::KSSLSettings(bool readConfig)
    : d(new KSSLSettingsPrivate)
{
    d->m_cfg = new KConfig(QStringLiteral("cryptodefaults"), KConfig::NoGlobals);

    if (readConfig) {
        load();
    }
}

// we don't save settings incase it was a temporary object
KSSLSettings::~KSSLSettings()
{
    delete d->m_cfg;
    delete d;
}

QString KSSLSettings::getCipherList()
{
    QString clist;
    // TODO fill in list here (or just remove this method!)
    return clist;
}

// FIXME - sync these up so that we can use them with the control module!!
void KSSLSettings::load()
{
    d->m_cfg->reparseConfiguration();

    KConfigGroup cfg(d->m_cfg, "Warnings");
    d->m_bWarnOnEnter = cfg.readEntry("OnEnter", false);
    d->m_bWarnOnLeave = cfg.readEntry("OnLeave", true);
    d->m_bWarnOnUnencrypted = cfg.readEntry("OnUnencrypted", false);
    d->m_bWarnOnMixed = cfg.readEntry("OnMixed", true);

    cfg = KConfigGroup(d->m_cfg, "Validation");
    d->m_bWarnSelfSigned = cfg.readEntry("WarnSelfSigned", true);
    d->m_bWarnExpired = cfg.readEntry("WarnExpired", true);
    d->m_bWarnRevoked = cfg.readEntry("WarnRevoked", true);

    cfg = KConfigGroup(d->m_cfg, "EGD");
    d->m_bUseEGD = cfg.readEntry("UseEGD", false);
    d->m_bUseEFile = cfg.readEntry("UseEFile", false);
    d->m_EGDPath = cfg.readPathEntry("EGDPath", QString());

    cfg = KConfigGroup(d->m_cfg, "Auth");
    d->m_bSendX509 = (QLatin1String("send") == cfg.readEntry("AuthMethod", ""));
    d->m_bPromptX509 = (QLatin1String("prompt") == cfg.readEntry("AuthMethod", ""));
}

void KSSLSettings::defaults()
{
    d->m_bWarnOnEnter = false;
    d->m_bWarnOnLeave = true;
    d->m_bWarnOnUnencrypted = true;
    d->m_bWarnOnMixed = true;
    d->m_bWarnSelfSigned = true;
    d->m_bWarnExpired = true;
    d->m_bWarnRevoked = true;
    d->m_bUseEGD = false;
    d->m_bUseEFile = false;
    d->m_EGDPath = QLatin1String("");
}

void KSSLSettings::save()
{
    KConfigGroup cfg(d->m_cfg, "Warnings");
    cfg.writeEntry("OnEnter", d->m_bWarnOnEnter);
    cfg.writeEntry("OnLeave", d->m_bWarnOnLeave);
    cfg.writeEntry("OnUnencrypted", d->m_bWarnOnUnencrypted);
    cfg.writeEntry("OnMixed", d->m_bWarnOnMixed);

    cfg = KConfigGroup(d->m_cfg, "Validation");
    cfg.writeEntry("WarnSelfSigned", d->m_bWarnSelfSigned);
    cfg.writeEntry("WarnExpired", d->m_bWarnExpired);
    cfg.writeEntry("WarnRevoked", d->m_bWarnRevoked);

    cfg = KConfigGroup(d->m_cfg, "EGD");
    cfg.writeEntry("UseEGD", d->m_bUseEGD);
    cfg.writeEntry("UseEFile", d->m_bUseEFile);
    cfg.writePathEntry("EGDPath", d->m_EGDPath);

    d->m_cfg->sync();
    // FIXME - ciphers
}

bool KSSLSettings::warnOnEnter() const
{
    return d->m_bWarnOnEnter;
}
void KSSLSettings::setWarnOnEnter(bool x)
{
    d->m_bWarnOnEnter = x;
}
bool KSSLSettings::warnOnUnencrypted() const
{
    return d->m_bWarnOnUnencrypted;
}
void KSSLSettings::setWarnOnUnencrypted(bool x)
{
    d->m_bWarnOnUnencrypted = x;
}
bool KSSLSettings::warnOnLeave() const
{
    return d->m_bWarnOnLeave;
}
void KSSLSettings::setWarnOnLeave(bool x)
{
    d->m_bWarnOnLeave = x;
}
bool KSSLSettings::warnOnMixed() const
{
    return d->m_bWarnOnMixed;
}
bool KSSLSettings::useEGD() const
{
    return d->m_bUseEGD;
}
bool KSSLSettings::useEFile() const
{
    return d->m_bUseEFile;
}
bool KSSLSettings::autoSendX509() const
{
    return d->m_bSendX509;
}
bool KSSLSettings::promptSendX509() const
{
    return d->m_bPromptX509;
}
QString &KSSLSettings::getEGDPath()
{
    return d->m_EGDPath;
}
