/* This file is part of the KDE libraries
    Copyright (C) 2013 David Faure <faure+bluesystems@kde.org>

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

#ifndef KIO_JOBUIDELEGATEFACTORY_H
#define KIO_JOBUIDELEGATEFACTORY_H

#include "kiocore_export.h"
#include <kio/global.h>
#include <QDateTime>

class KJobUiDelegate;

namespace KIO
{

/**
 * @class KIO::JobUiDelegateFactory jobuidelegatefactory.h <KIO/JobUiDelegateFactory>
 *
 * A factory for creating job ui delegates.
 * Every KIO job will get a delegate from this factory.
 * \since 5.0
 */
class KIOCORE_EXPORT JobUiDelegateFactory
{
protected:
    /**
     * Constructor
     */
    JobUiDelegateFactory();

    /**
     * Destructor
     */
    virtual ~JobUiDelegateFactory();

public:
    virtual KJobUiDelegate *createDelegate() const = 0;

private:
    class Private;
    Private *const d;
};

/**
 * Convenience method: use default factory, if there's one, to create a delegate and return it.
 */
KIOCORE_EXPORT KJobUiDelegate *createDefaultJobUiDelegate();

/**
 * Returns the default job UI delegate factory to be used by all KIO jobs (in which HideProgressInfo is not set)
 * Can return nullptr, if no kio GUI library is loaded.
 * @since 5.0
 */
KIOCORE_EXPORT JobUiDelegateFactory *defaultJobUiDelegateFactory();

/**
 * Internal. Allows the KIO widgets library to register its widget-based job UI delegate factory
 * automatically.
 * @since 5.0
 */
KIOCORE_EXPORT void setDefaultJobUiDelegateFactory(JobUiDelegateFactory *factory);

} // namespace KIO

#endif
