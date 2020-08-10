/*
    SPDX-FileCopyrightText: 2000 Dawit Alemayehu <adawit@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef KCOOKIESPOLICYSELECTIONDLG_H
#define KCOOKIESPOLICYSELECTIONDLG_H

#include <QDialog>
#include "ui_kcookiespolicyselectiondlg.h"

class QWidget;
class QDialogButtonBox;

class KCookieAdvice
{
public:
    enum Value {Dunno = 0, Accept, AcceptForSession, Reject, Ask};

    static const char* adviceToStr (const int& advice) {
        switch (advice) {
        case KCookieAdvice::Accept:
            return I18N_NOOP ("Accept");
        case KCookieAdvice::AcceptForSession:
            return I18N_NOOP ("Accept For Session");
        case KCookieAdvice::Reject:
            return I18N_NOOP ("Reject");
        case KCookieAdvice::Ask:
            return I18N_NOOP ("Ask");
        default:
            return I18N_NOOP ("Do Not Know");
        }
    }

    static KCookieAdvice::Value strToAdvice (const QString& _str) {
        if (_str.isEmpty())
            return KCookieAdvice::Dunno;

        QString advice = _str.toLower().remove(QLatin1Char(' '));

        if (advice == QLatin1String ("accept"))
            return KCookieAdvice::Accept;
        else if (advice == QLatin1String ("acceptforsession"))
            return KCookieAdvice::AcceptForSession;
        else if (advice == QLatin1String ("reject"))
            return KCookieAdvice::Reject;
        else if (advice == QLatin1String ("ask"))
            return KCookieAdvice::Ask;

        return KCookieAdvice::Dunno;
    }
};

class KCookiesPolicySelectionDlg : public QDialog
{
    Q_OBJECT

public:
    explicit KCookiesPolicySelectionDlg(QWidget* parent = nullptr, Qt::WindowFlags f = Qt::WindowFlags());
    ~KCookiesPolicySelectionDlg () {}

    int advice() const;
    QString domain() const;

    void setEnableHostEdit (bool, const QString& host = QString());
    void setPolicy (int policy);

protected Q_SLOTS:
    void slotTextChanged (const QString&);
    void slotPolicyChanged(const QString&);

private:
    int mOldPolicy;
    Ui::KCookiesPolicySelectionDlgUI mUi;
    QDialogButtonBox *mButtonBox;
};
#endif
