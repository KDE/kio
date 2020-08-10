/*
    SPDX-FileCopyrightText: 2015 Martin Blumenstingl <martin.blumenstingl@googlemail.com>

    SPDX-License-Identifier: LGPL-2.0-only
*/

#ifndef DELETEJOBTEST_H
#define DELETEJOBTEST_H

#include <QObject>
#include <kio/job.h>

class DeleteJobTest : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void initTestCase();
    void deleteFileTestCase_data() const;
    void deleteFileTestCase();
    void deleteDirectoryTestCase_data() const;
    void deleteDirectoryTestCase();

private:
    void createEmptyTestFiles(const QStringList &fileNames, const QString &path) const;
};

#endif
