/*
    This file is part of the KDE libraries
    SPDX-FileCopyrightText: 2026 Ramil Nurmanov <ramil2004nur@gmail.com>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#ifndef KIO_RENAMEFILEWARNING_H
#define KIO_RENAMEFILEWARNING_H

#include "kiowidgets_export.h"

#include <KFileItem>

#include <QObject>

class QWidget;

namespace KIO
{

/*!
 * \class KIO::RenameFileWarning
 * \inheaderfile KIO/RenameFileWarning
 * \inmodule KIOWidgets
 *
 * \brief Checks whether renaming a file/folder requires user confirmation
 * and shows the appropriate dialog if so.
 *
 * \since 6.27
 */
class KIOWIDGETS_EXPORT RenameFileWarning : public QObject
{
    Q_OBJECT

public:
    /*!
     * \a hiddenFilesVisible suppresses the hidden-file warning when the view
     * already shows hidden files.
     */
    explicit RenameFileWarning(const KFileItem &item, const QString &newName, QWidget *parent = nullptr, bool hiddenFilesVisible = false);

    ~RenameFileWarning() override;

    /*!
     * Connect to result() before calling this: it may be emitted synchronously.
     */
    void exec();

Q_SIGNALS:
    /*!
     * \a accepted is \c true if the rename should proceed.
     */
    void result(bool accepted);

private:
    const KFileItem m_item;
    const QString m_newName;
    QWidget *const m_parent;
    const bool m_hiddenFilesVisible;
};

} // namespace KIO

#endif // KIO_RENAMEFILEWARNING_H
