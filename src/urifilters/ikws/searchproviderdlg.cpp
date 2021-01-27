/*
    SPDX-FileCopyrightText: 2000 Malte Starostik <malte@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "searchproviderdlg.h"
#include "searchprovider.h"

#include <QClipboard>

#include <QApplication>
#include <QVBoxLayout>
#include <KCharsets>
#include <KMessageBox>
#include <KLocalizedString>

SearchProviderDialog::SearchProviderDialog(SearchProvider *provider, QList<SearchProvider *> &providers, QWidget *parent)
    : QDialog(parent)
    , m_provider(provider)
{
    setModal(true);

    m_buttons = new QDialogButtonBox(QDialogButtonBox::Ok|QDialogButtonBox::Cancel, this);
    connect(m_buttons, &QDialogButtonBox::accepted, this, &SearchProviderDialog::accept);
    connect(m_buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    QWidget *mainWidget = new QWidget(this);
    m_dlg.setupUi(mainWidget);

    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->addWidget(mainWidget);
    layout->addWidget(m_buttons);

    m_dlg.leQuery->setMinimumWidth(m_dlg.leQuery->fontMetrics().averageCharWidth() * 50);

    connect(m_dlg.leName, &QLineEdit::textChanged, this, &SearchProviderDialog::slotChanged);
    connect(m_dlg.leQuery, &QLineEdit::textChanged, this, &SearchProviderDialog::slotChanged);
    connect(m_dlg.leShortcut, &QLineEdit::textChanged, this, &SearchProviderDialog::slotChanged);
    connect(m_dlg.leShortcut, &QLineEdit::textChanged, this, &SearchProviderDialog::shortcutsChanged);
    connect(m_dlg.pbPaste, &QAbstractButton::clicked, this, &SearchProviderDialog::pastePlaceholder);

    // Data init
    m_providers = providers;
    QStringList charsets = KCharsets::charsets()->availableEncodingNames();
    charsets.prepend(i18nc("@item:inlistbox The default character set", "Default"));
    m_dlg.cbCharset->addItems(charsets);
    if (m_provider) {
        setWindowTitle(i18n("Modify Web Shortcut"));
        m_dlg.leName->setText(m_provider->name());
        m_dlg.leQuery->setText(m_provider->query());
        m_dlg.leShortcut->setText(m_provider->keys().join(QLatin1Char(',')));
        m_dlg.cbCharset->setCurrentIndex(m_provider->charset().isEmpty() ? 0 : charsets.indexOf(m_provider->charset()));
        m_dlg.leName->setEnabled(false);
        m_dlg.leQuery->setFocus();
    } else {
        setWindowTitle(i18n("New Web Shortcut"));
        m_dlg.leName->setFocus();

        //If the clipboard contains a url copy it to the query lineedit
        const QClipboard *clipboard = QApplication::clipboard();
        const QString url = clipboard->text();

        if (!QUrl(url).host().isEmpty()) {
            m_dlg.leQuery->setText(url);
        }

        m_buttons->button(QDialogButtonBox::Ok)->setEnabled(false);
    }
}

void SearchProviderDialog::slotChanged()
{
    m_buttons->button(QDialogButtonBox::Ok)->setEnabled(!(m_dlg.leName->text().isEmpty()
                                                          || m_dlg.leShortcut->text().isEmpty()
                                                          || m_dlg.leQuery->text().isEmpty())
                                                          && m_dlg.noteLabel->text().isEmpty());
}

// Check if the user wants to assign shorthands that are already assigned to
// another search provider. Invoked on every change to the shortcuts field.
void SearchProviderDialog::shortcutsChanged(const QString &newShorthands)
{
    // Convert all spaces to commas. A shorthand should be a single word.
    // Assume that the user wanted to enter an alternative shorthand and hit
    // space instead of the comma key. Save cursor position beforehand because
    // setText() will reset it to the end, which is not what we want when
    // backspacing something in the middle.
    int savedCursorPosition = m_dlg.leShortcut->cursorPosition();
    QString normalizedShorthands = QString(newShorthands).replace(QLatin1Char(' '), QLatin1Char(','));
    m_dlg.leShortcut->setText(normalizedShorthands);
    m_dlg.leShortcut->setCursorPosition(savedCursorPosition);

    QHash<QString, const SearchProvider *> contenders;
    const QStringList normList = normalizedShorthands.split(QLatin1Char(','));
    const QSet<QString> shorthands(normList.begin(), normList.end());

    // Look at each shorthand the user entered and wade through the search
    // provider list in search of a conflicting shorthand. Do not continue
    // search after finding one, because shorthands should be assigned only
    // once. Act like data inconsistencies regarding this don't exist (should
    // probably be handled on load).
    for (const QString &shorthand : shorthands) {
        for (const SearchProvider *provider : qAsConst(m_providers)) {
            if (provider != m_provider && provider->keys().contains(shorthand)) {
                contenders.insert(shorthand, provider);
                break;
            }
        }
    }

    const int contendersSize = contenders.size();
    if (contendersSize != 0) {
        if (contendersSize == 1) {
            auto it = contenders.cbegin();
            m_dlg.noteLabel->setText(i18n("The shortcut \"%1\" is already assigned to \"%2\". Please choose a different one.", it.key(), it.value()->name()));
        } else {
            QStringList contenderList;
            contenderList.reserve(contendersSize);
            for (auto it = contenders.cbegin(); it != contenders.cend(); ++it) {
                contenderList.append(i18nc("- web short cut (e.g. gg): what it refers to (e.g. Google)", "- %1: \"%2\"", it.key(), it.value()->name()));
            }

            m_dlg.noteLabel->setText(i18n("The following shortcuts are already assigned. Please choose different ones.\n%1", contenderList.join(QLatin1Char('\n'))));
        }
        m_buttons->button(QDialogButtonBox::Ok)->setEnabled(false);
    } else {
        m_dlg.noteLabel->clear();
    }
    slotChanged();
}

void SearchProviderDialog::accept()
{
    if ((m_dlg.leQuery->text().indexOf(QLatin1String("\\{")) == -1)
        && KMessageBox::warningContinueCancel(nullptr,
                                              i18n("The Shortcut URL does not contain a \\{...} placeholder for the user query.\n"
                                                   "This means that the same page is always going to be visited, "
                                                   "regardless of the text typed in with the shortcut."),
                                              QString(), KGuiItem(i18n("Keep It"))) == KMessageBox::Cancel) {
        return;
    }

    if (!m_provider) {
        m_provider = new SearchProvider;
    }

    const QString name = m_dlg.leName->text().trimmed();
    const QString query = m_dlg.leQuery->text().trimmed();
    QStringList keys = m_dlg.leShortcut->text().trimmed().toLower().split(QLatin1Char(','), Qt::SkipEmptyParts);
    keys.removeDuplicates();// #169801. Remove duplicates...
    const QString charset = (m_dlg.cbCharset->currentIndex() ? m_dlg.cbCharset->currentText().trimmed() : QString());

    m_provider->setDirty((name != m_provider->name() || query != m_provider->query()
                          || keys != m_provider->keys() || charset != m_provider->charset()));
    m_provider->setName(name);
    m_provider->setQuery(query);
    m_provider->setKeys(keys);
    m_provider->setCharset(charset);
    QDialog::accept();
}

void SearchProviderDialog::pastePlaceholder()
{
    m_dlg.leQuery->insert(QStringLiteral("\\{@}"));
    m_dlg.leQuery->setFocus();
}
