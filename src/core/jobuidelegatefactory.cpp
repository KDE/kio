/*
    This file is part of the KDE libraries
    SPDX-FileCopyrightText: 2013 David Faure <faure@kde.org>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#include <jobuidelegatefactory.h>

using namespace KIO;

JobUiDelegateFactory::JobUiDelegateFactory()
    : d(nullptr)
{
}

JobUiDelegateFactory::~JobUiDelegateFactory()
{
}

static JobUiDelegateFactory *s_factory = nullptr;

KJobUiDelegate *KIO::createDefaultJobUiDelegate()
{
    return s_factory ? s_factory->createDelegate() : nullptr;
}

JobUiDelegateFactory *KIO::defaultJobUiDelegateFactory()
{
    return s_factory;
}

void KIO::setDefaultJobUiDelegateFactory(JobUiDelegateFactory *factory)
{
    s_factory = factory;
}

