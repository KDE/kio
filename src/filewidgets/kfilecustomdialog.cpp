/* This file is part of the KDE libraries
    Copyright (C) 2017    Klarälvdalens Datakonsult AB, a KDAB Group
                          company, info@kdab.com. Work sponsored by the
                          LiMux project of the city of Munich

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License version 2, as published by the Free Software Foundation.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.
*/

#include "kfilecustomdialog.h"
#include "kfilewidget.h"

#include <QUrl>
#include <QVBoxLayout>
#include <QPushButton>

class KFileCustomDialogPrivate
{
public:
    explicit KFileCustomDialogPrivate(KFileCustomDialog *qq)
        : q(qq)
    {
        init();
    }
    void init();

    KFileWidget *mFileWidget = nullptr;
    KFileCustomDialog *q;
};

void KFileCustomDialogPrivate::init()
{
    QVBoxLayout *mainLayout = new QVBoxLayout(q);
    mainLayout->setObjectName(QStringLiteral("mainlayout"));

    mFileWidget = new KFileWidget(QUrl(), q);
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
