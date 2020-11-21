/*
    This file is part of the KDE libraries
    SPDX-FileCopyrightText: 2005 David Faure <faure@kde.org>

    SPDX-License-Identifier: LGPL-2.0-only
*/

#ifndef PASTEDIALOG_H
#define PASTEDIALOG_H

#include <QDialog>

class QComboBox;
class QLineEdit;
class QLabel;

namespace KIO
{

/**
 * @internal
 * Internal class used by paste.h. DO NOT USE.
 */
class PasteDialog : public QDialog
{
    Q_OBJECT
public:
    PasteDialog(const QString &caption, const QString &label,
                const QString &value, const QStringList &items,
                QWidget *parent, bool clipboard);

    QString lineEditText() const;
    int comboItem() const;
    bool clipboardChanged() const
    {
        return m_clipboardChanged;
    }

private Q_SLOTS:
    void slotClipboardDataChanged();

private:
    QLabel *m_label;
    QLineEdit *m_lineEdit;
    QComboBox *m_comboBox;
    bool m_clipboardChanged;
};

} // namespace

#endif /* PASTEDIALOG_H */

