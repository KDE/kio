remove_definitions(-DQT_NO_CAST_FROM_ASCII)
remove_definitions(-DQT_NO_CAST_FROM_BYTEARRAY)

include(ECMAddTests)

add_subdirectory(http)
add_subdirectory(kcookiejar)

find_package(Qt5Widgets REQUIRED)

########### unittests ###############

find_package(Qt5Concurrent ${REQUIRED_QT_VERSION} REQUIRED NO_MODULE)

if(NOT EXCLUDE_DEPRECATED_BEFORE_AND_AT STREQUAL "CURRENT" AND
   EXCLUDE_DEPRECATED_BEFORE_AND_AT VERSION_LESS 5.65.0)
    set(ktcpsockettest_SRC ktcpsockettest.cpp)
endif()

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
 ${ktcpsockettest_SRC}
 globaltest.cpp
 mkpathjobtest.cpp
 threadtest.cpp
 udsentrytest.cpp
 deletejobtest.cpp
 urlutiltest.cpp
 batchrenamejobtest.cpp
 ksambasharetest.cpp
 NAME_PREFIX "kiocore-"
 LINK_LIBRARIES KF5::KIOCore KF5::I18n Qt5::Test Qt5::Network
)

target_link_libraries(threadtest Qt5::Concurrent)

ecm_add_test(
    http_jobtest.cpp
    httpserver_p.cpp
    TEST_NAME http_jobtest
    NAME_PREFIX "kiocore-"
    LINK_LIBRARIES KF5::KIOCore KF5::I18n Qt5::Test Qt5::Network
)

include(FindGem)
find_gem(ftpd)
set_package_properties(Gem_ftpd PROPERTIES
    DESCRIPTION "Ruby gem 'ftpd' required for testing the ftp slave.")

if(Gem_ftpd_FOUND)
    add_definitions(-DRubyExe_EXECUTABLE="${RubyExe_EXECUTABLE}")
    ecm_add_tests(
        ftptest.cpp
        NAME_PREFIX "kiocore-"
        LINK_LIBRARIES KF5::KIOCore KF5::I18n Qt5::Test Qt5::Network
    )
endif()

find_package(WsgidavExe)
set_package_properties(Gem_ftpd PROPERTIES
    DESCRIPTION "wsgidav required for running the webdavtest.")
if(WsgidavExe_FOUND)
    ecm_add_tests(
        webdavtest.cpp
        NAME_PREFIX "kiocore-"
        LINK_LIBRARIES KF5::KIOCore Qt5::Test Qt5::Network
    )
endif()

if(UNIX)
  ecm_add_tests(
    privilegejobtest.cpp
    NAME_PREFIX "kiocore-"
    LINK_LIBRARIES KF5::KIOCore KF5::I18n Qt5::Test Qt5::Network
  )
endif()

if (TARGET KF5::KIOGui)
  ecm_add_tests(
    favicontest.cpp
    openurljobtest.cpp
    NAME_PREFIX "kiogui-"
    LINK_LIBRARIES KF5::KIOCore KF5::KIOGui Qt5::Test
  )

  foreach(_kprocessrunnerTest applicationlauncherjob commandlauncherjob)
    foreach(_systemd "" "SCOPE" "SERVICE")
      set(_scope 0)
      set(_service 0)
      if(_systemd STREQUAL "SERVICE")
        set(KPR_TEST_TARGET ${_kprocessrunnerTest}_servicetest)
        set(_service 1)
      elseif(_systemd STREQUAL "SCOPE")
        set(KPR_TEST_TARGET ${_kprocessrunnerTest}_scopetest)
        set(_scope 1)
      else()
        set(KPR_TEST_TARGET ${_kprocessrunnerTest}_forkingtest)
      endif()
      add_executable(${KPR_TEST_TARGET} ${_kprocessrunnerTest}test.cpp)
      target_link_libraries(${KPR_TEST_TARGET} KF5::KIOCore KF5::KIOGui Qt5::Test)
      ecm_mark_as_test(${KPR_TEST_TARGET})
      add_test(NAME ${KPR_TEST_TARGET} COMMAND ${KPR_TEST_TARGET})
      set_tests_properties(${KPR_TEST_TARGET} PROPERTIES ENVIRONMENT
        "KDE_APPLICATIONS_AS_SERVICE=${_service};KDE_APPLICATIONS_AS_SCOPE=${_scope}")
    endforeach()
  endforeach()

  target_link_libraries(favicontest Qt5::Concurrent)
endif()

if (TARGET KF5::KIOWidgets)
if(NOT EXCLUDE_DEPRECATED_BEFORE_AND_AT STREQUAL "CURRENT" AND
   EXCLUDE_DEPRECATED_BEFORE_AND_AT VERSION_LESS 5.4.0)
    set(jobguitest_SRC jobguitest.cpp)
endif()

