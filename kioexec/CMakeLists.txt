kde4_header()

include_directories( ${KDE4_INCLUDE_DIR} ${QT_INCLUDES} ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR}  )


########### next target ###############

set(kioexec_SRCS
main.cpp
)

kde4_automoc(${kioexec_SRCS})

kde4_add_executable(kioexec ${kioexec_SRCS})

target_link_libraries(kioexec  ${QT_AND_KDECORE_LIBS} kio )

install_targets(/bin kioexec )


########### install files ###############


kde4_create_doxygen_docs(  )

kde4_footer()



#original Makefile.am contents follow:

#AM_CPPFLAGS = $(all_includes)
#KDE_CXXFLAGS = -UQT3_SUPPORT
#
#bin_PROGRAMS = kioexec
#
#kioexec_SOURCES = main.cpp
#kioexec_LDFLAGS = $(all_libraries) $(KDE_RPATH)
#kioexec_LDADD   = $(LIB_KIO)
#
#noinst_HEADERS = main.h
#METASOURCES = AUTO
#
#messages:
#	$(XGETTEXT) $(kioexec_SOURCES) -o $(podir)/kioexec.pot
#
#install-exec-local:
#	@rm -f $(DESTDIR)$(bindir)/kfmexec
#	@$(LN_S) kioexec $(DESTDIR)$(bindir)/kfmexec
#
#include $(top_srcdir)/admin/Doxyfile.am
