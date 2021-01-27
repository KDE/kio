/*
    This file is part of the KDE libraries
    SPDX-FileCopyrightText: 2001 Frerich Raabe <raabe@kde.org>
    SPDX-FileCopyrightText: 2003 Carsten Pfeiffer <pfeiffer@kde.org>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#ifndef __KPREVIEWWIDGETBASE_H__
#define __KPREVIEWWIDGETBASE_H__

#include <QWidget>

#include "kiofilewidgets_export.h"

class QUrl;

/**
 * @class KPreviewWidgetBase kpreviewwidgetbase.h <KPreviewWidgetBase>
 *
 * Abstract baseclass for all preview widgets which shall be used via
 * KFileDialog::setPreviewWidget(const KPreviewWidgetBase *).
 * Ownership will be transferred to KFileDialog, so you have to create
 * the preview with "new" and let KFileDialog delete it.
 *
 * Just derive your custom preview widget from KPreviewWidgetBase and implement
 * all the pure virtual methods. The slot showPreview(const QUrl &) is called
 * every time the file selection changes.
 *
 * @short Abstract baseclass for all preview widgets.
 * @author Frerich Raabe <raabe@kde.org>
 */
class KIOFILEWIDGETS_EXPORT KPreviewWidgetBase : public QWidget
{
    Q_OBJECT

public:
    /**
     * Constructor. Construct the user interface of your preview widget here
     * and pass the KFileDialog this preview widget is going to be used in as
     * the parent.
     *
     * @param parent The KFileDialog this preview widget is going to be used in
     */
    explicit KPreviewWidgetBase(QWidget *parent);
    ~KPreviewWidgetBase();

public Q_SLOTS:
    /**
     * This slot is called every time the user selects another file in the
     * file dialog. Implement the stuff necessary to reflect the change here.
     *
     * @param url The URL of the currently selected file.
     */
    virtual void showPreview(const QUrl &url) = 0;

    /**
     * Reimplement this to clear the preview. This is called when e.g. the
     * selection is cleared or when multiple selections exist, or the directory
     * is changed.
     */
    virtual void clearPreview() = 0;

    // TODO KF6: make it a public method, it's not a slot
    QStringList supportedMimeTypes() const; // clazy:exclude=const-signal-or-slot

protected:
    void setSupportedMimeTypes(const QStringList &mimeTypes);

private:
    class KPreviewWidgetBasePrivate;
    KPreviewWidgetBasePrivate *const d;

    Q_DISABLE_COPY(KPreviewWidgetBase)
};

#endif
