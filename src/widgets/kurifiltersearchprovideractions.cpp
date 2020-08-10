/*
    SPDX-FileCopyrightText: 2015 Montel Laurent <montel@kde.org>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#include "kurifiltersearchprovideractions.h"
#include <QDesktopServices>
#include <KToolInvocation>
#include <kurifilter.h>
#include <KStringHandler>
#include <KLocalizedString>
#include <QMenu>
#include <QIcon>

using namespace KIO;

class KIO::WebShortcutsMenuManagerPrivate
{
public:
    WebShortcutsMenuManagerPrivate()
    {

    }

    QString mSelectedText;
};

KUriFilterSearchProviderActions::KUriFilterSearchProviderActions(QObject *parent)
    : QObject(parent),
      d(new KIO::WebShortcutsMenuManagerPrivate)
{

}

KUriFilterSearchProviderActions::~KUriFilterSearchProviderActions()
{
    delete d;
}

QString KUriFilterSearchProviderActions::selectedText() const
{
    return d->mSelectedText;
}

void KUriFilterSearchProviderActions::setSelectedText(const QString &selectedText)
{
    d->mSelectedText = selectedText;
}

void KUriFilterSearchProviderActions::slotConfigureWebShortcuts()
{
    KToolInvocation::kdeinitExec(QStringLiteral("kcmshell5"), QStringList() << QStringLiteral("webshortcuts"));
}

void KUriFilterSearchProviderActions::addWebShortcutsToMenu(QMenu *menu)
{
    if (d->mSelectedText.isEmpty()) {
        return;
    }

    const QString searchText = d->mSelectedText.simplified();

    if (searchText.isEmpty()) {
        return;
    }

    KUriFilterData filterData(searchText);

    filterData.setSearchFilteringOptions(KUriFilterData::RetrievePreferredSearchProvidersOnly);

    if (KUriFilter::self()->filterSearchUri(filterData, KUriFilter::NormalTextFilter)) {
        const QStringList searchProviders = filterData.preferredSearchProviders();

        if (!searchProviders.isEmpty()) {
            QMenu *webShortcutsMenu = new QMenu(menu);
            webShortcutsMenu->setIcon(QIcon::fromTheme(QStringLiteral("preferences-web-browser-shortcuts")));

            const QString squeezedText = KStringHandler::rsqueeze(searchText, 21);
            webShortcutsMenu->setTitle(i18n("Search for '%1' with", squeezedText));

            QActionGroup *actionGroup = new QActionGroup(this);
            connect(actionGroup, &QActionGroup::triggered, this, &KUriFilterSearchProviderActions::slotHandleWebShortcutAction);
            for (const QString &searchProvider : searchProviders) {
                QAction *action = new QAction(i18nc("@action:inmenu Search for <text> with", "%1", searchProvider), webShortcutsMenu);
                action->setIcon(QIcon::fromTheme(filterData.iconNameForPreferredSearchProvider(searchProvider)));
                action->setData(filterData.queryForPreferredSearchProvider(searchProvider));
                webShortcutsMenu->addAction(action);
                actionGroup->addAction(action);
            }

            if (!QStandardPaths::findExecutable(QStringLiteral("kcmshell5")).isEmpty()) {
                webShortcutsMenu->addSeparator();
                QAction *action = new QAction(i18n("Configure Web Shortcuts..."), webShortcutsMenu);
                action->setIcon(QIcon::fromTheme(QStringLiteral("configure")));
                connect(action, &QAction::triggered, this, &KUriFilterSearchProviderActions::slotConfigureWebShortcuts);
                webShortcutsMenu->addAction(action);
            }

            menu->addMenu(webShortcutsMenu);
        }
    }
}

void KUriFilterSearchProviderActions::slotHandleWebShortcutAction(QAction *action)
{
    KUriFilterData filterData(action->data().toString());
    if (KUriFilter::self()->filterSearchUri(filterData, KUriFilter::WebShortcutFilter)) {
        QDesktopServices::openUrl(filterData.uri());
    }
}
