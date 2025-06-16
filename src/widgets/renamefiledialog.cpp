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
#include <KMessageWidget>

#include <QComboBox>
#include <QDialogButtonBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMimeDatabase>
#include <QPushButton>
#include <QShowEvent>
#include <QSpinBox>
#include <QTimer>

#include <set>

namespace
{

enum Result {
    Ok,
    Invalid
};

// TODO c++23 port to std::expected
struct ValidationResult {
    Result result;
    QString text;
    KMessageWidget::MessageType type;
};
inline ValidationResult ok()
{
    return ValidationResult{Result::Ok, QString(), KMessageWidget::MessageType::Information};
};
inline ValidationResult invalid(const QString &text)
{
    return ValidationResult{Result::Invalid, text, KMessageWidget::MessageType::Error};
};

/// design pattern strategy
class RenameOperationAbstractStrategy
{
public:
    RenameOperationAbstractStrategy() { };
    virtual ~RenameOperationAbstractStrategy() { };

    virtual QWidget *init(const KFileItemList &items, QWidget *parent, std::function<void()> &updateCallback) = 0;
    virtual const std::function<QString(const QStringView fileName)> renameFunction() = 0;
    virtual ValidationResult validate(const KFileItemList &items, const QStringView fileName) = 0;
};

enum RenameStrategy {
    // SingleFileRename
    Enumerate,
    Replace,
    AddText,
    // Regex
};

class SingleFileRenameStrategy : public RenameOperationAbstractStrategy
{
public:
    ~SingleFileRenameStrategy() override
    {
    }

    QWidget *init(const KFileItemList &items, QWidget *parent, std::function<void()> &updateCallback) override
    {
        Q_UNUSED(updateCallback)

        QWidget *widget = new QWidget(parent);
        auto layout = new QVBoxLayout(widget);

        QString newName = items.first().name();
        auto fileNameLabel = new QLabel(xi18nc("@label:textbox", "Rename the item <filename>%1</filename> to:", newName), widget);
        fileNameLabel->setTextFormat(Qt::PlainText);

        int selectionLength = newName.length();
        // If the current item is a directory, select the whole file name.
        if (!items.first().isDir()) {
            QMimeDatabase db;
            const QString extension = db.suffixForFileName(items.first().name());
            if (extension.length() > 0) {
                // Don't select the extension
                selectionLength -= extension.length() + 1;
            }
        }

        fileNameEdit = new QLineEdit(newName, widget);
        fileNameEdit->setSelection(0, selectionLength);
        fileNameLabel->setBuddy(fileNameEdit);
        widget->setFocusProxy(fileNameEdit);

        QObject::connect(fileNameEdit, &QLineEdit::textChanged, updateCallback);

        layout->addWidget(fileNameLabel);
        layout->addWidget(fileNameEdit);

        fileNameEdit->setFocus();

        return widget;
    }

    const std::function<QString(const QStringView fileName)> renameFunction() override
    {
        return [this](const QStringView /*fileName */) {
            return fileNameEdit->text();
        };
    }

    ValidationResult validate(const KFileItemList &items, const QStringView fileName) override
    {
        const auto oldUrl = items.at(0).url();
        const auto placeholder = fileNameEdit->text();
        if (placeholder.isEmpty()) {
            return invalid(QString());
        }
        QUrl newUrl = oldUrl.adjusted(QUrl::RemoveFilename);
        newUrl.setPath(newUrl.path() + KIO::encodeFileName(fileName.toString()));
        bool fileExists = false;
        if (oldUrl.isLocalFile() && newUrl != oldUrl) {
            fileExists = QFile::exists(newUrl.toLocalFile());
        }
        if (fileExists) {
            return invalid(xi18nc("@info error a file already exists", "A file named <filename>%1</filename> already exists.", newUrl.fileName()));
        }
        if (placeholder == QLatin1String("..") || (placeholder == QLatin1String("."))) {
            return invalid(xi18nc("@info %1 is an invalid filename", "<filename>%1</filename> is not a valid file name.", placeholder));
        }
        return ok();
    }

