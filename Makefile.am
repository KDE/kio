INCLUDES = $(all_includes)

kde_module_LTLIBRARIES = kio_trash.la

kio_trash_la_SOURCES = kio_trash.cpp trashimpl.cpp
kio_trash_la_LIBADD  = $(LIB_KIO)
kio_trash_la_LDFLAGS = $(all_libraries) -module $(KDE_PLUGIN) -no-undefined

kde_services_DATA = trash.protocol

messages:
	$(XGETTEXT) `find . -name "*.cc" -o -name "*.cpp" -o -name "*.h"` -o $(podir)/kio_trash.pot

