/***************************************************************************
 *   Copyright (C) 2005 by Sean Harmer <sh@rama.homelinux.org>             *
 *                 2005 - 2007 Till Adam <adam@kde.org>                    *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU Library General Public License as       *
 *   published by  the Free Software Foundation; either version 2 of the   *
 *   License, or (at your option) any later version.                       *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.             *
 ***************************************************************************/
#ifndef KACLEDITWIDGET_H
#define KACLEDITWIDGET_H

#include <config-kiowidgets.h>

#if HAVE_POSIX_ACL || defined(Q_MOC_RUN)

#include <QWidget>

#include <kacl.h>

/// @internal
class KACLEditWidget : public QWidget
{
  Q_OBJECT
public:
    explicit KACLEditWidget(QWidget *parent = 0);
    ~KACLEditWidget();
    KACL getACL() const;
    KACL getDefaultACL() const;
    void setACL( const KACL & );
    void setDefaultACL( const KACL & );
    void setAllowDefaults( bool value );

private:
    class KACLEditWidgetPrivate;
    KACLEditWidgetPrivate *const d;

    Q_DISABLE_COPY(KACLEditWidget)

    Q_PRIVATE_SLOT(d, void _k_slotUpdateButtons())
};

#endif
#endif
