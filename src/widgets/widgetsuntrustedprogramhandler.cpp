/*
    This file is part of the KDE libraries
    SPDX-FileCopyrightText: 2020 David Faure <faure@kde.org>

    SPDX-License-Identifier: LGPL-2.0-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/

#include "widgetsuntrustedprogramhandler.h"

#include <KIconLoader>
#include <KJobWidgets>
#include <KLocalizedString>
#include <KStandardGuiItem>

#include <QApplication>
#include <QDialog>
#include <QDialogButtonBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScreen>
#include <QStyle>
#include <QVBoxLayout>

KIO::WidgetsUntrustedProgramHandler::WidgetsUntrustedProgramHandler(QObject *parent)
    : KIO::UntrustedProgramHandlerInterface(parent), d(nullptr)
{
}

KIO::WidgetsUntrustedProgramHandler::~WidgetsUntrustedProgramHandler()
{
}

// Simple QDialog that resizes the given text edit after being shown to more
// or less fit the enclosed text.
class SecureMessageDialog : public QDialog
{
    Q_OBJECT
public:
    SecureMessageDialog(QWidget *parent) : QDialog(parent), m_textEdit(nullptr)
    {
    }

    void setTextEdit(QPlainTextEdit *textEdit)
    {
        m_textEdit = textEdit;
    }

protected:
    void showEvent(QShowEvent *e) override
    {
        if (e->spontaneous()) {
            return;
        }

        // Now that we're shown, use our width to calculate a good
        // bounding box for the text, and resize m_textEdit appropriately.
        QDialog::showEvent(e);

        if (!m_textEdit) {
            return;
        }

        QSize fudge(20, 24); // About what it sounds like :-/

        // Form rect with a lot of height for bounding.  Use no more than
        // 5 lines.
        QRect curRect(m_textEdit->rect());
        QFontMetrics metrics(fontMetrics());
        curRect.setHeight(5 * metrics.lineSpacing());
        curRect.setWidth(qMax(curRect.width(), 300)); // At least 300 pixels ok?

        QString text(m_textEdit->toPlainText());
        curRect = metrics.boundingRect(curRect, Qt::TextWordWrap | Qt::TextSingleLine, text);

        // Scroll bars interfere.  If we don't think there's enough room, enable
        // the vertical scrollbar however.
        m_textEdit->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        if (curRect.height() < m_textEdit->height()) { // then we've got room
            m_textEdit->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
            m_textEdit->setMaximumHeight(curRect.height() + fudge.height());
        }

        m_textEdit->setMinimumSize(curRect.size() + fudge);
        m_textEdit->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Minimum);
    }

private:
    QPlainTextEdit *m_textEdit;
};


QDialog *KIO::WidgetsUntrustedProgramHandler::createDialog(QWidget *parentWidget, const QString &programName)
{
    SecureMessageDialog *baseDialog = new SecureMessageDialog(parentWidget);
    baseDialog->setWindowTitle(i18nc("Warning about executing unknown program", "Warning"));

    QVBoxLayout *topLayout = new QVBoxLayout(baseDialog);

    // Dialog will have explanatory text with a disabled lineedit with the
    // Exec= to make it visually distinct.
    QWidget *baseWidget = new QWidget(baseDialog);
    QHBoxLayout *mainLayout = new QHBoxLayout(baseWidget);

    QLabel *iconLabel = new QLabel(baseWidget);
    const QIcon icon = baseDialog->style()->standardIcon(QStyle::SP_MessageBoxWarning, nullptr, baseDialog);
    const QPixmap warningIcon(icon.pixmap(KIconLoader::SizeHuge));
    mainLayout->addWidget(iconLabel);
    iconLabel->setPixmap(warningIcon);

    QVBoxLayout *contentLayout = new QVBoxLayout;
    QString warningMessage = i18nc("program name follows in a line edit below",
                                   "This will start the program:");

    QLabel *message = new QLabel(warningMessage, baseWidget);
    contentLayout->addWidget(message);

    QPlainTextEdit *textEdit = new QPlainTextEdit(baseWidget);
    textEdit->setPlainText(programName);
    textEdit->setReadOnly(true);
    contentLayout->addWidget(textEdit);

    QLabel *footerLabel = new QLabel(i18n("If you do not trust this program, click Cancel"));
    contentLayout->addWidget(footerLabel);
    contentLayout->addStretch(0); // Don't allow the text edit to expand

    mainLayout->addLayout(contentLayout);

    topLayout->addWidget(baseWidget);
    baseDialog->setTextEdit(textEdit);

    QDialogButtonBox *buttonBox = new QDialogButtonBox(baseDialog);
    buttonBox->setStandardButtons(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    KGuiItem::assign(buttonBox->button(QDialogButtonBox::Ok), KStandardGuiItem::cont());
    buttonBox->button(QDialogButtonBox::Cancel)->setDefault(true);
    buttonBox->button(QDialogButtonBox::Cancel)->setFocus();
    QObject::connect(buttonBox, &QDialogButtonBox::accepted, baseDialog, &QDialog::accept);
    QObject::connect(buttonBox, &QDialogButtonBox::rejected, baseDialog, &QDialog::reject);
    topLayout->addWidget(buttonBox);

    // Constrain maximum size.  Minimum size set in
    // the dialog's show event.
    const QSize screenSize = baseDialog->screen()->size();
    baseDialog->resize(screenSize.width() / 4, 50);
    baseDialog->setMaximumHeight(screenSize.height() / 3);
    baseDialog->setMaximumWidth(screenSize.width() / 10 * 8);

    baseDialog->setAttribute(Qt::WA_DeleteOnClose);
    return baseDialog;
}

void KIO::WidgetsUntrustedProgramHandler::showUntrustedProgramWarning(KJob *job, const QString &programName)
{
    QWidget *parentWidget = job ? KJobWidgets::window(job) : qApp->activeWindow();
    QDialog *dialog = createDialog(parentWidget, programName);
    connect(dialog, &QDialog::accepted, this, [this]() { Q_EMIT result(true); });
    connect(dialog, &QDialog::rejected, this, [this]() { Q_EMIT result(false); });
    dialog->show();
}

bool KIO::WidgetsUntrustedProgramHandler::execUntrustedProgramWarning(QWidget *window, const QString &programName)
{
    QDialog *dialog = createDialog(window, programName);
    return dialog->exec() == QDialog::Accepted;
}


#include "widgetsuntrustedprogramhandler.moc"
