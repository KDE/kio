/*
    SPDX-FileCopyrightText: 2022 Carson Black <uhhadd@gmail.com>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#import <Foundation/Foundation.h>
#import "file_bookmark_mac.h"

bool insertBookmartIntoUDSEntry(const QString &filename, const QByteArray &path, KIO::UDSEntry &entry, KIO::StatDetails details, const QString &fullPath)
{
    NSError* theError = nil;
    NSURL* url = [NSURL fileURLWithPath: QString::fromLocal8Bit(path).toNSString()
                        isDirectory: NO];

    NSData* bookmark = [url bookmarkDataWithOptions: NSURLBookmarkCreationSuitableForBookmarkFile
                            includingResourceValuesForKeys: nil
                            relativeToURL: nil
                            error: &theError];

    if (theError || (bookmark == nil)) {
        return false;
    }

    entry.fastInsert(KIO::UDSEntry::UDS_PERSISTENT_IDENTIFIER, QString::fromLocal8Bit(QByteArray::fromNSData(bookmark)));
    return true;
}