ecm_add_tests(
 clipboardupdatertest.cpp
 dropjobtest.cpp
 kdynamicjobtrackernowidgetstest.cpp
 krununittest.cpp
 kdirlistertest.cpp
 kdirmodeltest.cpp
 kfileitemactionstest.cpp
 fileundomanagertest.cpp
 kurlcompletiontest.cpp
 ${jobguitest_SRC}
 pastetest.cpp
 accessmanagertest.cpp
 kurifiltersearchprovideractionstest.cpp
 NAME_PREFIX "kiowidgets-"
 LINK_LIBRARIES KF5::KIOCore KF5::KIOWidgets Qt5::Test Qt5::DBus
)
set_target_properties(krununittest PROPERTIES COMPILE_FLAGS "-DKDE_INSTALL_FULL_LIBEXECDIR_KF5=\"\\\"${KDKE_INSTALL_FULL_LIBEXECDIR_KF5}\\\"\"")

set(URIFILTER_COLON_TEST kiowidgets-kurifiltertest-colon-separator)
add_executable(${URIFILTER_COLON_TEST} kurifiltertest.cpp)
target_link_libraries(${URIFILTER_COLON_TEST} KF5::KIOCore KF5::KIOWidgets Qt5::Test Qt5::DBus)
ecm_mark_as_test(${URIFILTER_COLON_TEST})
add_test(NAME ${URIFILTER_COLON_TEST} COMMAND ${URIFILTER_COLON_TEST})
target_compile_definitions(${URIFILTER_COLON_TEST} PUBLIC "-DWEBSHORTCUT_SEPARATOR=':'")

set(URIFILTER_SPACE_TEST kiowidgets-kurifiltertest-space-separator)
add_executable(${URIFILTER_SPACE_TEST} kurifiltertest.cpp)
target_link_libraries(${URIFILTER_SPACE_TEST} KF5::KIOCore KF5::KIOWidgets Qt5::Test Qt5::DBus)
ecm_mark_as_test(${URIFILTER_SPACE_TEST})
add_test(NAME ${URIFILTER_SPACE_TEST} COMMAND ${URIFILTER_SPACE_TEST})
target_compile_definitions(${URIFILTER_SPACE_TEST} PUBLIC "-DWEBSHORTCUT_SEPARATOR=' '")

# Same as accessmanagertest, but using QNetworkAccessManager, to make sure we
# behave the same
ecm_add_test(
 accessmanagertest.cpp
 TEST_NAME accessmanagertest-qnam
 NAME_PREFIX "kiowidgets-"
 LINK_LIBRARIES KF5::KIOCore KF5::KIOWidgets Qt5::Test
)
set_target_properties(accessmanagertest-qnam PROPERTIES COMPILE_FLAGS "-DUSE_QNAM")

# Same as kurlcompletiontest, but with immediate return, and results posted by thread later
ecm_add_test(
 kurlcompletiontest.cpp
 TEST_NAME kurlcompletiontest-nowait
 NAME_PREFIX "kiowidgets-"
 LINK_LIBRARIES KF5::KIOCore KF5::KIOWidgets Qt5::Test
 )
set_target_properties(kurlcompletiontest-nowait PROPERTIES COMPILE_FLAGS "-DNO_WAIT")


endif()

if (TARGET KF5::KIOFileWidgets)
find_package(KF5XmlGui ${KF_DEP_VERSION} REQUIRED)
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
 NAME_PREFIX "kiofilewidgets-"
 LINK_LIBRARIES KF5::KIOFileWidgets KF5::KIOWidgets KF5::XmlGui KF5::Bookmarks Qt5::Test KF5::I18n
)

# TODO: fix symbol exports for windows -> 'KSambaShare::KSambaShare': inconsistent dll linkage 
if (NOT WIN32)
ecm_add_test(
    ksambashareprivatetest.cpp
    ../src/core/ksambashare.cpp
    ../src/core/kiocoredebug.cpp
    TEST_NAME ksambashareprivatetest
    NAME_PREFIX "kiocore-"
    LINK_LIBRARIES KF5::KIOCore Qt5::Test Qt5::Network
)
endif()

set_tests_properties(kiofilewidgets-kfileplacesmodeltest PROPERTIES RUN_SERIAL TRUE)
set_tests_properties(kiofilewidgets-kfileplacesviewtest PROPERTIES RUN_SERIAL TRUE)
set_tests_properties(kiowidgets-kurifiltertest-space-separator PROPERTIES RUN_SERIAL TRUE)
endif()

# this should be done by cmake, see bug 371721
if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang" OR CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND Qt5Core_VERSION VERSION_GREATER 5.8.0)
    set_property(TARGET jobtest APPEND PROPERTY AUTOMOC_MOC_OPTIONS --include ${CMAKE_BINARY_DIR}/src/core/moc_predefs.h)
endif()

# Benchmark, compiled, but not run automatically with ctest
add_executable(kcoredirlister_benchmark kcoredirlister_benchmark.cpp)
target_link_libraries(kcoredirlister_benchmark KF5::KIOCore KF5::KIOWidgets Qt5::Test)

add_executable(udsentry_api_comparison_benchmark udsentry_api_comparison_benchmark.cpp)
target_link_libraries(udsentry_api_comparison_benchmark KF5::KIOCore KF5::KIOWidgets Qt5::Test)

add_executable(udsentry_benchmark udsentry_benchmark.cpp)
target_link_libraries(udsentry_benchmark KF5::KIOCore KF5::KIOWidgets Qt5::Test)
