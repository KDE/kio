/*
    This file is part of the KDE libraries
    SPDX-FileCopyrightText: 2000 Stephan Kulow <coolo@kde.org>
    SPDX-FileCopyrightText: 1999-2008 David Faure <faure@kde.org>
    SPDX-FileCopyrightText: 2001, 2006 Holger Freyther <freyther@kde.org>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#include "kio/renamedialog.h"
#include "../utils_p.h"
#include "kio_widgets_debug.h"
#include "kshell.h"

#include <QApplication>
#include <QCheckBox>
#include <QDate>
#include <QLabel>
#include <QLayout>
#include <QLineEdit>
#include <QMenu>
#include <QMimeDatabase>
#include <QPixmap>
#include <QPushButton>
#include <QScreen>
#include <QScrollArea>
#include <QScrollBar>
#include <QToolButton>

#include <KFileUtils>
#include <KGuiItem>
#include <KIconLoader>
#include <KLocalizedString>
#include <KMessageBox>
#include <KSeparator>
#include <KSqueezedTextLabel>
#include <KStandardGuiItem>
#include <KStringHandler>
#include <kfileitem.h>
#include <kio/udsentry.h>
#include <previewjob.h>

using namespace KIO;

static QLabel *createLabel(QWidget *parent, const QString &text, bool containerTitle = false)
{
    auto *label = new QLabel(parent);

    if (containerTitle) {
        QFont font = label->font();
        font.setBold(true);
        label->setFont(font);
    }

    label->setAlignment(Qt::AlignHCenter);
    label->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Fixed);
    label->setText(text);
    return label;
}

static QLabel *createDateLabel(QWidget *parent, const KFileItem &item)
{
    const bool hasDate = item.entry().contains(KIO::UDSEntry::UDS_MODIFICATION_TIME);
    const QString text = hasDate ? i18n("Date: %1", item.timeString(KFileItem::ModificationTime)) : QString();
    QLabel *dateLabel = createLabel(parent, text);
    dateLabel->setAlignment(Qt::AlignLeft | Qt::AlignTop);
    return dateLabel;
}

static QLabel *createSizeLabel(QWidget *parent, const KFileItem &item)
{
    const bool hasSize = item.entry().contains(KIO::UDSEntry::UDS_SIZE);
    const QString text = hasSize ? i18n("Size: %1", KIO::convertSize(item.size())) : QString();
    QLabel *sizeLabel = createLabel(parent, text);
    sizeLabel->setAlignment(Qt::AlignLeft | Qt::AlignBottom);
    return sizeLabel;
}

static KSqueezedTextLabel *createSqueezedLabel(QWidget *parent, const QString &text)
{
    auto *label = new KSqueezedTextLabel(text, parent);
    label->setAlignment(Qt::AlignHCenter);
    label->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
    return label;
}

enum CompareFilesResult {
    Identical,
    PartiallyIdentical,
    Different,
};
static CompareFilesResult compareFiles(const QString &filepath, const QString &secondFilePath)
{
    const qint64 bufferSize = 4096; // 4kb
    QFile f(filepath);
    QFile f2(secondFilePath);
    const auto fileSize = f.size();

    if (fileSize != f2.size()) {
        return CompareFilesResult::Different;
    }
    if (!f.open(QFile::ReadOnly)) {
        qCWarning(KIO_WIDGETS) << "Could not open file for comparison:" << f.fileName();
        return CompareFilesResult::Different;
    }
    if (!f2.open(QFile::ReadOnly)) {
        f.close();
        qCWarning(KIO_WIDGETS) << "Could not open file for comparison:" << f2.fileName();
        return CompareFilesResult::Different;
    }

    QByteArray buffer(bufferSize, 0);
    QByteArray buffer2(bufferSize, 0);

    auto seekFillBuffer = [bufferSize](qint64 pos, QFile &f, QByteArray &buffer) {
        auto ioresult = f.seek(pos);
        if (ioresult) {
            const int bytesRead = f.read(buffer.data(), bufferSize);
            ioresult = bytesRead != -1;
        }
        if (!ioresult) {
            qCWarning(KIO_WIDGETS) << "Could not read file for comparison:" << f.fileName();
            return false;
        }
        return true;
    };

    // compare at the beginning of the files
    bool result = seekFillBuffer(0, f, buffer);
    result = result && seekFillBuffer(0, f2, buffer2);
    result = result && buffer == buffer2;

    if (result && fileSize > 2 * bufferSize) {
        // compare the contents in the middle of the files
        result = seekFillBuffer(fileSize / 2 - bufferSize / 2, f, buffer);
        result = result && seekFillBuffer(fileSize / 2 - bufferSize / 2, f2, buffer2);
        result = result && buffer == buffer2;
    }

    if (result && fileSize > bufferSize) {
        // compare the contents at the end of the files
        result = seekFillBuffer(fileSize - bufferSize, f, buffer);
        result = result && seekFillBuffer(fileSize - bufferSize, f2, buffer2);
        result = result && buffer == buffer2;
    }

    if (!result) {
        return CompareFilesResult::Different;
    }

    if (fileSize <= bufferSize * 3) {
        // for files smaller than bufferSize * 3, we in fact compared fully the files
        return CompareFilesResult::Identical;
    } else {
        return CompareFilesResult::PartiallyIdentical;
    }
}

/** @internal */
class Q_DECL_HIDDEN RenameDialog::RenameDialogPrivate
{
public:
    RenameDialogPrivate()
    {
    }

