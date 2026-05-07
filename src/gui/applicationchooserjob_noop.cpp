// SPDX-License-Identifier: LGPL-2.0-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
// SPDX-FileCopyrightText: 2022-2026 Harald Sitter <sitter@kde.org>

#include "applicationchooserjob.h"

namespace KIO
{

class ApplicationChooserJobPrivate
{
};

ApplicationChooserJob *
ApplicationChooserJob::create([[maybe_unused]] const QUrl &url, [[maybe_unused]] bool exportWritable, [[maybe_unused]] QWindow *parentWindow)
{
    return nullptr;
}

ApplicationChooserJob::ApplicationChooserJob([[maybe_unused]] std::unique_ptr<ApplicationChooserJobPrivate> dd)
{
}

// needed for unique_ptr to be fully qualified in the cpp rather than the header
ApplicationChooserJob::~ApplicationChooserJob() = default;

void ApplicationChooserJob::start()
{
}

} // namespace KIO
