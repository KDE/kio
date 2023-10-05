// SPDX-License-Identifier: LGPL-2.0-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
// SPDX-FileCopyrightText: 2023 Harald Sitter <sitter@kde.org>

#pragma once

#include "kiocore_export.h"

#include <QObject>

namespace KIO
{
class BuildSycocaInterfacePrivate;

/**
 * @brief Used by KIO::Job to display a progress dialog for while kbuildsycoca runs
 *
 * By default this is only implemented by a widgets provider inside KIOWidgets.
 * Would-be alternative implementations should derive this class and instantiate
 * it through KIO::JobUiDelegateFactoryV2.
 *
 * @since 6.0
 */
class KIOCORE_EXPORT BuildSycocaInterface : public QObject
{
    Q_OBJECT
public:
    explicit BuildSycocaInterface(QObject *parent = nullptr);
    ~BuildSycocaInterface() override;
    Q_DISABLE_COPY_MOVE(BuildSycocaInterface)

    /// Show progress information (e.g. open a dialog)
    virtual void showProgress();
    /// Hide progress information (e.g. close a dialog)
    virtual void hideProgress();

Q_SIGNALS:
    /// Emit when the progress visualization was canceled (e.g. a dialog's cancel button). This aborts the kbuildsycoca run.
    void canceled();

private:
    QScopedPointer<BuildSycocaInterfacePrivate> d;
};

} // namespace KIO