    void setRenameBoxText(const QString &fileName)
    {
        // sets the text in file name line edit box, selecting the filename (but not the extension if there is one).
        QMimeDatabase db;
        const QString extension = db.suffixForFileName(fileName);
        m_pLineEdit->setText(fileName);

        if (!extension.isEmpty()) {
            const int selectionLength = fileName.length() - extension.length() - 1;
            m_pLineEdit->setSelection(0, selectionLength);
        } else {
            m_pLineEdit->selectAll();
        }
    }

    QPushButton *bCancel = nullptr;
    QPushButton *bRename = nullptr;
    QPushButton *bSkip = nullptr;
    QToolButton *bOverwrite = nullptr;
    QAction *bOverwriteWhenOlder = nullptr;
    QPushButton *bResume = nullptr;
    QPushButton *bSuggestNewName = nullptr;
    QCheckBox *bApplyAll = nullptr;
    QLineEdit *m_pLineEdit = nullptr;
    QUrl src;
    QUrl dest;
    bool m_srcPendingPreview = false;
    bool m_destPendingPreview = false;
    QLabel *m_srcPreview = nullptr;
    QLabel *m_destPreview = nullptr;
    QLabel *m_srcDateLabel = nullptr;
    QLabel *m_destDateLabel = nullptr;
    KFileItem srcItem;
    KFileItem destItem;
};

