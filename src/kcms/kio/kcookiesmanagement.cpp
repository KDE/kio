/*
    kcookiesmanagement.cpp - Cookies manager

    SPDX-FileCopyrightText: 2000-2001 Marco Pinelli <pinmc@orion.it>
    SPDX-FileCopyrightText: 2000-2001 Dawit Alemayehu <adawit@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

// Own
#include "kcookiesmanagement.h"

// Qt
#include <QPushButton>
#include <QDBusConnection>
#include <QDBusReply>
#include <QDBusInterface>

// KDE
#include <KLocalizedString>
#include <QDateTime>
#include <KIconLoader>
#include <KMessageBox>
#include <QLocale>
// Local
#include "kcookiesmain.h"
#include "kcookiespolicies.h"

QString tolerantFromAce(const QByteArray& _domain);

struct CookieProp
{
    QString host;
    QString name;
    QString value;
    QString domain;
    QString path;
    QString expireDate;
    QString secure;
    bool allLoaded;
};

CookieListViewItem::CookieListViewItem(QTreeWidget *parent, const QString &dom)
                   :QTreeWidgetItem(parent)
{
    init( nullptr, dom );
}

CookieListViewItem::CookieListViewItem(QTreeWidgetItem *parent, CookieProp *cookie)
                   :QTreeWidgetItem(parent)
{
    init( cookie );
}

CookieListViewItem::~CookieListViewItem()
{
    delete mCookie;
}

void CookieListViewItem::init( CookieProp* cookie, const QString &domain,
                               bool cookieLoaded )
{
    mCookie = cookie;
    mDomain = domain;
    mCookiesLoaded = cookieLoaded;

    if (mCookie)
    {
        if (mDomain.isEmpty())
            setText(0, tolerantFromAce(mCookie->host.toLatin1()));
        else
            setText(0, tolerantFromAce(mDomain.toLatin1()));
        setText(1, mCookie->name);
    }
    else
    {
        QString siteName;
        if (mDomain.startsWith(QLatin1Char('.')))
            siteName = mDomain.mid(1);
        else
            siteName = mDomain;
        setText(0, tolerantFromAce(siteName.toLatin1()));
    }
}

CookieProp* CookieListViewItem::leaveCookie()
{
    CookieProp *ret = mCookie;
    mCookie = nullptr;
    return ret;
}

KCookiesManagement::KCookiesManagement(QWidget *parent)
                   : KCModule(parent),
                     mDeleteAllFlag(false),
                     mMainWidget(parent)
{
  mUi.setupUi(this);
  mUi.searchLineEdit->setTreeWidget(mUi.cookiesTreeWidget);
  mUi.cookiesTreeWidget->setColumnWidth(0, 150);

  connect(mUi.deleteButton, &QAbstractButton::clicked, this, &KCookiesManagement::deleteCurrent);
  connect(mUi.deleteAllButton, &QAbstractButton::clicked, this, &KCookiesManagement::deleteAll);
  connect(mUi.reloadButton, &QAbstractButton::clicked, this, &KCookiesManagement::reload);
  connect(mUi.cookiesTreeWidget, &QTreeWidget::itemExpanded, this, &KCookiesManagement::listCookiesForDomain);
  connect(mUi.cookiesTreeWidget, &QTreeWidget::currentItemChanged, this, &KCookiesManagement::updateForItem);
  connect(mUi.cookiesTreeWidget, &QTreeWidget::itemDoubleClicked, this, &KCookiesManagement::showConfigPolicyDialog);
  connect(mUi.configPolicyButton, &QAbstractButton::clicked, this, &KCookiesManagement::showConfigPolicyDialog);
}

KCookiesManagement::~KCookiesManagement()
{
}

void KCookiesManagement::load()
{
  defaults();
}

void KCookiesManagement::save()
{
  // If delete all cookies was requested!
  if(mDeleteAllFlag)
  {
    QDBusInterface kded(QStringLiteral("org.kde.kcookiejar5"), QStringLiteral("/modules/kcookiejar"), QStringLiteral("org.kde.KCookieServer"), QDBusConnection::sessionBus());
    QDBusReply<void> reply = kded.call( QStringLiteral("deleteAllCookies") );
    if (!reply.isValid())
    {
      const QString caption = i18n ("D-Bus Communication Error");
      const QString message = i18n ("Unable to delete all the cookies as requested.");
      KMessageBox::sorry (this, message, caption);
      return;
    }
    mDeleteAllFlag = false; // deleted[Cookies|Domains] have been cleared yet
  }

  // Certain groups of cookies were deleted...
  QMutableStringListIterator it (mDeletedDomains);
  while (it.hasNext())
  {    
    QDBusInterface kded(QStringLiteral("org.kde.kcookiejar5"), QStringLiteral("/modules/kcookiejar"), QStringLiteral("org.kde.KCookieServer"), QDBusConnection::sessionBus());
    QDBusReply<void> reply = kded.call( QStringLiteral("deleteCookiesFromDomain"),( it.next() ) );
    if (!reply.isValid())
    {
      const QString caption = i18n ("D-Bus Communication Error");
      const QString message = i18n ("Unable to delete cookies as requested.");
      KMessageBox::sorry (this, message, caption);
      return;
    }
    it.remove();
  }

  // Individual cookies were deleted...
  bool success = true; // Maybe we can go on...
  QMutableHashIterator<QString, CookiePropList> cookiesDom(mDeletedCookies);
  while(cookiesDom.hasNext())
  {
    cookiesDom.next();
    CookiePropList list = cookiesDom.value();
    for (auto it = list.begin(); it < list.end(); ++it) {
      CookieProp *cookie = *it;
      QDBusInterface kded(QStringLiteral("org.kde.kcookiejar5"), QStringLiteral("/modules/kcookiejar"),
                          QStringLiteral("org.kde.KCookieServer"), QDBusConnection::sessionBus());
      QDBusReply<void> reply = kded.call( QStringLiteral("deleteCookie"), cookie->domain,
                                          cookie->host, cookie->path,
                                          cookie->name );
      if (!reply.isValid())
      {
        success = false;
        break;
      }

      list.removeOne(cookie);
    }

    if (!success)
      break;

    mDeletedCookies.remove(cookiesDom.key());
  }

  Q_EMIT changed( false );
}

void KCookiesManagement::defaults()
{
  reset();
  reload();
}

void KCookiesManagement::reset(bool deleteAll)
{
  if (!deleteAll)
    mDeleteAllFlag = false;

  clearCookieDetails();
  mDeletedDomains.clear();
  mDeletedCookies.clear();
  
  mUi.cookiesTreeWidget->clear();
  mUi.deleteButton->setEnabled(false);
  mUi.deleteAllButton->setEnabled(false);
  mUi.configPolicyButton->setEnabled(false);
}

void KCookiesManagement::clearCookieDetails()
{
  mUi.nameLineEdit->clear();
  mUi.valueLineEdit->clear();
  mUi.domainLineEdit->clear();
  mUi.pathLineEdit->clear();
  mUi.expiresLineEdit->clear();
  mUi.secureLineEdit->clear();
}

QString KCookiesManagement::quickHelp() const
{
  return i18n("<h1>Cookie Management Quick Help</h1>" );
}

void KCookiesManagement::reload()
{
  QDBusInterface kded(QStringLiteral("org.kde.kcookiejar5"), QStringLiteral("/modules/kcookiejar"), QStringLiteral("org.kde.KCookieServer"), QDBusConnection::sessionBus());
  QDBusReply<QStringList> reply = kded.call( QStringLiteral("findDomains") );

  if (!reply.isValid())
  {
    const QString caption = i18n ("Information Lookup Failure");
    const QString message = i18n ("Unable to retrieve information about the "
                            "cookies stored on your computer.");
    KMessageBox::sorry (this, message, caption);
    return;
  }

  if (mUi.cookiesTreeWidget->topLevelItemCount() > 0)
      reset();

  CookieListViewItem *dom;
  const QStringList domains (reply.value());
  for (const QString& domain : domains)
  {
    const QString siteName = (domain.startsWith(QLatin1Char('.')) ? domain.mid(1) : domain);
    if (mUi.cookiesTreeWidget->findItems(siteName, Qt::MatchFixedString).isEmpty()) {
        dom = new CookieListViewItem(mUi.cookiesTreeWidget, domain);
        dom->setChildIndicatorPolicy(QTreeWidgetItem::ShowIndicator);
    }
  }

  // are there any cookies?
  mUi.deleteAllButton->setEnabled(mUi.cookiesTreeWidget->topLevelItemCount() > 0);
  mUi.cookiesTreeWidget->sortItems(0, Qt::AscendingOrder);
  Q_EMIT changed(false);
}

Q_DECLARE_METATYPE( QList<int> )

void KCookiesManagement::listCookiesForDomain(QTreeWidgetItem *item)
{
  CookieListViewItem* cookieDom = static_cast<CookieListViewItem*>(item);
  if (!cookieDom || cookieDom->cookiesLoaded())
    return;

  QStringList cookies;
  const QList<int> fields { 0, 1, 2, 3 };
  // Always check for cookies in both "foo.bar" and ".foo.bar" domains...
  const QString domain = cookieDom->domain() + QLatin1String(" .") + cookieDom->domain();
  QDBusInterface kded(QStringLiteral("org.kde.kcookiejar5"), QStringLiteral("/modules/kcookiejar"), QStringLiteral("org.kde.KCookieServer"), QDBusConnection::sessionBus());
  QDBusReply<QStringList> reply = kded.call(QStringLiteral("findCookies"), QVariant::fromValue(fields),
                                            domain, QString(), QString(), QString());
  if (reply.isValid())
      cookies.append(reply.value());

  QStringListIterator it(cookies);
  while (it.hasNext())
  {
    CookieProp *details = new CookieProp;
    details->domain = it.next();
    details->path = it.next();
    details->name = it.next();
    details->host = it.next();
    details->allLoaded = false;
    new CookieListViewItem(item, details);
  }

  if (!cookies.isEmpty())
  {
    static_cast<CookieListViewItem*>(item)->setCookiesLoaded();
    mUi.searchLineEdit->updateSearch();
  }
}

bool KCookiesManagement::cookieDetails(CookieProp *cookie)
{
  const QList<int> fields{ 4, 5, 7 };

  QDBusInterface kded(QStringLiteral("org.kde.kcookiejar5"), QStringLiteral("/modules/kcookiejar"), QStringLiteral("org.kde.KCookieServer"), QDBusConnection::sessionBus());
  QDBusReply<QStringList> reply = kded.call( QStringLiteral("findCookies"),
                                             QVariant::fromValue( fields ),
                                             cookie->domain,
                                             cookie->host,
                                             cookie->path,
                                             cookie->name);
  if (!reply.isValid())
    return false;

  const QStringList fieldVal = reply.value();

  QStringList::const_iterator c = fieldVal.begin();
  if (c == fieldVal.end()) // empty list, do not crash
    return false;

  bool ok;  
  cookie->value = *c++;
  qint64 tmp = (*c++).toLongLong(&ok);

  if (!ok || tmp == 0)
    cookie->expireDate = i18n("End of session");
  else
  {
    QDateTime expDate = QDateTime::fromSecsSinceEpoch(tmp);
    cookie->expireDate = QLocale().toString((expDate), QLocale::ShortFormat);
  }

  tmp = (*c).toUInt(&ok);
  cookie->secure = i18n((ok && tmp) ? "Yes" : "No");
  cookie->allLoaded = true;
  return true;
}

void KCookiesManagement::updateForItem(QTreeWidgetItem* item)
{
  if (item) {
    CookieListViewItem* cookieItem = static_cast<CookieListViewItem*>(item);
    CookieProp *cookie = cookieItem->cookie();

    if (cookie) {
      if (cookie->allLoaded || cookieDetails(cookie)) {
        mUi.nameLineEdit->setText(cookie->name);
        mUi.valueLineEdit->setText(cookie->value);
        mUi.domainLineEdit->setText(cookie->domain);
        mUi.pathLineEdit->setText(cookie->path);
        mUi.expiresLineEdit->setText(cookie->expireDate);
        mUi.secureLineEdit->setText(cookie->secure);
      }

      mUi.configPolicyButton->setEnabled(false);
    } else {
      clearCookieDetails();
      mUi.configPolicyButton->setEnabled(true);
    }
  } else {
      mUi.configPolicyButton->setEnabled(false);
  }
  mUi.deleteButton->setEnabled(item != nullptr);
}

void KCookiesManagement::showConfigPolicyDialog()
{
  // Get current item
  CookieListViewItem *item = static_cast<CookieListViewItem*>(mUi.cookiesTreeWidget->currentItem());
  Q_ASSERT(item); // the button is disabled otherwise

  if (item)
  {
    KCookiesMain* mainDlg = qobject_cast<KCookiesMain*>(mMainWidget);
    // must be present or something is really wrong.
    Q_ASSERT(mainDlg);

    KCookiesPolicies* policyDlg = mainDlg->policyDlg();
    // must be present unless someone rewrote the widget in which case
    // this needs to be re-written as well.
    Q_ASSERT(policyDlg);
    
    policyDlg->setPolicy(item->domain());
  }
}

void KCookiesManagement::deleteCurrent()
{
  QTreeWidgetItem* currentItem = mUi.cookiesTreeWidget->currentItem();
  Q_ASSERT(currentItem); // the button is disabled otherwise
  CookieListViewItem *item = static_cast<CookieListViewItem*>( currentItem );
  if (item->cookie()) {
    CookieListViewItem *parent = static_cast<CookieListViewItem*>(item->parent());
    CookiePropList list = mDeletedCookies.value(parent->domain());
    list.append(item->leaveCookie());
    mDeletedCookies.insert(parent->domain(), list);
    delete item;
    if (parent->childCount() == 0)
      delete parent;
  }
  else
  {
    mDeletedDomains.append(item->domain());
    delete item;
  }

  currentItem = mUi.cookiesTreeWidget->currentItem();
  if (currentItem)
  {
    mUi.cookiesTreeWidget->setCurrentItem( currentItem );
  }
  else
    clearCookieDetails();

  mUi.deleteAllButton->setEnabled(mUi.cookiesTreeWidget->topLevelItemCount() > 0);

  Q_EMIT changed( true );
}

void KCookiesManagement::deleteAll()
{
  mDeleteAllFlag = true;
  reset(true);
  Q_EMIT changed(true);
}

