//krazy:excludeall=license (it's a program, not a library)
/*
    SPDX-FileCopyrightText: 2001 Malte Starostik <malte@kde.org>
    based on kmailservice.cpp,
    SPDX-FileCopyrightText: 2000 Simon Hausmann <hausmann@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include <QApplication>
#include <KToolInvocation>
#include <KAuthorized>
#include <QMessageBox>
#include <QDebug>
#include <KLocalizedString>
#include <KConfig>
#include <KConfigGroup>
#include <QUrl>

int main(int argc, char **argv)
{
    QApplication a(argc, argv);

    if (argc != 2) {
        fprintf(stderr, "Usage: ktelnetservice5 <url>\n");
        return 1;
    }

    KConfig config(QStringLiteral("kdeglobals"));
    KConfigGroup cg(&config, "General");
    QString terminal = cg.readPathEntry("TerminalApplication", QStringLiteral("konsole"));

    QUrl url(QString::fromLocal8Bit(argv[1]));
    QStringList cmd;
    if (terminal == QLatin1String("konsole")) {
        cmd << QStringLiteral("--noclose");
    }

    cmd << QStringLiteral("-e");
    if (url.scheme() == QLatin1String("telnet")) {
        cmd << QStringLiteral("telnet");
    } else if (url.scheme() == QLatin1String("ssh")) {
        cmd << QStringLiteral("ssh");
    } else if (url.scheme() == QLatin1String("rlogin")) {
        cmd << QStringLiteral("rlogin");
    } else {
        qCritical() << "Invalid protocol " << url.scheme();
        return 2;
    }

    if (!KAuthorized::authorize(QStringLiteral("shell_access"))) {
        QMessageBox::critical(nullptr, i18n("Access denied"),
                           i18n("You do not have permission to access the %1 protocol.", url.scheme()));
        return 3;
    }

    if (!url.userName().isEmpty()) {
        cmd << QStringLiteral("-l");
        cmd << url.userName();
    }

    QString host;
    if (!url.host().isEmpty()) {
        host = url.host();    // telnet://host
    } else if (!url.path().isEmpty()) {
        host = url.path();    // telnet:host
    }

    if (host.isEmpty() || host.startsWith(QLatin1Char('-'))) {
        qCritical() << "Invalid hostname " << host;
        return 2;
    }

    cmd << host;

    if (url.port() > 0) {
        if (url.scheme() == QLatin1String("ssh")) {
            cmd << QStringLiteral("-p") << QString::number(url.port());
        } else {
            cmd << QString::number(url.port());
        }
    }

    KToolInvocation::kdeinitExec(terminal, cmd);

    return 0;
}