RenameDialog::RenameDialog(QWidget *parent,
                           const QString &title,
                           const QUrl &_src,
                           const QUrl &_dest,
                           RenameDialog_Options _options,
                           KIO::filesize_t sizeSrc,
                           KIO::filesize_t sizeDest,
                           const QDateTime &ctimeSrc,
                           const QDateTime &ctimeDest,
                           const QDateTime &mtimeSrc,
                           const QDateTime &mtimeDest)
    : QDialog(parent)
    , d(new RenameDialogPrivate)
{
    setObjectName(QStringLiteral("KIO::RenameDialog"));

    d->src = _src;
    d->dest = _dest;

    setWindowTitle(title);

    d->bCancel = new QPushButton(this);
    KGuiItem::assign(d->bCancel, KStandardGuiItem::cancel());
    connect(d->bCancel, &QAbstractButton::clicked, this, &RenameDialog::cancelPressed);

    if (_options & RenameDialog_MultipleItems) {
        d->bApplyAll = new QCheckBox(i18n("Appl&y to All"), this);
        d->bApplyAll->setToolTip((_options & RenameDialog_DestIsDirectory) ? i18n("When this is checked the button pressed will be applied to all "
                                                                                  "subsequent folder conflicts for the remainder of the current job.\n"
                                                                                  "Unless you press Skip you will still be prompted in case of a "
                                                                                  "conflict with an existing file in the directory.")
                                                                           : i18n("When this is checked the button pressed will be applied to "
                                                                                  "all subsequent conflicts for the remainder of the current job."));
        connect(d->bApplyAll, &QAbstractButton::clicked, this, &RenameDialog::applyAllPressed);
    }

    if (!(_options & RenameDialog_NoRename)) {
        d->bRename = new QPushButton(i18n("&Rename"), this);
        d->bRename->setEnabled(false);
        d->bSuggestNewName = new QPushButton(i18n("Suggest New &Name"), this);
        connect(d->bSuggestNewName, &QAbstractButton::clicked, this, &RenameDialog::suggestNewNamePressed);
        connect(d->bRename, &QAbstractButton::clicked, this, &RenameDialog::renamePressed);
    }

    if ((_options & RenameDialog_MultipleItems) && (_options & RenameDialog_Skip)) {
        d->bSkip = new QPushButton(i18n("&Skip"), this);
        d->bSkip->setToolTip((_options & RenameDialog_DestIsDirectory) ? i18n("Do not copy or move this folder, skip to the next item instead")
                                                                       : i18n("Do not copy or move this file, skip to the next item instead"));
        connect(d->bSkip, &QAbstractButton::clicked, this, &RenameDialog::skipPressed);
    }

    if (_options & RenameDialog_Overwrite) {
        d->bOverwrite = new QToolButton(this);
        d->bOverwrite->setText(KStandardGuiItem::overwrite().text());
        d->bOverwrite->setIcon(QIcon::fromTheme(KStandardGuiItem::overwrite().iconName()));
        d->bOverwrite->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);

        if (_options & RenameDialog_DestIsDirectory) {
            d->bOverwrite->setText(i18nc("Write files into an existing folder", "&Write Into"));
            d->bOverwrite->setIcon(QIcon());
            d->bOverwrite->setToolTip(
                i18n("Files and folders will be copied into the existing directory, alongside its existing contents.\nYou will be prompted again in case of a "
                     "conflict with an existing file in the directory."));

        } else if ((_options & RenameDialog_MultipleItems) && mtimeSrc.isValid() && mtimeDest.isValid()) {
            d->bOverwriteWhenOlder = new QAction(QIcon::fromTheme(KStandardGuiItem::overwrite().iconName()),
                                                 i18nc("Overwrite files into an existing folder when files are older", "&Overwrite older files"),
                                                 this);
            d->bOverwriteWhenOlder->setEnabled(false);
            d->bOverwriteWhenOlder->setToolTip(
                i18n("Destination files which have older modification times will be overwritten by the source, skipped otherwise."));
            connect(d->bOverwriteWhenOlder, &QAction::triggered, this, &RenameDialog::overwriteWhenOlderPressed);

            auto *overwriteMenu = new QMenu();
            overwriteMenu->addAction(d->bOverwriteWhenOlder);
            d->bOverwrite->setMenu(overwriteMenu);
            d->bOverwrite->setPopupMode(QToolButton::MenuButtonPopup);
        }
        connect(d->bOverwrite, &QAbstractButton::clicked, this, &RenameDialog::overwritePressed);
    }

    if (_options & RenameDialog_Resume) {
        d->bResume = new QPushButton(i18n("&Resume"), this);
        connect(d->bResume, &QAbstractButton::clicked, this, &RenameDialog::resumePressed);
    }

    auto *pLayout = new QVBoxLayout(this);
    pLayout->addStrut(400); // makes dlg at least that wide

    // User tries to overwrite a file with itself ?
    if (_options & RenameDialog_OverwriteItself) {
        auto *lb = new QLabel(i18n("This action would overwrite '%1' with itself.\n"
                                   "Please enter a new file name:",
                                   KStringHandler::csqueeze(d->src.toDisplayString(QUrl::PreferLocalFile), 100)),
                              this);
        lb->setTextFormat(Qt::PlainText);

        d->bRename->setText(i18n("C&ontinue"));
        pLayout->addWidget(lb);
    } else if (_options & RenameDialog_Overwrite) {
        if (d->src.isLocalFile()) {
            d->srcItem = KFileItem(d->src);
        } else {
            UDSEntry srcUds;

            srcUds.reserve(4);
            srcUds.fastInsert(UDSEntry::UDS_NAME, d->src.fileName());
            if (mtimeSrc.isValid()) {
                srcUds.fastInsert(UDSEntry::UDS_MODIFICATION_TIME, mtimeSrc.toMSecsSinceEpoch() / 1000);
            }
            if (ctimeSrc.isValid()) {
                srcUds.fastInsert(UDSEntry::UDS_CREATION_TIME, ctimeSrc.toMSecsSinceEpoch() / 1000);
            }
            if (sizeSrc != KIO::filesize_t(-1)) {
                srcUds.fastInsert(UDSEntry::UDS_SIZE, sizeSrc);
            }

            d->srcItem = KFileItem(srcUds, d->src);
        }

        if (d->dest.isLocalFile()) {
            d->destItem = KFileItem(d->dest);
        } else {
            UDSEntry destUds;

            destUds.reserve(4);
            destUds.fastInsert(UDSEntry::UDS_NAME, d->dest.fileName());
            if (mtimeDest.isValid()) {
                destUds.fastInsert(UDSEntry::UDS_MODIFICATION_TIME, mtimeDest.toMSecsSinceEpoch() / 1000);
            }
            if (ctimeDest.isValid()) {
                destUds.fastInsert(UDSEntry::UDS_CREATION_TIME, ctimeDest.toMSecsSinceEpoch() / 1000);
            }
            if (sizeDest != KIO::filesize_t(-1)) {
                destUds.fastInsert(UDSEntry::UDS_SIZE, sizeDest);
            }

            d->destItem = KFileItem(destUds, d->dest);
        }

        d->m_srcPreview = createLabel(this, QString());
        d->m_destPreview = createLabel(this, QString());

        d->m_srcPreview->setMinimumHeight(KIconLoader::SizeHuge);
        d->m_srcPreview->setMinimumWidth(KIconLoader::SizeHuge);
        d->m_destPreview->setMinimumHeight(KIconLoader::SizeHuge);
        d->m_destPreview->setMinimumWidth(KIconLoader::SizeHuge);

        d->m_srcPreview->setAlignment(Qt::AlignCenter);
        d->m_destPreview->setAlignment(Qt::AlignCenter);

        d->m_srcPendingPreview = true;
        d->m_destPendingPreview = true;

        // create layout
        auto *gridLayout = new QGridLayout();
        pLayout->addLayout(gridLayout);

        int gridRow = 0;
        auto question = i18n("Would you like to overwrite the destination?");
        if (d->srcItem.isDir() && d->destItem.isDir()) {
            question = i18n("Would you like to merge the contents of '%1' into '%2'?",
                            KShell::tildeCollapse(d->src.toDisplayString(QUrl::PreferLocalFile)),
                            KShell::tildeCollapse(d->dest.toDisplayString(QUrl::PreferLocalFile)));
        }
        auto *questionLabel = new QLabel(question, this);
        questionLabel->setAlignment(Qt::AlignHCenter);
        gridLayout->addWidget(questionLabel, gridRow, 0, 1, 4); // takes the complete first line

        QLabel *srcTitle = createLabel(this, i18n("Source"), true);
        gridLayout->addWidget(srcTitle, ++gridRow, 0, 1, 2);
        QLabel *destTitle = createLabel(this, i18n("Destination"), true);
        gridLayout->addWidget(destTitle, gridRow, 2, 1, 2);

        // The labels containing src and dest path
        QLabel *srcUrlLabel = createSqueezedLabel(this, d->src.toDisplayString(QUrl::PreferLocalFile));
        srcUrlLabel->setTextFormat(Qt::PlainText);
        gridLayout->addWidget(srcUrlLabel, ++gridRow, 0, 1, 2);
        QLabel *destUrlLabel = createSqueezedLabel(this, d->dest.toDisplayString(QUrl::PreferLocalFile));
        destUrlLabel->setTextFormat(Qt::PlainText);
        gridLayout->addWidget(destUrlLabel, gridRow, 2, 1, 2);

        gridRow++;

        // src container (preview, size, date)
        QLabel *srcSizeLabel = createSizeLabel(this, d->srcItem);
        d->m_srcDateLabel = createDateLabel(this, d->srcItem);
        QWidget *srcContainer = createContainerWidget(d->m_srcPreview, srcSizeLabel, d->m_srcDateLabel);
        gridLayout->addWidget(srcContainer, gridRow, 0, 1, 2);

        // dest container (preview, size, date)
        QLabel *destSizeLabel = createSizeLabel(this, d->destItem);
        d->m_destDateLabel = createDateLabel(this, d->destItem);
        QWidget *destContainer = createContainerWidget(d->m_destPreview, destSizeLabel, d->m_destDateLabel);
        gridLayout->addWidget(destContainer, gridRow, 2, 1, 2);

        // Verdicts
        auto *hbox_verdicts = new QHBoxLayout();
        pLayout->addLayout(hbox_verdicts);
        hbox_verdicts->addStretch(1);

        if (mtimeSrc > mtimeDest) {
            hbox_verdicts->addWidget(createLabel(this, i18n("The source is <b>more recent</b>.")));
        } else if (mtimeDest > mtimeSrc) {
            hbox_verdicts->addWidget(createLabel(this, i18n("The source is <b>older</b>.")));
        };

        if (d->srcItem.entry().contains(KIO::UDSEntry::UDS_SIZE) && d->destItem.entry().contains(KIO::UDSEntry::UDS_SIZE)
            && d->srcItem.size() != d->destItem.size()) {
            QString text;
            KIO::filesize_t diff = 0;
            if (d->destItem.size() > d->srcItem.size()) {
                diff = d->destItem.size() - d->srcItem.size();
                text = i18n("The source is <b>smaller by %1</b>.", KIO::convertSize(diff));
            } else {
                diff = d->srcItem.size() - d->destItem.size();
                text = i18n("The source is <b>bigger by %1</b>.", KIO::convertSize(diff));
            }
            hbox_verdicts->addWidget(createLabel(this, text));
        }

        // check files contents for local files
        if ((d->dest.isLocalFile() && !(_options & RenameDialog_DestIsDirectory)) && (d->src.isLocalFile() && !(_options & RenameDialog_SourceIsDirectory))
            && (d->srcItem.size() == d->destItem.size())) {
            const CompareFilesResult CompareFilesResult = compareFiles(d->src.toLocalFile(), d->dest.toLocalFile());

            QString text;
            switch (CompareFilesResult) {
            case CompareFilesResult::Identical:
                text = i18n("The files are <b>identical</b>.");
                break;
            case CompareFilesResult::PartiallyIdentical:
                text = i18n("The files <b>seem identical</b>.");
                break;
            case CompareFilesResult::Different:
                text = i18n("The files are <b>different</b>.");
                break;
            }
            QLabel *filesIdenticalLabel = createLabel(this, text);
            if (CompareFilesResult == CompareFilesResult::PartiallyIdentical) {
                auto *pixmapLabel = new QLabel(this);
                pixmapLabel->setPixmap(QIcon::fromTheme(QStringLiteral("help-about")).pixmap(QSize(16, 16)));
                pixmapLabel->setToolTip(
                    i18n("The files are likely to be identical: they have the same size and their contents are the same at the beginning, middle and end."));
                pixmapLabel->setCursor(Qt::WhatsThisCursor);

                auto *hbox = new QHBoxLayout();
                hbox->addWidget(filesIdenticalLabel);
                hbox->addWidget(pixmapLabel);
                hbox_verdicts->addLayout(hbox);
            } else {
                hbox_verdicts->addWidget(filesIdenticalLabel);
            }
        }
        hbox_verdicts->addStretch(1);

    } else {
        // This is the case where we don't want to allow overwriting, the existing
        // file must be preserved (e.g. when renaming).
        QString sentence1;

        if (mtimeDest < mtimeSrc) {
            sentence1 = i18n("An older item named '%1' already exists.", d->dest.toDisplayString(QUrl::PreferLocalFile));
        } else if (mtimeDest == mtimeSrc) {
            sentence1 = i18n("A similar file named '%1' already exists.", d->dest.toDisplayString(QUrl::PreferLocalFile));
        } else {
            sentence1 = i18n("A more recent item named '%1' already exists.", d->dest.toDisplayString(QUrl::PreferLocalFile));
        }

        QLabel *lb = new KSqueezedTextLabel(sentence1, this);
        lb->setTextFormat(Qt::PlainText);
        pLayout->addWidget(lb);
    }

    if (!(_options & RenameDialog_OverwriteItself) && !(_options & RenameDialog_NoRename)) {
        if (_options & RenameDialog_Overwrite) {
            pLayout->addSpacing(15); // spacer
        }

        auto *lb2 = new QLabel(i18n("Rename:"), this);
        pLayout->addWidget(lb2);
    }

    auto *layout2 = new QHBoxLayout();
    pLayout->addLayout(layout2);

    d->m_pLineEdit = new QLineEdit(this);
    layout2->addWidget(d->m_pLineEdit);

    if (d->bRename) {
        const QString fileName = d->dest.fileName();
        d->setRenameBoxText(KIO::decodeFileName(fileName));

        connect(d->m_pLineEdit, &QLineEdit::textChanged, this, &RenameDialog::enableRenameButton);

        d->m_pLineEdit->setFocus();
    } else {
        d->m_pLineEdit->hide();
    }

    if (d->bSuggestNewName) {
        layout2->addWidget(d->bSuggestNewName);
        setTabOrder(d->m_pLineEdit, d->bSuggestNewName);
    }

    auto *layout = new QHBoxLayout();
    pLayout->addLayout(layout);

    layout->setContentsMargins(0, 10, 0, 0); // add some space above the bottom row with buttons
    layout->addStretch(1);

    if (d->bApplyAll) {
        layout->addWidget(d->bApplyAll);
        setTabOrder(d->bApplyAll, d->bCancel);
    }

    if (d->bSkip) {
        layout->addWidget(d->bSkip);
        setTabOrder(d->bSkip, d->bCancel);
    }

    if (d->bRename) {
        layout->addWidget(d->bRename);
        setTabOrder(d->bRename, d->bCancel);
    }

    if (d->bOverwrite) {
        layout->addWidget(d->bOverwrite);
        setTabOrder(d->bOverwrite, d->bCancel);
    }

    if (d->bResume) {
        layout->addWidget(d->bResume);
        setTabOrder(d->bResume, d->bCancel);
    }

    d->bCancel->setDefault(true);
    layout->addWidget(d->bCancel);

    resize(sizeHint());

