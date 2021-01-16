/*
    This file is part of the KDE libraries
    SPDX-FileCopyrightText: 2000 Stephan Kulow <coolo@kde.org>
    SPDX-FileCopyrightText: 1999-2008 David Faure <faure@kde.org>
    SPDX-FileCopyrightText: 2001, 2006 Holger Freyther <freyther@kde.org>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#include "kio/renamedialog.h"
#include "kio_widgets_debug.h"
#include "../pathhelpers_p.h"

#include <QApplication>
#include <QCheckBox>
#include <QDate>
#include <QDesktopWidget>
#include <QLabel>
#include <QLineEdit>
#include <QMimeDatabase>
#include <QPixmap>
#include <QPushButton>
#include <QScrollArea>
#include <QScrollBar>
#include <QLayout>
#include <QMenu>
#include <QToolButton>

#include <KIconLoader>
#include <KMessageBox>
#include <kio/udsentry.h>
#include <KLocalizedString>
#include <kfileitem.h>
#include <KSeparator>
#include <KStringHandler>
#include <KStandardGuiItem>
#include <KGuiItem>
#include <KSqueezedTextLabel>
#include <previewjob.h>
#include <KFileUtils>

using namespace KIO;

static QLabel *createLabel(QWidget *parent, const QString &text, bool containerTitle = false)
{
    QLabel *label = new QLabel(parent);

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
    const QString text = i18n("Date: %1", item.timeString(KFileItem::ModificationTime));
    return createLabel(parent, text);
}

static QLabel *createSizeLabel(QWidget *parent, const KFileItem &item)
{
    const QString text = i18n("Size: %1", KIO::convertSize(item.size()));
    return createLabel(parent, text);
}

static KSqueezedTextLabel *createSqueezedLabel(QWidget *parent, const QString &text)
{
    KSqueezedTextLabel *label = new KSqueezedTextLabel(text, parent);
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

    bool result = true;

    auto seekFillBuffer = [bufferSize](qint64 pos, QFile &f, QByteArray &buffer) {
        auto ioresult = f.seek(pos);
        int bytesRead;
        if (ioresult) {
            bytesRead = f.read(buffer.data(), bufferSize);
            ioresult = bytesRead != -1;
        }
        if (!ioresult) {
            qCWarning(KIO_WIDGETS) << "Could not read file for comparison:" << f.fileName();
            return false;
        }
        return true;
    };

    // compare at the beginning of the files
    result = result && seekFillBuffer(0, f, buffer);
    result = result && seekFillBuffer(0, f2, buffer2);
    result = result && buffer == buffer2;

    if (result && fileSize > 2 * bufferSize) {
        // compare the contents in the middle of the files
        result = result && seekFillBuffer(fileSize / 2 - bufferSize / 2, f, buffer);
        result = result && seekFillBuffer(fileSize / 2 - bufferSize / 2, f2, buffer2);
        result = result && buffer == buffer2;
    }

    if (result && fileSize > bufferSize) {
        // compare the contents at the end of the files
        result = result && seekFillBuffer(fileSize - bufferSize, f, buffer);
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
    QAction     *bOverwriteWhenOlder = nullptr;
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
    QScrollArea *m_srcArea = nullptr;
    QScrollArea *m_destArea = nullptr;
    KFileItem srcItem;
    KFileItem destItem;
};

RenameDialog::RenameDialog(QWidget *parent, const QString &_caption,
                           const QUrl &_src, const QUrl &_dest,
                           RenameDialog_Options _options,
                           KIO::filesize_t sizeSrc,
                           KIO::filesize_t sizeDest,
                           const QDateTime &ctimeSrc,
                           const QDateTime &ctimeDest,
                           const QDateTime &mtimeSrc,
                           const QDateTime &mtimeDest)
    : QDialog(parent), d(new RenameDialogPrivate)
{
    setObjectName(QStringLiteral("KIO::RenameDialog"));

    d->src = _src;
    d->dest = _dest;

    setWindowTitle(_caption);

    d->bCancel = new QPushButton(this);
    KGuiItem::assign(d->bCancel, KStandardGuiItem::cancel());
    connect(d->bCancel, &QAbstractButton::clicked, this, &RenameDialog::cancelPressed);

    if (_options & RenameDialog_MultipleItems) {
        d->bApplyAll = new QCheckBox(i18n("Appl&y to All"), this);
        d->bApplyAll->setToolTip((_options & RenameDialog_DestIsDirectory) ?
                                 i18n("When this is checked the button pressed will be applied to all "
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
        d->bSkip->setToolTip((_options & RenameDialog_DestIsDirectory) ?
                             i18n("Do not copy or move this folder, skip to the next item instead")
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
            d->bOverwrite->setToolTip(i18n("Files and folders will be copied into the existing directory, alongside its existing contents.\nYou will be prompted again in case of a conflict with an existing file in the directory."));

        } else if ((_options & RenameDialog_MultipleItems) && mtimeSrc.isValid() && mtimeDest.isValid()) {
            d->bOverwriteWhenOlder = new QAction(QIcon::fromTheme(KStandardGuiItem::overwrite().iconName()),
                                                 i18nc("Overwrite files into an existing folder when files are older", "&Overwrite older files"), this);
            d->bOverwriteWhenOlder->setEnabled(false);
            d->bOverwriteWhenOlder->setToolTip(i18n("Destination files which have older modification times will be overwritten by the source, skipped otherwise."));
            connect(d->bOverwriteWhenOlder, &QAction::triggered, this, &RenameDialog::overwriteWhenOlderPressed);

            QMenu *overwriteMenu = new QMenu();
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

    QVBoxLayout *pLayout = new QVBoxLayout(this);
    pLayout->addStrut(400);     // makes dlg at least that wide

    // User tries to overwrite a file with itself ?
    if (_options & RenameDialog_OverwriteItself) {
        QLabel *lb = new QLabel(i18n("This action would overwrite '%1' with itself.\n"
                                     "Please enter a new file name:",
                                     KStringHandler::csqueeze(d->src.toDisplayString(QUrl::PreferLocalFile), 100)), this);
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
            srcUds.fastInsert(UDSEntry::UDS_MODIFICATION_TIME, mtimeSrc.toMSecsSinceEpoch() / 1000);
            srcUds.fastInsert(UDSEntry::UDS_CREATION_TIME, ctimeSrc.toMSecsSinceEpoch() / 1000);
            srcUds.fastInsert(UDSEntry::UDS_SIZE, sizeSrc);

            d->srcItem = KFileItem(srcUds, d->src);
        }

        if (d->dest.isLocalFile()) {
            d->destItem = KFileItem(d->dest);
        } else {
            UDSEntry destUds;

            destUds.reserve(4);
            destUds.fastInsert(UDSEntry::UDS_NAME, d->dest.fileName());
            destUds.fastInsert(UDSEntry::UDS_MODIFICATION_TIME, mtimeDest.toMSecsSinceEpoch() / 1000);
            destUds.fastInsert(UDSEntry::UDS_CREATION_TIME, ctimeDest.toMSecsSinceEpoch() / 1000);
            destUds.fastInsert(UDSEntry::UDS_SIZE, sizeDest);

            d->destItem = KFileItem(destUds, d->dest);
        }

        d->m_srcPreview = createLabel(parent, QString());
        d->m_destPreview = createLabel(parent, QString());
        QLabel *srcToDestArrow = createLabel(parent, QString());

        d->m_srcPreview->setMinimumHeight(KIconLoader::SizeEnormous);
        d->m_destPreview->setMinimumHeight(KIconLoader::SizeEnormous);
        srcToDestArrow->setMinimumHeight(KIconLoader::SizeEnormous);

        d->m_srcPreview->setAlignment(Qt::AlignCenter);
        d->m_destPreview->setAlignment(Qt::AlignCenter);
        srcToDestArrow->setAlignment(Qt::AlignCenter);

        d->m_srcPendingPreview = true;
        d->m_destPendingPreview = true;

        // widget
        d->m_srcArea = createContainerLayout(parent, d->srcItem, d->m_srcPreview);
        d->m_destArea = createContainerLayout(parent, d->destItem, d->m_destPreview);

        connect(d->m_srcArea->verticalScrollBar(), &QAbstractSlider::valueChanged, d->m_destArea->verticalScrollBar(), &QAbstractSlider::setValue);
        connect(d->m_destArea->verticalScrollBar(), &QAbstractSlider::valueChanged, d->m_srcArea->verticalScrollBar(), &QAbstractSlider::setValue);
        connect(d->m_srcArea->horizontalScrollBar(), &QAbstractSlider::valueChanged, d->m_destArea->horizontalScrollBar(), &QAbstractSlider::setValue);
        connect(d->m_destArea->horizontalScrollBar(), &QAbstractSlider::valueChanged, d->m_srcArea->horizontalScrollBar(), &QAbstractSlider::setValue);

        // create layout
        QGridLayout *gridLayout = new QGridLayout();
        pLayout->addLayout(gridLayout);

        int gridRow = 0;
        QLabel *titleLabel = new QLabel(i18n("This action will overwrite the destination."), this);
        gridLayout->addWidget(titleLabel, gridRow, 0, 1, 2);    // takes the complete first line


        gridLayout->setRowMinimumHeight(++gridRow, 15);    // spacer

        QLabel *srcTitle = createLabel(parent, i18n("Source"), true);
        gridLayout->addWidget(srcTitle, ++gridRow, 0);
        QLabel *destTitle = createLabel(parent, i18n("Destination"), true);
        gridLayout->addWidget(destTitle, gridRow, 2);

        QLabel *srcUrlLabel = createSqueezedLabel(parent, d->src.toDisplayString(QUrl::PreferLocalFile));
        srcUrlLabel->setTextFormat(Qt::PlainText);
        gridLayout->addWidget(srcUrlLabel, ++gridRow, 0);
        QLabel *destUrlLabel = createSqueezedLabel(parent, d->dest.toDisplayString(QUrl::PreferLocalFile));
        destUrlLabel->setTextFormat(Qt::PlainText);
        gridLayout->addWidget(destUrlLabel, gridRow, 2);

        gridLayout->addWidget(d->m_srcArea, ++gridRow, 0, 2, 1);

        // The labels containing previews or icons, and an arrow icon indicating
        // direction from src to dest
        const QString arrowName = qApp->isRightToLeft() ? QStringLiteral("go-previous")
                                                          : QStringLiteral("go-next");
        const QPixmap pix = QIcon::fromTheme(arrowName).pixmap(d->m_srcPreview->height());
        srcToDestArrow->setPixmap(pix);
        gridLayout->addWidget(srcToDestArrow, gridRow, 1);

        gridLayout->addWidget(d->m_destArea, gridRow, 2);

        QLabel *diffTitle = createLabel(parent, i18n("Differences"), true);
        gridLayout->addWidget(diffTitle, ++gridRow, 1);

        QLabel *srcDateLabel = createDateLabel(parent, d->srcItem);
        gridLayout->addWidget(srcDateLabel, ++gridRow, 0);

        if (mtimeDest > mtimeSrc) {
            gridLayout->addWidget(createLabel(parent, QStringLiteral("The destination is <b>more recent</b>")), gridRow, 1);
        }
        QLabel *destDateLabel = createLabel(parent, i18n("Date: %1", d->destItem.timeString(KFileItem::ModificationTime)));
        gridLayout->addWidget(destDateLabel, gridRow, 2);

        QLabel *srcSizeLabel = createSizeLabel(parent, d->srcItem);
        gridLayout->addWidget(srcSizeLabel, ++gridRow, 0);

        if (d->srcItem.size() != d->destItem.size()) {
            QString text;
            int diff = 0;
            if (d->srcItem.size() > d->destItem.size()) {
                diff = d->srcItem.size()  - d->destItem.size();
                text = i18n("The destination is <b>smaller by %1</b>", KIO::convertSize(diff));
            } else {
                diff = d->destItem.size()  - d->srcItem.size();
                text = i18n("The destination is <b>bigger by %1</b>", KIO::convertSize(diff));
            }
            gridLayout->addWidget(createLabel(parent, text), gridRow, 1);
        }
        QLabel *destSizeLabel = createLabel(parent, i18n("Size: %1", KIO::convertSize(d->destItem.size())));
        gridLayout->addWidget(destSizeLabel, gridRow, 2);

        // check files contents for local files
        if ((d->dest.isLocalFile() && !(_options & RenameDialog_DestIsDirectory))
            && (d->src.isLocalFile() && !(_options & RenameDialog_SourceIsDirectory))) {

            const CompareFilesResult CompareFilesResult = compareFiles(d->src.toLocalFile(), d->dest.toLocalFile());

            QString text;
            switch (CompareFilesResult) {
                case CompareFilesResult::Identical: text = i18n("The files are identical."); break;
                case CompareFilesResult::PartiallyIdentical: text = i18n("The files seem identical."); break;
                case CompareFilesResult::Different: text = i18n("The files are different."); break;
            }
            QLabel* filesIdenticalLabel = createLabel(this, text, true);
            if (CompareFilesResult == CompareFilesResult::PartiallyIdentical) {
                QLabel* pixmapLabel = new QLabel(this);
                pixmapLabel->setPixmap(QIcon::fromTheme(QStringLiteral("help-about")).pixmap(QSize(16,16)));
                pixmapLabel->setToolTip(
                            i18n("The files are likely to be identical: they have the same size and their contents are the same at the beginning, middle and end.")
                            );
                pixmapLabel->setCursor(Qt::WhatsThisCursor);

                QHBoxLayout* hbox = new QHBoxLayout(this);
                hbox->addWidget(filesIdenticalLabel);
                hbox->addWidget(pixmapLabel);
                gridLayout->addLayout(hbox, gridRow + 1, 1);
            } else {
                gridLayout->addWidget(filesIdenticalLabel, gridRow + 1, 1);
            }
        }

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
            pLayout->addSpacing(15);    // spacer
        }

        QLabel *lb2 = new QLabel(i18n("Rename:"), this);
        pLayout->addWidget(lb2);
    }

    QHBoxLayout *layout2 = new QHBoxLayout();
    pLayout->addLayout(layout2);

    d->m_pLineEdit = new QLineEdit(this);
    layout2->addWidget(d->m_pLineEdit);

    if (d->bRename) {
        const QString fileName = d->dest.fileName();
        d->setRenameBoxText(KIO::decodeFileName(fileName));

        connect(d->m_pLineEdit, &QLineEdit::textChanged,
                this, &RenameDialog::enableRenameButton);

        d->m_pLineEdit->setFocus();
    } else {
        d->m_pLineEdit->hide();
    }

    if (d->bSuggestNewName) {
        layout2->addWidget(d->bSuggestNewName);
        setTabOrder(d->m_pLineEdit, d->bSuggestNewName);
    }

    KSeparator *separator = new KSeparator(this);
    pLayout->addWidget(separator);

    QHBoxLayout *layout = new QHBoxLayout();
    pLayout->addLayout(layout);

    layout->addStretch(1);

    if (d->bApplyAll) {
        layout->addWidget(d->bApplyAll);
        setTabOrder(d->bApplyAll, d->bCancel);
    }

    if (d->bRename) {
        layout->addWidget(d->bRename);
        setTabOrder(d->bRename, d->bCancel);
    }

    if (d->bSkip) {
        layout->addWidget(d->bSkip);
        setTabOrder(d->bSkip, d->bCancel);
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
        QMetaObject::invokeMethod(this, "resizePanels", Qt::QueuedConnection);
    }
#endif
}

RenameDialog::~RenameDialog()
{
    delete d;
}

void RenameDialog::enableRenameButton(const QString &newDest)
{
    if (newDest != KIO::decodeFileName(d->dest.fileName()) && !newDest.isEmpty()) {
        d->bRename->setEnabled(true);
        d->bRename->setDefault(true);

        if (d->bOverwrite) {
            d->bOverwrite->setEnabled(false);   // prevent confusion (#83114)
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
    newDest.setPath(concatPaths(newDest.path(), newName));
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

    if (d->bApplyAll  && d->bApplyAll->isChecked()) {
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

#if KIOWIDGETS_BUILD_DEPRECATED_SINCE(5, 0)
QString RenameDialog::suggestName(const QUrl &baseURL, const QString &oldName)
{
    return KFileUtils::suggestName(baseURL, oldName);
}
#endif

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
    if (d->bApplyAll  && d->bApplyAll->isChecked()) {
        done(Result_AutoSkip);
    } else {
        done(Result_Skip);
    }
}

void RenameDialog::autoSkipPressed()
{
    done(Result_AutoSkip);
}

void RenameDialog::overwritePressed()
{
    if (d->bApplyAll  && d->bApplyAll->isChecked()) {
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
    if (d->bApplyAll  && d->bApplyAll->isChecked()) {
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
    Q_ASSERT(d->m_srcArea != nullptr);
    Q_ASSERT(d->m_destArea != nullptr);
    Q_ASSERT(d->m_srcPreview != nullptr);
    Q_ASSERT(d->m_destPreview != nullptr);

    // using QDesktopWidget geometry as Kephal isn't accessible here in kdelibs
    const QSize screenSize = QApplication::desktop()->availableGeometry(this).size();
    QSize halfSize = d->m_srcArea->widget()->sizeHint().expandedTo(d->m_destArea->widget()->sizeHint());
    const QSize currentSize = d->m_srcArea->size().expandedTo(d->m_destArea->size());
    const int maxHeightPossible = screenSize.height() - (size().height() - currentSize.height());
    QSize maxHalfSize = QSize(screenSize.width() / qreal(2.1), maxHeightPossible * qreal(0.9));

    if (halfSize.height() > maxHalfSize.height() &&
            halfSize.width() <= maxHalfSize.width() + d->m_srcArea->verticalScrollBar()->width()) {
        halfSize.rwidth() += d->m_srcArea->verticalScrollBar()->width();
        maxHalfSize.rwidth() += d->m_srcArea->verticalScrollBar()->width();
    }

    d->m_srcArea->setMinimumSize(halfSize.boundedTo(maxHalfSize));
    d->m_destArea->setMinimumSize(halfSize.boundedTo(maxHalfSize));

    KIO::PreviewJob *srcJob = KIO::filePreview(KFileItemList{d->srcItem},
                              QSize(d->m_srcPreview->width() * qreal(0.9), d->m_srcPreview->height()));
    srcJob->setScaleType(KIO::PreviewJob::Unscaled);

    KIO::PreviewJob *destJob = KIO::filePreview(KFileItemList{d->destItem},
                               QSize(d->m_destPreview->width() * qreal(0.9), d->m_destPreview->height()));
    destJob->setScaleType(KIO::PreviewJob::Unscaled);

    connect(srcJob, &PreviewJob::gotPreview,
            this, &RenameDialog::showSrcPreview);
    connect(destJob, &PreviewJob::gotPreview,
            this, &RenameDialog::showDestPreview);
    connect(srcJob, &PreviewJob::failed,
            this, &RenameDialog::showSrcIcon);
    connect(destJob, &PreviewJob::failed,
            this, &RenameDialog::showDestIcon);
}

QScrollArea *RenameDialog::createContainerLayout(QWidget *parent, const KFileItem &item, QLabel *preview)
{
    Q_UNUSED(item)
#if 0 // PENDING
    KFileItemList itemList;
    itemList << item;

    // KFileMetaDataWidget was deprecated for a Nepomuk widget, which is itself deprecated...
    // If we still want metadata shown, we need a plugin that fetches data from KFileMetaData::ExtractorCollection
    KFileMetaDataWidget *metaWidget =  new KFileMetaDataWidget(this);

    metaWidget->setReadOnly(true);
    metaWidget->setItems(itemList);
    // ### This is going to call resizePanels twice! Need to split it up to do preview job only once on each side
    connect(metaWidget, SIGNAL(metaDataRequestFinished(KFileItemList)), this, SLOT(resizePanels()));
#endif

    // Encapsulate the MetaDataWidgets inside a container with stretch at the bottom.
    // This prevents that the meta data widgets get vertically stretched
    // in the case where the height of m_metaDataArea > m_metaDataWidget.

    QWidget *widgetContainer = new QWidget(parent);
    QVBoxLayout *containerLayout = new QVBoxLayout(widgetContainer);

    containerLayout->setContentsMargins(0, 0, 0, 0);
    containerLayout->setSpacing(0);
    containerLayout->addWidget(preview);
    containerLayout->addStretch(1);

    QScrollArea *metaDataArea = new QScrollArea(parent);

    metaDataArea->setWidget(widgetContainer);
    metaDataArea->setWidgetResizable(true);
    metaDataArea->setFrameShape(QFrame::NoFrame);

    return metaDataArea;
}

