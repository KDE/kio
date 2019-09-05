/*****************************************************************************
 * Copyright (C) 2006-2010 by Peter Penz <peter.penz@gmx.at>                 *
 * Copyright (C) 2006 by Aaron J. Seigo <aseigo@kde.org>                     *
 * Copyright (C) 2007 by Kevin Ottens <ervin@kde.org>                        *
 * Copyright (C) 2007 by Urs Wolfer <uwolfer @ kde.org>                      *
 *                                                                           *
 * This library is free software; you can redistribute it and/or             *
 * modify it under the terms of the GNU Library General Public               *
 * License as published by the Free Software Foundation; either              *
 * version 2 of the License, or (at your option) any later version.          *
 *                                                                           *
 * This library is distributed in the hope that it will be useful,           *
 * but WITHOUT ANY WARRANTY; without even the implied warranty of            *
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU         *
 * Library General Public License for more details.                          *
 *                                                                           *
 * You should have received a copy of the GNU Library General Public License *
 * along with this library; see the file COPYING.LIB.  If not, write to      *
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,      *
 * Boston, MA 02110-1301, USA.                                               *
 *****************************************************************************/

#include "kurlnavigator.h"

#include "kurlnavigatorplacesselector_p.h"
#include "kurlnavigatorprotocolcombo_p.h"
#include "kurlnavigatordropdownbutton_p.h"
#include "kurlnavigatorbutton_p.h"
#include "kurlnavigatortogglebutton_p.h"
#include "kurlnavigatorpathselectoreventfilter_p.h"
#include "urlutil_p.h"

#include <kfileitem.h>
#include <kfileplacesmodel.h>
#include <klocalizedstring.h>
#include <kprotocolinfo.h>
#include <kurlcombobox.h>
#include <kurlcompletion.h>
#include <kurifilter.h>

#include <QDir>
#include <QLinkedList>
#include <QTimer>
#include <QApplication>
#include <QClipboard>
#include <QDropEvent>
#include <QKeyEvent>
#include <QMenu>
#include <qmimedatabase.h>
#include <QMimeData>
#include <QHBoxLayout>

using namespace KDEPrivate;

struct LocationData {
    QUrl url;
#ifndef KIOFILEWIDGETS_NO_DEPRECATED
    QUrl rootUrl;      // KDE5: remove after the deprecated methods have been removed
    QPoint pos;        // KDE5: remove after the deprecated methods have been removed
#endif
    QByteArray state;
};

class Q_DECL_HIDDEN KUrlNavigator::Private
{
public:
    Private(KUrlNavigator *q, KFilePlacesModel *placesModel);

    void initialize(const QUrl &url);

    /** Applies the edited URL in m_pathBox to the URL navigator */
    void applyUncommittedUrl();

    void slotReturnPressed();
    void slotProtocolChanged(const QString &);
    void openPathSelectorMenu();

    /**
     * Appends the widget at the end of the URL navigator. It is assured
     * that the filler widget remains as last widget to fill the remaining
     * width.
     */
    void appendWidget(QWidget *widget, int stretch = 0);

    /**
     * This slot is connected to the clicked signal of the navigation bar button. It calls switchView().
     * Moreover, if switching from "editable" mode to the breadcrumb view, it calls applyUncommittedUrl().
     */
    void slotToggleEditableButtonPressed();

    /**
     * Switches the navigation bar between the breadcrumb view and the
     * traditional view (see setUrlEditable()).
     */
    void switchView();

    /** Emits the signal urlsDropped(). */
    void dropUrls(const QUrl &destination, QDropEvent *event);

    /**
     * Is invoked when a navigator button has been clicked. Changes the URL
     * of the navigator if the left mouse button has been used. If the middle
     * mouse button has been used, the signal tabRequested() will be emitted.
     */
    void slotNavigatorButtonClicked(const QUrl &url, Qt::MouseButton button, Qt::KeyboardModifiers modifiers);

    void openContextMenu(const QPoint &p);

    void slotPathBoxChanged(const QString &text);

    void updateContent();

    /**
     * Updates all buttons to have one button for each part of the
     * current URL. Existing buttons, which are available by m_navButtons,
     * are reused if possible. If the URL is longer, new buttons will be
     * created, if the URL is shorter, the remaining buttons will be deleted.
     * @param startIndex    Start index of URL part (/), where the buttons
     *                      should be created for each following part.
     */
    void updateButtons(int startIndex);

    /**
     * Updates the visibility state of all buttons describing the URL. If the
     * width of the URL navigator is too small, the buttons representing the upper
     * paths of the URL will be hidden and moved to a drop down menu.
     */
    void updateButtonVisibility();

    /**
     * @return Text for the first button of the URL navigator.
     */
    QString firstButtonText() const;

    /**
     * Returns the URL that should be applied for the button with the index \a index.
     */
    QUrl buttonUrl(int index) const;

    void switchToBreadcrumbMode();

    /**
     * Deletes all URL navigator buttons. m_navButtons is
     * empty after this operation.
     */
    void deleteButtons();

    /**
     * Retrieves the place url for the current url.
     * E. g. for the path "fish://root@192.168.0.2/var/lib" the string
     * "fish://root@192.168.0.2" will be returned, which leads to the
     * navigation indication 'Custom Path > var > lib". For e. g.
     * "settings:///System/" the path "settings://" will be returned.
     */
    QUrl retrievePlaceUrl() const;