    QLineEdit *fileNameEdit;
};

class EnumerateStrategy : public RenameOperationAbstractStrategy
{
public:
    ~EnumerateStrategy() override
    {
    }

    QWidget *init(const KFileItemList &items, QWidget *parent, std::function<void()> &updateCallback) override
    {
        QWidget *widget = new QWidget(parent);
        auto layout = new QVBoxLayout(widget);

        auto renameLabel = new QLabel(i18ncp("@label:textbox", "Rename the %1 selected item to:", "Rename the %1 selected items to:", items.count()), widget);
        layout->addWidget(renameLabel);

        auto indexLabel = new QLabel(i18nc("@info", "# will be replaced by ascending numbers starting with:"), widget);
        indexSpinBox = new QSpinBox(widget);
        indexSpinBox->setMinimum(0);
        indexSpinBox->setMaximum(1'000'000'000);
        indexSpinBox->setSingleStep(1);
        indexSpinBox->setValue(1);
        indexSpinBox->setDisplayIntegerBase(10);
        indexLabel->setBuddy(indexSpinBox);

        auto newName = i18nc("This a template for new filenames, # is replaced by a number later, must be the end character", "New name #");
        placeHolderEdit = new QLineEdit(newName, widget);

        layout->addWidget(placeHolderEdit);

        // Layout
        auto indexLayout = new QHBoxLayout;
        indexLayout->setContentsMargins(0, 0, 0, 0);
        indexLayout->addWidget(indexLabel);
        indexLayout->addWidget(indexSpinBox);
        layout->addLayout(indexLayout);

        QObject::connect(indexSpinBox, &QSpinBox::valueChanged, updateCallback);
        QObject::connect(placeHolderEdit, &QLineEdit::textChanged, updateCallback);

        placeHolderEdit->setSelection(0, newName.length() - 1);
        placeHolderEdit->setFocus();

        widget->setTabOrder(placeHolderEdit, indexSpinBox);
        widget->setFocusProxy(placeHolderEdit);

        // Check for extensions.
        std::set<QString> extensions;
        QMimeDatabase db;
        for (const auto &fileItem : std::as_const(items)) {
            const QString extension = fileItem.suffix();
            const auto [it, isInserted] = extensions.insert(extension);
            if (!isInserted) {
                allExtensionsDifferent = false;
                break;
            }
        }

        return widget;
    }

    const std::function<QString(const QStringView fileName)> renameFunction() override
    {
        auto newName = placeHolderEdit->text();
        const auto placeHolder = QLatin1Char('#');

        // look for consecutive # groups
        static const QRegularExpression regex(QStringLiteral("%1+").arg(placeHolder));

        auto matchDashes = regex.globalMatch(newName);
        QRegularExpressionMatch lastMatchDashes;
        int matchCount = 0;
        while (matchDashes.hasNext()) {
            lastMatchDashes = matchDashes.next();
            matchCount++;
        }

        validPlaceholder = matchCount == 1;

        int placeHolderStart = lastMatchDashes.capturedStart(0);
        int placeHolderLength = lastMatchDashes.capturedLength(0);

        QString pattern(newName);

        if (!validPlaceholder) {
            if (allExtensionsDifferent) {
                // pattern: my-file
                // in: file-a.txt file-b.md
            } else {
                // pattern: my-file
                // in: file-a.txt file-b.txt
                // effective pattern: my-file#
                placeHolderLength = 1;
                placeHolderStart = pattern.length();
                pattern.append(placeHolder);
            }
        }
        bool allExtensionsDiff = allExtensionsDifferent;
        bool valid = validPlaceholder;

        index = indexSpinBox->value();
        std::function<QString(const QStringView fileName)> function =
            [pattern, allExtensionsDiff, valid, placeHolderStart, placeHolderLength, this](const QStringView fileName) {
                Q_UNUSED(fileName);

                QString indexString = QString::number(index);

                if (!valid) {
                    if (allExtensionsDiff) {
                        // pattern: my-file
                        // in: file-a.txt file-b.md
                        return pattern;
                    }
                }

                // Insert leading zeros if necessary
                indexString = indexString.prepend(QString(placeHolderLength - indexString.length(), QLatin1Char('0')));
                ++index;

                return QString(pattern).replace(placeHolderStart, placeHolderLength, indexString);
            };
        return function;
    }

    ValidationResult validate(const KFileItemList & /*items*/, const QStringView /* fileName */) override
    {
        const auto placeholder = placeHolderEdit->text();
        if (placeholder.isEmpty()) {
            return invalid(QString());
        }
        if (!validPlaceholder && !allExtensionsDifferent) {
            return invalid(
                i18nc("@info", "Invalid filename: The new name should contain one sequence of #, unless all the files have different file extensions."));
        }
        return ok();
    }

    bool validPlaceholder = false;
    bool allExtensionsDifferent = true;
    QLineEdit *placeHolderEdit;
    QSpinBox *indexSpinBox;
    int index;
};

class ReplaceStrategy : public RenameOperationAbstractStrategy
{
public:
    ~ReplaceStrategy() override
    {
    }

