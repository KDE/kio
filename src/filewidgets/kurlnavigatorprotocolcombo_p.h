/*
    SPDX-FileCopyrightText: 2006 Aaron J. Seigo <aseigo@kde.org>
    SPDX-FileCopyrightText: 2009 Peter Penz <peter.penz@kde.org>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#ifndef KURLNAVIGATORPROTOCOLCOMBO_P_H
#define KURLNAVIGATORPROTOCOLCOMBO_P_H

#include "kurlnavigatorbuttonbase_p.h"

#include <QHash>

class QMenu;

namespace KDEPrivate
{

/**
 * @brief A combobox listing available protocols.
 *
 * The widget is used by the URL navigator for offering the available
 * protocols for non-local URLs.
 *
 * @see KUrlNavigator
 */
class KUrlNavigatorProtocolCombo : public KUrlNavigatorButtonBase
{
    Q_OBJECT

public:
    explicit KUrlNavigatorProtocolCombo(const QString &protocol, KUrlNavigator *parent = nullptr);

    QString currentProtocol() const;

    void setCustomProtocols(const QStringList &protocols);

    QSize sizeHint() const override;

public Q_SLOTS:
    void setProtocol(const QString &protocol);

Q_SIGNALS:
    void activated(const QString &protocol);

protected:
    void showEvent(QShowEvent *event) override;
    void paintEvent(QPaintEvent *event) override;

private Q_SLOTS:
    void setProtocolFromMenu(QAction *action);

private:
    void updateMenu();
    void initializeCategories();

    enum ProtocolCategory {
        CoreCategory,
        PlacesCategory,
        DevicesCategory,
        SubversionCategory,
        OtherCategory,
        CategoryCount, // mandatory last entry
    };

    QMenu *m_menu;
    QStringList m_protocols;
    QHash<QString, ProtocolCategory> m_categories;
};

} // namespace KDEPrivate

#endif
