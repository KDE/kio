/*
    This file is part of the KDE project
    SPDX-FileCopyrightText: 2009 Andreas Hartmetz <ahartmetz@gmail.com>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#include "sslui.h"

#include "askignoresslerrorsjob.h"
#include "jobuidelegatefactory.h"
#include <KLocalizedString>
#include <KMessageBox>

bool KIO::SslUi::askIgnoreSslErrors(const KSslErrorUiData &uiData, RulesStorage storedRules)
{
    KIO::AskIgnoreSslErrorsJob::RulesStorages rulesStorage;
    if (storedRules & StoreRules) {
        rulesStorage |= KIO::AskIgnoreSslErrorsJob::RulesStorage::StoreRules;
    }
    if (storedRules & RecallRules) {
        rulesStorage |= KIO::AskIgnoreSslErrorsJob::RulesStorage::RecallRules;
    }
    auto job = new KIO::AskIgnoreSslErrorsJob(uiData, rulesStorage);
    job->setUiDelegate(KIO::createDefaultJobUiDelegate());
    job->exec();

    if (job->error() != KJob::NoError) {
        KMessageBox::error(nullptr, job->errorString());
    }

    return job->ignored();
}
