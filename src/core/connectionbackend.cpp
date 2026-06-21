/*
    This file is part of the KDE libraries
    SPDX-FileCopyrightText: 2000 Stephan Kulow <coolo@kde.org>
    SPDX-FileCopyrightText: 2000 David Faure <faure@kde.org>
    SPDX-FileCopyrightText: 2007 Thiago Macieira <thiago@kde.org>
    SPDX-FileCopyrightText: 2024 Harald Sitter <sitter@kde.org>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#include "connectionbackend_p.h"

// ConnectionBackend is an abstract base. This translation unit only exists to compile its
// meta-object (signals). The concrete backends live in socketconnectionbackend.cpp and
// threadconnectionbackend.cpp.

#include "moc_connectionbackend_p.cpp"
