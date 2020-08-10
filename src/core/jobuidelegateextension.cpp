/*
    This file is part of the KDE libraries
    SPDX-FileCopyrightText: 2013 David Faure <faure@kde.org>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#include <jobuidelegateextension.h>

using namespace KIO;

JobUiDelegateExtension::JobUiDelegateExtension()
    : d(nullptr)
{
}

JobUiDelegateExtension::~JobUiDelegateExtension()
{
}

ClipboardUpdater *KIO::JobUiDelegateExtension::createClipboardUpdater(Job *, ClipboardUpdaterMode)
{
    return nullptr;
}

void KIO::JobUiDelegateExtension::updateUrlInClipboard(const QUrl &, const QUrl &)
{
}

static JobUiDelegateExtension *s_extension = nullptr;

JobUiDelegateExtension *KIO::defaultJobUiDelegateExtension()
{
    return s_extension;
}

void KIO::setDefaultJobUiDelegateExtension(JobUiDelegateExtension *extension)
{
    s_extension = extension;
}

