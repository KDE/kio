/*
    SPDX-FileCopyrightText: 2005 Sean Harmer <sh@rama.homelinux.org>
    SPDX-FileCopyrightText: 2005-2007 Till Adam <adam@kde.org>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#ifndef KACLEDITWIDGET_H
#define KACLEDITWIDGET_H

#include <config-kiowidgets.h>

#if HAVE_POSIX_ACL || defined(Q_MOC_RUN)

#include <QWidget>

#include <kacl.h>

#include <memory>

class KACLEditWidget : public QWidget
{
    Q_OBJECT
public:
    explicit KACLEditWidget(QWidget *parent = nullptr);
    ~KACLEditWidget() override;
    KACL getACL() const;
    KACL getDefaultACL() const;
    void setACL(const KACL &);
    void setDefaultACL(const KACL &);
    void setAllowDefaults(bool value);

private:
    class KACLEditWidgetPrivate;
    std::unique_ptr<KACLEditWidgetPrivate> const d;

    Q_DISABLE_COPY(KACLEditWidget)
};

#endif
#endif
