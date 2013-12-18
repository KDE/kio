cmake_minimum_required(VERSION 2.8.12)

project(KIO)

find_package(ECM 0.0.9 REQUIRED NO_MODULE)

set(CMAKE_MODULE_PATH ${ECM_MODULE_PATH} ${ECM_KDE_MODULE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/cmake )

include(KDEInstallDirs)
include(KDEFrameworkCompilerSettings)
include(KDECMakeSettings)

include(FeatureSummary)

include(ECMSetupVersion)

set(KF5_VERSION "5.0.0")

ecm_setup_version(
  ${KF5_VERSION}
  VARIABLE_PREFIX KIO
  VERSION_HEADER "${CMAKE_CURRENT_BINARY_DIR}/src/kio_version.h"
  PACKAGE_VERSION_FILE "${CMAKE_CURRENT_BINARY_DIR}/KF5KIOConfigVersion.cmake"
)

if("${CMAKE_BINARY_DIR}" STREQUAL "${CMAKE_CURRENT_BINARY_DIR}")
  # Due to a CMake bug, we need to explicitly find private dependencies of our dependencies
  # Remove when we depend on CMake 3.0.0
  find_package(KF5DBusAddons ${KF5_VERSION} REQUIRED)
  find_package(KF5I18n ${KF5_VERSION} REQUIRED)
  find_package(KF5GuiAddons ${KF5_VERSION} REQUIRED)
  find_package(KF5ConfigWidgets ${KF5_VERSION} REQUIRED)
  find_package(KF5ItemViews ${KF5_VERSION} REQUIRED)
  find_package(KF5CoreAddons ${KF5_VERSION} REQUIRED)
  find_package(KF5WidgetsAddons ${KF5_VERSION} REQUIRED)
  find_package(KF5Service ${KF5_VERSION} REQUIRED)
  find_package(KF5Archive ${KF5_VERSION} REQUIRED)
  find_package(KF5Solid ${KF5_VERSION} REQUIRED)
  find_package(KF5JobWidgets ${KF5_VERSION} REQUIRED)
  find_package(KF5IconThemes ${KF5_VERSION} REQUIRED)
  find_package(KF5WindowSystem ${KF5_VERSION} REQUIRED)
  find_package(KF5Crash ${KF5_VERSION} REQUIRED)
  find_package(KF5Completion ${KF5_VERSION} REQUIRED)
  find_package(KF5DocTools ${KF5_VERSION} REQUIRED)
  find_package(KF5TextWidgets ${KF5_VERSION} REQUIRED)
  find_package(KF5XmlGui ${KF5_VERSION} REQUIRED)
  find_package(KF5Bookmarks ${KF5_VERSION} REQUIRED)
  find_package(KF5Notifications ${KF5_VERSION} REQUIRED) #needed for kpac
else()
    set(_kdeBootStrapping TRUE)
    include(${CMAKE_SOURCE_DIR}/tier2/kdoctools/KF5DocToolsMacros.cmake)
    set(KDOCTOOLS_CUSTOMIZATION_DIR "${KDocTools_BINARY_DIR}/src/customization/")
endif()

# TODO: Remove these
remove_definitions(-DQT_NO_CAST_TO_ASCII)
remove_definitions(-DQT_NO_CAST_FROM_ASCII)
remove_definitions(-DQT_NO_CAST_FROM_BYTEARRAY)

add_definitions(-DQT_NO_URL_CAST_FROM_STRING)

set(QT_REQUIRED_VERSION 5.2.0)
find_package(Qt5 ${QT_REQUIRED_VERSION} CONFIG REQUIRED Widgets DBus Network Concurrent Xml Test Script)

find_package(X11)
set(HAVE_X11 ${X11_FOUND})

add_subdirectory(docs)
add_subdirectory(src)
add_subdirectory(autotests)
add_subdirectory(tests)

# create a Config.cmake and a ConfigVersion.cmake file and install them
set(CMAKECONFIG_INSTALL_DIR "${CMAKECONFIG_INSTALL_PREFIX}/KF5KIO")

include(CMakePackageConfigHelpers)

configure_package_config_file(
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
        DESTINATION ${INCLUDE_INSTALL_DIR} COMPONENT Devel )

if ("${CMAKE_BINARY_DIR}" STREQUAL "${CMAKE_CURRENT_BINARY_DIR}")
  feature_summary(WHAT ALL   FATAL_ON_MISSING_REQUIRED_PACKAGES)
endif()
