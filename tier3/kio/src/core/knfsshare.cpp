/* This file is part of the KDE project
   Copyright (c) 2004 Jan Schaefer <j_schaef@informatik.uni-kl.de>

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

#include "knfsshare.h"

#include <QSet>
#include <QtCore/QFile>
#include <QtCore/QMutableStringListIterator>
#include <QtCore/QTextStream>
#include <QDebug>

#include <kdirwatch.h>
#include <kconfig.h>
#include <kconfiggroup.h>

class KNFSShare::KNFSSharePrivate
{
public:
  KNFSSharePrivate( KNFSShare *parent );

  void _k_slotFileChange(const QString&);

  bool readExportsFile();
  bool findExportsFile();

  KNFSShare *q;
  QSet<QString> sharedPaths;
  QString exportsFile;
};

KNFSShare::KNFSSharePrivate::KNFSSharePrivate( KNFSShare *parent )
    : q(parent)
{
  if (findExportsFile())
      readExportsFile();
}

/**
 * Try to find the nfs config file path
 * First tries the kconfig, then checks
 * several well-known paths
 * @return whether an 'exports' file was found.
 **/
bool KNFSShare::KNFSSharePrivate::findExportsFile()
{
  KConfig knfsshare("knfsshare");
  KConfigGroup config(&knfsshare, "General");
  exportsFile = config.readPathEntry("exportsFile", QString());

  if ( QFile::exists(exportsFile) )
    return true;

  if ( QFile::exists("/etc/exports") )
    exportsFile = "/etc/exports";
  else {
    //qDebug() << "Could not find exports file! /etc/exports doesn't exist. Configure it in share/config/knfsshare, [General], exportsFile=....";
    return false;
  }

  config.writeEntry("exportsFile",exportsFile);
  return true;
}

/**
 * Reads all paths from the exports file
 * and fills the sharedPaths dict with the values
 */
bool KNFSShare::KNFSSharePrivate::readExportsFile()
{
  QFile f(exportsFile);

  //qDebug() << exportsFile;

  if (!f.open(QIODevice::ReadOnly)) {
    qWarning() << "KNFSShare: Could not open" << exportsFile;
    return false;
  }

  sharedPaths.clear();

  QTextStream s( &f );

  bool continuedLine = false; // is true if the line before ended with a backslash
  QString completeLine;

  while ( !s.atEnd() )
  {
    QString currentLine = s.readLine().trimmed();

    if (continuedLine) {
      completeLine += currentLine;
      continuedLine = false;
    }
    else
      completeLine = currentLine;

    // is the line continued in the next line ?
    if ( completeLine.endsWith(QLatin1Char('\\')) )
    {
      continuedLine = true;
      // remove the ending backslash
      completeLine.chop(1);
      continue;
    }

    // comments or empty lines
    if (completeLine.startsWith(QLatin1Char('#')) || completeLine.isEmpty())
    {
      continue;
    }

    QString path;

    // Handle quotation marks
    if ( completeLine[0] == QLatin1Char('\"') ) {
      int i = completeLine.indexOf(QLatin1Char('"'), 1);
      if (i == -1) {
        qWarning() << "KNFSShare: Parse error: Missing quotation mark:" << completeLine;
        continue;
      }
      path = completeLine.mid(1,i-1);

    } else { // no quotation marks
      int i = completeLine.indexOf(QLatin1Char(' '));
      if (i == -1)
          i = completeLine.indexOf(QLatin1Char('\t'));

      if (i == -1)
        path = completeLine;
      else
        path = completeLine.left(i);

    }

    //qDebug() << "KNFSShare: Found path: " << path;

    if (!path.isEmpty()) {
        // normalize path
        if ( !path.endsWith(QLatin1Char('/')) )
            path += QLatin1Char('/');

        sharedPaths.insert(path);
    }
  }

  return true;
}

KNFSShare::KNFSShare()
    : d(new KNFSSharePrivate(this))
{
  if (QFile::exists(d->exportsFile)) {
    KDirWatch::self()->addFile(d->exportsFile);
    connect(KDirWatch::self(), SIGNAL(dirty(QString)),this,
               SLOT(_k_slotFileChange(QString)));
  }
}

KNFSShare::~KNFSShare()
{
  // This is not needed, we're exiting the process anyway, and KDirWatch is already deleted.
  //if (QFile::exists(d->exportsFile)) {
  //  KDirWatch::self()->removeFile(d->exportsFile);
  //}
  delete d;
}


bool KNFSShare::isDirectoryShared( const QString & path ) const
{
  if( path.isEmpty())
      return false;
  QString fixedPath = path;
  if ( path[path.length()-1] != '/' ) 
       fixedPath += '/';

  return d->sharedPaths.contains(fixedPath);
}

QStringList KNFSShare::sharedDirectories() const
{
  return d->sharedPaths.values();
}

QString KNFSShare::exportsPath() const
{
  return d->exportsFile;
}



void KNFSShare::KNFSSharePrivate::_k_slotFileChange( const QString & path )
{
  if (path == exportsFile)
     readExportsFile();

  emit q->changed();
}

class KNFSShareSingleton
{
public:
  KNFSShare instance;
};

Q_GLOBAL_STATIC(KNFSShareSingleton, _instance)

KNFSShare* KNFSShare::instance()
{
  return &_instance()->instance;
}

#include "moc_knfsshare.cpp"

