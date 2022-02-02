/*
    This file is part of the KDE project
    SPDX-FileCopyrightText: 2003 Waldo Bastian <bastian@kde.org>

    SPDX-License-Identifier: LGPL-2.0-only
*/
#include "kbuildsycocaprogressdialog.h"
#include "kio_widgets_debug.h"

#include <KLocalizedString>
#include <KSycoca>

#include <QDBusConnection>
#include <QDBusInterface>
#include <QDialogButtonBox>
#include <QProcess>
#include <QStandardPaths>

class KBuildSycocaProgressDialogPrivate
{
public:
    explicit KBuildSycocaProgressDialogPrivate(KBuildSycocaProgressDialog *parent)
        : m_parent(parent)
    {
    }

    KBuildSycocaProgressDialog *const m_parent;
};

void KBuildSycocaProgressDialog::rebuildKSycoca(QWidget *parent)
{
    KBuildSycocaProgressDialog dlg(parent, i18n("Updating System Configuration"), i18n("Updating system configuration."));

    const QString exec = QStandardPaths::findExecutable(QStringLiteral(KBUILDSYCOCA_EXENAME));
    if (exec.isEmpty()) {
        qCWarning(KIO_WIDGETS) << "Could not find kbuildsycoca executable:" << KBUILDSYCOCA_EXENAME;
        return;
    }
    QProcess *proc = new QProcess(&dlg);
    proc->start(exec, QStringList());
    QObject::connect(proc, qOverload<int, QProcess::ExitStatus>(&QProcess::finished), &dlg, &QWidget::close);

    dlg.exec();
}

KBuildSycocaProgressDialog::KBuildSycocaProgressDialog(QWidget *_parent, const QString &_caption, const QString &text)
    : QProgressDialog(_parent)
    , d(new KBuildSycocaProgressDialogPrivate(this))
{
    setWindowTitle(_caption);
    setModal(true);
    setLabelText(text);
    setRange(0, 0);
    setAutoClose(false);
    QDialogButtonBox *dialogButtonBox = new QDialogButtonBox(QDialogButtonBox::Cancel, this);
    setCancelButton(dialogButtonBox->button(QDialogButtonBox::Cancel));
}

KBuildSycocaProgressDialog::~KBuildSycocaProgressDialog() = default;

#include "moc_kbuildsycocaprogressdialog.cpp"