#if 1 // without kfilemetadata
    // don't wait for kfilemetadata, but wait until the layouting is done
    if (_options & RenameDialog_Overwrite) {
        QMetaObject::invokeMethod(this, &KIO::RenameDialog::resizePanels, Qt::QueuedConnection);
    }
#endif
}

RenameDialog::~RenameDialog() = default;

void RenameDialog::enableRenameButton(const QString &newDest)
{
    if (newDest != KIO::decodeFileName(d->dest.fileName()) && !newDest.isEmpty()) {
        d->bRename->setEnabled(true);
        d->bRename->setDefault(true);

        if (d->bOverwrite) {
            d->bOverwrite->setEnabled(false); // prevent confusion (#83114)
        }
    } else {
        d->bRename->setEnabled(false);

        if (d->bOverwrite) {
            d->bOverwrite->setEnabled(true);
        }
    }
}

QUrl RenameDialog::newDestUrl()
{
    const QString fileName = d->m_pLineEdit->text();
    QUrl newDest = d->dest.adjusted(QUrl::RemoveFilename); // keeps trailing slash
    newDest.setPath(newDest.path() + KIO::encodeFileName(fileName));
    return newDest;
}

QUrl RenameDialog::autoDestUrl() const
{
    const QUrl destDirectory = d->dest.adjusted(QUrl::RemoveFilename | QUrl::StripTrailingSlash);
    const QString newName = KFileUtils::suggestName(destDirectory, d->dest.fileName());
    QUrl newDest(destDirectory);
    newDest.setPath(Utils::concatPaths(newDest.path(), newName));
    return newDest;
}

