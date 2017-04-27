cmake_minimum_required(VERSION 3.0)

set(KF5_VERSION "5.35.0") # handled by release scripts
set(KF5_DEP_VERSION "5.35.0") # handled by release scripts

project(KIO VERSION ${KF5_VERSION})

include(FeatureSummary)
find_package(ECM 5.35.0  NO_MODULE)
set_package_properties(ECM PROPERTIES TYPE REQUIRED DESCRIPTION "Extra CMake Modules." URL "https://projects.kde.org/projects/kdesupport/extra-cmake-modules")
feature_summary(WHAT REQUIRED_PACKAGES_NOT_FOUND FATAL_ON_MISSING_REQUIRED_PACKAGES)


set(CMAKE_MODULE_PATH ${ECM_MODULE_PATH} ${ECM_KDE_MODULE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/cmake )

include(KDEInstallDirs)
include(KDEFrameworkCompilerSettings NO_POLICY_SCOPE)
include(KDECMakeSettings)


include(GenerateExportHeader)

include(ECMMarkAsTest)
include(ECMSetupVersion)
include(ECMGenerateHeaders)
include(ECMAddQch)
include(ECMMarkNonGuiExecutable)
include(ECMQtDeclareLoggingCategory)

ecm_setup_version(
  PROJECT
  VARIABLE_PREFIX KIO
  VERSION_HEADER "${CMAKE_CURRENT_BINARY_DIR}/src/kio_version.h"
  PACKAGE_VERSION_FILE "${CMAKE_CURRENT_BINARY_DIR}/KF5KIOConfigVersion.cmake"
  SOVERSION 5)

option(KIOCORE_ONLY "Only compile KIOCore, not KIOWidgets or anything that depends on it. This will disable support for cookies and passwordhandling (prompting and storing)." OFF)
option(KIO_FORK_SLAVES "If set we start the slaves via QProcess. It's also possible to change this by setting the environment variable KDE_FORK_SLAVES." OFF)

option(BUILD_QCH "Build API documentation in QCH format (for e.g. Qt Assistant, Qt Creator & KDevelop)" OFF)
add_feature_info(QCH ${BUILD_QCH} "API documentation in QCH format (for e.g. Qt Assistant, Qt Creator & KDevelop)")

find_package(KF5Archive ${KF5_DEP_VERSION} REQUIRED)
find_package(KF5Config ${KF5_DEP_VERSION} REQUIRED)
find_package(KF5CoreAddons ${KF5_DEP_VERSION} REQUIRED)
find_package(KF5DBusAddons ${KF5_DEP_VERSION} REQUIRED)
find_package(KF5I18n ${KF5_DEP_VERSION} REQUIRED)
find_package(KF5Service ${KF5_DEP_VERSION} REQUIRED)
find_package(KF5DocTools ${KF5_DEP_VERSION})
find_package(KF5Solid ${KF5_DEP_VERSION} REQUIRED) # for kio_trash

if (NOT KIOCORE_ONLY)
find_package(KF5Bookmarks ${KF5_DEP_VERSION} REQUIRED)
find_package(KF5Completion ${KF5_DEP_VERSION} REQUIRED)
find_package(KF5ConfigWidgets ${KF5_DEP_VERSION} REQUIRED)
find_package(KF5IconThemes ${KF5_DEP_VERSION} REQUIRED)
find_package(KF5ItemViews ${KF5_DEP_VERSION} REQUIRED)
find_package(KF5JobWidgets ${KF5_DEP_VERSION} REQUIRED)
find_package(KF5WidgetsAddons ${KF5_DEP_VERSION} REQUIRED)
find_package(KF5WindowSystem ${KF5_DEP_VERSION} REQUIRED)
endif()

# tell what is missing without doctools
set_package_properties(KF5DocTools PROPERTIES DESCRIPTION "Provides tools to generate documentation in various format from DocBook files"
                       TYPE OPTIONAL
                       PURPOSE "Required to build help ioslave and documentation"
                      )

# TODO: Remove these
remove_definitions(-DQT_NO_CAST_FROM_ASCII)
remove_definitions(-DQT_NO_CAST_FROM_BYTEARRAY)

add_definitions(-DQT_NO_URL_CAST_FROM_STRING)

set(REQUIRED_QT_VERSION 5.6.0)
find_package(Qt5 ${REQUIRED_QT_VERSION} CONFIG REQUIRED Widgets DBus Network Concurrent Xml Test)

find_package(GSSAPI)
set_package_properties(GSSAPI PROPERTIES DESCRIPTION "Allows KIO to make use of certain HTTP authentication services"
                       URL "http://web.mit.edu/kerberos/www"
                       TYPE OPTIONAL
                       PURPOSE "A MIT or HEIMDAL flavor of GSSAPI can be used"
                      )

if (NOT APPLE AND NOT WIN32)
    find_package(X11)
endif()

set(HAVE_X11 ${X11_FOUND})
if (HAVE_X11)
    find_package(Qt5 ${REQUIRED_QT_VERSION} CONFIG REQUIRED X11Extras)
endif()

add_definitions(-DTRANSLATION_DOMAIN=\"kio5\")
if (IS_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/po")
    ki18n_install(po)

    if (KF5DocTools_FOUND)
        kdoctools_install(po)
    endif()
endif()

if (KF5DocTools_FOUND)
    add_subdirectory(docs)
endif()

include(CheckLibraryExists)
add_subdirectory(src)
add_subdirectory(autotests)

if (NOT KIOCORE_ONLY)
add_subdirectory(tests)
endif()

# create a Config.cmake and a ConfigVersion.cmake file and install them
set(CMAKECONFIG_INSTALL_DIR "${KDE_INSTALL_CMAKEPACKAGEDIR}/KF5KIO")

if (BUILD_QCH)
    ecm_install_qch_export(
        TARGETS KF5KIO_QCH
        FILE KF5KIOQchTargets.cmake
        DESTINATION "${CMAKECONFIG_INSTALL_DIR}"
        COMPONENT Devel
    )
    set(PACKAGE_INCLUDE_QCHTARGETS "include(\"\${CMAKE_CURRENT_LIST_DIR}/KF5KIOQchTargets.cmake\")")
endif()

include(CMakePackageConfigHelpers)

configure_package_config_file(
  "${CMAKE_CURRENT_SOURCE_DIR}/KF5KIOConfig.cmake.in"
  "${CMAKE_CURRENT_BINARY_DIR}/KF5KIOConfig.cmake"
  PATH_VARS KDE_INSTALL_DBUSINTERFACEDIR
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
        DESTINATION ${KDE_INSTALL_INCLUDEDIR_KF5} COMPONENT Devel )

feature_summary(WHAT ALL   FATAL_ON_MISSING_REQUIRED_PACKAGES)
