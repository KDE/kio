// SPDX-License-Identifier: LGPL-2.0-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
// SPDX-FileCopyrightText: 2023 Harald Sitter <sitter@kde.org>

#pragma once

#include "buildsycocainterface.h"

class QProgressDialog;

namespace KIO
{

/**
 * @brief QtWidgets based sycoca build dialog
 * This provides a widget based implementation to be shown when kbuildsycoca is running.
 * This is purely internal and only gets used by JobUiDelegate.
 * Internally this uses KBuildSycocaProgressDialog to produce the dialog.
 */
class WidgetsBuildSycocaHandler : public BuildSycocaInterface
{
    Q_OBJECT
public:
    using BuildSycocaInterface::BuildSycocaInterface;

    /// Show progress dialog
    void showProgress() override;
    /// Hide progress dialog
    void hideProgress() override;

    /// Sets the parent window (if any)
    void setWindow(QWidget *window);

private:
    QProgressDialog *m_dialog = nullptr;
    QWidget *m_parentWidget = nullptr;
};

} // namespace KIO
