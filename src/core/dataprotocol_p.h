/*
    Interface of the KDE data protocol core operations

    SPDX-FileCopyrightText: 2002 Leo Savernik <l.savernik@aon.at>

    SPDX-License-Identifier: LGPL-2.0-only
*/

#ifndef DATAPROTOCOL_H
#define DATAPROTOCOL_H

// dataprotocol.* interprets the following defines
// TESTKIO: define for test-driving
// Both defines are mutually exclusive. Defining none of them compiles
// DataProtocol for internal usage within libkiocore.

/* Wondering what this is all about? Leo explained it to me:
 *
 * That's simple, you can compile it into a standalone executable that is
 * registered like any other kioslave.
 *
 * However, given that data-urls don't depend on any external data it seemed
 * overkill, therefore I added a special hack that the kio-dataslave is invoked
 * in-process on the client side.
 */

class QByteArray;

class QUrl;

#if !defined(TESTKIO)
#include "dataslave_p.h"
#endif

namespace KIO
{
/** This kioslave provides support of data urls as specified by rfc 2397
 * @see http://www.ietf.org/rfc/rfc2397.txt
 * @author Leo Savernik
 */
#if defined(TESTKIO)
class DataProtocol : public TestSlave
{
#else
class DataProtocol : public DataSlave
{
    Q_OBJECT
#endif

public:
    DataProtocol();

#if defined(TESTKIO)
    void mimetype(const QUrl &url);
    void get(const QUrl &url);
#else
    void mimetype(const QUrl &url) override;
    void get(const QUrl &url) override;
#endif
    ~DataProtocol() override;
};

} /*end namespace*/

#endif
