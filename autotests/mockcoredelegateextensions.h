/*
    This file is part of the KDE libraries
    SPDX-FileCopyrightText: 2020 David Faure <faure@kde.org>
    SPDX-FileCopyrightText: 2020 Ahmad Samir <a.samirh78@gmail.com>

    SPDX-License-Identifier: LGPL-2.0-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/

#ifndef MOCKDELEGATEEXTENSIONS_H
#define MOCKDELEGATEEXTENSIONS_H

#include <askuseractioninterface.h>
#include <untrustedprogramhandlerinterface.h>
#include <QUrl>

class MockUntrustedProgramHandler : public KIO::UntrustedProgramHandlerInterface
{
public:
    explicit MockUntrustedProgramHandler(QObject *parent) : KIO::UntrustedProgramHandlerInterface(parent) {}
    void showUntrustedProgramWarning(KJob *job, const QString &programName) override {
        Q_UNUSED(job)
        m_calls << programName;
        Q_EMIT result(m_retVal);
    }

    void setRetVal(bool b) { m_retVal = b; }

    QStringList m_calls;

private:
    bool m_retVal = false;
};

class MockAskUserInterface : public KIO::AskUserActionInterface
{
public:
    explicit MockAskUserInterface(QObject *parent = nullptr)
        : KIO::AskUserActionInterface(parent)
    {
    }

    void askUserRename(KJob *job,
                       const QString &caption,
                       const QUrl &src,
                       const QUrl &dest,
                       KIO::RenameDialog_Options options,
                       KIO::filesize_t sizeSrc = KIO::filesize_t(-1),
                       KIO::filesize_t sizeDest = KIO::filesize_t(-1),
                       const QDateTime &ctimeSrc = QDateTime(),
                       const QDateTime &ctimeDest = QDateTime(),
                       const QDateTime &mtimeSrc = QDateTime(),
                       const QDateTime &mtimeDest = QDateTime()) override
    {
        Q_UNUSED(caption)
        Q_UNUSED(src)
        Q_UNUSED(dest)
        Q_UNUSED(options)
        Q_UNUSED(sizeSrc)
        Q_UNUSED(sizeDest)
        Q_UNUSED(ctimeSrc)
        Q_UNUSED(ctimeDest)
        Q_UNUSED(mtimeSrc)
        Q_UNUSED(mtimeDest)

        ++m_askUserRenameCalled;
        Q_EMIT askUserRenameResult(m_renameResult, m_newDestUrl, job);
    }

    void askUserSkip(KJob *job,
                     KIO::SkipDialog_Options options,
                     const QString &error_text) override
    {
        Q_UNUSED(options)
        Q_UNUSED(error_text)

        ++m_askUserSkipCalled;
        Q_EMIT askUserSkipResult(m_skipResult, job);
    }

    void askUserDelete(const QList<QUrl> &urls,
                       DeletionType deletionType,
                       ConfirmationType confirmationType,
                       QWidget *parent = nullptr) override
    {
        Q_UNUSED(confirmationType)

        ++m_askUserDeleteCalled;
        Q_EMIT askUserDeleteResult(m_deleteResult, urls, deletionType, parent);
    }

    void requestUserMessageBox(MessageDialogType type,
                               const QString &text,
                               const QString &caption,
                               const QString &buttonYes,
                               const QString &buttonNo,
                               const QString &iconYes = QString(),
                               const QString &iconNo = QString(),
                               const QString &dontAskAgainName = QString(),
                               const QString &details = QString(),
                               const KIO::MetaData &metaData = KIO::MetaData(),
                               QWidget *parent = nullptr) override
    {
        Q_UNUSED(type)
        Q_UNUSED(text)
        Q_UNUSED(caption)
        Q_UNUSED(buttonYes)
        Q_UNUSED(buttonNo)
        Q_UNUSED(iconYes)
        Q_UNUSED(iconNo)
        Q_UNUSED(dontAskAgainName)
        Q_UNUSED(details)
        Q_UNUSED(metaData)
        Q_UNUSED(parent)
    }

    void clear()
    {
        m_askUserRenameCalled = 0;
        m_askUserSkipCalled = 0;
        m_askUserDeleteCalled = 0;
        m_messageBoxCalled = 0;
    }

    // yeah, public, for get and reset.
    int m_askUserRenameCalled = 0;
    int m_askUserSkipCalled = 0;
    int m_askUserDeleteCalled = 0;
    int m_messageBoxCalled = 0;

    KIO::RenameDialog_Result m_renameResult = KIO::Result_Skip;
    KIO::SkipDialog_Result m_skipResult = KIO::Result_Skip;
    bool m_deleteResult = false;
    int m_messageBoxResult = 0;
    QUrl m_newDestUrl;
};

#endif // MOCKDELEGATEEXTENSIONS_H