    /**
     * Returns true, if the MIME type of the path represents a
     * compressed file like TAR or ZIP.
     */
    bool isCompressedPath(const QUrl &path) const;

    void removeTrailingSlash(QString &url) const;

    /**
     * Returns the current history index, if \a historyIndex is
     * smaller than 0. If \a historyIndex is greater or equal than
     * the number of available history items, the largest possible
     * history index is returned. For the other cases just \a historyIndex
     * is returned.
     */
    int adjustedHistoryIndex(int historyIndex) const;

    bool m_editable : 1;
    bool m_active : 1;
    bool m_showPlacesSelector : 1;
    bool m_showFullPath : 1;
    int m_historyIndex;

    QHBoxLayout *m_layout;

    QList<LocationData> m_history;
    KUrlNavigatorPlacesSelector *m_placesSelector;
    KUrlComboBox *m_pathBox;
    KUrlNavigatorProtocolCombo *m_protocols;
    KUrlNavigatorDropDownButton *m_dropDownButton;
    QList<KUrlNavigatorButton *> m_navButtons;
    KUrlNavigatorButtonBase *m_toggleEditableMode;
    QUrl m_homeUrl;
    QStringList m_customProtocols;
    QWidget *m_dropWidget;
    KUrlNavigator * const q;
};

KUrlNavigator::Private::Private(KUrlNavigator *q, KFilePlacesModel *placesModel) :
    m_editable(false),
    m_active(true),
    m_showPlacesSelector(placesModel != nullptr),
    m_showFullPath(false),
    m_historyIndex(0),
    m_layout(new QHBoxLayout),
    m_placesSelector(nullptr),
    m_pathBox(nullptr),
    m_protocols(nullptr),
    m_dropDownButton(nullptr),
    m_navButtons(),
    m_toggleEditableMode(nullptr),
    m_homeUrl(),
    m_customProtocols(QStringList()),
    m_dropWidget(nullptr),
    q(q)
{
    m_layout->setSpacing(0);
    m_layout->setContentsMargins(0, 0, 0, 0);

    // initialize the places selector
    q->setAutoFillBackground(false);

    if (placesModel != nullptr) {
        m_placesSelector = new KUrlNavigatorPlacesSelector(q, placesModel);
        connect(m_placesSelector, &KUrlNavigatorPlacesSelector::placeActivated,
                q, &KUrlNavigator::setLocationUrl);
        connect(m_placesSelector, &KUrlNavigatorPlacesSelector::tabRequested,
                q, &KUrlNavigator::tabRequested);

        connect(placesModel, SIGNAL(rowsInserted(QModelIndex,int,int)),
                q, SLOT(updateContent()));
        connect(placesModel, SIGNAL(rowsRemoved(QModelIndex,int,int)),
                q, SLOT(updateContent()));
        connect(placesModel, SIGNAL(dataChanged(QModelIndex,QModelIndex)),
                q, SLOT(updateContent()));
    }

    // create protocol combo
    m_protocols = new KUrlNavigatorProtocolCombo(QString(), q);
    connect(m_protocols, SIGNAL(activated(QString)),
            q, SLOT(slotProtocolChanged(QString)));

    // create drop down button for accessing all paths of the URL
    m_dropDownButton = new KUrlNavigatorDropDownButton(q);
    m_dropDownButton->setForegroundRole(QPalette::WindowText);
    m_dropDownButton->installEventFilter(q);
    connect(m_dropDownButton, SIGNAL(clicked()),
            q, SLOT(openPathSelectorMenu()));

    // initialize the path box of the traditional view
    m_pathBox = new KUrlComboBox(KUrlComboBox::Directories, true, q);
    m_pathBox->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLength);
    m_pathBox->installEventFilter(q);

    KUrlCompletion *kurlCompletion = new KUrlCompletion(KUrlCompletion::DirCompletion);
    m_pathBox->setCompletionObject(kurlCompletion);
    m_pathBox->setAutoDeleteCompletionObject(true);

    connect(m_pathBox, SIGNAL(returnPressed()),
            q, SLOT(slotReturnPressed()));
    connect(m_pathBox, &KUrlComboBox::urlActivated,
            q, &KUrlNavigator::setLocationUrl);
    connect(m_pathBox, SIGNAL(editTextChanged(QString)),
            q, SLOT(slotPathBoxChanged(QString)));

    // create toggle button which allows to switch between
    // the breadcrumb and traditional view
    m_toggleEditableMode = new KUrlNavigatorToggleButton(q);
    m_toggleEditableMode->installEventFilter(q);
    m_toggleEditableMode->setMinimumWidth(20);
    connect(m_toggleEditableMode, SIGNAL(clicked()),
            q, SLOT(slotToggleEditableButtonPressed()));

    if (m_placesSelector != nullptr) {
        m_layout->addWidget(m_placesSelector);
    }
    m_layout->addWidget(m_protocols);
    m_layout->addWidget(m_dropDownButton);
    m_layout->addWidget(m_pathBox, 1);
    m_layout->addWidget(m_toggleEditableMode);

    q->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(q, SIGNAL(customContextMenuRequested(QPoint)),
            q, SLOT(openContextMenu(QPoint)));
}

void KUrlNavigator::Private::initialize(const QUrl &url)
{
    LocationData data;
    data.url = url.adjusted(QUrl::NormalizePathSegments);
    m_history.prepend(data);

    q->setLayoutDirection(Qt::LeftToRight);

    const int minHeight = m_pathBox->sizeHint().height();
    q->setMinimumHeight(minHeight);

    q->setLayout(m_layout);
    q->setMinimumWidth(100);

    updateContent();
}

