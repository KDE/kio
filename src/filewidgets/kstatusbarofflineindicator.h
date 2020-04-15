/*  This file is part of the KDE project
    Copyright (C) 2007 Will Stephenson <wstephenson@kde.org>

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License version 2 as published by the Free Software Foundation.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    along with this library.  If not, write to the Free Software
    Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.

    As a special exception, permission is given to link this library
    with any edition of Qt, and distribute the resulting executable,
    without including the source code for Qt in the source distribution.
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

