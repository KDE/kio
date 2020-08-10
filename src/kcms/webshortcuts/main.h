/*
    SPDX-FileCopyrightText: 2000 Yves Arrouye <yves@realnames.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef MAIN_H
#define MAIN_H

#include <KCModule>

class KUriFilter;

class KURIFilterModule : public KCModule
{
    Q_OBJECT

public:
    KURIFilterModule(QWidget *parent, const QVariantList &args);
    ~KURIFilterModule();

    void load() override; // TODO KF6: remove it, not needed
    void save() override;
    void defaults() override;

private:
    KUriFilter *filter;

    QWidget *m_widget;
    QList<KCModule *> modules;
};

#endif
