/* This file is part of the KDE project
   Copyright (C) 2004 David Faure <faure@kde.org>

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
   the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.
*/

#ifndef TESTTRASH_H
#define TESTTRASH_H

class TestTrash
{
public:
    TestTrash() {}
    void setup();
    void cleanTrash();
    void runAll();

    // tests

    void urlTestFile();
    void urlTestDirectory();
    void urlTestSubDirectory();

    void trashFileFromHome();
    void trashFileFromOther();
    void trashSymlinkFromHome();
    void trashSymlinkFromOther();
    void trashDirectoryFromHome();
    void trashDirectoryFromOther();

    void tryRenameInsideTrash();

    void delRootFile();
    void delFileInDirectory();
    void delDirectory();

    void statRoot();
    void statFileInRoot();
    void statDirectoryInRoot();
    void statSymlinkInRoot();
    void statFileInDirectory();

    void copyFileFromTrash();
    void copyDirectoryFromTrash();
    void copySymlinkFromTrash();

    void moveFileFromTrash();
    void moveDirectoryFromTrash();
    void moveSymlinkFromTrash();

private:
    void trashFile( const QString& origFilePath, const QString& fileId );
    void trashSymlink( const QString& origFilePath, const QString& fileName );
    void trashDirectory( const QString& origPath, const QString& fileName );
    void copyFromTrash( const QString& fileId, const QString& destPath );
    void moveFromTrash( const QString& fileId, const QString& destPath );

    QString homeTmpDir() const;
    QString otherTmpDir() const;
};

#endif
