// -*- c++ -*-
/*
    This file is part of the KDE libraries
    SPDX-FileCopyrightText: 2002 Jan-Pascal van Best <janpascal@vanbest.org>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#ifndef KIO_DAVJOB_H
#define KIO_DAVJOB_H

#include "global.h"
#include "kiocore_export.h"
#include "transferjob.h"

#if KIOCORE_ENABLE_DEPRECATED_SINCE(5, 86)
#include <QDomDocument>
#endif
#include <QObject>
#include <QPointer>
#include <QString>
#include <QStringList>

#include <sys/stat.h>
#include <sys/types.h>

namespace KIO
{
#if KIOCORE_ENABLE_DEPRECATED_SINCE(5, 101)
// unused forward declaration
class Slave;
#endif

class DavJobPrivate;
/**
 * @class KIO::DavJob davjob.h <KIO/DavJob>
 *
 * The transfer job pumps data into and/or out of a KIO worker.
 * Data is sent to the worker on request of the worker ( dataReq).
 * If data coming from the worker can not be handled, the
 * reading of data from the worker should be suspended.
 * @see KIO::davPropFind()
 * @see KIO::davPropPatch()
 * @see KIO::davSearch()
 */
class KIOCORE_EXPORT DavJob : public TransferJob
{
    Q_OBJECT
public:
    /**
     * Returns the reponse data.
     *  @since 5.86
     */
    QByteArray responseData() const;

#if KIOCORE_ENABLE_DEPRECATED_SINCE(5, 86)
    /**
     * Returns the response as a QDomDocument.
     * @return the response document
     * @deprecated Since 5.86. Use QDomDocument::setContent(job->responseData()) if you need
     * a QDomDocument of the resonse, but be aware that you need to handle the case that
     * responseData() doesn't return valid XML if that is a relevant error scenario for you
     * (response() does wrap such data into a DAV error XML structure).
     */
    KIOCORE_DEPRECATED_VERSION(5, 86, "Use responseData() instead.")
    QDomDocument &response();
#endif

protected Q_SLOTS:
    void slotFinished() override;
    void slotData(const QByteArray &data) override;

protected:
    KIOCORE_NO_EXPORT DavJob(DavJobPrivate &dd, int, const QString &);

private:
    Q_DECLARE_PRIVATE(DavJob)
};

/**
 * Creates a new DavJob that issues a PROPFIND command. PROPFIND retrieves
 * the properties of the resource identified by the given @p url.
 *
 * @param url the URL of the resource
 * @param properties a propfind document that describes the properties that
 *        should be retrieved
 * @param depth the depth of the request. Can be "0", "1" or "infinity"
 * @param flags We support HideProgressInfo here
 * @return the new DavJob
 * @since 5.84
 */
KIOCORE_EXPORT DavJob *davPropFind(const QUrl &url, const QString &properties, const QString &depth, JobFlags flags = DefaultFlags);

/**
 * Creates a new DavJob that issues a PROPPATCH command. PROPPATCH sets
 * the properties of the resource identified by the given @p url.
 *
 * @param url the URL of the resource
 * @param properties a PROPPACTCH document that describes the properties that
 *        should be modified and its new values
 * @param flags We support HideProgressInfo here
 * @return the new DavJob
 * @since 5.84
 */
KIOCORE_EXPORT DavJob *davPropPatch(const QUrl &url, const QString &properties, JobFlags flags = DefaultFlags);

#if KIOCORE_ENABLE_DEPRECATED_SINCE(5, 84)
/**
 * Creates a new DavJob that issues a PROPFIND command. PROPFIND retrieves
 * the properties of the resource identified by the given @p url.
 *
 * @param url the URL of the resource
 * @param properties a propfind document that describes the properties that
 *        should be retrieved
 * @param depth the depth of the request. Can be "0", "1" or "infinity"
 * @param flags We support HideProgressInfo here
 * @return the new DavJob
 * @deprecated since 5.84, use the overload taking a @c QString @p properties argument instead.
 * This can typically be done by replacing the properties argument with <tt>properties.toString()</tt>.
 */
KIOCORE_EXPORT
KIOCORE_DEPRECATED_VERSION(5, 84, "Use davPropFind(const QUrl &, const QString &, const QString &, JobFlags) instead.")
DavJob *davPropFind(const QUrl &url, const QDomDocument &properties, const QString &depth, JobFlags flags = DefaultFlags);
#endif

#if KIOCORE_ENABLE_DEPRECATED_SINCE(5, 84)
/**
 * Creates a new DavJob that issues a PROPPATCH command. PROPPATCH sets
 * the properties of the resource identified by the given @p url.
 *
 * @param url the URL of the resource
 * @param properties a PROPPACTCH document that describes the properties that
 *        should be modified and its new values
 * @param flags We support HideProgressInfo here
 * @return the new DavJob
 * @deprecated since 5.84, use the overload taking a @c QString @p properties argument instead.
 * This can typically be done by replacing the properties argument with <tt>properties.toString()</tt>.
 */
KIOCORE_EXPORT
KIOCORE_DEPRECATED_VERSION(5, 84, "Use davPropPatch(const QUrl &, const QString &, JobFlags) instead.")
DavJob *davPropPatch(const QUrl &url, const QDomDocument &properties, JobFlags flags = DefaultFlags);
#endif

/**
 * Creates a new DavJob that issues a SEARCH command.
 *
 * @param url the URL of the resource
 * @param nsURI the URI of the search method's qualified name
 * @param qName the local part of the search method's qualified name
 * @param query the search string
 * @param flags We support HideProgressInfo here
 * @return the new DavJob
 */
KIOCORE_EXPORT DavJob *davSearch(const QUrl &url, const QString &nsURI, const QString &qName, const QString &query, JobFlags flags = DefaultFlags);

/**
 * Creates a new DavJob that issues a REPORT command.
 *
 * @param url the URL of the resource
 * @param report a REPORT document that describes the request to make
 * @param depth the depth of the request. Can be "0", "1" or "infinity"
 * @param flags We support HideProgressInfo here
 * @return the new DavJob
 * @since 4.4
 */
KIOCORE_EXPORT DavJob *davReport(const QUrl &url, const QString &report, const QString &depth, JobFlags flags = DefaultFlags);

}

#endif
