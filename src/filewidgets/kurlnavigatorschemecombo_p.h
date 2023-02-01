/*
    SPDX-FileCopyrightText: 2006 Aaron J. Seigo <aseigo@kde.org>
    SPDX-FileCopyrightText: 2009 Peter Penz <peter.penz@kde.org>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#ifndef KURLNAVIGATORSCHEMECOMBO_P_H
#define KURLNAVIGATORSCHEMECOMBO_P_H

#include "kurlnavigatorbuttonbase_p.h"

#include <QHash>

class QMenu;

namespace KDEPrivate
{
/**
 * @brief A combobox listing available schemes.
 *
 * The widget is used by the URL navigator for offering the available
 * schemes for non-local URLs.
 *
 * @see KUrlNavigator
 */
class KUrlNavigatorSchemeCombo : public KUrlNavigatorButtonBase
{
    Q_OBJECT

public:
    explicit KUrlNavigatorSchemeCombo(const QString &scheme, KUrlNavigator *parent = nullptr);

    QString currentScheme() const;

    void setSupportedSchemes(const QStringList &schemes);

    QSize sizeHint() const override;

public Q_SLOTS:
    void setScheme(const QString &scheme);

Q_SIGNALS:
    void activated(const QString &scheme);

protected:
    void showEvent(QShowEvent *event) override;
    void paintEvent(QPaintEvent *event) override;

private Q_SLOTS:
    void setSchemeFromMenu(QAction *action);

private:
    void updateMenu();
    void initializeCategories();

    enum SchemeCategory {
        CoreCategory,
        PlacesCategory,
        DevicesCategory,
        SubversionCategory,
        OtherCategory,
        CategoryCount, // mandatory last entry
    };

    QMenu *m_menu;
    QStringList m_schemes;
    QHash<QString, SchemeCategory> m_categories;
};

} // namespace KDEPrivate

#endif