void KUrlNavigator::Private::appendWidget(QWidget *widget, int stretch)
{
    m_layout->insertWidget(m_layout->count() - 1, widget, stretch);
}

void KUrlNavigator::Private::applyUncommittedUrl()
{
    // Parts of the following code have been taken
    // from the class KateFileSelector located in
    // kate/app/katefileselector.hpp of Kate.
    // Copyright (C) 2001 Christoph Cullmann <cullmann@kde.org>
    // Copyright (C) 2001 Joseph Wenninger <jowenn@kde.org>
    // Copyright (C) 2001 Anders Lund <anders.lund@lund.tdcadsl.dk>

    const QUrl typedUrl = q->uncommittedUrl();
    QStringList urls = m_pathBox->urls();
    urls.removeAll(typedUrl.toString());
    urls.prepend(typedUrl.toString());
    m_pathBox->setUrls(urls, KUrlComboBox::RemoveBottom);

    q->setLocationUrl(typedUrl);
    // The URL might have been adjusted by KUrlNavigator::setUrl(), hence
    // synchronize the result in the path box.
    const QUrl currentUrl = q->locationUrl();
    m_pathBox->setUrl(currentUrl);
}

void KUrlNavigator::Private::slotReturnPressed()
{
    applyUncommittedUrl();

    emit q->returnPressed();

    if (QApplication::keyboardModifiers() & Qt::ControlModifier) {
        // Pressing Ctrl+Return automatically switches back to the breadcrumb mode.
        // The switch must be done asynchronously, as we are in the context of the
        // editor.
        QMetaObject::invokeMethod(q, "switchToBreadcrumbMode", Qt::QueuedConnection);
    }
}

void KUrlNavigator::Private::slotProtocolChanged(const QString &protocol)
{
    Q_ASSERT(m_editable);

    QUrl url;
    url.setScheme(protocol);
    if (protocol == QLatin1String("file")) {
        url.setPath(QStringLiteral("/"));
    } else {
        // With no authority set we'll get e.g. "ftp:" instead of "ftp://".
        // We want the latter, so let's set an empty authority.
        url.setAuthority(QString());
    }

    m_pathBox->setEditUrl(url);
}

void KUrlNavigator::Private::openPathSelectorMenu()
{
    if (m_navButtons.count() <= 0) {
        return;
    }

    const QUrl firstVisibleUrl = m_navButtons.first()->url();

    QString spacer;
    QPointer<QMenu> popup = new QMenu(q);

    auto *popupFilter = new KUrlNavigatorPathSelectorEventFilter(popup.data());
    connect(popupFilter, &KUrlNavigatorPathSelectorEventFilter::tabRequested, q, &KUrlNavigator::tabRequested);
    popup->installEventFilter(popupFilter);

    popup->setLayoutDirection(Qt::LeftToRight);

    const QUrl placeUrl = retrievePlaceUrl();
    int idx = placeUrl.path().count(QLatin1Char('/')); // idx points to the first directory
    // after the place path

    const QString path = m_history[m_historyIndex].url.path();
    QString dirName = path.section(QLatin1Char('/'), idx, idx);
    if (dirName.isEmpty()) {
        if (placeUrl.isLocalFile()) {
            dirName = QStringLiteral("/");
        } else {
            dirName = placeUrl.toDisplayString();
        }
    }
    do {
        const QString text = spacer + dirName;

        QAction *action = new QAction(text, popup);
        const QUrl currentUrl = buttonUrl(idx);
        if (currentUrl == firstVisibleUrl) {
            popup->addSeparator();
        }
        action->setData(QVariant(currentUrl.toString()));
        popup->addAction(action);

        ++idx;
        spacer.append(QLatin1String("  "));
        dirName = path.section(QLatin1Char('/'), idx, idx);
    } while (!dirName.isEmpty());

    const QPoint pos = q->mapToGlobal(m_dropDownButton->geometry().bottomRight());
    const QAction *activatedAction = popup->exec(pos);
    if (activatedAction != nullptr) {
        const QUrl url(activatedAction->data().toString());
        q->setLocationUrl(url);
    }

    // Delete the menu, unless it has been deleted in its own nested event loop already.
    if (popup) {
        popup->deleteLater();
    }
}

void KUrlNavigator::Private::slotToggleEditableButtonPressed()
{
    if (m_editable) {
        applyUncommittedUrl();
    }

    switchView();
}

void KUrlNavigator::Private::switchView()
{
    m_toggleEditableMode->setFocus();
    m_editable = !m_editable;
    m_toggleEditableMode->setChecked(m_editable);
    updateContent();
    if (q->isUrlEditable()) {
        m_pathBox->setFocus();
    }

    q->requestActivation();
    emit q->editableStateChanged(m_editable);
}

void KUrlNavigator::Private::dropUrls(const QUrl &destination, QDropEvent *event)
{
    if (event->mimeData()->hasUrls()) {
        m_dropWidget = qobject_cast<QWidget*>(q->sender());
        emit q->urlsDropped(destination, event);
    }
}

void KUrlNavigator::Private::slotNavigatorButtonClicked(const QUrl &url, Qt::MouseButton button, Qt::KeyboardModifiers modifiers)
{
    if (button & Qt::MidButton || (button & Qt::LeftButton && modifiers & Qt::ControlModifier)) {
        emit q->tabRequested(url);
    } else if (button & Qt::LeftButton) {
        q->setLocationUrl(url);
    }
}

