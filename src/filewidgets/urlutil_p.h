/***************************************************************************
 * Copyright (C) 2015 by Gregor Mi <codestruct@posteo.org>                 *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA            *
 ***************************************************************************/

#ifndef URLUTIL_P_H
#define URLUTIL_P_H

#include <QUrl>

namespace KIO {
namespace UrlUtil {

    /**
     * Given that @p lastUrl is a child of @p currentUrl
     * or put in other words:
     * @p currentUrl is a parent in the hierarchy of @p lastUrl,
     * then an URL 'currentUrl'/'childitem' is returned where
     * 'childitem' is the first item in the hierarchy down to
     * @p lastUrl. An example will illustrate this:

       \verbatim
       lastUrl    : "/home/test/data/documents/muh/"
       currentUrl : "/home/test/"
       returns    : "/home/test/data/"
       \endverbatim

     * In case @p currentUrl is a child of @p lastUrl, an invalid
     * URL is returned:

       \verbatim
       lastUrl    : "/home/"
       currentUrl : "/home/test/"
       returns    : "" (invalid url)
       \endverbatim

     * In case both URLs are equal, an invalid URL is returned:

       \verbatim
       lastUrl    : "/home/test/"
       currentUrl : "/home/test/"
       returns    : "" (invalid url)
       \endverbatim
     */
    static QUrl firstChildUrl(const QUrl& lastUrl, const QUrl& currentUrl)
    {
        if (!currentUrl.isParentOf(lastUrl)) {
            return QUrl();
        }

        const QUrl adjustedLastUrl = lastUrl.adjusted(QUrl::StripTrailingSlash);
        const QUrl adjustedCurrentUrl = currentUrl.adjusted(QUrl::StripTrailingSlash);
        const QString childPath = adjustedLastUrl.path();
        const QString parentPath = adjustedCurrentUrl.path();

        // e.g. this would just be ok:
        // childPath  = /home/a   len=7
        // parentPath = /home     len=5
        Q_ASSERT(childPath.length() >= (parentPath.length() + 2));

        const int idx2 = childPath.indexOf(QLatin1Char('/'), parentPath.length() + 2);
        // parentPath = /home
        // childPath  = /home/a
        //                 idx  = -1
        //              => len2 =  7
        //
        // childPath = /homa/a/b
        //                 idx  = 7
        //              => len2 = 7
        const int len2 = (idx2 < 0) ? childPath.length() : idx2;

        const QString path3 = childPath.left(len2);

        QUrl res = lastUrl; // keeps the scheme (e.g. file://)
        res.setPath(path3);
        return res;
    }
}
}

#endif
