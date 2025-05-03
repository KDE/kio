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

#include <QComboBox>
#include <QDialogButtonBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMimeDatabase>
#include <QPushButton>
#include <QShowEvent>
#include <QSpinBox>

namespace KIO
{
class Q_DECL_HIDDEN RenameFileDialog::RenameFileDialogPrivate
{
public:
    RenameFileDialogPrivate(const KFileItemList &items)
        : placeHolderEdit(nullptr)
        , items(items)
        , indexSpinBox(nullptr)
        , renameOneItem(false)
        , allExtensionsDifferent(true)
    {
    }

    enum RenameOperation {
        Enumerate,
        Replace,
        // Prepend,
        // Append
    };

    QList<QUrl> renamedItems;
    QLabel *placeHolderLabel;
    QLineEdit *placeHolderEdit;
    KFileItemList items;
    QLabel *indexLabel;
    QSpinBox *indexSpinBox;
    QPushButton *okButton;

    QLabel *patternLabel;
    QLineEdit *patternLineEdit;
    QLabel *replacementLabel;
    QLineEdit *replacementEdit;

    QLabel *previewLabel;
    QLineEdit *preview;

    bool renameOneItem;
    bool allExtensionsDifferent;

    RenameOperation operation = Enumerate;
};

RenameFileDialog::RenameFileDialog(const KFileItemList &items, QWidget *parent)
    : QDialog(parent)
    , d(new RenameFileDialogPrivate(items))
{
    setMinimumWidth(320);

    const int itemCount = items.count();
    Q_ASSERT(itemCount >= 1);
    d->renameOneItem = (itemCount == 1);

    setWindowTitle(d->renameOneItem ? i18nc("@title:window", "Rename Item") : i18nc("@title:window", "Rename Items"));
    QDialogButtonBox *buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    d->okButton = buttonBox->button(QDialogButtonBox::Ok);
    d->okButton->setDefault(true);
    d->okButton->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_Return));
    connect(buttonBox, &QDialogButtonBox::accepted, this, &RenameFileDialog::slotAccepted);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &RenameFileDialog::reject);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &QObject::deleteLater);
    d->okButton->setDefault(true);

    KGuiItem::assign(d->okButton, KGuiItem(i18nc("@action:button", "&Rename"), QStringLiteral("dialog-ok-apply")));

    QWidget *page = new QWidget(this);
    mainLayout->addWidget(page);
    mainLayout->addWidget(buttonBox);

    QVBoxLayout *topLayout = new QVBoxLayout(page);

    QString newName;
    if (d->renameOneItem) {
        newName = items.first().name();
        d->placeHolderLabel = new QLabel(xi18nc("@label:textbox", "Rename the item <filename>%1</filename> to:", newName), page);
        d->placeHolderLabel->setTextFormat(Qt::PlainText);
    } else {
        QLabel *renameTypeChoiceLabel = new QLabel(i18nc("@info", "How to rename:"), page);
        QComboBox *comboRenameType = new QComboBox(page);
        comboRenameType->addItems({i18nc("@info renaming operation", "Enumerate"), i18nc("@info renaming operation", "Replace text")});

        QHBoxLayout *renameTypeChoice = new QHBoxLayout;
        renameTypeChoice->setContentsMargins(0, 0, 0, 0);
        renameTypeChoice->addWidget(renameTypeChoiceLabel);
        renameTypeChoice->addWidget(comboRenameType);
        topLayout->addLayout(renameTypeChoice);

        connect(comboRenameType, &QComboBox::currentIndexChanged, this, &RenameFileDialog::slotTypeChoiceChanged);

        newName = i18nc("This a template for new filenames, # is replaced by a number later, must be the end character", "New name #");
        d->placeHolderLabel = new QLabel(i18ncp("@label:textbox", "Rename the %1 selected item to:", "Rename the %1 selected items to:", itemCount), page);

        d->indexLabel = new QLabel(i18nc("@info", "# will be replaced by ascending numbers starting with:"), page);
        d->indexSpinBox = new QSpinBox(page);
        d->indexSpinBox->setMinimum(0);
        d->indexSpinBox->setMaximum(1'000'000'000);
        d->indexSpinBox->setSingleStep(1);
        d->indexSpinBox->setValue(1);
        d->indexSpinBox->setDisplayIntegerBase(10);
        d->indexLabel->setBuddy(d->indexSpinBox);
        connect(d->indexSpinBox, &QSpinBox::valueChanged, this, &RenameFileDialog::slotTextChanged);

        d->previewLabel = new QLabel(i18nc("@info As in file name renaming preview", "Preview:"), page);
        d->preview = new QLineEdit(page);
        d->preview->setReadOnly(true);
        d->preview->setFocusPolicy(Qt::FocusPolicy::NoFocus);

        d->patternLabel = new QLabel(i18nc("@info replace as in replace with", "Replace:"), page);
        d->patternLineEdit = new QLineEdit(page);
        d->patternLineEdit->setPlaceholderText(i18nc("@info placeholder text", "Pattern"));
        d->patternLabel->setBuddy(d->patternLineEdit);

        d->replacementLabel = new QLabel(i18nc("@info with as in replace with", "With:"), page);
        d->replacementEdit = new QLineEdit(page);
        d->replacementEdit->setPlaceholderText(i18nc("@info placeholder text", "Replacement"));
        d->replacementLabel->setBuddy(d->replacementEdit);

        d->patternLabel->hide();
        d->patternLineEdit->hide();
        d->replacementLabel->hide();
        d->replacementEdit->hide();

        connect(d->patternLineEdit, &QLineEdit::textChanged, this, &RenameFileDialog::slotTextChanged);
        connect(d->replacementEdit, &QLineEdit::textChanged, this, &RenameFileDialog::slotTextChanged);
    }

    d->placeHolderEdit = new QLineEdit(page);
    d->placeHolderLabel->setBuddy(d->placeHolderEdit);
    mainLayout->addWidget(d->placeHolderEdit);
    connect(d->placeHolderEdit, &QLineEdit::textChanged, this, &RenameFileDialog::slotTextChanged);

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

    d->placeHolderEdit->setText(newName);
    d->placeHolderEdit->setSelection(0, selectionLength);

    topLayout->addWidget(d->placeHolderLabel);
    topLayout->addWidget(d->placeHolderEdit);

    if (!d->renameOneItem) {
        QMimeDatabase db;
        QSet<QString> extensions;
        for (const KFileItem &item : std::as_const(d->items)) {
            const QString extension = db.suffixForFileName(item.name());

            if (extensions.contains(extension)) {
                d->allExtensionsDifferent = false;
                break;
            }

            extensions.insert(extension);
        }

        // Layout
        QHBoxLayout *indexLayout = new QHBoxLayout;
        indexLayout->setContentsMargins(0, 0, 0, 0);
        indexLayout->addWidget(d->indexLabel);
        indexLayout->addWidget(d->indexSpinBox);
        topLayout->addLayout(indexLayout);

        QHBoxLayout *replaceLayout = new QHBoxLayout;
        replaceLayout->setContentsMargins(0, 0, 0, 0);
        replaceLayout->addWidget(d->patternLabel);
        replaceLayout->addWidget(d->patternLineEdit);
        replaceLayout->addWidget(d->replacementLabel);
        replaceLayout->addWidget(d->replacementEdit);
        topLayout->addLayout(replaceLayout);

        topLayout->addWidget(d->previewLabel);
        topLayout->addWidget(d->preview);
    }

    d->placeHolderEdit->setFocus();
}

