/* This file is part of the KDE project
   Copyright (C) 1998, 1999 Torben Weis <weis@kde.org>
   Copyright (c) 1999, 2000 Preston Brown <pbrown@kde.org>
   Copyright (c) 2000 Simon Hausmann <hausmann@kde.org>
   Copyright (c) 2000 David Faure <faure@kde.org>

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public
   License as published by the Free Software Foundation; either
   version 2 of the License, or (at your option) any later version.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public License
   along with this library; see the file COPYING.LIB.  If not, write to
   the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
   Boston, MA 02110-1301, USA.
*/

/*
 * This file holds the definitions for all classes used to
 * display a properties dialog.
 */


#ifndef KPROPERTIESDIALOGP_H
#define KPROPERTIESDIALOGP_H


#include "kpropertiesdialog.h"

class KComboBox;
class QComboBox;

class Ui_KPropertiesDesktopBase;

namespace KDEPrivate
{

/**
 * 'General' plugin
 *  This plugin displays the name of the file, its size and access times.
 * @internal
 */
class KFilePropsPlugin : public KPropertiesDialogPlugin
{
  Q_OBJECT
public:
  /**
   * Constructor
   */
  KFilePropsPlugin( KPropertiesDialog *_props );
  virtual ~KFilePropsPlugin();

  /**
   * Applies all changes made.  This plugin must be always the first
   * plugin in the dialog, since this function may rename the file which
   * may confuse other applyChanges functions.
   */
  virtual void applyChanges();

  /**
   * Tests whether the files specified by _items need a 'General' plugin.
   */
  static bool supports( const KFileItemList& _items );

  /**
   * Called after all plugins applied their changes
   */
  void postApplyChanges();

  void setFileNameReadOnly( bool ro );

protected Q_SLOTS:
  void slotEditFileType();
  void slotCopyFinished( KJob * );
  void slotFileRenamed(KIO::Job *, const QUrl &, const QUrl &);
  void slotDirSizeUpdate();
  void slotDirSizeFinished( KJob * );
  void slotFoundMountPoint( const QString& mp, quint64 kibSize,
                            quint64 kibUsed, quint64 kibAvail );
  void slotSizeStop();
  void slotSizeDetermine();

Q_SIGNALS:
  void leaveModality();
private Q_SLOTS:
  void nameFileChanged(const QString &text );
  void slotIconChanged();

private:
  bool enableIconButton() const;
  void determineRelativePath( const QString & path );
  void applyIconChanges();

  class KFilePropsPluginPrivate;
  KFilePropsPluginPrivate* const d;
};

/**
 * 'Permissions' plugin
 * In this plugin you can modify permissions and change
 * the owner of a file.
 * @internal
 */
class KFilePermissionsPropsPlugin : public KPropertiesDialogPlugin
{
  Q_OBJECT
public:
  enum PermissionsMode {
    PermissionsOnlyFiles = 0,
    PermissionsOnlyDirs = 1,
    PermissionsOnlyLinks = 2,
    PermissionsMixed = 3
  };

  enum PermissionsTarget {
    PermissionsOwner  = 0,
    PermissionsGroup  = 1,
    PermissionsOthers = 2
  };

  /**
   * Constructor
   */
  KFilePermissionsPropsPlugin( KPropertiesDialog *_props );
  virtual ~KFilePermissionsPropsPlugin();

  virtual void applyChanges();

  /**
   * Tests whether the file specified by _items needs a 'Permissions' plugin.
   */
  static bool supports( const KFileItemList& _items );

private Q_SLOTS:

  void slotChmodResult( KJob * );
  void slotShowAdvancedPermissions();

Q_SIGNALS:
  void leaveModality();

private:
  void setComboContent(QComboBox *combo, PermissionsTarget target,
		       mode_t permissions, mode_t partial);
  bool isIrregular(mode_t permissions, bool isDir, bool isLink);
  void enableAccessControls(bool enable);
  void updateAccessControls();
  void getPermissionMasks(mode_t &andFilePermissions,
			  mode_t &andDirPermissions,
			  mode_t &orFilePermissions,
			  mode_t &orDirPermissions);

  static const mode_t permissionsMasks[3];
  static const mode_t standardPermissions[4];
  static const char *permissionsTexts[4][4];

  static mode_t fperm[3][4];

  class KFilePermissionsPropsPluginPrivate;
  KFilePermissionsPropsPluginPrivate* const d;
};


/**
 * Used to edit the files containing
 * [Desktop Entry]
 * URL=....
 *
 * Such files are used to represent a program in kicker and konqueror.
 * @internal
 */
class KUrlPropsPlugin : public KPropertiesDialogPlugin
{
  Q_OBJECT
public:
  /**
   * Constructor
   */
  KUrlPropsPlugin( KPropertiesDialog *_props );
  virtual ~KUrlPropsPlugin();

  virtual void applyChanges();

  static bool supports( const KFileItemList& _items );

private:
  class KUrlPropsPluginPrivate;
  KUrlPropsPluginPrivate* const d;
};


/**
 * Properties plugin for device .desktop files
 * @internal
 */
class KDevicePropsPlugin : public KPropertiesDialogPlugin
{
  Q_OBJECT
public:
  KDevicePropsPlugin( KPropertiesDialog *_props );
  virtual ~KDevicePropsPlugin();

  virtual void applyChanges();

  static bool supports( const KFileItemList& _items );

private Q_SLOTS:
  void slotActivated( int );
  void slotDeviceChanged();
  void slotFoundMountPoint( const QString& mp, quint64 kibSize,
                            quint64 kibUsed, quint64 kibAvail );

private:
  void updateInfo();

private:
  class KDevicePropsPluginPrivate;
  KDevicePropsPluginPrivate* const d;
};

/**
 * Used to edit the files containing
 * [Desktop Entry]
 * Type=Application
 *
 * Such files are used to represent a program in kicker and konqueror.
 * @internal
 */
class KDesktopPropsPlugin : public KPropertiesDialogPlugin
{
  Q_OBJECT
public:
  /**
   * Constructor
   */
  KDesktopPropsPlugin( KPropertiesDialog *_props );
  virtual ~KDesktopPropsPlugin();

  virtual void applyChanges();

  static bool supports( const KFileItemList& _items );

public Q_SLOTS:
  void slotAddFiletype();
  void slotDelFiletype();
  void slotBrowseExec();
  void slotAdvanced();

private:
  void checkCommandChanged();

private:
  class KDesktopPropsPluginPrivate;
  KDesktopPropsPluginPrivate * const d;
};

}

#endif
