project(kioslave-http)

include(ECMMarkNonGuiExecutable)

include(ConfigureChecks.cmake)
configure_file(config-kioslave-http.h.cmake ${CMAKE_CURRENT_BINARY_DIR}/config-kioslave-http.h )

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

if (NOT KIOCORE_ONLY)
add_subdirectory( kcookiejar )
endif()

########### next target ###############

set(kio_http_cache_cleaner_SRCS
   http_cache_cleaner.cpp
   )


add_executable(kio_http_cache_cleaner ${kio_http_cache_cleaner_SRCS})

# Mark it as non-gui so we won't create an app bundle on Mac OS X
ecm_mark_nongui_executable(kio_http_cache_cleaner)

target_link_libraries(kio_http_cache_cleaner
   Qt5::DBus
   Qt5::Network # QLocalSocket
   ${ZLIB_LIBRARY}
   KF5::KIOCore # KProtocolManager
   KF5::I18n)

install(TARGETS kio_http_cache_cleaner DESTINATION ${KDE_INSTALL_LIBEXECDIR_KF5} )

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
   ${ZLIB_LIBRARY}
   KF5::I18n
)
if(GSSAPI_FOUND)
  target_link_libraries(kio_http ${GSSAPI_LIBS} )
endif()

set_target_properties(kio_http PROPERTIES OUTPUT_NAME "http")
install(TARGETS kio_http  DESTINATION ${KDE_INSTALL_PLUGINDIR}/kf5/kio)


########### install files ###############

install( FILES
    http_cache_cleaner.desktop
    http.protocol
    https.protocol
    webdav.protocol
    webdavs.protocol
  DESTINATION  ${KDE_INSTALL_KSERVICES5DIR} )


