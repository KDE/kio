/*
    This file is part of the KDE libraries
    SPDX-FileCopyrightText: 2020 Ahmad Samir <a.samirh78@gmail.com>

    SPDX-License-Identifier: LGPL-2.0-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/

#ifndef OPENOREXECUTEFILEINTERFACE_H
#define OPENOREXECUTEFILEINTERFACE_H

#include <QObject>
#include <kiogui_export.h>

class KJob;

namespace KIO {
class OpenOrExecuteFileInterfacePrivate;

/**
 * @class OpenOrExecuteFileInterface openorexecutefileinterface.h <KIO/OpenOrExecuteFileInterface>
 * @brief The OpenOrExecuteFileInterface class allows OpenUrlJob to ask
 * the user about how to handle various types of executable files, basically
 * whether to run/execute the file, or in the case of text-based ones (shell
 * scripts and .desktop files) open them as text.
 *
 * This extension mechanism for jobs is similar to KIO::JobUiDelegateExtension,
 * OpenWithHandlerInterface and UntrustedProgramHandlerInterface.
 *
 * @since 5.73
 */
class KIOGUI_EXPORT OpenOrExecuteFileInterface : public QObject
{
    Q_OBJECT
protected:
    /**
     * Constructor
     */
    explicit OpenOrExecuteFileInterface(QObject *parent = nullptr);

    /**
     * Destructor
     */
    ~OpenOrExecuteFileInterface() override;

public:
    /**
     * Show a dialog to ask the user how to handle various types of executable
     * files, basically whether to run/execute the file, or in the case of text-based
     * ones (shell scripts and .desktop files) open them as text.
     *
     * @param job the job calling this. This is useful if you need to
     * get any of its properties
     * @param mimetype the MIME type of the file being handled
     *
     * Implementations of this method must emit either executeFile or canceled.
     *
     * The default implementation in this base class simply emits canceled().
     * Any application using KIO::JobUiDelegate (from KIOWidgets) will benefit
     * from an automatically registered subclass which implements this method,
     * which in turn uses ExecutableFileOpenDialog (from KIOWidgets).
     */
    virtual void promptUserOpenOrExecute(KJob *job, const QString &mimetype);

Q_SIGNALS:
    /**
     * Emitted by promptUserOpenOrExecute() once the user chooses an action.
     * @param enable \c true if the user selected to execute/run the file or
     * \c false if the user selected to open the file as text (the latter is
     * only valid for shell scripts and .desktop files)
     */
    void executeFile(bool enable);

    /**
     * Emitted by promptUserOpenOrExecute() if user selects cancel.
     */
    void canceled();

private:
    QScopedPointer<OpenOrExecuteFileInterfacePrivate> d;
};

}

#endif // OPENOREXECUTEFILEINTERFACE_H
