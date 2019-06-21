/***
    Copyright (C) 2017 by Chinmoy Ranjan Pradhan <chinmoyrp65@gmail.com>

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 2.1 of the License, or (at your option) version 3, or any
    later version accepted by the membership of KDE e.V. (or its
    successor approved by the membership of KDE e.V.), which shall
    act as a proxy defined in Section 6 of version 3 of the license.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public
    License along with this library.  If not, see <http://www.gnu.org/licenses/>.
***/

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
