INCLUDES = $(all_includes)

kde_module_LTLIBRARIES = kio_trash.la

kio_trash_la_SOURCES = dummy.cpp
kio_trash_la_LIBADD  = libtrashcommon.la $(LIB_KIO)
kio_trash_la_LDFLAGS = $(all_libraries) -module $(KDE_PLUGIN) -no-undefined

dummy.cpp:
	echo > dummy.cpp

kde_services_DATA = trash.protocol

noinst_LTLIBRARIES = libtrashcommon.la
libtrashcommon_la_SOURCES = kio_trash.cpp trashimpl.cpp

# In theory I should use a noinst_LTLIBRARIES...
check_PROGRAMS = testtrash
testtrash_SOURCES = testtrash.cpp
testtrash_LDADD = libtrashcommon.la $(LIB_KIO)
testtrash_LDFLAGS = $(all_libraries)

## TODO in unsermake: TESTS = testtrash
check: testtrash
	./testtrash

messages:
	$(XGETTEXT) `find . -name "*.cc" -o -name "*.cpp" -o -name "*.h"` -o $(podir)/kio_trash.pot