    QWidget *init(const KFileItemList &items, QWidget *parent, std::function<void()> &updateCallback) override
    {
        Q_UNUSED(items)

        QWidget *widget = new QWidget(parent);
        auto layout = new QVBoxLayout(widget);

        auto renameLabel = new QLabel(
            i18ncp("@label:textbox by: [Replacing: xx] [With: yy]", "Rename the %1 selected item by:", "Rename the %1 selected items by:", items.count()),
            widget);
        layout->addWidget(renameLabel);

        auto patternLabel = new QLabel(i18nc("@info replace as in replacing [value] with [value]", "Replacing:"), widget);
        patternLineEdit = new QLineEdit(widget);
        patternLineEdit->setPlaceholderText(i18nc("@info placeholder text", "Pattern"));
        patternLabel->setBuddy(patternLineEdit);
        widget->setFocusProxy(patternLineEdit);

        auto replacementLabel = new QLabel(i18nc("@info with as in replacing [value] with [value]", "With:"), widget);
        replacementEdit = new QLineEdit(widget);
        replacementEdit->setPlaceholderText(i18nc("@info placeholder text", "Replacement"));
        replacementLabel->setBuddy(replacementEdit);

        QObject::connect(patternLineEdit, &QLineEdit::textChanged, updateCallback);
        QObject::connect(replacementEdit, &QLineEdit::textChanged, updateCallback);

        auto replaceLayout = new QHBoxLayout();
        replaceLayout->setContentsMargins(0, 0, 0, 0);

        replaceLayout->addWidget(patternLabel);
        replaceLayout->addWidget(patternLineEdit);
        replaceLayout->addWidget(replacementLabel);
        replaceLayout->addWidget(replacementEdit);

        layout->addLayout(replaceLayout);

        return widget;
    }

    const std::function<QString(const QStringView fileName)> renameFunction() override
    {
        const auto pattern = patternLineEdit->text();
        const auto replacement = replacementEdit->text();
        std::function<QString(const QStringView fileName)> renameFunction = [pattern, replacement](const QStringView fileName) {
            auto output = fileName.toString();
            if (pattern.isEmpty()) {
                return output;
            }
            output.replace(pattern, replacement);
            while (output.startsWith(QLatin1Char(' '))) {
                output = output.mid(1);
            }
            return output;
        };
        return renameFunction;
    }

