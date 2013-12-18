/* This file is part of the KDE libraries
   Copyright (c) 2006 The KDE Project

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public
   License version 2 as published by the Free Software Foundation.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public License
   along with this library; see the file COPYING.LIB.  If not, write to
   the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
   Boston, MA 02110-1301, USA.
*/

#cmakedefine01 HAVE_GETGROUPLIST
#if ! HAVE_GETGROUPLIST
#include <sys/types.h> /* for gid_t */
#ifdef __cplusplus
extern "C" {
#endif
int getgrouplist(const char *, gid_t , gid_t *, int *);
#ifdef __cplusplus
}
#endif
#endif
