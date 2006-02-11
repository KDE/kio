kde4_header()

include_directories( ${CMAKE_SOURCE_DIR}/kwallet/client ${KDE4_INCLUDE_DIR} ${QT_INCLUDES} ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR}  )


########### next target ###############

set(kded_kpasswdserver_PART_SRCS kpasswdserver.cpp )

kde4_automoc(${kded_kpasswdserver_PART_SRCS})

set( kded_kpasswdserver_DCOP_SKEL_SRCS kpasswdserver.h )

kde4_add_dcop_skels(kded_kpasswdserver_PART_SRCS ${kded_kpasswdserver_DCOP_SKEL_SRCS})

kde4_add_plugin(kded_kpasswdserver ${kded_kpasswdserver_PART_SRCS})


target_link_libraries(kded_kpasswdserver  ${QT_AND_KDECORE_LIBS} kwalletclient kio )

kde4_install_libtool_file(${KDE4_PLUGIN_INSTALL_DIR} kded_kpasswdserver )
install_targets(${KDE4_PLUGIN_INSTALL_DIR} kded_kpasswdserver )


########### install files ###############


kde4_create_doxygen_docs(  )

kde4_footer()



#original Makefile.am contents follow:

#INCLUDES= -I$(top_srcdir)/kwallet/client $(all_includes)
#
######## Files
#
#kde_module_LTLIBRARIES = kded_kpasswdserver.la
#
#kded_kpasswdserver_la_SOURCES = kpasswdserver.cpp kpasswdserver.skel 
#kded_kpasswdserver_la_METASOURCES = AUTO
#kded_kpasswdserver_la_LDFLAGS = $(all_libraries) -module -avoid-version
#kded_kpasswdserver_la_LIBADD = $(top_builddir)/kwallet/client/libkwalletclient.la $(LIB_KIO) $(LIB_KDED)
#
#include $(top_srcdir)/admin/Doxyfile.am
