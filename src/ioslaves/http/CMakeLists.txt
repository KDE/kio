project(kioslave-http)

find_package(ZLIB)
set_package_properties(ZLIB PROPERTIES DESCRIPTION "Support for gzip compressed files and data streams"
                       URL "http://www.zlib.net"
                       TYPE REQUIRED
                       PURPOSE "Required by the hspell sonnet plugin"
                      )

include(ConfigureChecks.cmake)
configure_file(config-kioslave-http.h.cmake ${CMAKE_CURRENT_BINARY_DIR}/config-kioslave-http.h )

find_package(GSSAPI)
set_package_properties(GSSAPI PROPERTIES DESCRIPTION "Allows KIO to make use of certain HTTP authentication services"
                       URL "http://web.mit.edu/kerberos/www"
                       TYPE OPTIONAL
                       PURPOSE "A MIT or HEIMDAL flavor of GSSAPI can be used"
                      )
find_package(X11)
set(HAVE_X11 ${X11_FOUND})

if(GSSAPI_FOUND)
    set(HAVE_LIBGSSAPI 1)
    if(GSSAPI_FLAVOR STREQUAL "MIT")
        set(GSSAPI_MIT 1)
    else()
        set(GSSAPI_MIT 0)
    endif()

    include_directories( ${GSSAPI_INCS} )

else()
    set(HAVE_LIBGSSAPI 0)
    set(GSSAPI_MIT 0)
endif()

configure_file(config-gssapi.h.cmake ${CMAKE_CURRENT_BINARY_DIR}/config-gssapi.h )

include_directories(${ZLIB_INCLUDE_DIR})

remove_definitions(-DQT_NO_CAST_FROM_ASCII)

add_subdirectory( kcookiejar )
add_subdirectory( tests )

########### next target ###############

set(kio_http_cache_cleaner_SRCS
   http_cache_cleaner.cpp
   )


add_executable(kio_http_cache_cleaner ${kio_http_cache_cleaner_SRCS})

target_link_libraries(kio_http_cache_cleaner
   Qt5::DBus
   Qt5::Network # QLocalSocket
   ${ZLIB_LIBRARY}
   KF5::KIOCore # KProtocolManager
   KF5::I18n)

install(TARGETS kio_http_cache_cleaner DESTINATION ${LIBEXEC_INSTALL_DIR} )

########### next target ###############

# kio/httpfilter/Makefile.am: httpfilter

set(kio_http_PART_SRCS
   http.cpp
   httpauthentication.cpp
   httpfilter.cpp
   )


add_library(kio_http MODULE ${kio_http_PART_SRCS})

target_link_libraries(kio_http
   Qt5::DBus
   Qt5::Network # QLocalSocket etc.
   Qt5::Xml     # QDom
   KF5::KIOCore
   KF5::KIONTLM
   KF5::Archive
   KF5::Solid
   ${ZLIB_LIBRARY}
   KF5::I18n
)
if(GSSAPI_FOUND)
  target_link_libraries(kio_http ${GSSAPI_LIBS} )
endif()

install(TARGETS  kio_http  DESTINATION ${PLUGIN_INSTALL_DIR} )


########### install files ###############

install( FILES
    http_cache_cleaner.desktop
    http.protocol
    https.protocol
    webdav.protocol
    webdavs.protocol
  DESTINATION  ${SERVICES_INSTALL_DIR} )


