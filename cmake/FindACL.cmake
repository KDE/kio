# - Determine if system supports ACLs.
# Once done this will define
#
#  ACL_FOUND - system has the ACL feature
#  ACL_LIBS - The libraries needed to use ACL

# SPDX-FileCopyrightText: 2006 Pino Toscano <toscano.pino@tiscali.it>
#
# SPDX-License-Identifier: BSD-3-Clause

include(CheckIncludeFiles)

check_include_files(attr/libattr.h HAVE_ATTR_LIBATTR_H)
check_include_files(sys/xattr.h HAVE_SYS_XATTR_H)
check_include_files(sys/acl.h HAVE_SYS_ACL_H)
check_include_files(acl/libacl.h HAVE_ACL_LIBACL_H)
check_include_files("sys/types.h;sys/extattr.h" HAVE_SYS_EXTATTR_H)

if ((HAVE_ATTR_LIBATTR_H AND HAVE_SYS_XATTR_H AND HAVE_SYS_ACL_H AND HAVE_ACL_LIBACL_H)
    OR HAVE_SYS_EXTATTR_H)
   set(ACL_HEADERS_FOUND TRUE)
endif ()

find_library(ACL_LIBS NAMES acl )
find_library(ATTR_LIBS NAMES attr )

if (ACL_LIBS AND ATTR_LIBS)
   set(ACL_LIBS_FOUND TRUE)
endif()

# FreeBSD has ACL bits in its libc, so no extra libraries are required
if (CMAKE_SYSTEM_NAME MATCHES "FreeBSD")
   set(ACL_LIBS "" CACHE STRING "" FORCE)
   set(ATTR_LIBS "" CACHE STRING "" FORCE)
   set(ACL_LIBS_FOUND TRUE)
endif()

# Now check for non-POSIX acl_*() functions we are requiring
include(CheckSymbolExists)

set(_required_headers)
if (HAVE_SYS_ACL_H)
   list(APPEND _required_headers "sys/types.h" "sys/acl.h")
endif()
if (HAVE_ACL_LIBACL_H)
   list(APPEND _required_headers "acl/libacl.h")
endif()

set(CMAKE_REQUIRED_LIBRARIES ${ACL_LIBS})

check_symbol_exists(acl_cmp "${_required_headers}" HAVE_ACL_CMP)
check_symbol_exists(acl_from_mode "${_required_headers}" HAVE_ACL_FROM_MODE)
check_symbol_exists(acl_equiv_mode "${_required_headers}" HAVE_ACL_EQUIV_MODE)
check_symbol_exists(acl_extended_file "${_required_headers}" HAVE_ACL_EXTENDED_FILE)
check_symbol_exists(acl_get_perm "${_required_headers}" HAVE_ACL_GET_PERM)

check_symbol_exists(acl_cmp_np "${_required_headers}" HAVE_ACL_CMP_NP)
check_symbol_exists(acl_from_mode_np "${_required_headers}" HAVE_ACL_FROM_MODE_NP)
check_symbol_exists(acl_equiv_mode_np "${_required_headers}" HAVE_ACL_EQUIV_MODE_NP)
check_symbol_exists(acl_extended_file_np "${_required_headers}" HAVE_ACL_EXTENDED_FILE_NP)
check_symbol_exists(acl_get_perm_np "${_required_headers}" HAVE_ACL_GET_PERM_NP)

if (HAVE_ACL_CMP AND HAVE_ACL_FROM_MODE AND HAVE_ACL_EQUIV_MODE
   AND HAVE_ACL_EXTENDED_FILE AND HAVE_ACL_GET_PERM)
   set(ACL_NONSTANDARD_FUNCS_FOUND TRUE)
endif()

if (HAVE_ACL_CMP_NP AND HAVE_ACL_FROM_MODE_NP AND HAVE_ACL_EQUIV_MODE_NP
   AND HAVE_ACL_EXTENDED_FILE_NP AND HAVE_ACL_GET_PERM_NP)
   set(ACL_NONSTANDARD_FUNCS_FOUND TRUE)
endif()

if (ACL_HEADERS_FOUND AND ACL_LIBS_FOUND AND ACL_NONSTANDARD_FUNCS_FOUND)
   set(ACL_FOUND TRUE)
   set(ACL_LIBS ${ACL_LIBS} ${ATTR_LIBS})
   message(STATUS "Found ACL support: ${ACL_LIBS}")
endif ()

mark_as_advanced(ACL_LIBS  ATTR_LIBS)
