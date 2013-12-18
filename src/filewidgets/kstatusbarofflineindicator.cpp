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

#include "kstatusbarofflineindicator.h"

#include <QLabel>
#include <QVBoxLayout>
#include <kiconloader.h>
#include <klocalizedstring.h>

class KStatusBarOfflineIndicatorPrivate
{
    public:
        KStatusBarOfflineIndicatorPrivate( KStatusBarOfflineIndicator *parent )
            : q(parent)
        {
        }

        void initialize();
        void _k_networkStatusChanged( Solid::Networking::Status status );

        KStatusBarOfflineIndicator *q;
};

KStatusBarOfflineIndicator::KStatusBarOfflineIndicator( QWidget * parent)
    : QWidget( parent),
      d( new KStatusBarOfflineIndicatorPrivate( this ) )
{
    QVBoxLayout * layout = new QVBoxLayout( this );
    layout->setMargin( 2 );
    QLabel * label = new QLabel( this );
    label->setPixmap( SmallIcon("network-disconnect") );
    label->setToolTip( i18n( "The desktop is offline" ) );
    layout->addWidget( label );
    d->initialize();
    connect( Solid::Networking::notifier(), SIGNAL(statusChanged(Solid::Networking::Status)),
             SLOT(_k_networkStatusChanged(Solid::Networking::Status)) );
}

KStatusBarOfflineIndicator::~KStatusBarOfflineIndicator()
{
    delete d;
}

void KStatusBarOfflineIndicatorPrivate::initialize()
{
    _k_networkStatusChanged( Solid::Networking::status());
}

void KStatusBarOfflineIndicatorPrivate::_k_networkStatusChanged( Solid::Networking::Status status )
{
    if ( status == Solid::Networking::Connected || status == Solid::Networking::Unknown ) {
        q->hide();
    } else {
        q->show();
    }
}

#include "moc_kstatusbarofflineindicator.cpp"
