# SPDX-FileCopyrightText: 2021 Ahmad Samir <a.samirh78@gmail.com>
#
# SPDX-License-Identifier: BSD-3-Clause

#[=======================================================================[.rst:

FindBlkid
------------

Try to find the blkid library (part of util-linux), once done this will define:

``Blkid_FOUND``
    blkid was found on the system.

``Blkid_INCLUDE_DIRS``
    The blkid library include directory.

``Blkid_LIBRARIES``
    The blkid libraries.

``Blkid_VERSION``
    The blkid library version.

If ``Blkid_FOUND`` is TRUE, it will also define the following imported target:

``Blkid::Blkid``
    The blkid library

Since 5.85.0
#]=======================================================================]

find_package(PkgConfig QUIET)
pkg_check_modules(PC_BLKID QUIET blkid)

find_path(Blkid_INCLUDE_DIRS NAMES blkid/blkid.h HINTS ${PC_BLKID_INCLUDE_DIRS})
find_library(Blkid_LIBRARIES NAMES blkid HINTS ${PC_BLKID_LIBRARY_DIRS})

set(Blkid_VERSION ${PC_BLKID_VERSION})

if(Blkid_INCLUDE_DIRS AND NOT Blkid_VERSION)
    file(READ "${Blkid_INCLUDE_DIRS}/blkid/blkid.h" _Blkid_header_contents)
    string(REGEX MATCHALL "#define[ \t]+BLKID_VERSION[ \t]+\"*[0-9.]+" _Blkid_version_line "${_Blkid_header_contents}")
    unset(_Blkid_header_contents)
    string(REGEX REPLACE ".*BLKID_VERSION[ \t]+\"*([0-9.]+)\"*" "\\1" _version "${_Blkid_version_line}")
    set(Blkid_VERSION "${_version}")
    unset(_Blkid_version_line)
endif()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(Blkid
    FOUND_VAR Blkid_FOUND
    REQUIRED_VARS Blkid_INCLUDE_DIRS Blkid_LIBRARIES
    VERSION_VAR Blkid_VERSION)

mark_as_advanced(Blkid_INCLUDE_DIRS Blkid_LIBRARIES)

if(Blkid_FOUND AND NOT TARGET Blkid::Blkid)
    add_library(Blkid::Blkid UNKNOWN IMPORTED)
    set_target_properties(Blkid::Blkid PROPERTIES
        IMPORTED_LOCATION "${Blkid_LIBRARIES}"
        INTERFACE_INCLUDE_DIRECTORIES "${Blkid_INCLUDE_DIRS}"
        INTERFACE_COMPILE_DEFINITIONS "${PC_BLKID_CFLAGS_OTHER}"
    )
endif()

include(FeatureSummary)
set_package_properties(Blkid PROPERTIES
    DESCRIPTION "Block device identification library (part of util-linux)"
    URL "https://www.kernel.org/pub/linux/utils/util-linux/"
)