void KUrlNavigator::Private::openContextMenu(const QPoint &p)
{
    q->setActive(true);

    QPointer<QMenu> popup = new QMenu(q);

    // provide 'Copy' action, which copies the current URL of
    // the URL navigator into the clipboard
    QAction *copyAction = popup->addAction(QIcon::fromTheme(QStringLiteral("edit-copy")), i18n("Copy"));

    // provide 'Paste' action, which copies the current clipboard text
    // into the URL navigator
    QAction *pasteAction = popup->addAction(QIcon::fromTheme(QStringLiteral("edit-paste")), i18n("Paste"));
    QClipboard *clipboard = QApplication::clipboard();
    pasteAction->setEnabled(!clipboard->text().isEmpty());

    popup->addSeparator();

    //We are checking for receivers because it's odd to have a tab entry even if it's not supported, like in the case of the open dialog
    if (q->receivers(SIGNAL(tabRequested(QUrl))) > 0) {
        for (auto button : qAsConst(m_navButtons)) {
            if (button->geometry().contains(p)) {
                const auto url = button->url();
                QAction* openInTab = popup->addAction(QIcon::fromTheme(QStringLiteral("tab-new")), i18n("Open %1 in tab", button->text()));
                q->connect(openInTab, &QAction::triggered, q, [this, url](){ Q_EMIT q->tabRequested(url); });
                break;
            }
        }
    }

    // provide radiobuttons for toggling between the edit and the navigation mode
    QAction *editAction = popup->addAction(i18n("Edit"));
    editAction->setCheckable(true);

    QAction *navigateAction = popup->addAction(i18n("Navigate"));
    navigateAction->setCheckable(true);

    QActionGroup *modeGroup = new QActionGroup(popup);
    modeGroup->addAction(editAction);
    modeGroup->addAction(navigateAction);
    if (q->isUrlEditable()) {
        editAction->setChecked(true);
    } else {
        navigateAction->setChecked(true);
    }

    popup->addSeparator();

    // allow showing of the full path
    QAction *showFullPathAction = popup->addAction(i18n("Show Full Path"));
    showFullPathAction->setCheckable(true);
    showFullPathAction->setChecked(q->showFullPath());

    QAction *activatedAction = popup->exec(QCursor::pos());
    if (activatedAction == copyAction) {
        QMimeData *mimeData = new QMimeData();
        mimeData->setText(q->locationUrl().toDisplayString(QUrl::PreferLocalFile));
        clipboard->setMimeData(mimeData);
    } else if (activatedAction == pasteAction) {
        q->setLocationUrl(QUrl::fromUserInput(clipboard->text()));
    } else if (activatedAction == editAction) {
        q->setUrlEditable(true);
    } else if (activatedAction == navigateAction) {
        q->setUrlEditable(false);
    } else if (activatedAction == showFullPathAction) {
        q->setShowFullPath(showFullPathAction->isChecked());
    }

    // Delete the menu, unless it has been deleted in its own nested event loop already.
    if (popup) {
        popup->deleteLater();
    }
}

void KUrlNavigator::Private::slotPathBoxChanged(const QString &text)
{
    if (text.isEmpty()) {
        const QString protocol = q->locationUrl().scheme();
        m_protocols->setProtocol(protocol);
        if (m_customProtocols.count() != 1) {
            m_protocols->show();
        }
    } else {
        m_protocols->hide();
    }
}

void KUrlNavigator::Private::updateContent()
{
    const QUrl currentUrl = q->locationUrl();
    if (m_placesSelector != nullptr) {
        m_placesSelector->updateSelection(currentUrl);
    }

    if (m_editable) {
        m_protocols->hide();
        m_dropDownButton->hide();

        deleteButtons();
        m_toggleEditableMode->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Preferred);
        q->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Fixed);

        m_pathBox->show();
        m_pathBox->setUrl(currentUrl);
    } else {
        m_pathBox->hide();

        m_protocols->hide();

        m_toggleEditableMode->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
        q->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

        // Calculate the start index for the directories that should be shown as buttons
        // and create the buttons
        QUrl placeUrl;
        if ((m_placesSelector != nullptr) && !m_showFullPath) {
            placeUrl = m_placesSelector->selectedPlaceUrl();
        }

        if (!placeUrl.isValid()) {
            placeUrl = retrievePlaceUrl();
        }
        QString placePath = placeUrl.path();
        removeTrailingSlash(placePath);

        const int startIndex = placePath.count(QLatin1Char('/'));
        updateButtons(startIndex);
    }
}