void RenameDialog::cancelPressed()
{
    done(Result_Cancel);
}

// Rename
void RenameDialog::renamePressed()
{
    if (d->m_pLineEdit->text().isEmpty()) {
        return;
    }

    if (d->bApplyAll && d->bApplyAll->isChecked()) {
        done(Result_AutoRename);
    } else {
        const QUrl u = newDestUrl();
        if (!u.isValid()) {
            KMessageBox::error(this, i18n("Malformed URL\n%1", u.errorString()));
            qCWarning(KIO_WIDGETS) << u.errorString();
            return;
        }

        done(Result_Rename);
    }
}

// Propose button clicked
void RenameDialog::suggestNewNamePressed()
{
    /* no name to play with */
    if (d->m_pLineEdit->text().isEmpty()) {
        return;
    }

    QUrl destDirectory = d->dest.adjusted(QUrl::RemoveFilename | QUrl::StripTrailingSlash);
    d->setRenameBoxText(KFileUtils::suggestName(destDirectory, d->m_pLineEdit->text()));
}

void RenameDialog::skipPressed()
{
    if (d->bApplyAll && d->bApplyAll->isChecked()) {
        done(Result_AutoSkip);
    } else {
        done(Result_Skip);
    }
}

void RenameDialog::overwritePressed()
{
    if (d->bApplyAll && d->bApplyAll->isChecked()) {
        done(Result_OverwriteAll);
    } else {
        done(Result_Overwrite);
    }
}

