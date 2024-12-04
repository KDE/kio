remove_definitions(-DQT_NO_CAST_FROM_ASCII)
remove_definitions(-DQT_NO_CAST_FROM_BYTEARRAY)

include(ECMAddTests)

find_package(Qt6Widgets REQUIRED)

########### unittests ###############

add_subdirectory(http)

find_package(Qt6Concurrent ${REQUIRED_QT_VERSION} REQUIRED NO_MODULE)

if (ACL_FOUND)
    set (kacltest_SRC kacltest.cpp)
endif()

ecm_add_tests(
 ${kacltest_SRC}
 listdirtest.cpp
 kmountpointtest.cpp
 upurltest.cpp
 dataprotocoltest.cpp
 jobtest.cpp
 jobremotetest.cpp
 kfileitemtest.cpp
 kprotocolinfotest.cpp
 globaltest.cpp
 mimetypefinderjobtest.cpp
 mkpathjobtest.cpp
 threadtest.cpp
 udsentrytest.cpp
 deletejobtest.cpp
 deleteortrashjobtest.cpp
 urlutiltest.cpp
 batchrenamejobtest.cpp
 ksambasharetest.cpp
 krecentdocumenttest.cpp
 filefiltertest.cpp
 NAME_PREFIX "kiocore-"
 LINK_LIBRARIES KF6::KIOCore KF6::I18n KF6::ConfigCore KF6::Service Qt6::Test Qt6::Network Qt6::Xml
)

target_link_libraries(threadtest Qt6::Concurrent)

target_link_libraries(deleteortrashjobtest KF6::KIOWidgets)

ecm_add_test(
    connectionbackendtest.cpp
    ../src/core/connectionbackend.cpp
    ../src/core/kiocoreconnectiondebug.cpp
    TEST_NAME connectionbackendtest
    NAME_PREFIX "kiocore-"
    LINK_LIBRARIES KF6::KIOCore Qt6::Test Qt6::Network KF6::I18n
)

# as per sysadmin request these are limited to linux only! https://invent.kde.org/frameworks/kio/-/merge_requests/1008
if(CMAKE_SYSTEM_NAME STREQUAL "Linux" AND USE_FTPD_WSGIDAV_UNITTEST)
    include(FindGem)
    find_gem(ftpd)
    set_package_properties(Gem_ftpd PROPERTIES TYPE REQUIRED
        DESCRIPTION "Ruby ftpd gem. Either install it from your distribution's repos or by running `gem install --user-install ftpd`, which will install it under your user's home dir."
        PURPOSE "Ruby gem 'ftpd' required for testing the ftp worker."
        URL "https://rubygems.org/gems/ftpd")
    add_definitions(-DRubyExe_EXECUTABLE="${RubyExe_EXECUTABLE}")
    ecm_add_tests(
        ftptest.cpp
        NAME_PREFIX "kiocore-"
        LINK_LIBRARIES KF6::KIOCore KF6::I18n Qt6::Test Qt6::Network
    )

    find_package(WsgidavExe)
    set_package_properties(WsgidavExe PROPERTIES TYPE REQUIRED
        DESCRIPTION "Python webdav server. Can be easily installed by running `pip3 install wsgidav`, which will install it under your user's home dir."
        PURPOSE "wsgidav required for running the webdavtest."
        URL "https://pypi.org/project/WsgiDAV/")
    ecm_add_tests(
        webdavtest.cpp
        NAME_PREFIX "kiocore-"
        LINK_LIBRARIES KF6::KIOCore Qt6::Test Qt6::Network
    )
endif()

if(UNIX)
  ecm_add_tests(
    privilegejobtest.cpp
    NAME_PREFIX "kiocore-"
    LINK_LIBRARIES KF6::KIOCore KF6::I18n Qt6::Test Qt6::Network
  )
endif()

if (TARGET KF6::KIOGui)
  ecm_add_tests(
    favicontest.cpp
    openurljobtest.cpp
    kemailclientlauncherjobtest.cpp
    NAME_PREFIX "kiogui-"
    LINK_LIBRARIES KF6::KIOCore KF6::KIOGui KF6::WindowSystem Qt6::Test
  )

  foreach(_kprocessrunnerTest applicationlauncherjob commandlauncherjob kterminallauncherjob)
    foreach(_systemd "" "SCOPE" "SERVICE")
      set(_scope 0)
      set(_service 0)
      set(_forking 0)
      if(_systemd STREQUAL "SERVICE")
        set(KPR_TEST_TARGET ${_kprocessrunnerTest}_servicetest)
        set(_service 1)
      elseif(_systemd STREQUAL "SCOPE")
        set(KPR_TEST_TARGET ${_kprocessrunnerTest}_scopetest)
        set(_scope 1)
      else()
        set(KPR_TEST_TARGET ${_kprocessrunnerTest}_forkingtest)
        set(_forking 1)
      endif()
      add_executable(${KPR_TEST_TARGET} ${_kprocessrunnerTest}test.cpp)
      target_link_libraries(${KPR_TEST_TARGET} KF6::WindowSystem KF6::KIOCore KF6::KIOGui Qt6::Test)
      ecm_mark_as_test(${KPR_TEST_TARGET})
      add_test(NAME ${KPR_TEST_TARGET} COMMAND ${KPR_TEST_TARGET})
      set_tests_properties(${KPR_TEST_TARGET} PROPERTIES ENVIRONMENT
        "_KDE_APPLICATIONS_AS_SERVICE=${_service};_KDE_APPLICATIONS_AS_SCOPE=${_scope};_KDE_APPLICATIONS_AS_FORKING=${_forking}"
        )
    endforeach()
  endforeach()

  target_link_libraries(favicontest Qt6::Concurrent)