void KUrlNavigator::Private::updateButtons(int startIndex)
{
    QUrl currentUrl = q->locationUrl();
    if (!currentUrl.isValid()) { // QFileDialog::setDirectory not called yet
        return;
    }

    const QString path = currentUrl.path();

    bool createButton = false;
    const int oldButtonCount = m_navButtons.count();

    int idx = startIndex;
    bool hasNext = true;
    do {
        createButton = (idx - startIndex >= oldButtonCount);
        const bool isFirstButton = (idx == startIndex);
        const QString dirName = path.section(QLatin1Char('/'), idx, idx);
        hasNext = isFirstButton || !dirName.isEmpty();
        if (hasNext) {
            KUrlNavigatorButton *button = nullptr;
            if (createButton) {
                button = new KUrlNavigatorButton(buttonUrl(idx), q);
                button->installEventFilter(q);
                button->setForegroundRole(QPalette::WindowText);
                connect(button, SIGNAL(urlsDropped(QUrl,QDropEvent*)),
                        q, SLOT(dropUrls(QUrl,QDropEvent*)));
                connect(button, SIGNAL(clicked(QUrl,Qt::MouseButton,Qt::KeyboardModifiers)),
                        q, SLOT(slotNavigatorButtonClicked(QUrl,Qt::MouseButton,Qt::KeyboardModifiers)));
                connect(button, SIGNAL(finishedTextResolving()),
                        q, SLOT(updateButtonVisibility()));
                appendWidget(button);
            } else {
                button = m_navButtons[idx - startIndex];
                button->setUrl(buttonUrl(idx));
            }

            if (isFirstButton) {
                button->setText(firstButtonText());
            }
            button->setActive(q->isActive());

            if (createButton) {
                if (!isFirstButton) {
                    setTabOrder(m_navButtons.last(), button);
                }
                m_navButtons.append(button);
            }

            ++idx;
            button->setActiveSubDirectory(path.section(QLatin1Char('/'), idx, idx));
        }
    } while (hasNext);

    // delete buttons which are not used anymore
    const int newButtonCount = idx - startIndex;
    if (newButtonCount < oldButtonCount) {
        const QList<KUrlNavigatorButton *>::iterator itBegin = m_navButtons.begin() + newButtonCount;
        const QList<KUrlNavigatorButton *>::iterator itEnd = m_navButtons.end();
        QList<KUrlNavigatorButton *>::iterator it = itBegin;
        while (it != itEnd) {
            (*it)->hide();
            (*it)->deleteLater();
            ++it;
        }
        m_navButtons.erase(itBegin, itEnd);
    }

    setTabOrder(m_dropDownButton, m_navButtons.first());
    setTabOrder(m_navButtons.last(), m_toggleEditableMode);

    updateButtonVisibility();
}

void KUrlNavigator::Private::updateButtonVisibility()
{
    if (m_editable) {
        return;
    }

    const int buttonsCount = m_navButtons.count();
    if (buttonsCount == 0) {
        m_dropDownButton->hide();
        return;
    }

    // Subtract all widgets from the available width, that must be shown anyway
    int availableWidth = q->width() - m_toggleEditableMode->minimumWidth();

    if ((m_placesSelector != nullptr) && m_placesSelector->isVisible()) {
        availableWidth -= m_placesSelector->width();
    }

    if ((m_protocols != nullptr) && m_protocols->isVisible()) {
        availableWidth -= m_protocols->width();
    }

    // Check whether buttons must be hidden at all...
    int requiredButtonWidth = 0;
    foreach (const KUrlNavigatorButton *button, m_navButtons) {
        requiredButtonWidth += button->minimumWidth();
    }

    if (requiredButtonWidth > availableWidth) {
        // At least one button must be hidden. This implies that the
        // drop-down button must get visible, which again decreases the
        // available width.
        availableWidth -= m_dropDownButton->width();
    }

    // Hide buttons...
    QList<KUrlNavigatorButton *>::const_iterator it = m_navButtons.constEnd();
    const QList<KUrlNavigatorButton *>::const_iterator itBegin = m_navButtons.constBegin();
    bool isLastButton = true;
    bool hasHiddenButtons = false;

    QLinkedList<KUrlNavigatorButton *> buttonsToShow;
    while (it != itBegin) {
        --it;
        KUrlNavigatorButton *button = (*it);
        availableWidth -= button->minimumWidth();
        if ((availableWidth <= 0) && !isLastButton) {
            button->hide();
            hasHiddenButtons = true;
        } else {
            // Don't show the button immediately, as setActive()
            // might change the size and a relayout gets triggered
            // after showing the button. So the showing of all buttons
            // is postponed until all buttons have the correct
            // activation state.
            buttonsToShow.append(button);
        }
        isLastButton = false;
    }

    // All buttons have the correct activation state and
    // can be shown now
    foreach (KUrlNavigatorButton *button, buttonsToShow) {
        button->show();
    }

    if (hasHiddenButtons) {
        m_dropDownButton->show();
    } else {
        // Check whether going upwards is possible. If this is the case, show the drop-down button.
        QUrl url(m_navButtons.front()->url());
        const bool visible = !url.matches(KIO::upUrl(url), QUrl::StripTrailingSlash) && (url.scheme() != QLatin1String("nepomuksearch"));
        m_dropDownButton->setVisible(visible);
    }
}

QString KUrlNavigator::Private::firstButtonText() const
{
    QString text;

    // The first URL navigator button should get the name of the
    // place instead of the directory name
    if ((m_placesSelector != nullptr) && !m_showFullPath) {
        text = m_placesSelector->selectedPlaceText();
    }

    if (text.isEmpty()) {
        const QUrl currentUrl = q->locationUrl();
        if (currentUrl.isLocalFile()) {
#ifdef Q_OS_WIN
            text = currentUrl.path().length() > 1 ? currentUrl.path().left(2) : QDir::rootPath();
#else
            text = m_showFullPath ? QStringLiteral("/") : i18n("Custom Path");
#endif
        } else {
            text = currentUrl.scheme() + QLatin1Char(':');
            if (!currentUrl.host().isEmpty()) {
                text += QLatin1Char(' ') + currentUrl.host();
            }
        }
    }

    return text;
}

