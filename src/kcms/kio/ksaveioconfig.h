/*
    SPDX-FileCopyrightText: 2001 Dawit Alemayehu <adawit@kde.org>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#ifndef KSAVEIO_CONFIG_H_
#define KSAVEIO_CONFIG_H_

#include <kprotocolmanager.h>

class QWidget;

namespace KSaveIOConfig
{
int proxyDisplayUrlFlags();
void setProxyDisplayUrlFlags (int);

/* Reload config file (kioslaverc) */
void reparseConfiguration();

/** Timeout Settings */
void setReadTimeout (int);

void setConnectTimeout (int);

void setProxyConnectTimeout (int);

void setResponseTimeout (int);


/** Cache Settings */
void setMaxCacheAge (int);

void setUseCache (bool);

void setMaxCacheSize (int);

void setCacheControl (KIO::CacheControl);


/** Proxy Settings */
void setUseReverseProxy (bool);

void setProxyType (KProtocolManager::ProxyType);

void setProxyConfigScript (const QString&);

void setProxyFor (const QString&, const QString&);

QString noProxyFor();
void setNoProxyFor (const QString&);


/** Miscellaneous Settings */
void setMarkPartial (bool);

void setMinimumKeepSize (int);

void setAutoResume (bool);

/** Update all running io-slaves */
void updateRunningIOSlaves (QWidget* parent = nullptr);

/** Update proxy scout */
void updateProxyScout (QWidget* parent = nullptr);
}

#endif