endif()

if (NOT ANDROID)
if (TARGET KF6::KIOWidgets)
ecm_add_tests(
 clipboardupdatertest.cpp
 dropjobtest.cpp
 kdynamicjobtrackernowidgetstest.cpp
 desktopexecparsertest.cpp
 kdirlistertest.cpp
 kdirmodeltest.cpp
 kfileitemactionstest.cpp
 fileundomanagertest.cpp
 kurlcompletiontest.cpp
 ${jobguitest_SRC}
 pastetest.cpp
 kurifiltersearchprovideractionstest.cpp
 NAME_PREFIX "kiowidgets-"
 LINK_LIBRARIES KF6::KIOCore KF6::KIOWidgets Qt6::Test ${DBUS_LIB}
)

set(_libexec_dir_kf "${KDKE_INSTALL_FULL_LIBEXECDIR_KF}")
target_compile_definitions(desktopexecparsertest PRIVATE "KDE_INSTALL_FULL_LIBEXECDIR_KF=${_libexec_dir_kf}")

set(kurifiltertest_libs KF6::KIOCore KF6::KIOWidgets Qt6::Test ${DBUS_LIB})

set(URIFILTER_COLON_TEST kiowidgets-kurifiltertest-colon-separator)
ecm_add_test(kurifiltertest.cpp
    LINK_LIBRARIES ${kurifiltertest_libs}
    TEST_NAME ${URIFILTER_COLON_TEST}
)
target_compile_definitions(${URIFILTER_COLON_TEST} PUBLIC "-DWEBSHORTCUT_SEPARATOR=':'")

set(URIFILTER_SPACE_TEST kiowidgets-kurifiltertest-space-separator)
ecm_add_test(kurifiltertest.cpp
    LINK_LIBRARIES ${kurifiltertest_libs}
    TEST_NAME ${URIFILTER_SPACE_TEST}
)
target_compile_definitions(${URIFILTER_SPACE_TEST} PUBLIC "-DWEBSHORTCUT_SEPARATOR=' '")

# Same as kurlcompletiontest, but with immediate return, and results posted by thread later
ecm_add_test(
 kurlcompletiontest.cpp
 TEST_NAME kurlcompletiontest-nowait
 NAME_PREFIX "kiowidgets-"
 LINK_LIBRARIES KF6::KIOCore KF6::KIOWidgets Qt6::Test
 )
set_target_properties(kurlcompletiontest-nowait PROPERTIES COMPILE_FLAGS "-DNO_WAIT")


endif()

if (TARGET KF6::KIOFileWidgets)
include_directories(${CMAKE_SOURCE_DIR}/src/filewidgets ${CMAKE_BINARY_DIR}/src/filewidgets)
ecm_add_tests(
 kurlnavigatortest.cpp
 kurlcomboboxtest.cpp
 kdiroperatortest.cpp
 kfilewidgettest.cpp
 kfilecustomdialogtest.cpp
 knewfilemenutest.cpp
 kfilecopytomenutest.cpp
 kfileplacesmodeltest.cpp
 kfileplacesviewtest.cpp
 kurlrequestertest.cpp
 kfilefiltercombotest.cpp
 NAME_PREFIX "kiofilewidgets-"
 LINK_LIBRARIES KF6::KIOFileWidgets KF6::KIOWidgets KF6::Bookmarks Qt6::Test KF6::I18n KF6::WindowSystem
)

# TODO: fix symbol exports for windows -> 'KSambaShare::KSambaShare': inconsistent dll linkage
if (NOT WIN32)
ecm_add_test(
    ksambashareprivatetest.cpp
    ../src/core/ksambashare.cpp
    ../src/core/kiocoredebug.cpp
    TEST_NAME ksambashareprivatetest
    NAME_PREFIX "kiocore-"
    LINK_LIBRARIES KF6::KIOCore Qt6::Test Qt6::Network
)
endif()

set_tests_properties(kiofilewidgets-kfileplacesmodeltest PROPERTIES RUN_SERIAL TRUE)
set_tests_properties(kiofilewidgets-kfileplacesviewtest PROPERTIES RUN_SERIAL TRUE)
set_tests_properties(kiowidgets-kurifiltertest-space-separator PROPERTIES RUN_SERIAL TRUE)
endif()
endif()

# this should be done by cmake, see bug 371721
if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang" OR CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    set_property(TARGET jobtest APPEND PROPERTY AUTOMOC_MOC_OPTIONS --include ${CMAKE_BINARY_DIR}/src/core/moc_predefs.h)
endif()

# Benchmark, compiled, but not run automatically with ctest
add_executable(kcoredirlister_benchmark kcoredirlister_benchmark.cpp)
target_link_libraries(kcoredirlister_benchmark KF6::KIOCore KF6::KIOWidgets Qt6::Test)

add_executable(udsentry_api_comparison_benchmark udsentry_api_comparison_benchmark.cpp)
target_link_libraries(udsentry_api_comparison_benchmark KF6::KIOCore KF6::KIOWidgets Qt6::Test)

add_executable(udsentry_benchmark udsentry_benchmark.cpp)
target_link_libraries(udsentry_benchmark KF6::KIOCore KF6::KIOWidgets Qt6::Test)
