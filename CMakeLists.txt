cmake_minimum_required(VERSION 3.16)

set(KF_VERSION "6.10.0") # handled by release scripts
set(KF_DEP_VERSION "6.9.0") # handled by release scripts

project(KIO VERSION ${KF_VERSION})

include(FeatureSummary)
find_package(ECM 6.9.0 NO_MODULE)
set_package_properties(ECM PROPERTIES TYPE REQUIRED DESCRIPTION "Extra CMake Modules." URL "https://commits.kde.org/extra-cmake-modules")
feature_summary(WHAT REQUIRED_PACKAGES_NOT_FOUND FATAL_ON_MISSING_REQUIRED_PACKAGES)

set(CMAKE_MODULE_PATH ${ECM_MODULE_PATH} ${CMAKE_CURRENT_SOURCE_DIR}/cmake )

include(KDEInstallDirs)
include(KDEFrameworkCompilerSettings NO_POLICY_SCOPE)
include(KDECMakeSettings)
include(KDEGitCommitHooks)
include(KDEPackageAppTemplates)

include(ECMGenerateExportHeader)
include(ECMMarkAsTest)
include(ECMSetupVersion)
include(ECMGenerateHeaders)
include(ECMAddQch)
include(ECMMarkNonGuiExecutable)
include(ECMQtDeclareLoggingCategory)
include(ECMSourceVersionControl)
include(ECMDeprecationSettings)

ecm_setup_version(
  PROJECT
  VARIABLE_PREFIX KIO
  VERSION_HEADER "${CMAKE_CURRENT_BINARY_DIR}/src/kio_version.h"
  PACKAGE_VERSION_FILE "${CMAKE_CURRENT_BINARY_DIR}/KF6KIOConfigVersion.cmake"
  SOVERSION 6)

set(EXCLUDE_DEPRECATED_BEFORE_AND_AT 0 CACHE STRING "Control the range of deprecated API excluded from the build [default=0].")

option(KIOCORE_ONLY "Only compile KIOCore, not KIOWidgets or anything that depends on it. This will disable support for cookies and passwordhandling (prompting and storing)." OFF)

# Enable state assertion by default on Jenkins and from-git builds.
# This option should eventually be dropped and always be enabled.
set(ASSERT_WORKER_STATES_DEFAULT OFF)
if(DEFINED ENV{JENKINS_SERVER_COOKIE} OR ECM_SOURCE_UNDER_VERSION_CONTROL)
    set(ASSERT_WORKER_STATES_DEFAULT ON)
endif()
option(KIO_ASSERT_WORKER_STATES
    "Used to control whether worker state assertions are enabled. When not enabled only warnings are generated."
    ${ASSERT_WORKER_STATES_DEFAULT})

option(BUILD_QCH "Build API documentation in QCH format (for e.g. Qt Assistant, Qt Creator & KDevelop)" OFF)
add_feature_info(QCH ${BUILD_QCH} "API documentation in QCH format (for e.g. Qt Assistant, Qt Creator & KDevelop)")

option(BUILD_DESIGNERPLUGIN "Build plugin for Qt Designer" ON)
add_feature_info(DESIGNERPLUGIN ${BUILD_DESIGNERPLUGIN} "Build plugin for Qt Designer")

option(USE_FTPD_WSGIDAV_UNITTEST "Use ftpd Ruby Gem (for ftptest) and wsgidav Python module (for webdavtest) in unittests. Note that this is a temporary option that will be removed when distros add those packages in their repos." OFF)

find_package(KF6Archive ${KF_DEP_VERSION} REQUIRED)
find_package(KF6Config ${KF_DEP_VERSION} REQUIRED)
find_package(KF6CoreAddons ${KF_DEP_VERSION} REQUIRED)
find_package(KF6I18n ${KF_DEP_VERSION} REQUIRED)
find_package(KF6Service ${KF_DEP_VERSION} REQUIRED)
find_package(KF6DocTools ${KF_DEP_VERSION})
find_package(KF6Solid ${KF_DEP_VERSION} REQUIRED) # for kio_trash
if (NOT ANDROID)
    find_package(KF6Crash ${KF_DEP_VERSION} REQUIRED)
endif()

if (NOT KIOCORE_ONLY)
find_package(KF6Bookmarks ${KF_DEP_VERSION} REQUIRED)
find_package(KF6ColorScheme ${KF_DEP_VERSION} REQUIRED)
find_package(KF6Completion ${KF_DEP_VERSION} REQUIRED)
find_package(KF6GuiAddons ${KF_DEP_VERSION} REQUIRED)
find_package(KF6IconThemes ${KF_DEP_VERSION} REQUIRED)
find_package(KF6ItemViews ${KF_DEP_VERSION} REQUIRED)
find_package(KF6JobWidgets ${KF_DEP_VERSION} REQUIRED)
find_package(KF6WidgetsAddons ${KF_DEP_VERSION} REQUIRED)
find_package(KF6WindowSystem ${KF_DEP_VERSION} REQUIRED)

find_package(KF6KDED ${KF_DEP_VERSION})
set_package_properties(KF6KDED PROPERTIES DESCRIPTION "Daemon providing session services"
                       TYPE RUNTIME
                       PURPOSE "KDED is used at runtime for proxy management and cookie storage"
                      )

find_package(SwitcherooControl)
set_package_properties(SwitcherooControl PROPERTIES
    TYPE RUNTIME
    PURPOSE "Hybrid GPU support"
)