void RenameDialog::overwriteWhenOlderPressed()
{
    if (d->bApplyAll && d->bApplyAll->isChecked()) {
        done(Result_OverwriteWhenOlder);
    }
}

void RenameDialog::overwriteAllPressed()
{
    done(Result_OverwriteAll);
}

void RenameDialog::resumePressed()
{
    if (d->bApplyAll && d->bApplyAll->isChecked()) {
        done(Result_ResumeAll);
    } else {
        done(Result_Resume);
    }
}

void RenameDialog::resumeAllPressed()
{
    done(Result_ResumeAll);
}

void RenameDialog::applyAllPressed()
{
    const bool applyAll = d->bApplyAll && d->bApplyAll->isChecked();

    if (applyAll) {
        d->m_pLineEdit->setText(KIO::decodeFileName(d->dest.fileName()));
        d->m_pLineEdit->setEnabled(false);
    } else {
        d->m_pLineEdit->setEnabled(true);
    }

    if (d->bRename) {
        d->bRename->setEnabled(applyAll);
    }

    if (d->bSuggestNewName) {
        d->bSuggestNewName->setEnabled(!applyAll);
    }

    if (d->bOverwriteWhenOlder) {
        d->bOverwriteWhenOlder->setEnabled(applyAll);
    }
}

