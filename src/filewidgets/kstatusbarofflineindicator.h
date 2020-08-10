/*
    This file is part of the KDE project
    SPDX-FileCopyrightText: 2007 Will Stephenson <wstephenson@kde.org>

    SPDX-License-Identifier: LGPL-2.0-only WITH Qt-Commercial-exception-1.0
*/

#ifndef KDE_KSTATUSBAROFFLINEINDICATOR_H
#define KDE_KSTATUSBAROFFLINEINDICATOR_H

#include <QWidget>
#include "kiofilewidgets_export.h"

class KStatusBarOfflineIndicatorPrivate;

#if KIOFILEWIDGETS_ENABLE_DEPRECATED_SINCE(5, 70)
/**
 * @class KStatusBarOfflineIndicator kstatusbarofflineindicator.h <KStatusBarOfflineIndicator>
 *
 * Widget indicating network connection status using an icon and tooltip.  This widget uses
 * QNetworkConfigurationMAnager internally to automatically show and hide itself as required.
 *
 * @code
 * KStatusBarOfflineIndicator * indicator = new KStatusBarOfflineIndicator( this );
 * statusBar()->addWidget( indicator, 0, false );
 * @endcode
 *
 * @deprecated since 5.70, no known users.
 *
 * @author Will Stephenson <wstephenson@kde.org>
 */
class KIOFILEWIDGETS_EXPORT KStatusBarOfflineIndicator : public QWidget
{
    Q_OBJECT
public:
    /**
     * Default constructor.
     * @param parent the widget's parent
     * @deprecated since 5.70, no known users.
     */
    KIOFILEWIDGETS_DEPRECATED_VERSION(5, 70, "No known users")
    explicit KStatusBarOfflineIndicator(QWidget *parent);
    ~KStatusBarOfflineIndicator();

private:
    KStatusBarOfflineIndicatorPrivate *const d;

    Q_PRIVATE_SLOT(d, void _k_networkStatusChanged(bool isOnline))
};
#endif

#endif

