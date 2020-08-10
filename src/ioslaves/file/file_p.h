/*
    SPDX-FileCopyrightText: 2017 Chinmoy Ranjan Pradhan <chinmoyrp65@gmail.com>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/

#ifndef FILE_P_H
#define FILE_P_H

#include <errno.h>

enum ActionType {
    UNKNOWN,
    CHMOD = 1,
    CHOWN,
    DEL,
    MKDIR,
    OPEN,
    OPENDIR,
    RENAME,
    RMDIR,
    SYMLINK,
    UTIME,
};

/**
 * PrivilegeOperationReturnValue encapsulates the return value from execWithElevatedPrivilege() in a convenient way.
 * Warning, this class will cast to an int that is zero on success and non-zero on failure. This unusual solution allows
 * to write kioslave code like this:

if (!dir.rmdir(itemPath)) {
    if (auto ret = execWithElevatedPrivilege(RMDIR, itemPath)) {
        if (!ret.wasCanceled()) {
            error(KIO::ERR_CANNOT_DELETE, itemPath);
        }
        return false;
    }
}
// directory successfully removed, continue with the next operation
*/
class PrivilegeOperationReturnValue
{
public:
    static PrivilegeOperationReturnValue success() { return PrivilegeOperationReturnValue{false, 0}; }
    static PrivilegeOperationReturnValue canceled() { return PrivilegeOperationReturnValue{true, ECANCELED}; }
    static PrivilegeOperationReturnValue failure(int error) { return PrivilegeOperationReturnValue{false, error}; }
    operator int() const { return m_error; }
    bool operator==(int error) const { return m_error == error; }
    bool wasCanceled() const { return m_canceled; }
private:
    PrivilegeOperationReturnValue(bool canceled, int error) : m_canceled(canceled), m_error(error) {}
    const bool m_canceled;
    const int m_error;
};

#endif
