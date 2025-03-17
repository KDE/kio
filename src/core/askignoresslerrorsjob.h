// SPDX-FileCopyrightText: 2025 Carl Schwan <carl@carlschwan.eu>
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "kiocore_export.h"
#include <KJob>
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

    explicit AskIgnoreSslErrorsJob(const KSslErrorUiData &uiData, RulesStorage storedRules = RecallAndStoreRules, QObject *parent = nullptr);
    ~AskIgnoreSslErrorsJob() override;

    /**
     * Returns whether the user decided to ignore or not the SSL errors.
     */
    bool ignored() const;

    void start() override;

private:
    class Private;
    std::unique_ptr<Private> d;
};

};
