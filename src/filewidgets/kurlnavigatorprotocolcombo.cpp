/*
    SPDX-FileCopyrightText: 2006 Aaron J. Seigo <aseigo@kde.org>
    SPDX-FileCopyrightText: 2009 Peter Penz <peter.penz@kde.org>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#include "kurlnavigatorprotocolcombo_p.h"

#include <QAction>
#include <QMenu>
#include <QPainter>
#include <QPaintEvent>
#include <QStyleOption>

#include <KLocalizedString>
#include <kprotocolinfo.h>
#include <kprotocolmanager.h>
#include <kurlnavigator.h>

namespace
{
const int ArrowSize = 10;
}

namespace KDEPrivate
{

KUrlNavigatorProtocolCombo::KUrlNavigatorProtocolCombo(const QString &protocol, KUrlNavigator *parent) :
    KUrlNavigatorButtonBase(parent),
    m_menu(nullptr),
    m_protocols(),
    m_categories()
{
    m_menu = new QMenu(this);
    connect(m_menu, &QMenu::triggered, this, &KUrlNavigatorProtocolCombo::setProtocolFromMenu);
    setText(protocol);
    setMenu(m_menu);
}

void KUrlNavigatorProtocolCombo::setCustomProtocols(const QStringList &protocols)
{
    m_protocols = protocols;
    m_menu->clear();

    for (const QString &protocol : protocols) {
        QAction *action = m_menu->addAction(protocol);
        action->setData(protocol);
    }
}

QSize KUrlNavigatorProtocolCombo::sizeHint() const
{
    const QSize size = KUrlNavigatorButtonBase::sizeHint();

    int width = fontMetrics().boundingRect(KLocalizedString::removeAcceleratorMarker(text())).width();
    width += (3 * BorderWidth) + ArrowSize;

    return QSize(width, size.height());
}

void KUrlNavigatorProtocolCombo::setProtocol(const QString &protocol)
{
    setText(protocol);
}

QString KUrlNavigatorProtocolCombo::currentProtocol() const
{
    return text();
}

void KUrlNavigatorProtocolCombo::showEvent(QShowEvent *event)
{
    KUrlNavigatorButtonBase::showEvent(event);
    if (!event->spontaneous() && m_protocols.isEmpty()) {
        m_protocols = KProtocolInfo::protocols();
        std::sort(m_protocols.begin(), m_protocols.end());

        QStringList::iterator it = m_protocols.begin();
        while (it != m_protocols.end()) {
            QUrl url;
            url.setScheme(*it);
            if (!KProtocolManager::supportsListing(url)) {
                it = m_protocols.erase(it);
            } else {
                ++it;
            }
        }

        updateMenu();
    }
}

void KUrlNavigatorProtocolCombo::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);

    QPainter painter(this);
    const int buttonWidth  = width();
    const int buttonHeight = height();

    drawHoverBackground(&painter);

    const QColor fgColor = foregroundColor();
    painter.setPen(fgColor);

    // draw arrow
    const int arrowX = buttonWidth - ArrowSize - BorderWidth;
    const int arrowY = (buttonHeight - ArrowSize) / 2;

    QStyleOption option;
    option.rect = QRect(arrowX, arrowY, ArrowSize, ArrowSize);
    option.palette = palette();
    option.palette.setColor(QPalette::Text, fgColor);
    option.palette.setColor(QPalette::WindowText, fgColor);
    option.palette.setColor(QPalette::ButtonText, fgColor);
    style()->drawPrimitive(QStyle::PE_IndicatorArrowDown, &option, &painter, this);

    // draw text
    const int textWidth = arrowX - (2 * BorderWidth);
    int alignment = Qt::AlignCenter | Qt::TextShowMnemonic;
    if (!style()->styleHint(QStyle::SH_UnderlineShortcut, &option, this)) {
        alignment |= Qt::TextHideMnemonic;
    }
    style()->drawItemText(&painter, QRect(BorderWidth, 0, textWidth, buttonHeight),
                          alignment, option.palette, isEnabled(), text());
}

void KUrlNavigatorProtocolCombo::setProtocolFromMenu(QAction *action)
{
    const QString protocol = action->data().toString();
    setText(protocol);
    Q_EMIT activated(protocol);
}

void KUrlNavigatorProtocolCombo::updateMenu()
{
    initializeCategories();
    std::sort(m_protocols.begin(), m_protocols.end());

    // move all protocols into the corresponding category of 'items'
    QList<QString> items[CategoryCount];
    for (const QString &protocol : qAsConst(m_protocols)) {
        if (m_categories.contains(protocol)) {
            const ProtocolCategory category = m_categories.value(protocol);
            items[category].append(protocol);
        } else {
            items[OtherCategory].append(protocol);
        }
    }

    // Create the menu that includes all entries from 'items'. The categories
    // CoreCategory and PlacesCategory are placed at the top level, the remaining
    // categories are placed in sub menus.
    QMenu *menu = m_menu;
    for (int category = 0; category < CategoryCount; ++category) {
        if (!items[category].isEmpty()) {
            switch (category) {
            case DevicesCategory:
                menu = m_menu->addMenu(i18nc("@item:inmenu", "Devices"));
                break;

            case SubversionCategory:
                menu = m_menu->addMenu(i18nc("@item:inmenu", "Subversion"));
                break;

            case OtherCategory:
                menu = m_menu->addMenu(i18nc("@item:inmenu", "Other"));
                break;

            case CoreCategory:
            case PlacesCategory:
            default:
                break;
            }

            for (const QString &protocol : qAsConst(items[category])) {
                QAction *action = menu->addAction(protocol);
                action->setData(protocol);
            }

            if (menu == m_menu) {
                menu->addSeparator();
            }
        }
    }
}

void KUrlNavigatorProtocolCombo::initializeCategories()
{
    if (m_categories.isEmpty()) {
        m_categories.insert(QStringLiteral("file"), CoreCategory);
        m_categories.insert(QStringLiteral("ftp"), CoreCategory);
        m_categories.insert(QStringLiteral("fish"), CoreCategory);
        m_categories.insert(QStringLiteral("nfs"), CoreCategory);
        m_categories.insert(QStringLiteral("sftp"), CoreCategory);
        m_categories.insert(QStringLiteral("smb"), CoreCategory);
        m_categories.insert(QStringLiteral("webdav"), CoreCategory);

        m_categories.insert(QStringLiteral("desktop"), PlacesCategory);
        m_categories.insert(QStringLiteral("fonts"), PlacesCategory);
        m_categories.insert(QStringLiteral("programs"), PlacesCategory);
        m_categories.insert(QStringLiteral("settings"), PlacesCategory);
        m_categories.insert(QStringLiteral("trash"), PlacesCategory);

        m_categories.insert(QStringLiteral("floppy"), DevicesCategory);
        m_categories.insert(QStringLiteral("camera"), DevicesCategory);
        m_categories.insert(QStringLiteral("remote"), DevicesCategory);

        m_categories.insert(QStringLiteral("svn"), SubversionCategory);
        m_categories.insert(QStringLiteral("svn+file"), SubversionCategory);
        m_categories.insert(QStringLiteral("svn+http"), SubversionCategory);
        m_categories.insert(QStringLiteral("svn+https"), SubversionCategory);
        m_categories.insert(QStringLiteral("svn+ssh"), SubversionCategory);
    }
}

} // namespace KDEPrivate

#include "moc_kurlnavigatorprotocolcombo_p.cpp"
