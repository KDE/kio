// SPDX-FileCopyrightText: 2025 Carl Schwan <carl@carlschwan.eu>
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "kiocore_export.h"
#include <KJob>
#include <QFlags>
#include <ksslerroruidata.h>

namespace KIO
{

class KIOCORE_EXPORT AskIgnoreSslErrorsJob : public KJob
{
    Q_OBJECT
public:
    /** Error rule storage behavior. */
    enum RulesStorage {
        RecallRules = 1, ///< apply stored certificate rules (typically ignored errors)
        StoreRules = 2, ///< make new ignore rules from the user's choice and store them
        RecallAndStoreRules = 3, ///< apply stored rules and store new rules
    };
    Q_ENUM(RulesStorage)
    Q_DECLARE_FLAGS(RulesStorages, RulesStorage)

    explicit AskIgnoreSslErrorsJob(const KSslErrorUiData &uiData, RulesStorages storedRules = RecallAndStoreRules, QObject *parent = nullptr);
    ~AskIgnoreSslErrorsJob() override;

    /**
     * Returns whether the user decided to ignore or not the SSL errors.
     */
    [[nodiscard]] bool ignored() const;

    void start() override;

private:
    KIOCORE_NO_EXPORT void slotProcessRequest(int result);

    class Private;
    std::unique_ptr<Private> d;
};

};

Q_DECLARE_OPERATORS_FOR_FLAGS(KIO::AskIgnoreSslErrorsJob::RulesStorages)
