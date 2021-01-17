/*
    This file is part of the KDE libraries
    SPDX-FileCopyrightText: 2006-2010 Peter Penz <peter.penz@gmx.at>
    SPDX-FileCopyrightText: 2020 Méven Car <meven.car@kdemail.net>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#include "renamefiledialog.h"

#include <KGuiItem>
#include <KIO/BatchRenameJob>
#include <KIO/CopyJob>
#include <KIO/FileUndoManager>
#include <KJobUiDelegate>
#include <KJobWidgets>
#include <KLocalizedString>

#include <QDialogButtonBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMimeDatabase>
#include <QPushButton>
#include <QSpinBox>
#include <QShowEvent>

namespace KIO {
class Q_DECL_HIDDEN RenameFileDialog::RenameFileDialogPrivate
{
public:

    RenameFileDialogPrivate(const KFileItemList &items)
        : lineEdit(nullptr)
        , items(items)
        , spinBox(nullptr)
        , renameOneItem(false)
        , allExtensionsDifferent(true)
    {
    }

    QList<QUrl> renamedItems;
    QLineEdit *lineEdit;
    KFileItemList items;
    QSpinBox *spinBox;
    QPushButton *okButton;
    bool renameOneItem;
    bool allExtensionsDifferent;
};

RenameFileDialog::RenameFileDialog(const KFileItemList &items, QWidget *parent)
    : QDialog(parent)
    , d(new RenameFileDialogPrivate(items))
{
    setMinimumWidth(320);

    const int itemCount = items.count();
    Q_ASSERT(itemCount >= 1);
    d->renameOneItem = (itemCount == 1);

    setWindowTitle(d->renameOneItem
                   ? i18nc("@title:window", "Rename Item")
                   : i18nc("@title:window", "Rename Items"));
    QDialogButtonBox *buttonBox = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    d->okButton = buttonBox->button(QDialogButtonBox::Ok);
    d->okButton->setDefault(true);
    d->okButton->setShortcut(Qt::CTRL | Qt::Key_Return);
    connect(buttonBox, &QDialogButtonBox::accepted, this, &RenameFileDialog::slotAccepted);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &RenameFileDialog::reject);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &QObject::deleteLater);
    d->okButton->setDefault(true);

    KGuiItem::assign(d->okButton,
                     KGuiItem(i18nc("@action:button", "&Rename"),
                              QStringLiteral("dialog-ok-apply")));

    QWidget *page = new QWidget(this);
    mainLayout->addWidget(page);
    mainLayout->addWidget(buttonBox);

    QVBoxLayout *topLayout = new QVBoxLayout(page);

    QLabel *editLabel = nullptr;
    QString newName;
    if (d->renameOneItem) {
        newName = items.first().name();
        editLabel
            = new QLabel(xi18nc("@label:textbox", "Rename the item <filename>%1</filename> to:",
                                newName),
                         page);
        editLabel->setTextFormat(Qt::PlainText);
    } else {
        newName = i18nc(
            "This a template for new filenames, # is replaced by a number later, must be the end character",
            "New name #");
        editLabel = new QLabel(i18ncp("@label:textbox",
                                      "Rename the %1 selected item to:",
                                      "Rename the %1 selected items to:", itemCount),
                               page);
    }

    d->lineEdit = new QLineEdit(page);
    mainLayout->addWidget(d->lineEdit);
    connect(d->lineEdit, &QLineEdit::textChanged, this, &RenameFileDialog::slotTextChanged);

    int selectionLength = newName.length();
    if (d->renameOneItem) {
        // If the current item is a directory, select the whole file name.
        if (!items.first().isDir()) {
            QMimeDatabase db;
            const QString extension = db.suffixForFileName(items.first().name());
            if (extension.length() > 0) {
                // Don't select the extension
                selectionLength -= extension.length() + 1;
            }
        }
    } else {
        // Don't select the # character
        --selectionLength;
    }

    d->lineEdit->setText(newName);
    d->lineEdit->setSelection(0, selectionLength);

    topLayout->addWidget(editLabel);
    topLayout->addWidget(d->lineEdit);

    if (!d->renameOneItem) {
        QMimeDatabase db;
        QSet<QString> extensions;
        for (const KFileItem &item : qAsConst(d->items)) {
            const QString extension = db.suffixForFileName(item.name());

            if (extensions.contains(extension)) {
                d->allExtensionsDifferent = false;
                break;
            }

            extensions.insert(extension);
        }

        QLabel *infoLabel
            = new QLabel(i18nc("@info",
                               "# will be replaced by ascending numbers starting with:"), page);
        mainLayout->addWidget(infoLabel);
        d->spinBox = new QSpinBox(page);
        d->spinBox->setMaximum(10000);
        d->spinBox->setMinimum(0);
        d->spinBox->setSingleStep(1);
        d->spinBox->setValue(1);
        d->spinBox->setDisplayIntegerBase(10);

        QHBoxLayout *horizontalLayout = new QHBoxLayout(page);
        horizontalLayout->setContentsMargins(0, 0, 0, 0);
        horizontalLayout->addWidget(infoLabel);
        horizontalLayout->addWidget(d->spinBox);

        topLayout->addLayout(horizontalLayout);
    }

    d->lineEdit->setFocus();
}

RenameFileDialog::~RenameFileDialog()
{
}

void RenameFileDialog::slotAccepted()
{
    QWidget *widget = parentWidget();
    if (!widget) {
        widget = this;
    }

    const QList<QUrl> srcList = d->items.urlList();
    const QString newName = d->lineEdit->text();
    KIO::FileUndoManager::CommandType cmdType;
    KIO::Job *job = nullptr;
    if (d->renameOneItem) {
        Q_ASSERT(d->items.count() == 1);
        cmdType = KIO::FileUndoManager::Rename;
        const QUrl oldUrl = d->items.constFirst().url();
        QUrl newUrl = oldUrl.adjusted(QUrl::RemoveFilename);
        newUrl.setPath(newUrl.path() + KIO::encodeFileName(newName));
        d->renamedItems << newUrl;
        job = KIO::moveAs(oldUrl, newUrl, KIO::HideProgressInfo);
    } else {
        d->renamedItems.reserve(d->items.count());
        cmdType = KIO::FileUndoManager::BatchRename;
        job = KIO::batchRename(srcList, newName, d->spinBox->value(), QLatin1Char('#'));
        connect(qobject_cast<KIO::BatchRenameJob *>(
                    job), &KIO::BatchRenameJob::fileRenamed, this,
                &RenameFileDialog::slotFileRenamed);
    }

    KJobWidgets::setWindow(job, widget);
    const QUrl parentUrl
        = srcList.first().adjusted(QUrl::RemoveFilename | QUrl::StripTrailingSlash);
    KIO::FileUndoManager::self()->recordJob(cmdType, srcList, parentUrl, job);

    connect(job, &KJob::result, this, &RenameFileDialog::slotResult);
    connect(job, &KJob::result, this, &QObject::deleteLater);

    accept();
}

void RenameFileDialog::slotTextChanged(const QString &newName)
{
    bool enable = !newName.isEmpty() && (newName != QLatin1String(".."))
                  && (newName != QLatin1String("."));
    if (enable && !d->renameOneItem) {
        const int count = newName.count(QLatin1Char('#'));
        if (count == 0) {
            // Renaming multiple files without '#' will only work if all extensions are different.
            enable = d->allExtensionsDifferent;
        } else {
            // Ensure that the new name contains exactly one # (or a connected sequence of #'s)
            const int first = newName.indexOf(QLatin1Char('#'));
            const int last = newName.lastIndexOf(QLatin1Char('#'));
            enable = (last - first + 1 == count);
        }
    }
    d->okButton->setEnabled(enable);
}

void RenameFileDialog::slotFileRenamed(const QUrl &oldUrl, const QUrl &newUrl)
{
    Q_UNUSED(oldUrl)
    d->renamedItems << newUrl;
}

void RenameFileDialog::slotResult(KJob *job)
{
    if (!job->error()) {
        Q_EMIT renamingFinished(d->renamedItems);
    } else {
        Q_EMIT error(job);
    }
}

} // namespace KIO
