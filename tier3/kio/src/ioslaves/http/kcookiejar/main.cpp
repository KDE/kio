/*
This file is part of KDE

  Copyright (C) 1998-2000 Waldo Bastian (bastian@kde.org)

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

#include <QDBusConnection>
#include <QDBusInterface>

#include <klocalizedstring.h>
#include <qcommandlineparser.h>
#include <qcommandlineoption.h>
#include "kcookieserverinterface.h"

static void callKded(const QString &arg1, const QString &arg2)
{
    QDBusInterface iface("org.kde.kded5", "/kded", "org.kde.kded5");
    iface.call(arg1, arg2);
}

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    app.setApplicationVersion("1.0");
    KLocalizedString::setApplicationDomain("kio5");

    QString description = QCoreApplication::translate("main", "HTTP Cookie Daemon");

    QCommandLineParser parser;
    parser.addVersionOption();
    parser.setApplicationDescription(description);
    parser.addHelpOption();
    parser.addOption(QCommandLineOption(QStringList() << "shutdown", QCoreApplication::translate("main", "Shut down cookie jar")));
    parser.addOption(QCommandLineOption(QStringList() << "remove", QCoreApplication::translate("main", "Remove cookies for domain"), "domain"));
    parser.addOption(QCommandLineOption(QStringList() << "remove-all", QCoreApplication::translate("main", "Remove all cookies")));
    parser.addOption(QCommandLineOption(QStringList() << "reload-config", QCoreApplication::translate("main", "Reload configuration file")));
    parser.process(app);

    org::kde::KCookieServer *kcookiejar = new org::kde::KCookieServer("org.kde.kded5", "/modules/kcookiejar", QDBusConnection::sessionBus());
    if (parser.isSet("remove-all")) {
      kcookiejar->deleteAllCookies();
    }
    if (parser.isSet("remove")) {
      QString domain = parser.value("remove");
      kcookiejar->deleteCookiesFromDomain(domain);
    }
    if (parser.isSet("shutdown")) {
      callKded("unloadModule", "kcookiejar");
    } else if (parser.isSet("reload-config")) {
      kcookiejar->reloadPolicy();
    } else {
      callKded("loadModule", "kcookiejar");
    }
    delete kcookiejar;

    return 0;
}
