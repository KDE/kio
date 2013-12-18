/* This file is part of the KDE libraries
    Copyright (C) 2008 Jarosław Staniek <staniek@kde.org>

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

#include "krun.h"
#include "krun_p.h"

#include <QDir>
#include <QWidget>

#include <windows.h>

// TODO move to a shared lib
static int runDll(WId windowId, const QString& libraryName, const QByteArray& functionName,
            const QString& arguments)
{
  HMODULE libHandle = LoadLibraryW( (LPCWSTR)libraryName.utf16() );
  if (!libHandle)
    return 0;
  typedef int (WINAPI *FunctionType)(HWND, HMODULE, LPCWSTR, int);
#ifdef _WIN32_WCE
  QString functionNamestr = QString(functionName);
  FunctionType function
    = (FunctionType)GetProcAddressW( libHandle, functionNamestr.utf16() );
#else
  FunctionType function
    = (FunctionType)GetProcAddress( libHandle, functionName.constData() );
#endif
  if (!function)
    return 0;
  int result = function((HWND)windowId, libHandle, (LPCWSTR)arguments.utf16(), SW_SHOW);
  FreeLibrary(libHandle);
  return result;
}

static int runDll(WId windowId, const QString& libraryName, const QByteArray& functionName,
            const QByteArray& arguments)
{
  HMODULE libHandle = LoadLibraryW( (LPCWSTR)libraryName.utf16() );
  if (!libHandle)
    return 0;
  typedef int (WINAPI *FunctionType)(HWND, HMODULE, LPCSTR, int);
#ifdef _WIN32_WCE
  QString functionNamestr = QString(functionName);
  FunctionType function
    = (FunctionType)GetProcAddressW( libHandle, functionNamestr.utf16() );
#else
  FunctionType function
    = (FunctionType)GetProcAddress( libHandle, functionName.constData() );
#endif
  if (!function)
    return 0;
  int result = function((HWND)windowId, libHandle, (LPCSTR)arguments.constData(), SW_SHOW);
  FreeLibrary(libHandle);
  return result;
}

// TODO move to a shared lib
static int runDll(QWidget* parent, const QString& libraryName, const QByteArray& functionName,
            const QString& arguments)
{
  return runDll(parent ? parent->winId() : 0, libraryName, functionName, arguments);
}


// Windows implementation using "OpenAs_RunDLL" entry
bool KRun::KRunPrivate::displayNativeOpenWithDialog( const QList<QUrl>& lst, QWidget* window, bool tempFiles,
                                               const QString& suggestedFileName, const QByteArray& asn )
{
    Q_UNUSED(tempFiles);
    Q_UNUSED(suggestedFileName);
    Q_UNUSED(asn);

    QStringList fnames;
    foreach( const QUrl& url, lst )
    {
      fnames += QDir::toNativeSeparators( url.path() );
    }
    int result = runDll( window,
                         QLatin1String("shell32.dll"),
                         "OpenAs_RunDLLW",
                         fnames.join(QLatin1String(" ")) );
    return result == 0;
}