    ValidationResult validate(const KFileItemList &items, const QStringView /* fileName */) override
    {
        const auto pattern = patternLineEdit->text();
        if (pattern.isEmpty()) {
            return invalid(QString());
        }
        auto any_match = std::any_of(items.cbegin(), items.cend(), [pattern](const KFileItem &item) {
            return item.url().fileName().contains(pattern);
        });
        if (!any_match) {
            return invalid(i18nc("@info pattern as in text replacement pattern", "No file name contains the pattern."));
        }
        const auto replacement = replacementEdit->text();
        if (replacement.isEmpty()) {
            auto it = std::find_if(items.cbegin(), items.cend(), [pattern](const KFileItem &item) {
                return item.url().fileName() == pattern;
            });
            if (it != items.cend()) {
                return invalid(xi18nc("@info pattern as in text replacement pattern",
                                      "Replacing “%1” with an empty replacement would cause <filename>%2</filename> to have an empty file name.",
                                      pattern,
                                      it->url().fileName()));
            }
        }
        return ok();
    }

    QLineEdit *patternLineEdit;
    QLineEdit *replacementEdit;
};

class AddTextStrategy : public RenameOperationAbstractStrategy
{
public:
    ~AddTextStrategy() override
    {
    }

    QWidget *init(const KFileItemList &items, QWidget *parent, std::function<void()> &updateCallback) override
    {
        Q_UNUSED(items)

        QWidget *widget = new QWidget(parent);
        auto layout = new QVBoxLayout(widget);

        auto renameLabel = new QLabel(i18ncp("@label:textbox", "Rename the %1 selected item:", "Rename the %1 selected items:", items.count()), widget);
        layout->addWidget(renameLabel);

        auto textLabel = new QLabel(i18nc("@label:textbox add text to a filename", "Add Text:"), widget);
        textLineEdit = new QLineEdit(widget);
        textLineEdit->setPlaceholderText(i18nc("@info:placeholder", "Text to add"));
        textLabel->setBuddy(textLineEdit);
        widget->setFocusProxy(textLineEdit);

        beforeAfterCombo = new QComboBox(widget);
        beforeAfterCombo->addItems({i18nc("@item:inlistbox as in insert text before filename", "Before filename"),
                                    i18nc("@item:inlistbox as in insert text after filename", "After filename")});

        QObject::connect(textLineEdit, &QLineEdit::textChanged, updateCallback);
        QObject::connect(beforeAfterCombo, &QComboBox::currentIndexChanged, updateCallback);

        auto addTextLayout = new QHBoxLayout();
        addTextLayout->setContentsMargins(0, 0, 0, 0);

        addTextLayout->addWidget(textLabel);
        addTextLayout->addWidget(textLineEdit);
        addTextLayout->addWidget(beforeAfterCombo);

        layout->addLayout(addTextLayout);

        return widget;
    }

    const std::function<QString(const QStringView fileName)> renameFunction() override
    {
        const auto textToAdd = textLineEdit->text();
        const auto append = beforeAfterCombo->currentIndex() == 1;
        std::function<QString(const QStringView fileName)> renameFunction = [textToAdd, append](const QStringView fileName) {
            QString output = fileName.toString();
            if (textToAdd.isEmpty()) {
                return output;
            }

            QMimeDatabase db;
            const QString extension = db.suffixForFileName(output);

            if (!extension.isEmpty()) {
                output = output.chopped(extension.length() + 1);
            }
            if (append) {
                output = output + textToAdd;
            } else {
                // prepend
                output = textToAdd + output;
            }
            if (!extension.isEmpty()) {
                output += QLatin1Char('.') + extension;
            }
            return output;
        };
        return renameFunction;
    }

    ValidationResult validate(const KFileItemList &items, const QStringView /* fileName */) override
    {
        const auto prefix = textLineEdit->text();
        if (prefix.isEmpty()) {
            return invalid(QString());
        }
        const auto rename = renameFunction();
        QUrl newUrl;

        auto it = std::find_if(items.cbegin(), items.cend(), [&rename, &newUrl](const KFileItem &item) {
            bool fileExists = false;
            auto oldUrl = item.url();
            newUrl = oldUrl.adjusted(QUrl::RemoveFilename);
            newUrl.setPath(newUrl.path() + KIO::encodeFileName(rename(item.url().fileName())));
            if (oldUrl.isLocalFile() && newUrl != oldUrl) {
                fileExists = QFile::exists(newUrl.toLocalFile());
            }

            return fileExists;
        });
        if (it != items.cend()) {
            return invalid(xi18nc("@info error a file already exists", "A file named <filename>%1</filename> already exists.", newUrl.fileName()));
        }
        return ok();
    }

