/*
    This file is part of the KDE libraries
    SPDX-FileCopyrightText: 2000 David Smith <dsmith@algonet.se>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#ifndef KSHELLCOMPLETION_H
#define KSHELLCOMPLETION_H

#include <QString>
#include <QStringList>

#include "kurlcompletion.h"

class KShellCompletionPrivate;

/**
 * @class KShellCompletion kshellcompletion.h <KShellCompletion>
 *
 * This class does shell-like completion of file names.
 * A string passed to makeCompletion() will be interpreted as a shell
 * command line. Completion will be done on the last argument on the line.
 * Returned matches consist of the first arguments (uncompleted) plus the
 * completed last argument.
 *
 * @short Shell-like completion of file names
 * @author David Smith <dsmith@algonet.se>
 */
class KIOWIDGETS_EXPORT KShellCompletion : public KUrlCompletion
{
    Q_OBJECT

public:
    /**
     * Constructs a KShellCompletion object.
     */
    KShellCompletion();
    ~KShellCompletion();

    /**
     * Finds completions to the given text.
     * The first match is returned and emitted in the signal match().
     * @param text the text to complete
     * @return the first match, or QString() if not found
     */
    QString makeCompletion(const QString &text) override;

protected:
    // Called by KCompletion
    void postProcessMatch(QString *match) const override;
    void postProcessMatches(QStringList *matches) const override;
    void postProcessMatches(KCompletionMatches *matches) const override;

private:
    KShellCompletionPrivate *const d;
};

#endif // KSHELLCOMPLETION_H