QUrl KUrlNavigator::Private::buttonUrl(int index) const
{
    if (index < 0) {
        index = 0;
    }

    // Keep scheme, hostname etc. as this is needed for e. g. browsing
    // FTP directories
    QUrl url = q->locationUrl();
    QString path = url.path();

    if (!path.isEmpty()) {
        if (index == 0) {
            // prevent the last "/" from being stripped
            // or we end up with an empty path
#ifdef Q_OS_WIN
            path = path.length() > 1 ? path.left(2) : QDir::rootPath();
#else
            path = QStringLiteral("/");
#endif
        } else {
            path = path.section(QLatin1Char('/'), 0, index);
        }
    }

    url.setPath(path);
    return url;
}

void KUrlNavigator::Private::switchToBreadcrumbMode()
{
    q->setUrlEditable(false);
}

void KUrlNavigator::Private::deleteButtons()
{
    foreach (KUrlNavigatorButton *button, m_navButtons) {
        button->hide();
        button->deleteLater();
    }
    m_navButtons.clear();
}

QUrl KUrlNavigator::Private::retrievePlaceUrl() const
{
    QUrl currentUrl = q->locationUrl();
    currentUrl.setPath(QString());
    return currentUrl;
}

bool KUrlNavigator::Private::isCompressedPath(const QUrl &url) const
{
    QMimeDatabase db;
    const QMimeType mime = db.mimeTypeForUrl(QUrl(url.toString(QUrl::StripTrailingSlash)));
    // Note: this list of MIME types depends on the protocols implemented by kio_archive and krarc
    return  mime.inherits(QStringLiteral("application/x-compressed-tar")) ||
            mime.inherits(QStringLiteral("application/x-bzip-compressed-tar")) ||
            mime.inherits(QStringLiteral("application/x-lzma-compressed-tar")) ||
            mime.inherits(QStringLiteral("application/x-xz-compressed-tar")) ||
            mime.inherits(QStringLiteral("application/x-tar")) ||
            mime.inherits(QStringLiteral("application/x-tarz")) ||
            mime.inherits(QStringLiteral("application/x-tzo")) || // (not sure KTar supports those?)
            mime.inherits(QStringLiteral("application/zip")) ||
            mime.inherits(QStringLiteral("application/x-archive")) ||
            mime.inherits(QStringLiteral("application/vnd.rar")) || // the following depends on krarc
            mime.inherits(QStringLiteral("application/x-7z-compressed")) ||
            mime.inherits(QStringLiteral("application/x-ace")) ||
            mime.inherits(QStringLiteral("application/x-arj")) ||
            mime.inherits(QStringLiteral("application/x-cpio")) ||
            mime.inherits(QStringLiteral("application/x-lha"));
}

void KUrlNavigator::Private::removeTrailingSlash(QString &url) const
{
    const int length = url.length();
    if ((length > 0) && (url.at(length - 1) == QLatin1Char('/'))) {
        url.remove(length - 1, 1);
    }
}

int KUrlNavigator::Private::adjustedHistoryIndex(int historyIndex) const
{
    if (historyIndex < 0) {
        historyIndex = m_historyIndex;
    } else if (historyIndex >= m_history.size()) {
        historyIndex = m_history.size() - 1;
        Q_ASSERT(historyIndex >= 0); // m_history.size() must always be > 0
    }
    return historyIndex;
}

// ------------------------------------------------------------------------------------------------

KUrlNavigator::KUrlNavigator(QWidget *parent) :
    QWidget(parent),
    d(new Private(this, nullptr))
{
    d->initialize(QUrl());
}

KUrlNavigator::KUrlNavigator(KFilePlacesModel *placesModel,
                             const QUrl &url,
                             QWidget *parent) :
    QWidget(parent),
    d(new Private(this, placesModel))
{
    d->initialize(url);
}

KUrlNavigator::~KUrlNavigator()
{
    delete d;
}

QUrl KUrlNavigator::locationUrl(int historyIndex) const
{
    historyIndex = d->adjustedHistoryIndex(historyIndex);
    return d->m_history[historyIndex].url;
}

void KUrlNavigator::saveLocationState(const QByteArray &state)
{
    d->m_history[d->m_historyIndex].state = state;
}

QByteArray KUrlNavigator::locationState(int historyIndex) const
{
    historyIndex = d->adjustedHistoryIndex(historyIndex);
    return d->m_history[historyIndex].state;
}

bool KUrlNavigator::goBack()
{
    const int count = d->m_history.count();
    if (d->m_historyIndex < count - 1) {
        const QUrl newUrl = locationUrl(d->m_historyIndex + 1);
        emit urlAboutToBeChanged(newUrl);

        ++d->m_historyIndex;
        d->updateContent();

        emit historyChanged();
        emit urlChanged(locationUrl());
        return true;
    }

    return false;
}

bool KUrlNavigator::goForward()
{
    if (d->m_historyIndex > 0) {
        const QUrl newUrl = locationUrl(d->m_historyIndex - 1);
        emit urlAboutToBeChanged(newUrl);

        --d->m_historyIndex;
        d->updateContent();

        emit historyChanged();
        emit urlChanged(locationUrl());
        return true;
    }

    return false;
}

bool KUrlNavigator::goUp()
{
    const QUrl currentUrl = locationUrl();
    const QUrl upUrl = KIO::upUrl(currentUrl);
    if (upUrl != currentUrl) { // TODO use url.matches(KIO::upUrl(url), QUrl::StripTrailingSlash)
        setLocationUrl(upUrl);
        return true;
    }

    return false;
}

