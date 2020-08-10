/*
    This file is part of the KDE File Manager
    SPDX-FileCopyrightText: 1998 Waldo Bastian <bastian@kde.org>
    SPDX-FileCopyrightText: 2000 Dawit Alemayehu <adawit@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

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
    KCookieDetail(const KHttpCookieList &cookieList, int cookieCount, QWidget *parent = nullptr);
    ~KCookieDetail();

private Q_SLOTS:
    void slotNextCookie();

private:
    void displayCookieDetails();

    QLineEdit   *m_name;
    QLineEdit   *m_value;
    QLineEdit   *m_expires;
    QLineEdit   *m_domain;
    QLineEdit   *m_path;
    QLineEdit   *m_secure;

    KHttpCookieList m_cookieList;
    int m_cookieNumber;
};

class KCookieWin : public QDialog
{
    Q_OBJECT

public :
    KCookieWin(QWidget *parent, KHttpCookieList cookieList, int defaultButton = 0,
               bool showDetails = false);
    ~KCookieWin();

    KCookieAdvice advice(KCookieJar *cookiejar, const KHttpCookie &cookie);

private Q_SLOTS:
    void slotSessionOnlyClicked();
    void slotToggleDetails();

private :
    QPushButton *m_detailsButton;
    QRadioButton *m_onlyCookies, *m_allCookies, *m_allCookiesDomain;
    KCookieDetail *m_detailView;
};

#endif
