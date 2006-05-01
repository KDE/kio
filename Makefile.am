INCLUDES = $(all_includes)
METASOURCES = AUTO

SUBDIRS = . kfile-plugin

kde_module_LTLIBRARIES = kio_trash.la

kio_trash_la_SOURCES = kio_trash.cpp
kio_trash_la_LIBADD  = libtrashcommon.la $(LIB_KIO)
kio_trash_la_LDFLAGS = $(all_libraries) -module $(KDE_PLUGIN) -no-undefined

bin_PROGRAMS = ktrash
ktrash_SOURCES = ktrash.cpp
ktrash_LDADD = $(LIB_KIO)
ktrash_LDFLAGS = $(all_libraries) $(KDE_RPATH)

kde_services_DATA = trash.protocol

noinst_LTLIBRARIES = libtrashcommon.la
libtrashcommon_la_SOURCES = trashimpl.cpp

check_PROGRAMS = testtrash
testtrash_SOURCES = testtrash.cpp
testtrash_LDADD = libtrashcommon.la $(LIB_KIO)
testtrash_LDFLAGS = $(all_libraries)

TESTS = testtrash


