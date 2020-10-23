/*
    This file is part of the KDE libraries
    SPDX-FileCopyrightText: 2017 Klarälvdalens Datakonsult AB, a KDAB Group company <info@kdab.com>
    Work sponsored by the LiMux project of the city of Munich

    SPDX-License-Identifier: LGPL-2.0-only
*/

#include "kfilecustomdialog.h"

#include <QUrl>
#include <QVBoxLayout>
#include <QPushButton>

class KFileCustomDialogPrivate
{
public:
    explicit KFileCustomDialogPrivate(KFileCustomDialog *qq)
        : q(qq)
    {
    }
    void init(const QUrl &startDir);

    KFileWidget *mFileWidget = nullptr;
    KFileCustomDialog * const q;
};

void KFileCustomDialogPrivate::init(const QUrl &startDir)
{
    QVBoxLayout *mainLayout = new QVBoxLayout(q);
    mainLayout->setObjectName(QStringLiteral("mainlayout"));

    mFileWidget = new KFileWidget(startDir, q);
    mFileWidget->setObjectName(QStringLiteral("filewidget"));
    mainLayout->addWidget(mFileWidget);

    mFileWidget->okButton()->show();
    q->connect(mFileWidget->okButton(), &QPushButton::clicked, q, [this]() { mFileWidget->slotOk(); });
    mFileWidget->cancelButton()->show();
    q->connect(mFileWidget->cancelButton(), &QPushButton::clicked, q, [this]() {
        mFileWidget->slotCancel();
        q->reject();
    });
    q->connect(mFileWidget, &KFileWidget::accepted, q, [this] { q->accept(); });
}

KFileCustomDialog::KFileCustomDialog(QWidget *parent)
    : QDialog(parent),
      d(new KFileCustomDialogPrivate(this))
{
    d->init(QUrl());
}

KFileCustomDialog::KFileCustomDialog(const QUrl &startDir, QWidget *parent)
    : QDialog(parent),
      d(new KFileCustomDialogPrivate(this))
{
    d->init(startDir);
}

KFileCustomDialog::~KFileCustomDialog()
{
    delete d;
}

void KFileCustomDialog::setUrl(const QUrl &url)
{
    d->mFileWidget->setUrl(url);
}

void KFileCustomDialog::setCustomWidget(QWidget *widget)
{
    d->mFileWidget->setCustomWidget(QString(), widget);
}

KFileWidget *KFileCustomDialog::fileWidget() const
{
    return d->mFileWidget;
}

void KFileCustomDialog::setOperationMode(KFileWidget::OperationMode op)
{
    d->mFileWidget->setOperationMode(op);
}

void KFileCustomDialog::accept()
{
    d->mFileWidget->accept();
    QDialog::accept();
}
