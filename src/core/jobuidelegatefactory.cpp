/*
    This file is part of the KDE libraries
    SPDX-FileCopyrightText: 2013 David Faure <faure@kde.org>
    SPDX-FileCopyrightText: 2022 Harald Sitter <sitter@kde.org>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#include <jobuidelegatefactory.h>

using namespace KIO;

JobUiDelegateFactory::JobUiDelegateFactory()
    : d(nullptr)
{
}

JobUiDelegateFactory::~JobUiDelegateFactory() = default;

static JobUiDelegateFactory *s_factory = nullptr;
static JobUiDelegateFactoryV2 *s_factoryV2 = nullptr;

KJobUiDelegate *KIO::createDefaultJobUiDelegate()
{
    return s_factory ? s_factory->createDelegate() : nullptr;
}

KJobUiDelegate *KIO::createDefaultJobUiDelegate(KJobUiDelegate::Flags flags, QWidget *window)
{
    return s_factoryV2 ? s_factoryV2->createDelegate(flags, window) : nullptr;
}

#if KIOCORE_ENABLE_DEPRECATED_SINCE(5, 98)
JobUiDelegateFactory *KIO::defaultJobUiDelegateFactory()
{
    return s_factory;
}
#endif

JobUiDelegateFactoryV2 *KIO::defaultJobUiDelegateFactoryV2()
{
    return s_factoryV2;
}

#if KIOCORE_ENABLE_DEPRECATED_SINCE(5, 98)
void KIO::setDefaultJobUiDelegateFactory(JobUiDelegateFactory *factory)
{
    s_factory = factory;
}
#endif

void KIO::setDefaultJobUiDelegateFactoryV2(JobUiDelegateFactoryV2 *factory)
{
    s_factoryV2 = factory;
    s_factory = factory;
}
