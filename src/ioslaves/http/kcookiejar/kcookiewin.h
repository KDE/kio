/*
    This file is part of the KDE File Manager

    Copyright (C) 1998- Waldo Bastian (bastian@kde.org)
    Copyright (C) 2000- Dawit Alemayehu (adawit@kde.org)

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public License
    as published by the Free Software Foundation; either version 2
    of the License, or (at your option) any later version.

    This software is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this library; see the file COPYING. If not, write to
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.
*/
//----------------------------------------------------------------------------
//
// KDE File Manager -- HTTP Cookie Dialogs

#ifndef KCOOKIEWIN_H
#define KCOOKIEWIN_H

#include <QGroupBox>
#include <QRadioButton>
#include <QDialog>
#include "kcookiejar.h"

class QLineEdit;
class QPushButton;

class KCookieDetail : public QGroupBox
{
    Q_OBJECT

public:
    KCookieDetail(KHttpCookieList cookieList, int cookieCount, QWidget *parent=0);
    ~KCookieDetail();


private Q_SLOTS:
    void slotNextCookie();

private:
    void displayCookieDetails();

    QLineEdit*   m_name;
    QLineEdit*   m_value;
    QLineEdit*   m_expires;
    QLineEdit*   m_domain;
    QLineEdit*   m_path;
    QLineEdit*   m_secure;

    KHttpCookieList m_cookieList;
    int m_cookieNumber;
};

class KCookieWin : public QDialog
{
    Q_OBJECT

public :
    KCookieWin( QWidget *parent, KHttpCookieList cookieList, int defaultButton=0,
                bool showDetails=false );
    ~KCookieWin();

    KCookieAdvice advice( KCookieJar *cookiejar, const KHttpCookie& cookie );

private Q_SLOTS:
    void slotSessionOnlyClicked();
    void slotToggleDetails();

private :
    QPushButton *m_detailsButton;
    QRadioButton* m_onlyCookies, *m_allCookies, *m_allCookiesDomain;
    KCookieDetail* m_detailView;
};

#endif
