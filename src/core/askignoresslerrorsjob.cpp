// SPDX-FileCopyrightText: 2025 Carl Schwan <carl@carlschwan.eu>
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "askignoresslerrorsjob.h"
#include <KLocalizedString>
#include <ksslcertificatemanager.h>
#include <ksslerroruidata_p.h>

using namespace KIO;

class KIO::AskIgnoreSslErrorsJob::Private
{
public:
    KSslErrorUiData uiData;
    RulesStorage storedRules;
};

AskIgnoreSslErrorsJob::AskIgnoreSslErrorsJob(const KSslErrorUiData &uiData, RulesStorage storedRules, QObject *parent)
    : KJob(parent)
    , d(std::make_unique<AskIgnoreSslErrorsJob::Private>(uiData, storedRules))
{
}

AskIgnoreSslErrorsJob::~AskIgnoreSslErrorsJob() = default;

void AskIgnoreSslErrorsJob::start()
{
    const KSslErrorUiData::Private *ud = KSslErrorUiData::Private::get(&d->uiData);
    if (ud->sslErrors.isEmpty()) {
        emitResult();
        return;
    }

    const QList<QSslError> fatalErrors = KSslCertificateManager::nonIgnorableErrors(ud->sslErrors);
    if (!fatalErrors.isEmpty()) {
        setError(KJob::UserDefinedError);
        setErrorText(i18nc("@info:status", "Fatal SSL error detected"));
        emitResult();
        return;
    }

    if (ud->certificateChain.isEmpty()) {
        // SSL without certificates is quite useless and should never happen
        setError(KJob::UserDefinedError);
        setErrorText(
            i18n("The remote host did not send any SSL certificates.\n"
                 "Aborting because the identity of the host cannot be established."));
        emitResult();
        return;
    }

    KSslCertificateManager *const cm = KSslCertificateManager::self();
    KSslCertificateRule rule(ud->certificateChain.first(), ud->host);
    if (d->storedRules & RecallRules) {
        rule = cm->rule(ud->certificateChain.first(), ud->host);
        // remove previously seen and acknowledged errors
        const QList<QSslError> remainingErrors = rule.filterErrors(ud->sslErrors);
        if (remainingErrors.isEmpty()) {
            // qDebug() << "Error list empty after removing errors to be ignored. Continuing.";
            emitResult();
            return;
        }
    }

    // TODO ui delegate
}
