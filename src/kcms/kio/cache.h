/*
    cache.h - Proxy configuration dialog
    SPDX-FileCopyrightText: 2001, 2002, 2003 Dawit Alemayehu <adawit@kde.org>

    SPDX-License-Identifier: LGPL-2.0-only
*/

#ifndef CACHECONFIGMODULE_H
#define CACHECONFIGMODULE_H

// KDE
#include <KCModule>

// Local
#include "ui_cache.h"

class CacheConfigModule : public KCModule
{
  Q_OBJECT

public:
  CacheConfigModule(QWidget *parent, const QVariantList &args);
  ~CacheConfigModule();

  void load() override;
  void save() override;
  void defaults() override;
  QString quickHelp() const override;

private Q_SLOTS:
  void configChanged();
  void clearCache();

private:
  Ui::CacheConfigUI ui;
};

#endif // CACHE_H
