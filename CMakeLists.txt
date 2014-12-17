cmake_minimum_required(VERSION 2.8.12)

project(KIO)

find_package(ECM 1.5.0 REQUIRED NO_MODULE)

set(CMAKE_MODULE_PATH ${ECM_MODULE_PATH} ${ECM_KDE_MODULE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/cmake )

include(KDEInstallDirs)
include(KDEFrameworkCompilerSettings)
include(KDECMakeSettings)

include(FeatureSummary)
include(GenerateExportHeader)

include(ECMMarkAsTest)
include(ECMSetupVersion)
include(ECMGenerateHeaders)

set(KF5_VERSION "5.6.0") # handled by release scripts
set(KF5_DEP_VERSION "5.5.0") # handled by release scripts

ecm_setup_version(
  ${KF5_VERSION}
  VARIABLE_PREFIX KIO
  VERSION_HEADER "${CMAKE_CURRENT_BINARY_DIR}/src/kio_version.h"
  PACKAGE_VERSION_FILE "${CMAKE_CURRENT_BINARY_DIR}/KF5KIOConfigVersion.cmake"
  SOVERSION 5)

find_package(KF5Archive ${KF5_DEP_VERSION} REQUIRED)
find_package(KF5Bookmarks ${KF5_DEP_VERSION} REQUIRED)
find_package(KF5Codecs ${KF5_DEP_VERSION} REQUIRED)
find_package(KF5Completion ${KF5_DEP_VERSION} REQUIRED)
find_package(KF5Config ${KF5_DEP_VERSION} REQUIRED)
find_package(KF5ConfigWidgets ${KF5_DEP_VERSION} REQUIRED)
find_package(KF5CoreAddons ${KF5_DEP_VERSION} REQUIRED)
find_package(KF5DBusAddons ${KF5_DEP_VERSION} REQUIRED)
find_package(KF5DocTools ${KF5_DEP_VERSION} REQUIRED)
find_package(KF5I18n ${KF5_DEP_VERSION} REQUIRED)
find_package(KF5IconThemes ${KF5_DEP_VERSION} REQUIRED)
find_package(KF5ItemViews ${KF5_DEP_VERSION} REQUIRED)
find_package(KF5JobWidgets ${KF5_DEP_VERSION} REQUIRED)
find_package(KF5Notifications ${KF5_DEP_VERSION} REQUIRED) #needed for kpac
find_package(KF5Service ${KF5_DEP_VERSION} REQUIRED)
find_package(KF5Solid ${KF5_DEP_VERSION} REQUIRED)
find_package(KF5WidgetsAddons ${KF5_DEP_VERSION} REQUIRED)
find_package(KF5WindowSystem ${KF5_DEP_VERSION} REQUIRED)
find_package(KF5XmlGui ${KF5_DEP_VERSION} REQUIRED)
find_package(KF5Wallet ${KF5_DEP_VERSION} REQUIRED)

# TODO: Remove these
remove_definitions(-DQT_NO_CAST_FROM_ASCII)
remove_definitions(-DQT_NO_CAST_FROM_BYTEARRAY)

add_definitions(-DQT_NO_URL_CAST_FROM_STRING)

set(REQUIRED_QT_VERSION 5.2.0)
find_package(Qt5 ${REQUIRED_QT_VERSION} CONFIG REQUIRED Widgets DBus Network Concurrent Xml Test)

find_package(GSSAPI)
set_package_properties(GSSAPI PROPERTIES DESCRIPTION "Allows KIO to make use of certain HTTP authentication services"
                       URL "http://web.mit.edu/kerberos/www"
                       TYPE OPTIONAL
                       PURPOSE "A MIT or HEIMDAL flavor of GSSAPI can be used"
                      )
find_package(X11)
set(HAVE_X11 ${X11_FOUND})
if (HAVE_X11)
    find_package(Qt5 ${REQUIRED_QT_VERSION} CONFIG REQUIRED X11Extras)
endif()

add_definitions(-DTRANSLATION_DOMAIN=\"kio5\")
if (IS_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/po")
    ki18n_install(po)
    kdoctools_install(po)
endif()
add_subdirectory(docs)
include(CheckLibraryExists)
add_subdirectory(src)
add_subdirectory(autotests)
add_subdirectory(tests)

# create a Config.cmake and a ConfigVersion.cmake file and install them
set(CMAKECONFIG_INSTALL_DIR "${CMAKECONFIG_INSTALL_PREFIX}/KF5KIO")

include(ECMPackageConfigHelpers)

ecm_configure_package_config_file(
  "${CMAKE_CURRENT_SOURCE_DIR}/KF5KIOConfig.cmake.in"
  "${CMAKE_CURRENT_BINARY_DIR}/KF5KIOConfig.cmake"
  INSTALL_DESTINATION  ${CMAKECONFIG_INSTALL_DIR}
)

install(FILES
  "${CMAKE_CURRENT_BINARY_DIR}/KF5KIOConfig.cmake"
  "${CMAKE_CURRENT_BINARY_DIR}/KF5KIOConfigVersion.cmake"
  DESTINATION "${CMAKECONFIG_INSTALL_DIR}"
  COMPONENT Devel
)

install(EXPORT KF5KIOTargets DESTINATION "${CMAKECONFIG_INSTALL_DIR}" FILE KF5KIOTargets.cmake NAMESPACE KF5:: )

install(FILES ${CMAKE_CURRENT_BINARY_DIR}/src/kio_version.h
        DESTINATION ${KF5_INCLUDE_INSTALL_DIR} COMPONENT Devel )

feature_summary(WHAT ALL   FATAL_ON_MISSING_REQUIRED_PACKAGES)
