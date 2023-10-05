// SPDX-License-Identifier: LGPL-2.0-only
// SPDX-FileCopyrightText: 2003 Waldo Bastian <bastian@kde.org>
// SPDX-FileCopyrightText: 2023 Harald Sitter <sitter@kde.org>

#pragma once

#include "kiocore_export.h"

#include <memory>

#include "job_base.h"

class QWindow;

namespace KIO
{

class BuildSycocaJobPrivate;

/**
 * Rebuild KSycoca and show a progress dialog while doing so.
 * @param parent Parent widget for the progress dialog
 * @since 6.0
 */
class KIOCORE_EXPORT BuildSycocaJob : public KJob
{
    Q_OBJECT
public:
    explicit BuildSycocaJob(QObject *parent = nullptr);
    ~BuildSycocaJob() override;
    Q_DISABLE_COPY_MOVE(BuildSycocaJob)

    void start() override;

private:
    std::unique_ptr<BuildSycocaJobPrivate> d;
};

/**
 * @brief Creates a new BuildSycocaJob
 *
 * Use this to conveniently construct a new BuildSycocaJob.
 *
 * @param flags We support HideProgressInfo here
 * @return a BuildSycocaJob
 * @since 6.0
 */
[[nodiscard]] KIOCORE_EXPORT BuildSycocaJob *buildSycoca(JobFlags flags = DefaultFlags);

} // namespace KIO
