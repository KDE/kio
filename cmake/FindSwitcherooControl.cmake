# - Find switcheroo-control daemon

# SPDX-FileCopyrightText: 2023 Dave Vasilevsky <dave@vasilevsky.ca>
#
# SPDX-License-Identifier: BSD-3-Clause

find_program(switcherooctl_EXECUTABLE
    NAMES switcherooctl
)
find_package_handle_standard_args(SwitcherooControl
    REQUIRED_VARS switcherooctl_EXECUTABLE)
mark_as_advanced(switcherooctl_EXECUTABLE)
