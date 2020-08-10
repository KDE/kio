# SPDX-FileCopyrightText: 2020 Ben Gruber <bengruber250@gmail.com>
#
# SPDX-License-Identifier: BSD-3-Clause

find_program(WsgidavExe_EXECUTABLE wsgidav)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(WsgidavExe
    FOUND_VAR
        WsgidavExe_FOUND
    REQUIRED_VARS
        WsgidavExe_EXECUTABLE
)
