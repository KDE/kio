// -*- c++ -*-
/*
    This file is part of the KDE libraries
    SPDX-FileCopyrightText: 2002 Jan-Pascal van Best <janpascal@vanbest.org>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#ifndef KIO_DAVJOB_H
#define KIO_DAVJOB_H

#include <QObject>
#include <QString>
#include <QStringList>
#include <QPointer>
#include <QDomDocument>

#include <sys/types.h>
#include <sys/stat.h>

#include "kiocore_export.h"
#include "transferjob.h"
#include "global.h"

namespace KIO
{

class Slave;

class DavJobPrivate;
/**
 * @class KIO::DavJob davjob.h <KIO/DavJob>
 *
 * The transfer job pumps data into and/or out of a Slave.
 * Data is sent to the slave on request of the slave ( dataReq).
 * If data coming from the slave can not be handled, the
 * reading of data from the slave should be suspended.
 * @see KIO::davPropFind()
 * @see KIO::davPropPatch()
 * @see KIO::davSearch()
 */
class KIOCORE_EXPORT DavJob : public TransferJob
{
    Q_OBJECT
public:
    /**
    * Returns the response as a QDomDocument.
     * @return the response document
     */
    QDomDocument &response();

protected Q_SLOTS:
    void slotFinished() override;
    void slotData(const QByteArray &data) override;

protected:
    DavJob(DavJobPrivate &dd, int, const QString &);
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
 */
KIOCORE_EXPORT DavJob *davPropFind(const QUrl &url, const QDomDocument &properties, const QString &depth, JobFlags flags = DefaultFlags);

/**
 * Creates a new DavJob that issues a PROPPATCH command. PROPPATCH sets
 * the properties of the resource identified by the given @p url.
 *
 * @param url the URL of the resource
 * @param properties a PROPPACTCH document that describes the properties that
 *        should be modified and its new values
 * @param flags We support HideProgressInfo here
 * @return the new DavJob
 */
KIOCORE_EXPORT DavJob *davPropPatch(const QUrl &url, const QDomDocument &properties, JobFlags flags = DefaultFlags);

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

