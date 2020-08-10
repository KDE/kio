/*
    This file is part of the KDE project
    SPDX-FileCopyrightText: 1999 Simon Hausmann <hausmann@kde.org>
    SPDX-FileCopyrightText: 2000 Yves Arrouye <yves@realnames.com>
    SPDX-FileCopyrightText: 2002, 2003 Dawit Alemayehu <adawit@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef KURIIKWSFILTER_H
#define KURIIKWSFILTER_H

#include <kurifilter.h>
#include <QVariant>

class KAutoWebSearch : public KUriFilterPlugin
{
    Q_OBJECT
public:
    explicit KAutoWebSearch(QObject *parent = nullptr, const QVariantList &args = QVariantList());
    ~KAutoWebSearch();
    bool filterUri(KUriFilterData &) const override;

public Q_SLOTS:
    void configure();

private:
    void populateProvidersList(QList<KUriFilterSearchProvider *> &searchProviders, const KUriFilterData &, bool allproviders = false) const;
};

#endif
