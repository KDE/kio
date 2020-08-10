/*
    SPDX-FileCopyrightText: 2015 Gregor Mi <codestruct@posteo.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

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
        const QUrl adjustedLastUrl = lastUrl.adjusted(QUrl::StripTrailingSlash);
        const QUrl adjustedCurrentUrl = currentUrl.adjusted(QUrl::StripTrailingSlash);
        if (!adjustedCurrentUrl.isParentOf(adjustedLastUrl)) {
            return QUrl();
        }

        const QString childPath = adjustedLastUrl.path();
        const QString parentPath = adjustedCurrentUrl.path();
        // if the parent path is root "/"
        // one char more is a valid path, otherwise "/" and another char are needed.
        const int minIndex = (parentPath == QLatin1String("/")) ? 1 : 2;

        // e.g. this would just be ok:
        // childPath  = /a        len=2
        // parentPath = /         len=1
        // childPath  = /home/a   len=7
        // parentPath = /home     len=5

        if (childPath.length() < (parentPath.length() + minIndex) ) {
            return QUrl();
        }

        const int idx2 = childPath.indexOf(QLatin1Char('/'), parentPath.length() + minIndex);
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
