//krazy:excludeall=license (it's a program, not a library)
/*
   Copyright (c) 2001 Malte Starostik <malte@kde.org>
   based on kmailservice.cpp,
   Copyright (c) 2000 Simon Hausmann <hausmann@kde.org>

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public
   License as published by the Free Software Foundation; either
   version 2 of the License, or (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; see the file COPYING.  If not, write to
   the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
   Boston, MA 02110-1301, USA.
*/

#undef QT_NO_CAST_FROM_ASCII

#include <QApplication>
#include <ktoolinvocation.h>
#include <kauthorized.h>
#include <kmessagebox.h>
#include <QDebug>
#include <klocalizedstring.h>
#include <kconfig.h>
#include <kconfiggroup.h>
#include <QUrl>

int main(int argc, char **argv)
{
    QApplication a(argc, argv);

    if (argc != 1) {
        fprintf(stderr, "Usage: ktelnetservice <url>\n");
        return 1;
    }

    KConfig config("kdeglobals");
    KConfigGroup cg(&config, "General");
    QString terminal = cg.readPathEntry("TerminalApplication", "konsole");

    QUrl url(argv[1]);
    QStringList cmd;
    if (terminal == "konsole")
        cmd << "--noclose";

    cmd << "-e";
    if ( url.scheme() == "telnet" )
        cmd << "telnet";
    else if ( url.scheme() == "ssh" )
        cmd << "ssh";
    else if ( url.scheme() == "rlogin" )
        cmd << "rlogin";
    else {
        qCritical() << "Invalid protocol " << url.scheme() << endl;
        return 2;
    }

    if (!KAuthorized::authorize("shell_access")) {
        KMessageBox::sorry(0,
            i18n("You do not have permission to access the %1 protocol.", url.scheme()));
        return 3;
    }

    if (!url.userName().isEmpty()) {
        cmd << "-l";
        cmd << url.userName();
    }

    QString host;
    if (!url.host().isEmpty())
       host = url.host(); // telnet://host
    else if (!url.path().isEmpty())
       host = url.path(); // telnet:host

    if (host.isEmpty() || host.startsWith('-')) {
        qCritical() << "Invalid hostname " << host << endl;
        return 2;
    }

    cmd << host;

    if (url.port() > 0) {
        if (url.scheme() == QLatin1String("ssh"))
            cmd << "-p" << QString::number(url.port());
        else
            cmd << QString::number(url.port());
    }

    KToolInvocation::kdeinitExec(terminal, cmd);

    return 0;
}
