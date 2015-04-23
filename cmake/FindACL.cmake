# - Try to find the ACL library
# Once done this will define
#
#  ACL_FOUND - system has the ACL library
#  ACL_LIBS - The libraries needed to use ACL

# Copyright (c) 2006, Pino Toscano, <toscano.pino@tiscali.it>
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
# 3. Neither the name of the University nor the names of its contributors
#    may be used to endorse or promote products derived from this software
#    without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
# ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
# ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
# FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
# OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
# HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
# LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
# OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
# SUCH DAMAGE.

include(CheckIncludeFiles)

check_include_files(attr/libattr.h HAVE_ATTR_LIBATTR_H)
check_include_files(sys/xattr.h HAVE_SYS_XATTR_H)
check_include_files(sys/acl.h HAVE_SYS_ACL_H)
check_include_files(acl/libacl.h HAVE_ACL_LIBACL_H)

if (HAVE_ATTR_LIBATTR_H AND HAVE_SYS_XATTR_H AND HAVE_SYS_ACL_H AND HAVE_ACL_LIBACL_H)
   set(ACL_HEADERS_FOUND TRUE)
endif (HAVE_ATTR_LIBATTR_H AND HAVE_SYS_XATTR_H AND HAVE_SYS_ACL_H AND HAVE_ACL_LIBACL_H)

if (ACL_HEADERS_FOUND)
   find_library(ACL_LIBS NAMES acl )

   find_library(ATTR_LIBS NAMES attr )
endif (ACL_HEADERS_FOUND)

if (ACL_HEADERS_FOUND AND ACL_LIBS AND ATTR_LIBS)
   set(ACL_FOUND TRUE)
   set(ACL_LIBS ${ACL_LIBS} ${ATTR_LIBS})
   message(STATUS "Found ACL support: ${ACL_LIBS}")
endif (ACL_HEADERS_FOUND AND ACL_LIBS AND ATTR_LIBS)

mark_as_advanced(ACL_LIBS  ATTR_LIBS)