void RenameDialog::showSrcIcon(const KFileItem &fileitem)
{
    // The preview job failed, show a standard file icon.
    d->m_srcPendingPreview = false;

    const int size = d->m_srcPreview->height();
    const QPixmap pix = QIcon::fromTheme(fileitem.iconName(), QIcon::fromTheme(QStringLiteral("application-octet-stream"))).pixmap(size);
    d->m_srcPreview->setPixmap(pix);
}

void RenameDialog::showDestIcon(const KFileItem &fileitem)
{
    // The preview job failed, show a standard file icon.
    d->m_destPendingPreview = false;

    const int size = d->m_destPreview->height();
    const QPixmap pix = QIcon::fromTheme(fileitem.iconName(), QIcon::fromTheme(QStringLiteral("application-octet-stream"))).pixmap(size);
    d->m_destPreview->setPixmap(pix);
}

void RenameDialog::showSrcPreview(const KFileItem &fileitem, const QPixmap &pixmap)
{
    Q_UNUSED(fileitem);

    if (d->m_srcPendingPreview) {
        d->m_srcPreview->setPixmap(pixmap);
        d->m_srcPendingPreview = false;
    }
}

void RenameDialog::showDestPreview(const KFileItem &fileitem, const QPixmap &pixmap)
{
    Q_UNUSED(fileitem);

    if (d->m_destPendingPreview) {
        d->m_destPreview->setPixmap(pixmap);
        d->m_destPendingPreview = false;
    }
}