void KUrlNavigator::goHome()
{
    if (d->m_homeUrl.isEmpty() || !d->m_homeUrl.isValid()) {
        setLocationUrl(QUrl::fromLocalFile(QDir::homePath()));
    } else {
        setLocationUrl(d->m_homeUrl);
    }
}

void KUrlNavigator::setHomeUrl(const QUrl &url)
{
    d->m_homeUrl = url;
}

QUrl KUrlNavigator::homeUrl() const
{
    return d->m_homeUrl;
}

void KUrlNavigator::setUrlEditable(bool editable)
{
    if (d->m_editable != editable) {
        d->switchView();
    }
}

bool KUrlNavigator::isUrlEditable() const
{
    return d->m_editable;
}

void KUrlNavigator::setShowFullPath(bool show)
{
    if (d->m_showFullPath != show) {
        d->m_showFullPath = show;
        d->updateContent();
    }
}

bool KUrlNavigator::showFullPath() const
{
    return d->m_showFullPath;
}

void KUrlNavigator::setActive(bool active)
{
    if (active != d->m_active) {
        d->m_active = active;

        d->m_dropDownButton->setActive(active);
        foreach (KUrlNavigatorButton *button, d->m_navButtons) {
            button->setActive(active);
        }

        update();
        if (active) {
            emit activated();
        }
    }
}

bool KUrlNavigator::isActive() const
{
    return d->m_active;
}

void KUrlNavigator::setPlacesSelectorVisible(bool visible)
{
    if (visible == d->m_showPlacesSelector) {
        return;
    }

    if (visible  && (d->m_placesSelector == nullptr)) {
        // the places selector cannot get visible as no
        // places model is available
        return;
    }

    d->m_showPlacesSelector = visible;
    d->m_placesSelector->setVisible(visible);
}

bool KUrlNavigator::isPlacesSelectorVisible() const
{
    return d->m_showPlacesSelector;
}

QUrl KUrlNavigator::uncommittedUrl() const
{
    KUriFilterData filteredData(d->m_pathBox->currentText().trimmed());
    filteredData.setCheckForExecutables(false);
    if (KUriFilter::self()->filterUri(filteredData, QStringList{ QStringLiteral("kshorturifilter"), QStringLiteral("kurisearchfilter")})) {
        return filteredData.uri();
    } else {
        return QUrl::fromUserInput(filteredData.typedString());
    }
}

void KUrlNavigator::setLocationUrl(const QUrl &newUrl)
{
    if (newUrl == locationUrl()) {
        return;
    }

    QUrl url = newUrl.adjusted(QUrl::NormalizePathSegments);

    // This will be used below; we define it here because in the lower part of the
    // code locationUrl() and url become the same URLs
    QUrl firstChildUrl = KIO::UrlUtil::firstChildUrl(locationUrl(), url);

    if ((url.scheme() == QLatin1String("tar")) || (url.scheme() == QLatin1String("zip")) || (url.scheme() == QLatin1String("krarc"))) {
        // The URL represents a tar- or zip-file, or an archive file supported by krarc.
        // Check whether the URL is really part of the archive file, otherwise
        // replace it by the local path again.
        bool insideCompressedPath = d->isCompressedPath(url);
        if (!insideCompressedPath) {
            QUrl prevUrl = url;
            QUrl parentUrl = KIO::upUrl(url);
            while (parentUrl != prevUrl) {
                if (d->isCompressedPath(parentUrl)) {
                    insideCompressedPath = true;
                    break;
                }
                prevUrl = parentUrl;
                parentUrl = KIO::upUrl(parentUrl);
            }
        }
        if (!insideCompressedPath) {
            // drop the tar: or zip: or krarc: protocol since we are not
            // inside the compressed path
            url.setScheme(QStringLiteral("file"));
            firstChildUrl.setScheme(QStringLiteral("file"));
        }
    }

    // Check whether current history element has the same URL.
    // If this is the case, just ignore setting the URL.
    const LocationData &data = d->m_history[d->m_historyIndex];
    const bool isUrlEqual = url.matches(locationUrl(), QUrl::StripTrailingSlash) ||
                            (!url.isValid() && url.matches(data.url, QUrl::StripTrailingSlash));
    if (isUrlEqual) {
        return;
    }

    emit urlAboutToBeChanged(url);

    if (d->m_historyIndex > 0) {
        // If an URL is set when the history index is not at the end (= 0),
        // then clear all previous history elements so that a new history
        // tree is started from the current position.
        QList<LocationData>::iterator begin = d->m_history.begin();
        QList<LocationData>::iterator end = begin + d->m_historyIndex;
        d->m_history.erase(begin, end);
        d->m_historyIndex = 0;
    }

    Q_ASSERT(d->m_historyIndex == 0);
    LocationData newData;
    newData.url = url;
    d->m_history.insert(0, newData);

    // Prevent an endless growing of the history: remembering
    // the last 100 Urls should be enough...
    const int historyMax = 100;
    if (d->m_history.size() > historyMax) {
        QList<LocationData>::iterator begin = d->m_history.begin() + historyMax;
        QList<LocationData>::iterator end = d->m_history.end();
        d->m_history.erase(begin, end);
    }

    emit historyChanged();
    emit urlChanged(url);

    if (firstChildUrl.isValid()) {
        emit urlSelectionRequested(firstChildUrl);
    }

    d->updateContent();

    requestActivation();
}

