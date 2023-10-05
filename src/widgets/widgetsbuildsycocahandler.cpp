// SPDX-License-Identifier: LGPL-2.0-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
// SPDX-FileCopyrightText: 2023 Harald Sitter <sitter@kde.org>

#include "widgetsbuildsycocahandler.h"

#include <KLocalizedString>

#include "kbuildsycocaprogressdialog.h"

namespace KIO
{

void WidgetsBuildSycocaHandler::showProgress()
{
    m_dialog = new KBuildSycocaProgressDialog(m_parentWidget, i18n("Updating System Configuration"), i18n("Updating system configuration."));
    connect(m_dialog, &QProgressDialog::canceled, this, &WidgetsBuildSycocaHandler::canceled);
    m_dialog->show();
}

void WidgetsBuildSycocaHandler::hideProgress()
{
    m_dialog->close();
    m_dialog->deleteLater();
    m_dialog = nullptr;
}

void WidgetsBuildSycocaHandler::setWindow(QWidget *window)
{
    m_parentWidget = window;
}

} // namespace KIO
