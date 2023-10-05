// SPDX-License-Identifier: LGPL-2.0-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
// SPDX-FileCopyrightText: 2023 Harald Sitter <sitter@kde.org>

#include "buildsycocainterface.h"

namespace KIO
{

class BuildSycocaInterfacePrivate
{
};

BuildSycocaInterface::BuildSycocaInterface(QObject *parent)
    : QObject(parent)
{
}

BuildSycocaInterface::~BuildSycocaInterface() = default;

void BuildSycocaInterface::showProgress()
{
}

void BuildSycocaInterface::hideProgress()
{
}

} // namespace KIO
