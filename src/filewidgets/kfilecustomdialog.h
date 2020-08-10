/*
    This file is part of the KDE libraries
    SPDX-FileCopyrightText: 2017 Klarälvdalens Datakonsult AB, a KDAB Group company <info@kdab.com>
    Work sponsored by the LiMux project of the city of Munich

    SPDX-License-Identifier: LGPL-2.0-only
*/

#ifndef KFILECUSTOMDIALOG_H
#define KFILECUSTOMDIALOG_H

#include "kiofilewidgets_export.h"
#include "kfilewidget.h"
#include <QDialog>
class KFileWidget;
class KFileCustomDialogPrivate;

/**
  * This class implement a custom file dialog.
  * It uses a KFileWidget and allows the application to provide a custom widget.
  * @since 5.42
  */
class KIOFILEWIDGETS_EXPORT KFileCustomDialog : public QDialog
{
    Q_OBJECT
public:
    /**
     * Constructs a custom file dialog
     */
    explicit KFileCustomDialog(QWidget *parent = nullptr);

    /**
     * Constructs a custom file dialog
     * @param startDir see the KFileWidget constructor for documentation
     * @since 5.67
     */
    explicit KFileCustomDialog(const QUrl &startDir, QWidget *parent = nullptr);

    ~KFileCustomDialog() override;

    /**
     * Sets the directory to view.
     *
     * @param url URL to show.
     */
    void setUrl(const QUrl &url);

    /**
     * Set a custom widget that should be added to the file dialog.
     * @param widget A widget, or a widget of widgets, for displaying custom
     *               data in the file widget. This can be used, for example, to
     *               display a check box with the caption "Open as read-only".
     *               When creating this widget, you don't need to specify a parent,
     *               since the widget's parent will be set automatically by KFileWidget.
     */
    void setCustomWidget(QWidget *widget);

    /**
     * @brief fileWidget
     * @return the filewidget used inside this dialog
     */
    KFileWidget *fileWidget() const;

    /**
     * Sets the operational mode of the filedialog to @p Saving, @p Opening
     * or @p Other. This will set some flags that are specific to loading
     * or saving files. E.g. setKeepLocation() makes mostly sense for
     * a save-as dialog. So setOperationMode( KFileWidget::Saving ); sets
     * setKeepLocation for example.
     *
     * The mode @p Saving, together with a default filter set via
     * setMimeFilter() will make the filter combobox read-only.
     *
     * The default mode is @p Opening.
     *
     * Call this method right after instantiating KFileWidget.
     *
     * @see operationMode
     * @see KFileWidget::OperationMode
     */
    void setOperationMode(KFileWidget::OperationMode op);

public Q_SLOTS:
    void accept() override;

private:
    KFileCustomDialogPrivate *const d;
};

#endif // KFILECUSTOMDIALOG_H
