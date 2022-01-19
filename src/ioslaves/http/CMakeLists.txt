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

if (NOT KIOCORE_ONLY)
add_subdirectory( kcookiejar )
endif()

########### next target ###############

add_executable(kio_http_cache_cleaner)
# Mark it as non-gui so we won't create an app bundle on Mac OS X
ecm_mark_nongui_executable(kio_http_cache_cleaner)

target_sources(kio_http_cache_cleaner PRIVATE
    http_cache_cleaner.cpp
)

target_link_libraries(kio_http_cache_cleaner
   Qt${QT_MAJOR_VERSION}::DBus
   Qt${QT_MAJOR_VERSION}::Network # QLocalSocket
   KF5::KIOCore # KProtocolManager
   KF5::I18n)

install(TARGETS kio_http_cache_cleaner DESTINATION ${KDE_INSTALL_LIBEXECDIR_KF} )

########### next target ###############

# kio/httpfilter/Makefile.am: httpfilter
kcoreaddons_add_plugin(kio_http INSTALL_NAMESPACE "kf${QT_MAJOR_VERSION}/kio")

target_sources(kio_http PRIVATE
   http.cpp
   httpauthentication.cpp
   httpfilter.cpp
   )

ecm_qt_export_logging_category(
    IDENTIFIER KIO_HTTP
    CATEGORY_NAME kf.kio.slaves.http
    OLD_CATEGORY_NAMES kf5.kio.kio_http
    DEFAULT_SEVERITY Warning
    DESCRIPTION "KIO HTTP slave (KIO)"
    EXPORT KIO
)

ecm_qt_export_logging_category(
    IDENTIFIER KIO_HTTP_AUTH
    CATEGORY_NAME kf.kio.slaves.http.auth
    OLD_CATEGORY_NAMES kf5.kio.kio_http.auth
    DESCRIPTION "kio http auth (KIO)"
    EXPORT KIO
)

ecm_qt_export_logging_category(
    IDENTIFIER KIO_HTTP_FILTER
    CATEGORY_NAME kf.kio.slaves.http.filter
    OLD_CATEGORY_NAMES kf5.kio.kio_http.filter
    DESCRIPTION "kio http filter (KIO)"
    EXPORT KIO
)

target_link_libraries(kio_http
   PRIVATE
   Qt${QT_MAJOR_VERSION}::DBus
   Qt${QT_MAJOR_VERSION}::Network # QLocalSocket etc.
   Qt${QT_MAJOR_VERSION}::Xml     # QDom
   KF5::ConfigCore
   KF5::KIOCore
   KF5::KIONTLM
   KF5::Archive
   KF5::I18n
)
if(GSSAPI_FOUND)
  target_link_libraries(kio_http PRIVATE ${GSSAPI_LIBS})
endif()

if (TARGET Qt6::Core5Compat)
    target_link_libraries(kio_http PRIVATE Qt6::Core5Compat) # QTextCodec
endif()

########### install files ###############

install( FILES
    http_cache_cleaner.desktop
  DESTINATION  ${KDE_INSTALL_KSERVICESDIR} )


