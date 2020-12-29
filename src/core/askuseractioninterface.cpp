/*
    This file is part of the KDE libraries
    SPDX-FileCopyrightText: 2020 Ahmad Samir <a.samirh78@gmail.com>

    SPDX-License-Identifier: LGPL-2.0-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/

#include "askuseractioninterface.h"

class KIO::AskUserActionInterfacePrivate
{
public:
    AskUserActionInterfacePrivate()
    {
    }
};

KIO::AskUserActionInterface::AskUserActionInterface(QObject *parent)
    : QObject(parent)
{
}

KIO::AskUserActionInterface::~AskUserActionInterface()
{
}
