/*
    This file is part of the KDE project
    SPDX-FileCopyrightText: 2003 Waldo Bastian <bastian@kde.org>

    SPDX-License-Identifier: LGPL-2.0-only
*/
#include "kbuildsycocaprogressdialog.h"
#include <KSycoca>
#include <QStandardPaths>
#include <KLocalizedString>
#include <QDBusConnection>
#include <QDBusInterface>
#include <QProcess>
#include <QDialogButtonBox>

class KBuildSycocaProgressDialogPrivate
{
public:
    explicit KBuildSycocaProgressDialogPrivate(KBuildSycocaProgressDialog *parent)
        : m_parent(parent)
    {
    }

    KBuildSycocaProgressDialog * const m_parent;
};

void KBuildSycocaProgressDialog::rebuildKSycoca(QWidget *parent)
{
    KBuildSycocaProgressDialog dlg(parent,
                                   i18n("Updating System Configuration"),
                                   i18n("Updating system configuration."));

    // FIXME HACK: kdelibs 4 doesn't evaluate mimeapps.list at query time; refresh
    // its cache as well.
    QDBusInterface kbuildsycoca4(QStringLiteral("org.kde.kded"), QStringLiteral("/kbuildsycoca"), QStringLiteral("org.kde.kbuildsycoca"));
    if (kbuildsycoca4.isValid()) {
        kbuildsycoca4.call(QDBus::NoBlock, QStringLiteral("recreate"));
    } else {
        QProcess::startDetached(QStringLiteral("kbuildsycoca4"), QStringList());
    }

    QProcess *proc = new QProcess(&dlg);
    proc->start(QStringLiteral(KBUILDSYCOCA_EXENAME), QStringList());
    QObject::connect(proc, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
                     &dlg, &QWidget::close);

    dlg.exec();
}

KBuildSycocaProgressDialog::KBuildSycocaProgressDialog(QWidget *_parent,
        const QString &_caption, const QString &text)
    : QProgressDialog(_parent)
    , d(new KBuildSycocaProgressDialogPrivate(this))
{
    setWindowTitle(_caption);
    setModal(true);
    setLabelText(text);
    setRange(0, 0);
    setAutoClose(false);
    QDialogButtonBox* dialogButtonBox = new QDialogButtonBox(QDialogButtonBox::Cancel, this);
    setCancelButton(dialogButtonBox->button(QDialogButtonBox::Cancel));
}

KBuildSycocaProgressDialog::~KBuildSycocaProgressDialog()
{
    delete d;
}

#include "moc_kbuildsycocaprogressdialog.cpp"
