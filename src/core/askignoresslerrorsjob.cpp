// SPDX-FileCopyrightText: 2025 Carl Schwan <carl@carlschwan.eu>
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "askignoresslerrorsjob.h"

#include <KLocalizedString>

#include "askuseractioninterface.h"
#include "jobuidelegatefactory.h"
#include "ksslcertificatemanager.h"
#include "ksslerroruidata_p.h"

using namespace KIO;

class KIO::AskIgnoreSslErrorsJob::Private
{
public:
    KSslErrorUiData uiData;
    RulesStorage storedRules;
    bool ignored = false;
};

AskIgnoreSslErrorsJob::AskIgnoreSslErrorsJob(const KSslErrorUiData &uiData, RulesStorage storedRules, QObject *parent)
    : KJob(parent)
    , d(std::make_unique<AskIgnoreSslErrorsJob::Private>())
{
    d->uiData = uiData;
    d->storedRules = storedRules;
    d->ignored = false;
}

AskIgnoreSslErrorsJob::~AskIgnoreSslErrorsJob() = default;

void AskIgnoreSslErrorsJob::start()
{
    // copy the logic of SslUi
    const KSslErrorUiData::Private *ud = KSslErrorUiData::Private::get(&d->uiData);
    if (ud->sslErrors.isEmpty()) {
        d->ignored = true; // no errors, should not happen
        emitResult();
        return;
    }

    const QList<QSslError> fatalErrors = KSslCertificateManager::nonIgnorableErrors(ud->sslErrors);
    if (!fatalErrors.isEmpty()) {
        setError(KJob::UserDefinedError);
        setErrorText(i18nc("@info:status", "Fatal SSL error detected"));
        d->ignored = false;
        emitResult();
        return;
    }

    if (ud->certificateChain.isEmpty()) {
        // SSL without certificates is quite useless and should never happen
        setError(KJob::UserDefinedError);
        setErrorText(
            i18n("The remote host did not send any SSL certificates.\n"
                 "Aborting because the identity of the host cannot be established."));
        d->ignored = false;
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

    KIO::AskUserActionInterface *askUserIface = KIO::delegateExtension<KIO::AskUserActionInterface *>(this);
    if (!askUserIface) {
        qWarning() << "No ui delegate implementing KIO::AskUserActionInterface provided to AskIgnoreSslErrorsJob";
        setError(KJob::UserDefinedError);
        setErrorText(i18n("Unable to prompt user for SSL error exception."));
        d->ignored = false;
        emitResult();
        return;
    }

    connect(askUserIface, &AskUserActionInterface::askIgnoreSslErrorsResult, this, &AskIgnoreSslErrorsJob::slotProcessRequest);
    askUserIface->askIgnoreSslErrors(d->uiData, d->storedRules);
}

void AskIgnoreSslErrorsJob::slotProcessRequest(int result)
{
    if (result == 1) {
        // continue
        d->ignored = true;
        emitResult();
    } else if (result == 0) {
        // cancel
        d->ignored = false;
        emitResult();
    } else {
        Q_ASSERT(false); // unknow error code
    }
}
