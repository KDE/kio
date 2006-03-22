kde4_header()

project(kioslave-trash)

include_directories( ${KDE4_INCLUDE_DIR} ${QT_INCLUDES} ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR}  )

set (trashcommon_PART_SRCS ${CMAKE_SOURCE_DIR}/kioslave/trash/trashimpl.cpp)

########### next target ###############

set(kio_trash_PART_SRCS kio_trash.cpp ${trashcommon_PART_SRCS})

kde4_automoc(${kio_trash_PART_SRCS})

kde4_add_plugin(kio_trash ${kio_trash_PART_SRCS})

kde4_install_libtool_file( ${PLUGIN_INSTALL_DIR} kio_trash )

target_link_libraries(kio_trash  ${LIB_KDE3SUPPORT})

install_targets(${LIB_INSTALL_DIR}/kde4 kio_trash )



########### next target ###############

set(ktrash_SRCS ktrash.cpp )

kde4_automoc(${ktrash_SRCS})

kde4_add_executable(ktrash ${ktrash_SRCS})

target_link_libraries(ktrash  ${LIB_KIO} )

install_targets(/bin ktrash )

########### next target ###############


########### next target ###############

set(testtrash_SRCS testtrash.cpp ${trashcommon_PART_SRCS})

kde4_automoc(${testtrash_SRCS})

if(KDE4_BUILD_TESTS)

kde4_add_executable(testtrash ${testtrash_SRCS})

target_link_libraries(testtrash  ${KDE4_KDECORE_LIBS} kio )

endif(KDE4_BUILD_TESTS)

########### next target ###############
#Add sub dir after: generate  /kioslave/trash/trashimpl.cpp before to go to this directory
add_subdirectory( kfile-plugin )

########### install files ###############

install_files( ${SERVICES_INSTALL_DIR} FILES trash.protocol )

kde4_footer()



#original Makefile.am contents follow:

#INCLUDES = $(all_includes)
#METASOURCES = AUTO
#
#SUBDIRS = . kfile-plugin
#
#kde_module_LTLIBRARIES = kio_trash.la
#
#kio_trash_la_SOURCES = kio_trash.cpp
#kio_trash_la_LIBADD  = libtrashcommon.la $(LIB_KIO)
#kio_trash_la_LDFLAGS = $(all_libraries) -module $(KDE_PLUGIN) -no-undefined
#
#bin_PROGRAMS = ktrash
#ktrash_SOURCES = ktrash.cpp
#ktrash_LDADD = $(LIB_KIO)
#ktrash_LDFLAGS = $(all_libraries) $(KDE_RPATH)
#
#kde_services_DATA = trash.protocol
#
#noinst_LTLIBRARIES = libtrashcommon.la
#libtrashcommon_la_SOURCES = trashimpl.cpp
#
#check_PROGRAMS = testtrash
#testtrash_SOURCES = testtrash.cpp
#testtrash_LDADD = libtrashcommon.la $(LIB_KIO)
#testtrash_LDFLAGS = $(all_libraries)
#
#TESTS = testtrash
#
#messages:
#	$(XGETTEXT) `find . -name "*.cc" -o -name "*.cpp" -o -name "*.h"` -o $(podir)/kio_trash.pot
#