RenameFileDialog::~RenameFileDialog()
{
}

static QString replaceFunction(const QStringView fileName, const QString &pattern, const QString &replacement)
{
    auto output = QString(fileName);
    if (pattern.isEmpty()) {
        return output;
    }
    output.replace(pattern, replacement);
    while (output.startsWith(QLatin1Char(' '))) {
        output = output.mid(1);
    }
    return output;
}

void RenameFileDialog::slotAccepted()
{
    QWidget *widget = parentWidget();
    if (!widget) {
        widget = this;
    }

    const QList<QUrl> srcList = d->items.urlList();
    const QString newName = d->placeHolderEdit->text();
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
        cmdType = KIO::FileUndoManager::BatchRename;
        d->renamedItems.reserve(d->items.count());

        switch (d->operation) {
        case RenameFileDialogPrivate::Enumerate: {
            job = KIO::batchRename(srcList, newName, d->indexSpinBox->value(), QLatin1Char('#'));
            break;
        }
        case RenameFileDialogPrivate::Replace: {
            auto pattern = d->patternLineEdit->text();
            auto replacement = d->replacementEdit->text();
            std::function<QString(const QStringView view, int index)> renameFunction = [pattern, replacement](const QStringView view, int index) {
                Q_UNUSED(index);
                return replaceFunction(view, pattern, replacement);
            };

            job = KIO::batchRename(srcList, renameFunction, d->indexSpinBox->value());
            break;
        }
        }

        connect(qobject_cast<KIO::BatchRenameJob *>(job), &KIO::BatchRenameJob::fileRenamed, this, &RenameFileDialog::slotFileRenamed);
    }

    KJobWidgets::setWindow(job, widget);
    const QUrl parentUrl = srcList.first().adjusted(QUrl::RemoveFilename | QUrl::StripTrailingSlash);
    KIO::FileUndoManager::self()->recordJob(cmdType, srcList, parentUrl, job);

    connect(job, &KJob::result, this, &RenameFileDialog::slotResult);
    connect(job, &KJob::result, this, &QObject::deleteLater);

    accept();
}

