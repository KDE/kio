/*
    This file is part of the KDE File Manager
    SPDX-FileCopyrightText: 1998 Waldo Bastian <bastian@kde.org>

    SPDX-License-Identifier: LGPL-2.0-only OR LGPL-3.0-only
*/

// KDE Cookie Server

#ifndef KCOOKIESERVER_H
#define KCOOKIESERVER_H

#include <QStringList>
#include <KDEDModule>
#include <QDBusConnection>
#include <QDBusContext>

class KHttpCookieList;
class KCookieJar;
class KHttpCookie;
class QTimer;
class RequestList;
class KConfig;

// IMPORTANT: Do *NOT* replace qlonglong with WId in this class.
//
// KCookieServer is exposed over DBus and DBus does not know how to handle the
// WId type. If a method has a WId argument it is not exposed and one gets a
// warning at compile time:
//
// "addFunction: Unregistered input type in parameter list: WId"

class KCookieServer : public KDEDModule, protected QDBusContext
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.kde.KCookieServer")
public:
    KCookieServer(QObject *parent, const QList<QVariant> &);
    ~KCookieServer();

public Q_SLOTS:
    // KDE5 TODO: don't overload names here, it prevents calling e.g. findCookies from the command-line using qdbus.
    QString listCookies(const QString &url);
    QString findCookies(const QString &url, qlonglong windowId);
    QStringList findDomains();
    // KDE5: rename
    QStringList findCookies(const QList<int> &fields, const QString &domain, const QString &fqdn, const QString &path, const QString &name);
    QString findDOMCookies(const QString &url);
    QString findDOMCookies(const QString &url, qlonglong windowId); // KDE5: merge with above, using default value (windowId = 0)
    void addCookies(const QString &url, const QByteArray &cookieHeader, qlonglong windowId);
    void deleteCookie(const QString &domain, const QString &fqdn, const QString &path, const QString &name);
    void deleteCookiesFromDomain(const QString &domain);
    void deleteSessionCookies(qlonglong windowId);
    void deleteSessionCookiesFor(const QString &fqdn, qlonglong windowId);
    void deleteAllCookies();
    void addDOMCookies(const QString &url, const QByteArray &cookieHeader, qlonglong windowId);
    /**
     * Sets the cookie policy for the domain associated with the specified URL.
     */
    bool setDomainAdvice(const QString &url, const QString &advice);
    /**
     * Returns the cookie policy in effect for the specified URL.
     */
    QString getDomainAdvice(const QString &url);
    void reloadPolicy();
    void shutdown();

public:
    bool cookiesPending(const QString &url, KHttpCookieList *cookieList = nullptr);
    void addCookies(const QString &url, const QByteArray &cookieHeader,
                    qlonglong windowId, bool useDOMFormat);
    void checkCookies(KHttpCookieList *cookieList);
    // TODO: KDE5 merge with above function and make all these public functions
    // private since they are not used externally.
    void checkCookies(KHttpCookieList *cookieList, qlonglong windowId);

private Q_SLOTS:
    void slotSave();
    void slotDeleteSessionCookies(qlonglong windowId);

private:
    KCookieJar *mCookieJar;
    KHttpCookieList *mPendingCookies;
    RequestList *mRequestList;
    QTimer *mTimer;
    bool mAdvicePending;
    KConfig *mConfig;
    QString mFilename;

private:
    bool cookieMatches(const KHttpCookie &, const QString &, const QString &, const QString &, const QString &);
    void putCookie(QStringList &, const KHttpCookie &, const QList<int> &);
    void saveCookieJar();
};

#endif
