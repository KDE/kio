## Makefile.am for trash file meta info plugin

AM_CPPFLAGS      = $(all_includes)

kde_module_LTLIBRARIES = kfile_trash.la

kfile_trash_la_SOURCES = kfile_trash.cpp
kfile_trash_la_LDFLAGS = $(all_libraries) -module $(KDE_PLUGIN)
kfile_trash_la_LIBADD = ../libtrashcommon.la $(LIB_KIO)

METASOURCES = AUTO

services_DATA = kfile_trash.desktop
servicesdir = $(kde_servicesdir)