void RenameFileDialog::slotTypeChoiceChanged(int index)
{
    if (d->renameOneItem) {
        return;
    }

    d->indexSpinBox->hide();
    d->indexLabel->hide();

    d->placeHolderLabel->hide();
    d->placeHolderEdit->hide();

    d->patternLabel->hide();
    d->patternLineEdit->hide();
    d->replacementLabel->hide();
    d->replacementEdit->hide();

    if (index == RenameFileDialogPrivate::Enumerate) {
        d->operation = RenameFileDialogPrivate::Enumerate;

        d->indexSpinBox->show();
        d->indexLabel->show();

        d->placeHolderLabel->show();
        d->placeHolderEdit->show();
    } else if (index == RenameFileDialogPrivate::Replace) {
        d->operation = RenameFileDialogPrivate::Replace;

        d->patternLabel->show();
        d->patternLineEdit->show();
        d->replacementLabel->show();
        d->replacementEdit->show();
    }

    slotTextChanged();

    adjustSize();
}

void RenameFileDialog::slotTextChanged()
{
    const auto placeholder = d->placeHolderEdit->text();
    if (d->renameOneItem) {
        bool enabled = !placeholder.isEmpty() && (placeholder != QLatin1String("..")) && (placeholder != QLatin1String("."));
        d->okButton->setEnabled(enabled);
        return;
    }

    const KFileItem &firstItem = d->items.constFirst();

    bool enabled = false;
    switch (d->operation) {
    case RenameFileDialogPrivate::Enumerate: {
        auto previewText = QString(placeholder);
        const int countDash = placeholder.count(QLatin1Char('#'));
        if (countDash == 0) {
            // append # at the end
            previewText += QLatin1Char('#');
        }
        auto indexStart = d->indexSpinBox ? d->indexSpinBox->value() : 1;

        // look for consecutive # groups
        static const QRegularExpression regex(QStringLiteral("#+"));
        auto matchDashes = regex.globalMatch(previewText);

        QRegularExpressionMatch lastMatchDashes;
        int matchCount = 0;
        while (matchDashes.hasNext()) {
            lastMatchDashes = matchDashes.next();
            matchCount++;
        }
        Q_ASSERT(matchCount > 0); // since we add # at the end always

        previewText = previewText.replace(lastMatchDashes.capturedStart(0), lastMatchDashes.capturedLength(0), QString::number(indexStart));

        d->preview->setText(QStringLiteral("%1.%2").arg(previewText, firstItem.suffix()));

        enabled = !placeholder.isEmpty() && (placeholder != QLatin1String("..")) && (placeholder != QLatin1String(".")) && matchCount == 1;
        break;
    }

    case RenameFileDialogPrivate::Replace: {
        auto previewText = replaceFunction(firstItem.name(), d->patternLineEdit->text(), d->replacementEdit->text());
        d->preview->setText(previewText);

        enabled = !d->patternLineEdit->text().isEmpty() && previewText != firstItem.name();
        break;
    }
    }

    d->okButton->setEnabled(enabled);
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

#include "moc_renamefiledialog.cpp"
