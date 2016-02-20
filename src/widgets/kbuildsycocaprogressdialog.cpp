/* This file is part of the KDE project
   Copyright (C) 2003 Waldo Bastian <bastian@kde.org>

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public
   License version 2 as published by the Free Software Foundation.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public License
   along with this library; see the file COPYING.LIB.  If not, write to
   the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
   Boston, MA 02110-1301, USA.
*/
#include "kbuildsycocaprogressdialog.h"
#include <ksycoca.h>
#include <qstandardpaths.h>
#include <klocalizedstring.h>
#include <QDBusConnection>
#include <QDBusInterface>
#include <QtCore/QProcess>

class KBuildSycocaProgressDialogPrivate
{
public:
    KBuildSycocaProgressDialogPrivate(KBuildSycocaProgressDialog *parent)
        : m_parent(parent)
    {
    }

    KBuildSycocaProgressDialog *m_parent;
};

void KBuildSycocaProgressDialog::rebuildKSycoca(QWidget *parent)
{
    KBuildSycocaProgressDialog dlg(parent,
                                   i18n("Updating System Configuration"),
                                   i18n("Updating system configuration."));

    // FIXME HACK: kdelibs 4 doesn't evaluate mimeapps.list at query time; refresh
    // its cache as well.
    QDBusInterface kbuildsycoca4(QStringLiteral("org.kde.kded"), QStringLiteral("/kbuildsycoca"), QStringLiteral("org.kde.kbuildsycoca"));
    if (kbuildsycoca4.isValid()) {
        kbuildsycoca4.call(QDBus::NoBlock, QStringLiteral("recreate"));
    } else {
        QProcess::startDetached(QStringLiteral("kbuildsycoca4"));
    }

    QProcess *proc = new QProcess(&dlg);
    proc->start(QStringLiteral(KBUILDSYCOCA_EXENAME));
    QObject::connect(proc, SIGNAL(finished(int)), &dlg, SLOT(close()));

    dlg.exec();
}

KBuildSycocaProgressDialog::KBuildSycocaProgressDialog(QWidget *_parent,
        const QString &_caption, const QString &text)
    : QProgressDialog(_parent)
    , d(new KBuildSycocaProgressDialogPrivate(this))
{
    setWindowTitle(_caption);
    setModal(true);
    setLabelText(text);
    setRange(0, 0);
    setAutoClose(false);
}

KBuildSycocaProgressDialog::~KBuildSycocaProgressDialog()
{
    delete d;
}

#include "moc_kbuildsycocaprogressdialog.cpp"
