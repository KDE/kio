/*
    kproxydlg.h - Proxy configuration dialog
    SPDX-FileCopyrightText: 2001, 2011 Dawit Alemayehu <adawit@kde.org>

    SPDX-License-Identifier: LGPL-2.0-only
*/

#ifndef KPROXYDLG_H
#define KPROXYDLG_H

#include <KCModule>
#include "ui_kproxydlg.h"

class KProxyDialog : public KCModule
{
    Q_OBJECT

public:
    enum DisplayUrlFlag {
        HideNone = 0x00,
        HideHttpUrlScheme = 0x01,
        HideHttpsUrlScheme = 0x02,
        HideFtpUrlScheme = 0x04,
        HideSocksUrlScheme = 0x08,
    };
    Q_DECLARE_FLAGS(DisplayUrlFlags, DisplayUrlFlag)

    KProxyDialog(QWidget* parent, const QVariantList& args);
    ~KProxyDialog();

    void load() override;
    void save() override;
    void defaults() override;
    QString quickHelp() const override;

private Q_SLOTS:
    void autoDetect();
    void showEnvValue(bool);
    void setUseSameProxy(bool);
    void syncProxies(const QString&);
    void syncProxyPorts(int);

    void slotChanged();

private:
    bool autoDetectSystemProxy(QLineEdit* edit, const QString& envVarStr, bool showValue);

    Ui::ProxyDialogUI mUi;
    QStringList mNoProxyForList;
    QMap<QString, QString> mProxyMap;
};

Q_DECLARE_OPERATORS_FOR_FLAGS (KProxyDialog::DisplayUrlFlags)

#endif // KPROXYDLG_H
