/*
    This file is part of the KDE project
    SPDX-FileCopyrightText: 2010 Sebastian Trueg <trueg@kde.org>

    SPDX-License-Identifier: LGPL-2.0-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/

#include "kabstractfileitemactionplugin.h"

#include <QUrl>
#include <QWidget>

KAbstractFileItemActionPlugin::KAbstractFileItemActionPlugin(QObject *parent)
    : QObject(parent)
{
}

KAbstractFileItemActionPlugin::~KAbstractFileItemActionPlugin()
{
}

KFileItemActionPluginV2::KFileItemActionPluginV2(QObject *parent)
    : KAbstractFileItemActionPlugin(parent)
{
}

KFileItemActionPluginV2::~KFileItemActionPluginV2() = default;

QList<QAction *> KFileItemActionPluginV2::actions(const KFileItemListProperties &fileItemInfos, QWidget *parentWidget)
{
    // No caller-supplied notion of a "current working directory" reaches us through this
    // legacy entry point; plugins relying on it need the actionsForParent() call from
    // KFileItemActions (see setCurrentWorkingDirectory()) instead.
    return actionsForParent(fileItemInfos, parentWidget, QUrl());
}

#include "moc_kabstractfileitemactionplugin.cpp"
