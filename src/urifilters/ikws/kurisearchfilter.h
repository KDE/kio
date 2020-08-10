/*
    This file is part of the KDE project
    SPDX-FileCopyrightText: 1999 Simon Hausmann <hausmann@kde.org>
    SPDX-FileCopyrightText: 2000 Yves Arrouye <yves@realnames.com>
    SPDX-FileCopyrightText: 2002, 2003 Dawit Alemayehu <adawit@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef KURISEARCHFILTER_H
#define KURISEARCHFILTER_H

#include <kurifilter.h>

class KUriSearchFilter : public KUriFilterPlugin
{
    Q_OBJECT
public:
    explicit KUriSearchFilter(QObject *parent = nullptr, const QVariantList &args = QVariantList());
    ~KUriSearchFilter();

    bool filterUri(KUriFilterData &) const override;
    KCModule *configModule(QWidget *parent = nullptr, const char *name = nullptr) const override;
    QString configName() const override;

public Q_SLOTS:
    void configure(); // maybe move to KUriFilterPlugin?
};

#endif
