/*
This file is part of KDE

  Copyright (C) 2000- Waldo Bastian <bastian@kde.org>
  Copyright (C) 2000- Dawit Alemayehu <adawit@kde.org>

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN
AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/
//----------------------------------------------------------------------------
//
// KDE File Manager -- HTTP Cookie Dialogs

// The purpose of the QT_NO_TOOLTIP and QT_NO_WHATSTHIS ifdefs is because
// this file is also used in Konqueror/Embedded. One of the aims of
// Konqueror/Embedded is to be a small as possible to fit on embedded
// devices. For this it's also useful to strip out unneeded features of
// Qt, like for example QToolTip or QWhatsThis. The availability (or the
// lack thereof) can be determined using these preprocessor defines.
// The same applies to the QT_NO_ACCEL ifdef below. I hope it doesn't make
// too much trouble... (Simon)

#include "kcookiewin.h"
#include "kcookiejar.h"

#include <QDateTime>
#include <QDialogButtonBox>
#include <QLabel>
#include <QLayout>
#include <QLineEdit>
#include <QGroupBox>
#include <QPushButton>
#include <QRadioButton>
#include <QShortcut>
#include <QStyle>
#include <QUrl>

#include <kwindowsystem.h>
#include <klocalizedstring.h>
#include <kusertimestamp.h>

enum {
    AcceptedForSession = QDialog::Accepted + 1
};

KCookieWin::KCookieWin( QWidget *parent, KHttpCookieList cookieList,
                        int defaultButton, bool showDetails )
    : QDialog( parent )
{
    setModal(true);
    setObjectName("cookiealert");
    setWindowTitle( i18n("Cookie Alert") );
    setWindowIcon( QIcon::fromTheme("preferences-web-browser-cookies") );
    // all cookies in the list should have the same window at this time, so let's take the first
    if( cookieList.first().windowIds().count() > 0 )
    {
#ifdef Q_OS_WIN
        KWindowSystem::setMainWindow( this, reinterpret_cast<WId>( cookieList.first().windowIds().first() ) );
#else
        KWindowSystem::setMainWindow( this, cookieList.first().windowIds().first());
#endif
    }
    else
    {
        // No window associated... make sure the user notices our dialog.
#if HAVE_X11
        KWindowSystem::setState( winId(), NET::KeepAbove );
#endif
        KUserTimestamp::updateUserTimestamp();
    }

    const int count = cookieList.count();
    const KHttpCookie& cookie = cookieList.first();
    QString host (cookie.host());
    int pos = host.indexOf(':');
    if ( pos > 0 ) {
        QString portNum = host.left(pos);
        host.remove(0, pos+1);
        host += ':';
        host += portNum;
    }

    QString txt = QLatin1String("<html><body style=\"p {line-height: 150%}; text-align: center;\">");
    txt += i18ncp("%2 hostname, %3 optional cross domain suffix (translated below)",
                  "<p>You received a cookie from<br/>"
                  "<b>%2%3</b><br/>"
                  "Do you want to accept or reject this cookie?</p>",
                  "<p>You received %1 cookies from<br/>"
                  "<b>%2%3</b><br/>"
                  "Do you want to accept or reject these cookies?</p>",
                  count,
                  QUrl::fromAce(host.toLatin1()),
                  cookie.isCrossDomain() ? i18nc("@item:intext cross domain cookie", " [Cross Domain]") : QString());
    txt += QLatin1String("</body></html>");


    QVBoxLayout *topLayout = new QVBoxLayout;
    setLayout(topLayout);

    QFrame* vBox1 = new QFrame( this );
    topLayout->addWidget(vBox1);

    m_detailsButton = new QPushButton;
    m_detailsButton->setText(i18n("See or modify the cookie information") + " >>");
    connect(m_detailsButton, SIGNAL(clicked()), this, SLOT(slotToggleDetails()));

    QPushButton *sessionOnlyButton = new QPushButton;
    sessionOnlyButton->setText(i18n("Accept for this &session"));
    sessionOnlyButton->setIcon(QIcon::fromTheme("chronometer"));
    sessionOnlyButton->setToolTip(i18n("Accept cookie(s) until the end of the current session"));
    connect(sessionOnlyButton, SIGNAL(clicked()), this, SLOT(slotSessionOnlyClicked()));

    QDialogButtonBox *buttonBox = new QDialogButtonBox(this);
    buttonBox->addButton(m_detailsButton, QDialogButtonBox::ActionRole);
    buttonBox->addButton(sessionOnlyButton, QDialogButtonBox::ActionRole);
    buttonBox->setStandardButtons(QDialogButtonBox::Yes | QDialogButtonBox::No);
    buttonBox->button(QDialogButtonBox::Yes)->setText(i18n("&Accept"));
    buttonBox->button(QDialogButtonBox::Yes)->setText(i18n("&Reject"));
    topLayout->addWidget(buttonBox);

    QVBoxLayout* vBox1Layout = new QVBoxLayout( vBox1 );
    vBox1Layout->setSpacing( -1 );
    vBox1Layout->setMargin( 0 );

    // Cookie image and message to user
    QFrame* hBox = new QFrame( vBox1 );
    vBox1Layout->addWidget( hBox );
    QHBoxLayout* hBoxLayout = new QHBoxLayout( hBox );
    hBoxLayout->setSpacing( 0 );
    hBoxLayout->setMargin( 0 );
    QLabel* icon = new QLabel( hBox );
    hBoxLayout->addWidget( icon );
    icon->setPixmap(QIcon::fromTheme("dialog-warning").pixmap(style()->pixelMetric(QStyle::PM_LargeIconSize)));
    icon->setAlignment( Qt::AlignCenter );
    icon->setFixedSize( 2*icon->sizeHint() );

    QFrame* vBox = new QFrame( hBox );
    QVBoxLayout* vBoxLayout = new QVBoxLayout( vBox );
    vBoxLayout->setSpacing( 0 );
    vBoxLayout->setMargin( 0 );
    hBoxLayout->addWidget( vBox );
    QLabel* lbl = new QLabel(txt, vBox);
    vBoxLayout->addWidget( lbl );
    lbl->setAlignment(Qt::AlignCenter);

    // Cookie Details dialog...
    m_detailView = new KCookieDetail( cookieList, count, vBox1 );
    vBox1Layout->addWidget(m_detailView);
    m_detailView->hide();

    // Cookie policy choice...
    QGroupBox *m_btnGrp = new QGroupBox(i18n("Apply Choice To"),vBox1);
    vBox1Layout->addWidget(m_btnGrp);
    QVBoxLayout *vbox = new QVBoxLayout;
    txt = (count == 1)? i18n("&Only this cookie") : i18n("&Only these cookies");
    m_onlyCookies = new QRadioButton( txt, m_btnGrp );
    vbox->addWidget(m_onlyCookies);
#ifndef QT_NO_WHATSTHIS
    m_onlyCookies->setWhatsThis(i18n("Select this option to only accept or reject this cookie. "
                                     "You will be prompted again if you receive another cookie."));
#endif
    m_allCookiesDomain = new QRadioButton( i18n("All cookies from this do&main"), m_btnGrp );
    vbox->addWidget(m_allCookiesDomain);
#ifndef QT_NO_WHATSTHIS
    m_allCookiesDomain->setWhatsThis(i18n("Select this option to accept or reject all cookies from "
                                          "this site. Choosing this option will add a new policy for "
                                          "the site this cookie originated from. This policy will be "
                                          "permanent until you manually change it from the System Settings."));
#endif
    m_allCookies = new QRadioButton( i18n("All &cookies"), m_btnGrp);
    vbox->addWidget(m_allCookies);
#ifndef QT_NO_WHATSTHIS
    m_allCookies->setWhatsThis(i18n("Select this option to accept/reject all cookies from "
                                    "anywhere. Choosing this option will change the global "
                                    "cookie policy for all cookies until you manually change "
                                    "it from the System Settings."));
#endif
    m_btnGrp->setLayout(vbox);

    switch (defaultButton) {
    case KCookieJar::ApplyToShownCookiesOnly:
        m_onlyCookies->setChecked(true);
        break;
    case KCookieJar::ApplyToCookiesFromDomain:
        m_allCookiesDomain->setChecked(true);
        break;
    case KCookieJar::ApplyToAllCookies:
        m_allCookies->setChecked(true);
        break;
    default:
        m_onlyCookies->setChecked(true);
        break;
    }

    if (showDetails) {
        slotToggleDetails();
    }
}

KCookieWin::~KCookieWin()
{
}

KCookieAdvice KCookieWin::advice( KCookieJar *cookiejar, const KHttpCookie& cookie )
{
    const int result = exec();

    cookiejar->setShowCookieDetails (m_detailView->isVisible());

    KCookieAdvice advice;

    switch (result) {
    case QDialog::Accepted:
        advice = KCookieAccept;
        break;
    case AcceptedForSession:
        advice = KCookieAcceptForSession;
        break;
    default:
        advice = KCookieReject;
        break;
    }

    KCookieJar::KCookieDefaultPolicy preferredPolicy = KCookieJar::ApplyToShownCookiesOnly;
    if (m_allCookiesDomain->isChecked()) {
        preferredPolicy = KCookieJar::ApplyToCookiesFromDomain;
        cookiejar->setDomainAdvice( cookie, advice );
    } else if (m_allCookies->isChecked()) {
        preferredPolicy = KCookieJar::ApplyToAllCookies;
        cookiejar->setGlobalAdvice( advice );
    }
    cookiejar->setPreferredDefaultPolicy( preferredPolicy );

    return advice;
}

KCookieDetail::KCookieDetail( KHttpCookieList cookieList, int cookieCount,
                              QWidget* parent )
              :QGroupBox( parent )
{
    setTitle( i18n("Cookie Details") );
    QGridLayout* grid = new QGridLayout( this );
    grid->addItem( new QSpacerItem(0, fontMetrics().lineSpacing()), 0, 0 );
    grid->setColumnStretch( 1, 3 );

    QLabel* label = new QLabel( i18n("Name:"), this );
    grid->addWidget( label, 1, 0 );
    m_name = new QLineEdit( this );
    m_name->setReadOnly( true );
    m_name->setMaximumWidth( fontMetrics().maxWidth() * 25 );
    grid->addWidget( m_name, 1 ,1 );

    //Add the value
    label = new QLabel( i18n("Value:"), this );
    grid->addWidget( label, 2, 0 );
    m_value = new QLineEdit( this );
    m_value->setReadOnly( true );
    m_value->setMaximumWidth( fontMetrics().maxWidth() * 25 );
    grid->addWidget( m_value, 2, 1);

    label = new QLabel( i18n("Expires:"), this );
    grid->addWidget( label, 3, 0 );
    m_expires = new QLineEdit( this );
    m_expires->setReadOnly( true );
    m_expires->setMaximumWidth(fontMetrics().maxWidth() * 25 );
    grid->addWidget( m_expires, 3, 1);

    label = new QLabel( i18n("Path:"), this );
    grid->addWidget( label, 4, 0 );
    m_path = new QLineEdit( this );
    m_path->setReadOnly( true );
    m_path->setMaximumWidth( fontMetrics().maxWidth() * 25 );
    grid->addWidget( m_path, 4, 1);

    label = new QLabel( i18n("Domain:"), this );
    grid->addWidget( label, 5, 0 );
    m_domain = new QLineEdit( this );
    m_domain->setReadOnly( true );
    m_domain->setMaximumWidth( fontMetrics().maxWidth() * 25 );
    grid->addWidget( m_domain, 5, 1);

    label = new QLabel( i18n("Exposure:"), this );
    grid->addWidget( label, 6, 0 );
    m_secure = new QLineEdit( this );
    m_secure->setReadOnly( true );
    m_secure->setMaximumWidth( fontMetrics().maxWidth() * 25 );
    grid->addWidget( m_secure, 6, 1 );

    if ( cookieCount > 1 )
    {
        QPushButton* btnNext = new QPushButton( i18nc("Next cookie","&Next >>"), this );
        btnNext->setFixedSize( btnNext->sizeHint() );
        grid->addWidget( btnNext, 8, 0, 1, 2 );
        connect( btnNext, SIGNAL(clicked()), SLOT(slotNextCookie()) );
#ifndef QT_NO_TOOLTIP
        btnNext->setToolTip(i18n("Show details of the next cookie") );
#endif
    }
    m_cookieList = cookieList;
    m_cookieNumber = 0;
    slotNextCookie();
}

KCookieDetail::~KCookieDetail()
{
}

void KCookieDetail::slotNextCookie()
{
    if (m_cookieNumber == m_cookieList.count() - 1)
        m_cookieNumber = 0;
    else
        ++m_cookieNumber;
    displayCookieDetails();
}

void KCookieDetail::displayCookieDetails()
{
    const KHttpCookie& cookie = m_cookieList.at(m_cookieNumber);
    m_name->setText(cookie.name());
    m_value->setText((cookie.value()));
    if (cookie.domain().isEmpty())
        m_domain->setText(i18n("Not specified"));
    else
        m_domain->setText(cookie.domain());
    m_path->setText(cookie.path());
    QDateTime cookiedate;
    cookiedate.setTime_t(cookie.expireDate());
    if (cookie.expireDate())
        m_expires->setText(cookiedate.toString());
    else
        m_expires->setText(i18n("End of Session"));
    QString sec;
    if (cookie.isSecure())
    {
        if (cookie.isHttpOnly())
            sec = i18n("Secure servers only");
        else
            sec = i18n("Secure servers, page scripts");
    }
    else
    {
        if (cookie.isHttpOnly())
            sec = i18n("Servers");
        else
            sec = i18n("Servers, page scripts");
    }
    m_secure->setText(sec);
}

void KCookieWin::slotSessionOnlyClicked()
{
    done(AcceptedForSession);
}

void KCookieWin::slotToggleDetails()
{
    const QString baseText = i18n("See or modify the cookie information");

    if (m_detailView->isVisible()) {
        m_detailsButton->setText(baseText + " >>");
        m_detailView->hide();
    } else {
        m_detailsButton->setText(baseText + " <<");
        m_detailView->show();
    }
}
