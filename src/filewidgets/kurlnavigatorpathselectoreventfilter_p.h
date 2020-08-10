/*
    SPDX-FileCopyrightText: 2018 Kai Uwe Broulik <kde@privat.broulik.de>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/

#pragma once

#include <QObject>

namespace KDEPrivate
{

class KUrlNavigatorPathSelectorEventFilter : public QObject
{
    Q_OBJECT

public:
    explicit KUrlNavigatorPathSelectorEventFilter(QObject *parent);
    ~KUrlNavigatorPathSelectorEventFilter() override;

Q_SIGNALS:
    void tabRequested(const QUrl &url);

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

};

} // namespace KDEPrivate