endif()

if (UNIX AND NOT ANDROID)
    find_package(KF6Auth ${KF_DEP_VERSION} REQUIRED)
endif()

# tell what is missing without doctools
set_package_properties(KF6DocTools PROPERTIES DESCRIPTION "Provides tools to generate documentation in various format from DocBook files"
                       TYPE OPTIONAL
                       PURPOSE "Required to build help KIO worker and documentation"
                      )

set(REQUIRED_QT_VERSION 6.6.0)
find_package(Qt6 ${REQUIRED_QT_VERSION} CONFIG REQUIRED Widgets Network Concurrent Xml Test)

# shall we use DBus?
# enabled per default on Linux & BSD systems
set(USE_DBUS_DEFAULT OFF)
if(UNIX AND NOT APPLE AND NOT ANDROID AND NOT HAIKU)
    set(USE_DBUS_DEFAULT ON)
endif()
option(USE_DBUS "Build components using DBus" ${USE_DBUS_DEFAULT})
set(DBUS_LIB)
if(USE_DBUS)
    find_package(KF6DBusAddons ${KF_DEP_VERSION} REQUIRED)
    find_package(Qt6 ${REQUIRED_QT_VERSION} CONFIG REQUIRED DBus)
    set(HAVE_QTDBUS ${Qt6DBus_FOUND})
    add_definitions(-DWITH_QTDBUS)
    set(DBUS_LIB Qt6::DBus)
endif()

if (Qt6Core_VERSION VERSION_LESS 6.7.0)
    find_package(Qt6Core5Compat ${REQUIRED_QT_VERSION} REQUIRED) # QTextCodec
endif()

if (NOT APPLE AND NOT WIN32 AND NOT ANDROID AND NOT HAIKU)
    option(WITH_X11 "Build with X11 integration" ON)
    set(HAVE_X11 ${WITH_X11})

    option(WITH_WAYLAND "Build with Wayland integration" ON)
    set(HAVE_WAYLAND ${WITH_WAYLAND})
endif()

find_package(ACL)
set(HAVE_LIBACL ${ACL_FOUND})
set(HAVE_POSIX_ACL ${ACL_FOUND})
set_package_properties(ACL PROPERTIES DESCRIPTION "LibACL"
                       URL "ftp://oss.sgi.com/projects/xfs/cmd_tars"
                       TYPE RECOMMENDED
                       PURPOSE "Support for manipulating access control lists")

# Used by KMountPoint
if (CMAKE_SYSTEM_NAME MATCHES "Linux")
    find_package(LibMount REQUIRED)
    set(HAVE_LIB_MOUNT ${LibMount_FOUND})
endif()

ecm_set_disabled_deprecation_versions(
    QT 6.8.0
    KF 6.8.0
)

add_definitions(-DTRANSLATION_DOMAIN=\"kio6\")
ki18n_install(po)

if (KF6DocTools_FOUND)
    kdoctools_install(po)
endif()

if (KF6DocTools_FOUND)
     add_subdirectory(docs)
endif()

include(CheckLibraryExists)
add_subdirectory(src)

if(BUILD_TESTING)
    add_subdirectory(autotests)
    if (NOT KIOCORE_ONLY)
        add_subdirectory(tests)
    endif()
endif()

add_subdirectory(templates)

# create a Config.cmake and a ConfigVersion.cmake file and install them
set(CMAKECONFIG_INSTALL_DIR "${KDE_INSTALL_CMAKEPACKAGEDIR}/KF6KIO")

if (BUILD_QCH)
    ecm_install_qch_export(
        TARGETS KF6KIO_QCH
        FILE KF6KIOQchTargets.cmake
        DESTINATION "${CMAKECONFIG_INSTALL_DIR}"
        COMPONENT Devel
    )
    set(PACKAGE_INCLUDE_QCHTARGETS "include(\"\${CMAKE_CURRENT_LIST_DIR}/KF6KIOQchTargets.cmake\")")
endif()

include(CMakePackageConfigHelpers)

configure_package_config_file(
  "${CMAKE_CURRENT_SOURCE_DIR}/KF6KIOConfig.cmake.in"
  "${CMAKE_CURRENT_BINARY_DIR}/KF6KIOConfig.cmake"
  PATH_VARS KDE_INSTALL_DBUSINTERFACEDIR
  INSTALL_DESTINATION  ${CMAKECONFIG_INSTALL_DIR}
)

install(FILES
  "${CMAKE_CURRENT_BINARY_DIR}/KF6KIOConfig.cmake"
  "${CMAKE_CURRENT_BINARY_DIR}/KF6KIOConfigVersion.cmake"
  DESTINATION "${CMAKECONFIG_INSTALL_DIR}"
  COMPONENT Devel
)

install(EXPORT KF6KIOTargets DESTINATION "${CMAKECONFIG_INSTALL_DIR}" FILE KF6KIOTargets.cmake NAMESPACE KF6:: )

install(FILES ${CMAKE_CURRENT_BINARY_DIR}/src/kio_version.h
        DESTINATION ${KDE_INSTALL_INCLUDEDIR_KF}/KIO COMPONENT Devel)

include(ECMFeatureSummary)
ecm_feature_summary(WHAT ALL   FATAL_ON_MISSING_REQUIRED_PACKAGES)

kde_configure_git_pre_commit_hook(CHECKS CLANG_FORMAT)
