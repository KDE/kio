remove_definitions(-DQT_NO_CAST_FROM_ASCII)

include(ECMMarkAsTest)

########### unittests ###############

find_package(Qt5Concurrent 5.2.0 REQUIRED NO_MODULE)

macro(KIOCORE_UNIT_TESTS)
  foreach(_testname ${ARGN})
    add_executable(${_testname} ${_testname}.cpp)
    add_test(kiocore-${_testname} ${_testname})
    target_link_libraries(${_testname}
       KF5::KIOCore
       KF5::I18n
       Qt5::Test
       Qt5::Network
    )
    ecm_mark_as_test(${_testname})
  endforeach()
endmacro(KIOCORE_UNIT_TESTS)

kiocore_unit_tests(
 kacltest
 listdirtest
 kmountpointtest
 upurltest
 dataprotocoltest
 jobtest
 jobremotetest
 kfileitemtest
 kprotocolinfotest
 ktcpsockettest
 globaltest
 udsentrytest
)

if(UNIX)
  kiocore_unit_tests(
    klocalsockettest
    klocalsocketservertest
  )
endif()

macro(KIOWIDGETS_UNIT_TESTS)
  foreach(_testname ${ARGN})
    add_executable(${_testname} ${_testname}.cpp)
    add_test(kiowidgets-${_testname} ${_testname})
    target_link_libraries(${_testname}
       KF5::KIOCore
       KF5::KIOWidgets
       Qt5::Test
    )
    ecm_mark_as_test(${_testname})
  endforeach()
endmacro(KIOWIDGETS_UNIT_TESTS)

kiowidgets_unit_tests(
 clipboardupdatertest
 krununittest
 kdirlistertest
 kdirmodeltest
 fileundomanagertest
 kurlcompletiontest
 kurlrequestertest
 jobguitest
)

set_target_properties(krununittest PROPERTIES COMPILE_FLAGS "-DCMAKE_INSTALL_PREFIX=\\\"${CMAKE_INSTALL_PREFIX}\\\" -DLIBEXEC_INSTALL_DIR=\\\"${LIBEXEC_INSTALL_DIR}\\\"")

macro(KIOFILEWIDGETS_UNIT_TESTS)
  foreach(_testname ${ARGN})
    add_executable(${_testname} ${_testname}.cpp)
    add_test(kiofilewidgets-${_testname} ${_testname})
    target_link_libraries(${_testname}
       KF5::KIOFileWidgets
       Qt5::Test
    )
    ecm_mark_as_test(${_testname})
  endforeach()
endmacro()

kiofilewidgets_unit_tests(
 kurlnavigatortest
 kdiroperatortest
 knewfilemenutest
 kfileplacesmodeltest
)