void RenameDialog::resizePanels()
{
    Q_ASSERT(d->m_srcPreview != nullptr);
    Q_ASSERT(d->m_destPreview != nullptr);

    // Force keep the same (max) width of date width for src and dest
    int destDateWidth = d->m_destDateLabel->width();
    int srcDateWidth = d->m_srcDateLabel->width();
    int minDateWidth = std::max(destDateWidth, srcDateWidth);
    d->m_srcDateLabel->setMinimumWidth(minDateWidth);
    d->m_destDateLabel->setMinimumWidth(minDateWidth);

    KIO::PreviewJob *srcJob = KIO::filePreview(KFileItemList{d->srcItem}, QSize(d->m_srcPreview->width() * qreal(0.9), d->m_srcPreview->height()));
    srcJob->setScaleType(KIO::PreviewJob::Unscaled);

    KIO::PreviewJob *destJob = KIO::filePreview(KFileItemList{d->destItem}, QSize(d->m_destPreview->width() * qreal(0.9), d->m_destPreview->height()));
    destJob->setScaleType(KIO::PreviewJob::Unscaled);

    connect(srcJob, &PreviewJob::gotPreview, this, &RenameDialog::showSrcPreview);
    connect(destJob, &PreviewJob::gotPreview, this, &RenameDialog::showDestPreview);
    connect(srcJob, &PreviewJob::failed, this, &RenameDialog::showSrcIcon);
    connect(destJob, &PreviewJob::failed, this, &RenameDialog::showDestIcon);
}

QWidget *RenameDialog::createContainerWidget(QLabel *preview, QLabel *SizeLabel, QLabel *DateLabel)
{
    auto *widgetContainer = new QWidget();
    auto *containerLayout = new QHBoxLayout(widgetContainer);

    containerLayout->addStretch(1);
    containerLayout->addWidget(preview);

    auto *detailsLayout = new QVBoxLayout(widgetContainer);
    detailsLayout->addStretch(1);
    detailsLayout->addWidget(SizeLabel);
    detailsLayout->addWidget(DateLabel);
    detailsLayout->addStretch(1);

    containerLayout->addLayout(detailsLayout);
    containerLayout->addStretch(1);

    return widgetContainer;
}

#include "moc_renamedialog.cpp"
