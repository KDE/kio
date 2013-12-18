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
    KBuildSycocaProgressDialogPrivate( KBuildSycocaProgressDialog *parent )
        : m_parent(parent)
    {
    }

    void _k_slotProgress();
    void _k_slotFinished();

    KBuildSycocaProgressDialog *m_parent;
    QTimer m_timer;
    int m_timeStep;
};

void KBuildSycocaProgressDialog::rebuildKSycoca(QWidget *parent)
{
  KBuildSycocaProgressDialog dlg(parent,
                                 i18n("Updating System Configuration"),
                                 i18n("Updating system configuration."));

  QDBusInterface kbuildsycoca("org.kde.kded5", "/kbuildsycoca",
                              "org.kde.kbuildsycoca");
  if (kbuildsycoca.isValid()) {
     kbuildsycoca.callWithCallback("recreate", QVariantList(), &dlg, SLOT(_k_slotFinished()));
  } else {
      // kded not running, e.g. when using keditfiletype out of a KDE session
      QObject::connect(KSycoca::self(), SIGNAL(databaseChanged(QStringList)), &dlg, SLOT(_k_slotFinished()));
      QProcess* proc = new QProcess(&dlg);
      proc->start(KBUILDSYCOCA_EXENAME);
  }
  dlg.exec();
}

KBuildSycocaProgressDialog::KBuildSycocaProgressDialog(QWidget *_parent,
                          const QString &_caption, const QString &text)
 : QProgressDialog(_parent)
 , d( new KBuildSycocaProgressDialogPrivate(this) )
{
  connect(&d->m_timer, SIGNAL(timeout()), this, SLOT(_k_slotProgress()));
  setWindowTitle(_caption);
  setModal(true);
  setLabelText(text);
  setRange(0, 20);
  d->m_timeStep = 700;
  d->m_timer.start(d->m_timeStep);
  setAutoClose(false);
}

KBuildSycocaProgressDialog::~KBuildSycocaProgressDialog()
{
    delete d;
}

void KBuildSycocaProgressDialogPrivate::_k_slotProgress()
{
  const int p = m_parent->value();
  if (p == 18)
  {
     m_parent->reset();
     m_parent->setValue(1);
     m_timeStep = m_timeStep * 2;
     m_timer.start(m_timeStep);
  }
  else
  {
     m_parent->setValue(p+1);
  }
}

void KBuildSycocaProgressDialogPrivate::_k_slotFinished()
{
  m_parent->setValue(20);
  m_timer.stop();
  QTimer::singleShot(1000, m_parent, SLOT(close()));
}


#include "moc_kbuildsycocaprogressdialog.cpp"