void KUrlNavigator::requestActivation()
{
    setActive(true);
}

void KUrlNavigator::setFocus()
{
    if (isUrlEditable()) {
        d->m_pathBox->setFocus();
    } else {
        QWidget::setFocus();
    }
}

#ifndef KIOFILEWIDGETS_NO_DEPRECATED
void KUrlNavigator::setUrl(const QUrl &url)
{
    // deprecated
    setLocationUrl(url);
}
#endif

#ifndef KIOFILEWIDGETS_NO_DEPRECATED
void KUrlNavigator::saveRootUrl(const QUrl &url)
{
    // deprecated
    d->m_history[d->m_historyIndex].rootUrl = url;
}
#endif

#ifndef KIOFILEWIDGETS_NO_DEPRECATED
void KUrlNavigator::savePosition(int x, int y)
{
    // deprecated
    d->m_history[d->m_historyIndex].pos = QPoint(x, y);
}
#endif

void KUrlNavigator::keyPressEvent(QKeyEvent *event)
{
    if (isUrlEditable() && (event->key() == Qt::Key_Escape)) {
        setUrlEditable(false);
    } else {
        QWidget::keyPressEvent(event);
    }
}

void KUrlNavigator::keyReleaseEvent(QKeyEvent *event)
{
    QWidget::keyReleaseEvent(event);
}

void KUrlNavigator::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::MiddleButton) {
        requestActivation();
    }
    QWidget::mousePressEvent(event);
}

void KUrlNavigator::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::MidButton) {
        const QRect bounds = d->m_toggleEditableMode->geometry();
        if (bounds.contains(event->pos())) {
            // The middle mouse button has been clicked above the
            // toggle-editable-mode-button. Paste the clipboard content
            // as location URL.
            QClipboard *clipboard = QApplication::clipboard();
            const QMimeData *mimeData = clipboard->mimeData();
            if (mimeData->hasText()) {
                const QString text = mimeData->text();
                setLocationUrl(QUrl::fromUserInput(text));
            }
        }
    }
    QWidget::mouseReleaseEvent(event);
}

void KUrlNavigator::resizeEvent(QResizeEvent *event)
{
    QTimer::singleShot(0, this, SLOT(updateButtonVisibility()));
    QWidget::resizeEvent(event);
}

void KUrlNavigator::wheelEvent(QWheelEvent *event)
{
    setActive(true);
    QWidget::wheelEvent(event);
}

bool KUrlNavigator::eventFilter(QObject *watched, QEvent *event)
{
    switch (event->type()) {
    case QEvent::FocusIn:
        if (watched == d->m_pathBox) {
            requestActivation();
            setFocus();
        }
        foreach (KUrlNavigatorButton *button, d->m_navButtons) {
            button->setShowMnemonic(true);
        }
        break;

    case QEvent::FocusOut:
        foreach (KUrlNavigatorButton *button, d->m_navButtons) {
            button->setShowMnemonic(false);
        }
        break;

    default:
        break;
    }

    return QWidget::eventFilter(watched, event);
}

int KUrlNavigator::historySize() const
{
    return d->m_history.count();
}

int KUrlNavigator::historyIndex() const
{
    return d->m_historyIndex;
}

KUrlComboBox *KUrlNavigator::editor() const
{
    return d->m_pathBox;
}

void KUrlNavigator::setCustomProtocols(const QStringList &protocols)
{
    d->m_customProtocols = protocols;
    d->m_protocols->setCustomProtocols(d->m_customProtocols);
}

QStringList KUrlNavigator::customProtocols() const
{
    return d->m_customProtocols;
}

QWidget *KUrlNavigator::dropWidget() const
{
    return d->m_dropWidget;
}

#ifndef KIOFILEWIDGETS_NO_DEPRECATED
const QUrl &KUrlNavigator::url() const
{
    // deprecated

    // Workaround required because of flawed interface ('const QUrl&' is returned
    // instead of 'QUrl'): remember the URL to prevent a dangling pointer
    static QUrl url;
    url = locationUrl();
    return url;
}
#endif

#ifndef KIOFILEWIDGETS_NO_DEPRECATED
QUrl KUrlNavigator::url(int index) const
{
    // deprecated
    return d->buttonUrl(index);
}
#endif

#ifndef KIOFILEWIDGETS_NO_DEPRECATED
QUrl KUrlNavigator::historyUrl(int historyIndex) const
{
    // deprecated
    return locationUrl(historyIndex);
}
#endif

#ifndef KIOFILEWIDGETS_NO_DEPRECATED
const QUrl &KUrlNavigator::savedRootUrl() const
{
    // deprecated

    // Workaround required because of flawed interface ('const QUrl&' is returned
    // instead of 'QUrl'): remember the root URL to prevent a dangling pointer
    static QUrl rootUrl;
    rootUrl = d->m_history[d->m_historyIndex].rootUrl;
    return rootUrl;
}
#endif

#ifndef KIOFILEWIDGETS_NO_DEPRECATED
QPoint KUrlNavigator::savedPosition() const
{
    // deprecated
    return d->m_history[d->m_historyIndex].pos;
}
#endif

#ifndef KIOFILEWIDGETS_NO_DEPRECATED
void KUrlNavigator::setHomeUrl(const QString &homeUrl)
{
    // deprecated
    setLocationUrl(QUrl::fromUserInput(homeUrl));
}
#endif

#include "moc_kurlnavigator.cpp"
