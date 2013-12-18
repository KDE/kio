/* This file is part of the KDE libraries
   Copyright (C) 2005 David Faure <faure@kde.org>

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public
   License version 2 as published by the Free Software Foundation.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public License
   along with this library; see the file COPYING.LIB.  If not, write to
   the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
   Boston, MA 02110-1301, USA.
*/

#ifndef PASTEDIALOG_H
#define PASTEDIALOG_H

#include <QDialog>

class KComboBox;
class QLineEdit;
class QLabel;

namespace KIO {

/**
 * @internal
 * Internal class used by paste.h. DO NOT USE.
 */
class PasteDialog : public QDialog
{
    Q_OBJECT
public:
    PasteDialog( const QString &caption, const QString &label,
                 const QString &value, const QStringList& items,
                 QWidget *parent, bool clipboard );

    QString lineEditText() const;
    int comboItem() const;
    bool clipboardChanged() const { return m_clipboardChanged; }

private Q_SLOTS:
    void slotClipboardDataChanged();

private:
    QLabel* m_label;
    QLineEdit* m_lineEdit;
    KComboBox* m_comboBox;
    bool m_clipboardChanged;

    class Private;
    Private* d;
};

} // namespace


#endif /* PASTEDIALOG_H */

