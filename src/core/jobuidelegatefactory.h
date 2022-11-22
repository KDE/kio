/*
    This file is part of the KDE libraries
    SPDX-FileCopyrightText: 2013 David Faure <faure+bluesystems@kde.org>
    SPDX-FileCopyrightText: 2022 Harald Sitter <sitter@kde.org>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#ifndef KIO_JOBUIDELEGATEFACTORY_H
#define KIO_JOBUIDELEGATEFACTORY_H

#include "job_base.h"
#include "kiocore_export.h"
#include <KJobUiDelegate>
#include <QDateTime>
#include <kio/global.h>

#include <KCompositeJob>

class QWidget;

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

class KIOCORE_EXPORT JobUiDelegateFactoryV2 : public JobUiDelegateFactory
{
protected:
    using JobUiDelegateFactory::JobUiDelegateFactory;

public:
    KJobUiDelegate *createDelegate() const override = 0;
    virtual KJobUiDelegate *createDelegate(KJobUiDelegate::Flags flags, QWidget *window) const = 0;
};

/**
 * Convenience method: use default factory, if there's one, to create a delegate and return it.
 */
KIOCORE_EXPORT KJobUiDelegate *createDefaultJobUiDelegate();

/**
 * Convenience method: use default factory, if there's one, to create a delegate and return it.
 *
 * @since 5.98
 */
KIOCORE_EXPORT KJobUiDelegate *createDefaultJobUiDelegate(KJobUiDelegate::Flags flags, QWidget *window);

#if KIOCORE_ENABLE_DEPRECATED_SINCE(5, 98)
/**
 * Returns the default job UI delegate factory to be used by all KIO jobs (in which HideProgressInfo is not set)
 * Can return nullptr, if no kio GUI library is loaded.
 * @since 5.0
 * @deprecated Since 5.98, use defaultJobUiDelegateFactoryV2 instead.
 */
KIOCORE_EXPORT
KIOCORE_DEPRECATED_VERSION(5, 98, "use defaultJobUiDelegateFactoryV2")
JobUiDelegateFactory *defaultJobUiDelegateFactory();
#endif

/**
 * Returns the default job UI delegate factory to be used by all KIO jobs (in which HideProgressInfo is not set)
 * Can return nullptr, if no kio GUI library is loaded.
 * @since 5.98
 */
KIOCORE_EXPORT JobUiDelegateFactoryV2 *defaultJobUiDelegateFactoryV2();

#if KIOCORE_ENABLE_DEPRECATED_SINCE(5, 98)
/**
 * Internal. Allows the KIO widgets library to register its widget-based job UI delegate factory
 * automatically.
 * @since 5.0
 * @deprecated Since 5.98, use setDefaultJobUiDelegateFactoryV2
 */
KIOCORE_EXPORT
KIOCORE_DEPRECATED_VERSION(5, 98, "use setDefaultJobUiDelegateFactoryV2")
void setDefaultJobUiDelegateFactory(JobUiDelegateFactory *factory);
#endif

/**
 * Internal. Allows the KIO widgets library to register its widget-based job UI delegate factory
 * automatically.
 * @since 5.98
 */
KIOCORE_EXPORT void setDefaultJobUiDelegateFactoryV2(JobUiDelegateFactoryV2 *factory);

/**
 * Returns the child of the job's uiDelegate() that implements the given extension,
 * or nullptr if none was found (or if the job had no uiDelegate).
 * @since 5.78
 */
template<typename T>
inline T delegateExtension(KJob *job)
{
    KJobUiDelegate *ui = job->uiDelegate();

    // If setParentJob() was used, try the uiDelegate of parentJob first
    if (!ui) {
        if (KIO::Job *kiojob = qobject_cast<KIO::Job *>(job)) {
            if (KJob *parentJob = kiojob->parentJob()) {
                ui = parentJob->uiDelegate();
            }
        }
    }

    // Still nothing? if compositeJob->addSubjob(job) was used, try the ui delegate
    // of compositeJob
    while (!ui) {
        job = qobject_cast<KCompositeJob *>(job->parent());
        if (job) {
            ui = job->uiDelegate();
        } else {
            break;
        }
    }

    return ui ? ui->findChild<T>(QString(), Qt::FindDirectChildrenOnly) : nullptr;
}

} // namespace KIO

#endif
