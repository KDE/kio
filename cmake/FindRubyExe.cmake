# SPDX-FileCopyrightText: 2019 Harald Sitter <sitter@kde.org>
#
# SPDX-License-Identifier: BSD-3-Clause

# FindRuby from cmake is also covering ruby dev headers and stuff. For when
# you only need ruby, this only finds ruby!

find_program(RubyExe_EXECUTABLE ruby)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(RubyExe
    FOUND_VAR
        RubyExe_FOUND
    REQUIRED_VARS
        RubyExe_EXECUTABLE
)
