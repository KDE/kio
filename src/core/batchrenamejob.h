/*
    This file is part of the KDE libraries
    SPDX-FileCopyrightText: 2017 Chinmoy Ranjan Pradhan <chinmoyrp65@gmail.com>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/

#ifndef BATCHRENAMEJOB_H
#define BATCHRENAMEJOB_H

#include "kiocore_export.h"
#include "job_base.h"

namespace KIO
{

class BatchRenameJobPrivate;

/**
 * @class KIO::BatchRenameJob batchrenamejob.h <KIO/BatchRenameJob>
 *
 * A KIO job that renames multiple files in one go.
 *
 * @since 5.42
 */
class KIOCORE_EXPORT BatchRenameJob : public Job
{
    Q_OBJECT

public:
    ~BatchRenameJob() override;

Q_SIGNALS:
    /**
     * Signals that a file was renamed.
     */
    void fileRenamed(const QUrl &oldUrl, const QUrl &newUrl);

protected Q_SLOTS:
    void slotResult(KJob *job) override;

protected:
    /// @internal
    BatchRenameJob(BatchRenameJobPrivate &dd);

private:
    Q_PRIVATE_SLOT(d_func(), void slotStart())
    Q_DECLARE_PRIVATE(BatchRenameJob)
};

/**
 * Renames multiple files at once.
 *
 * The new filename is obtained by replacing the characters represented by
 * @p placeHolder by the index @p index.
 * E.g. Calling batchRename({"file:///Test.jpg"}, "Test #" 12, '#') renames
 * the file to "Test 12.jpg". A connected sequence of placeholders results in
 * leading zeros. batchRename({"file:///Test.jpg"}, "Test ####" 12, '#') renames
 * the file to "Test 0012.jpg". And if no placeholder is there then @p index is
 * appended to @p newName. Calling batchRename({"file:///Test.jpg"}, "NewTest" 12, '#')
 * renames the file to "NewTest12.jpg".
 *
 * @param src The list of items to rename.
 * @param newName The base name to use in all new filenames.
 * @param index The integer(incremented after renaming a file) to add to the base name.
 * @param placeHolder The character(s) which @p index will replace.
 *
 * @return A pointer to the job handling the operation.
 * @since 5.42
 */
KIOCORE_EXPORT BatchRenameJob *batchRename(const QList<QUrl> &src, const QString &newName,
                                           int index, QChar placeHolder,
                                           JobFlags flags = DefaultFlags);

}

#endif