    QLineEdit *textLineEdit;
    QLineEdit *appendEdit;
    QComboBox *beforeAfterCombo;
};
}

namespace KIO
{

class Q_DECL_HIDDEN RenameFileDialog::RenameFileDialogPrivate
{
public:
    RenameFileDialogPrivate(const KFileItemList &items)
        : items(items)
        , renameOneItem(false)
        , allExtensionsDifferent(true)
    {
    }

    QList<QUrl> renamedItems;
    KFileItemList items;
    QPushButton *okButton;

    KMessageWidget *messageWidget;
    QLabel *previewLabel;
    QLineEdit *preview;

    bool renameOneItem;
    bool allExtensionsDifferent;

    QComboBox *comboRenameType;
    QVBoxLayout *m_topLayout;
    QWidget *m_contentWidget;

    std::unique_ptr<RenameOperationAbstractStrategy> renameStrategy;
};

RenameFileDialog::RenameFileDialog(const KFileItemList &items, QWidget *parent)
    : QDialog(parent)
    , d(new RenameFileDialogPrivate(items))
{
    Q_ASSERT(items.count() >= 1);
    d->renameOneItem = items.count() == 1;

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

    d->m_topLayout = new QVBoxLayout(page);

    if (!d->renameOneItem) {
        QLabel *renameTypeChoiceLabel = new QLabel(i18nc("@info", "How to rename:"), page);
        d->comboRenameType = new QComboBox(page);
        d->comboRenameType->addItems(
            {i18nc("@info renaming operation", "Enumerate"), i18nc("@info renaming operation", "Replace text"), i18nc("@info renaming operation", "Add text")});
        renameTypeChoiceLabel->setBuddy(d->comboRenameType);

        QHBoxLayout *renameTypeChoice = new QHBoxLayout;
        renameTypeChoice->setContentsMargins(0, 0, 0, 0);

        renameTypeChoice->addWidget(renameTypeChoiceLabel);
        renameTypeChoice->addWidget(d->comboRenameType);
        d->m_topLayout->addLayout(renameTypeChoice);

        connect(d->comboRenameType, &QComboBox::currentIndexChanged, this, &RenameFileDialog::slotOperationChanged);

        d->previewLabel = new QLabel(i18nc("@info As in filename renaming preview", "Preview:"), page);
        d->preview = new QLineEdit(page);
        d->preview->setReadOnly(true);
        d->previewLabel->setBuddy(d->preview);
    }

    d->m_contentWidget = new QWidget();
    d->m_topLayout->addWidget(d->m_contentWidget);

    d->messageWidget = new KMessageWidget(page);
    d->messageWidget->setCloseButtonVisible(false);
    d->messageWidget->setWordWrap(true);
    d->m_topLayout->addWidget(d->messageWidget);

    if (!d->renameOneItem) {
        d->m_topLayout->addWidget(d->previewLabel, Qt::AlignBottom);
        d->m_topLayout->addWidget(d->preview, Qt::AlignBottom);
    }

    // initialize UI
    slotOperationChanged(RenameStrategy::Enumerate);

    setFixedWidth(sizeHint().width());
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
    d->renamedItems.reserve(d->items.count());

    KIO::FileUndoManager::CommandType cmdType;
    KIO::Job *job = nullptr;

    if (d->renameOneItem) {
        Q_ASSERT(d->items.count() == 1);
        cmdType = KIO::FileUndoManager::Rename;
        const QUrl oldUrl = d->items.constFirst().url();
        QUrl newUrl = oldUrl.adjusted(QUrl::RemoveFilename);
        newUrl.setPath(newUrl.path() + KIO::encodeFileName(d->renameStrategy->renameFunction()(oldUrl.fileName())));

        job = KIO::moveAs(oldUrl, newUrl, KIO::HideProgressInfo);
        connect(qobject_cast<KIO::CopyJob *>(job),
                &KIO::CopyJob::copyingDone,
                this,
                [this](KIO::Job * /* job */, const QUrl &from, const QUrl &to, const QDateTime & /*mtime*/, bool /*directory*/, bool /*renamed*/) {
                    slotFileRenamed(from, to);
                });
    } else {
        cmdType = KIO::FileUndoManager::BatchRename;

        job = KIO::batchRenameWithFunction(srcList, d->renameStrategy->renameFunction());
        connect(qobject_cast<KIO::BatchRenameJob *>(job), &KIO::BatchRenameJob::fileRenamed, this, &RenameFileDialog::slotFileRenamed);
    }

    KJobWidgets::setWindow(job, widget);
    const QUrl parentUrl = srcList.first().adjusted(QUrl::RemoveFilename | QUrl::StripTrailingSlash);
    KIO::FileUndoManager::self()->recordJob(cmdType, srcList, parentUrl, job);

    connect(job, &KJob::result, this, &RenameFileDialog::slotResult);

    accept();
}

void RenameFileDialog::slotOperationChanged(int index)
{
    setUpdatesEnabled(false);

    if (d->renameOneItem) {
        d->renameStrategy.reset(new SingleFileRenameStrategy());
    } else {
        if (index == RenameStrategy::Enumerate) {
            d->renameStrategy.reset(new EnumerateStrategy());
        } else if (index == RenameStrategy::Replace) {
            d->renameStrategy.reset(new ReplaceStrategy());
        } else if (index == RenameStrategy::AddText) {
            d->renameStrategy.reset(new AddTextStrategy());
        }
    }

    std::function<void()> updateCallback = std::bind(&RenameFileDialog::slotStateChanged, this);

    auto newWidget = d->renameStrategy->init(d->items, this, updateCallback);
    d->m_topLayout->replaceWidget(d->m_contentWidget, newWidget);
    newWidget->setFocus();
    newWidget->setFocusPolicy(Qt::FocusPolicy::StrongFocus);

    delete d->m_contentWidget;
    d->m_contentWidget = newWidget;

    if (!d->renameOneItem) {
        setTabOrder(d->comboRenameType, d->m_contentWidget);
        setTabOrder(d->m_contentWidget, d->preview);
    }

    setUpdatesEnabled(true);

    slotStateChanged();
}

void RenameFileDialog::slotStateChanged()
{
    const auto firstItem = d->items.first();
    auto previewText = d->renameStrategy->renameFunction()(firstItem.url().fileName());

    const QString suffix = QLatin1Char('.') + firstItem.suffix();
    if (!firstItem.suffix().isEmpty() && !previewText.endsWith(suffix) && !previewText.isEmpty() && previewText != suffix) {
        previewText.append(suffix);
    }

    if (!d->renameOneItem) {
        d->preview->setText(previewText);
        d->preview->setAccessibleName(previewText);
    }
    ValidationResult validationResult;
    if (previewText.isEmpty()) {
        validationResult = invalid(xi18nc("@info", "<filename>%1</filename> cannot be renamed to an empty file name.", firstItem.name()));
    } else {
        validationResult = d->renameStrategy->validate(d->items, previewText);
    }
    d->okButton->setEnabled(validationResult.result == Result::Ok);
    if (validationResult.result == Result::Ok || validationResult.text.isEmpty()) {
        d->messageWidget->hide();
        QTimer::singleShot(0, this, [this]() {
            adjustSize();
        });
    } else {
        d->messageWidget->setMessageType(validationResult.type);
        d->messageWidget->setText(validationResult.text);
        d->messageWidget->animatedShow();
    }
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
