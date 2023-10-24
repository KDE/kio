/*
    SPDX-FileCopyrightText: 2000 David Faure <faure@kde.org>
    SPDX-License-Identifier: LGPL-2.0-or-later
*/
#include "kpropertiesdialogplugin.h"

#include "kio_widgets_debug.h"
#include "kpropertiesdialog.h"

class KPropertiesDialogPluginPrivate
{
public:
    KPropertiesDialogPluginPrivate()
    {
    }
    ~KPropertiesDialogPluginPrivate()
    {
    }

    bool m_bDirty;
    int fontHeight;
};

KPropertiesDialogPlugin::KPropertiesDialogPlugin(KPropertiesDialog *_props)
    : QObject(_props)
    , d(new KPropertiesDialogPluginPrivate)
{
    properties = _props;
    d->fontHeight = 2 * properties->fontMetrics().height();
    d->m_bDirty = false;
}

KPropertiesDialogPlugin::~KPropertiesDialogPlugin() = default;

void KPropertiesDialogPlugin::setDirty(bool b)
{
    d->m_bDirty = b;
}

bool KPropertiesDialogPlugin::isDirty() const
{
    return d->m_bDirty;
}

void KPropertiesDialogPlugin::applyChanges()
{
    qCWarning(KIO_WIDGETS) << "applyChanges() not implemented in page !";
}

int KPropertiesDialogPlugin::fontHeight() const
{
    return d->fontHeight;
}

#include "moc_kpropertiesdialogplugin.cpp"
