/*
    This file is part of the KDE libraries
    SPDX-FileCopyrightText: 2013 David Faure <faure+bluesystems@kde.org>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#ifndef KIO_JOBUIDELEGATEFACTORY_H
#define KIO_JOBUIDELEGATEFACTORY_H

#include "kiocore_export.h"
#include <kio/global.h>
#include <KJobUiDelegate>
#include <QDateTime>

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


/**
 * Returns the child of the job's uiDelegate() that implements the given extension,
 * or nullptr if none was found (or if the job had no uiDelegate).
 * @since 5.78
 */
template <typename T>
inline T delegateExtension(KJob *job) {
    const KJobUiDelegate *ui = job->uiDelegate();
    return ui ? ui->findChild<T>(QString(), Qt::FindDirectChildrenOnly) : nullptr;
}

} // namespace KIO

#endif
