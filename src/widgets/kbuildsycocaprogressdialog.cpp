/*
    This file is part of the KDE project
    SPDX-FileCopyrightText: 2003 Waldo Bastian <bastian@kde.org>

    SPDX-License-Identifier: LGPL-2.0-only
*/
#include "kbuildsycocaprogressdialog.h"
#include "kio_widgets_debug.h"

#include <KJobWidgets>
#include <KLocalizedString>

#include <QDialogButtonBox>
#include <QEventLoop>

#include "buildsycocajob.h"
#include "jobuidelegatefactory.h"

class KBuildSycocaProgressDialogPrivate
{
};

void KBuildSycocaProgressDialog::rebuildKSycoca(QWidget *parent)
{
    KIO::BuildSycocaJob job;
    KJobWidgets::setWindow(&job, parent);
    job.setUiDelegate(KIO::createDefaultJobUiDelegate());
    QEventLoop loop;
    connect(&job, &KJob::result, &loop, &QEventLoop::quit);
    job.start();
    loop.exec();
}

KBuildSycocaProgressDialog::KBuildSycocaProgressDialog(QWidget *_parent, const QString &title, const QString &text)
    : QProgressDialog(_parent)
    , d(new KBuildSycocaProgressDialogPrivate)
{
    setWindowTitle(title);
    setModal(true);
    setLabelText(text);
    setRange(0, 0);
    setAutoClose(false);
    QDialogButtonBox *dialogButtonBox = new QDialogButtonBox(QDialogButtonBox::Cancel, this);
    setCancelButton(dialogButtonBox->button(QDialogButtonBox::Cancel));
}

KBuildSycocaProgressDialog::~KBuildSycocaProgressDialog() = default;

#include "moc_kbuildsycocaprogressdialog.cpp"
