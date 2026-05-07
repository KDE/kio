// SPDX-License-Identifier: LGPL-2.0-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
// SPDX-FileCopyrightText: 2022-2026 Harald Sitter <sitter@kde.org>

#pragma once

#include "kiogui_export.h"

#include <KJob>

class QWindow;

namespace KIO
{

/*!
 * \class KIO::ApplicationChooserJob
 * \inheaderfile KIO/ApplicationChooserJob
 * \inmodule KIOGui
 *
 * \brief Brings up an application chooser UI for a URL.
 * \since 6.TODO
 *
 * This job is backed by the xdg-desktop-portal and as such is only useful on platforms where that is available (i.e. Linux and friends).
 */
class KIOGUI_EXPORT ApplicationChooserJob : public KJob
{
    Q_OBJECT

public:
    /*!
     * Create a new job. This being a KJob it will auto-delete by default!
     *
     * \a url the URL of the file to open. \a exportWritable indicates whether the file should be passed on as writable
     * to a sandboxed application. This has no effect unless the URL is a local file and the target application is sandboxed.
     * \a parentWindow is the window the chooser dialog will be parented to
     */
    [[nodiscard]] static ApplicationChooserJob *create(const QUrl &url, bool exportWritable, QWindow *parentWindow);
    ~ApplicationChooserJob() override;
    Q_DISABLE_COPY_MOVE(ApplicationChooserJob);
    /*! start the job */
    void start() override;

private:
    explicit ApplicationChooserJob(std::unique_ptr<class ApplicationChooserJobPrivate> dd);
    std::unique_ptr<class ApplicationChooserJobPrivate> d;
    friend class ApplicationChooserJobPrivate;
};

} // namespace KIO
