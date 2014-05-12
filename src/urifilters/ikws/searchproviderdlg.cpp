/*
 * Copyright (c) 2000 Malte Starostik <malte@kde.org>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#include "searchproviderdlg.h"
#include "searchprovider.h"

#include <QClipboard>

#include <qapplication.h>
#include <QVBoxLayout>
#include <kcharsets.h>
#include <kmessagebox.h>
#include <klocalizedstring.h>

SearchProviderDialog::SearchProviderDialog(SearchProvider *provider, QList<SearchProvider*> &providers, QWidget *parent)
    : QDialog( parent )
    , m_provider(provider)
{
    setModal(true);

    m_buttons = new QDialogButtonBox(QDialogButtonBox::Ok|QDialogButtonBox::Cancel, this);
    connect(m_buttons, SIGNAL(accepted()), SLOT(accept()));
    connect(m_buttons, SIGNAL(rejected()), SLOT(reject()));

    QWidget* mainWidget = new QWidget(this);
    m_dlg.setupUi(mainWidget);

    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->addWidget(mainWidget);
    layout->addWidget(m_buttons);

    m_dlg.leQuery->setMinimumWidth(m_dlg.leQuery->fontMetrics().averageCharWidth() * 50);

    connect(m_dlg.leName,      SIGNAL(textChanged(QString)), SLOT(slotChanged()));
    connect(m_dlg.leQuery,     SIGNAL(textChanged(QString)), SLOT(slotChanged()));
    connect(m_dlg.leShortcut,  SIGNAL(textChanged(QString)), SLOT(slotChanged()));
    connect(m_dlg.leShortcut,  SIGNAL(textChanged(QString)), SLOT(shortcutsChanged(QString)));
    connect(m_dlg.pbPaste,     SIGNAL(clicked()),            SLOT(pastePlaceholder()));

    // Data init
    m_providers = providers;
    QStringList charsets = KCharsets::charsets()->availableEncodingNames();
    charsets.prepend(i18nc("@item:inlistbox The default character set", "Default"));
    m_dlg.cbCharset->addItems(charsets);
    if (m_provider)
    {
        setWindowTitle(i18n("Modify Web Shortcut"));
        m_dlg.leName->setText(m_provider->name());
        m_dlg.leQuery->setText(m_provider->query());
        m_dlg.leShortcut->setText(m_provider->keys().join(","));
        m_dlg.cbCharset->setCurrentIndex(m_provider->charset().isEmpty() ? 0 : charsets.indexOf(m_provider->charset()));
        m_dlg.leName->setEnabled(false);
        m_dlg.leQuery->setFocus();
    }
    else
    {
        setWindowTitle(i18n("New Web Shortcut"));
        m_dlg.leName->setFocus();

        //If the clipboard contains a url copy it to the query lineedit
        const QClipboard *clipboard = QApplication::clipboard();
        const QString url = clipboard->text();

        if (!QUrl(url).host().isEmpty())
            m_dlg.leQuery->setText(url);

        m_buttons->button(QDialogButtonBox::Ok)->setEnabled(false);
    }
}

void SearchProviderDialog::slotChanged()
{
    m_buttons->button(QDialogButtonBox::Ok)->setEnabled(!(m_dlg.leName->text().isEmpty()
                       || m_dlg.leShortcut->text().isEmpty()
                       || m_dlg.leQuery->text().isEmpty()));
}

// Check if the user wants to assign shorthands that are already assigned to
// another search provider. Invoked on every change to the shortcuts field.
void SearchProviderDialog::shortcutsChanged(const QString& newShorthands) {
    // Convert all spaces to commas. A shorthand should be a single word.
    // Assume that the user wanted to enter an alternative shorthand and hit
    // space instead of the comma key. Save cursor position beforehand because
    // setText() will reset it to the end, which is not what we want when
    // backspacing something in the middle.
    int savedCursorPosition = m_dlg.leShortcut->cursorPosition();
    QString normalizedShorthands = QString(newShorthands).replace(' ', ',');
    m_dlg.leShortcut->setText(normalizedShorthands);
    m_dlg.leShortcut->setCursorPosition(savedCursorPosition);

    QHash<QString, const SearchProvider*> contenders;
    QSet<QString> shorthands = normalizedShorthands.split(',').toSet();

    // Look at each shorthand the user entered and wade through the search
    // provider list in search of a conflicting shorthand. Do not continue
    // search after finding one, because shorthands should be assigned only
    // once. Act like data inconsistencies regarding this don't exist (should
    // probably be handled on load).
    Q_FOREACH (const QString &shorthand, shorthands) {
        Q_FOREACH (const SearchProvider* provider, m_providers) {
            if (provider != m_provider && provider->keys().contains(shorthand)) {
                contenders.insert(shorthand, provider);
                break;
            }
        }
    }

    if (!contenders.isEmpty()) {
        if (contenders.size() == 1) {
            m_dlg.noteLabel->setText(i18n("The shortcut \"%1\" is already assigned to \"%2\". Please choose a different one.", contenders.keys().at(0), contenders.values().at(0)->name()));
        } else {
            QStringList contenderList;
            QHash<QString, const SearchProvider*>::const_iterator i = contenders.constBegin();
            while (i != contenders.constEnd()) {
                contenderList.append(i18nc("- web short cut (e.g. gg): what it refers to (e.g. Google)", "- %1: \"%2\"", i.key(), i.value()->name()));
                ++i;
            }

            m_dlg.noteLabel->setText(i18n("The following shortcuts are already assigned. Please choose different ones.\n%1", contenderList.join("\n")));
        }
        m_buttons->button(QDialogButtonBox::Ok)->setEnabled(false);
    } else {
        m_dlg.noteLabel->clear();
    }
}

void SearchProviderDialog::slotAcceptClicked()
{
    if ((m_dlg.leQuery->text().indexOf("\\{") == -1)
        && KMessageBox::warningContinueCancel(0,
            i18n("The Shortcut URL does not contain a \\{...} placeholder for the user query.\n"
                    "This means that the same page is always going to be visited, "
                    "regardless of the text typed in with the shortcut."),
            QString(), KGuiItem(i18n("Keep It"))) == KMessageBox::Cancel) {
        return;
    }

    if (!m_provider)
        m_provider = new SearchProvider;

    const QString name = m_dlg.leName->text().trimmed();
    const QString query = m_dlg.leQuery->text().trimmed();
    QStringList keys = m_dlg.leShortcut->text().trimmed().toLower().split(',', QString::SkipEmptyParts);
    keys.removeDuplicates();// #169801. Remove duplicates...
    const QString charset = (m_dlg.cbCharset->currentIndex() ? m_dlg.cbCharset->currentText().trimmed() : QString());

    m_provider->setDirty((name != m_provider->name() || query != m_provider->query() ||
                            keys != m_provider->keys() || charset != m_provider->charset()));
    m_provider->setName(name);
    m_provider->setQuery(query);
    m_provider->setKeys(keys);
    m_provider->setCharset(charset);
    QDialog::accept();
}

void SearchProviderDialog::pastePlaceholder() {
    m_dlg.leQuery->insert("\\{@}");
    m_dlg.leQuery->setFocus();
}

#include "searchproviderdlg.moc"
