# SPDX-FileCopyrightText: 2023 Nicolas Fella <nicolas.fella@gmx.de>
# SPDX-License-Identifier: BSD-3-Clause

add_executable(sslerrortest sslerrortest.cpp)
target_link_libraries(sslerrortest PRIVATE Qt::Network KF6::KIOWidgets)

add_executable(gettest gettest.cpp)
target_link_libraries(gettest PRIVATE Qt::Network Qt::Test KF6::KIOCore)

add_executable(headtest headtest.cpp)
target_link_libraries(headtest PRIVATE Qt::Network Qt::Test KF6::KIOCore)

add_executable(redirecttest redirecttest.cpp)
target_link_libraries(redirecttest PRIVATE Qt::Network Qt::Test KF6::KIOCore)

add_executable(puttest puttest.cpp)
target_link_libraries(puttest PRIVATE Qt::Network Qt::Test KF6::KIOCore)

add_executable(posttest posttest.cpp)
target_link_libraries(posttest PRIVATE Qt::Network Qt::Test KF6::KIOCore)

add_executable(stattest stattest.cpp)
target_link_libraries(stattest PRIVATE Qt::Network Qt::Test KF6::KIOCore)

add_executable(cookiestest cookiestest.cpp)
target_link_libraries(cookiestest PRIVATE Qt::Network Qt::Test KF6::KIOCore)

add_executable(authtest authtest.cpp)
target_link_libraries(authtest PRIVATE Qt::Network Qt::Test KF6::KIOCore)

add_executable(propagateheaderstest propagateheaderstest.cpp)
target_link_libraries(propagateheaderstest PRIVATE Qt::Network Qt::Test KF6::KIOCore)

add_executable(useragenttest useragenttest.cpp)
target_link_libraries(useragenttest PRIVATE Qt::Network Qt::Test KF6::KIOCore)

add_executable(errortest errortest.cpp)
target_link_libraries(errortest PRIVATE Qt::Network Qt::Test KF6::KIOCore)

add_executable(accepttest accepttest.cpp)
target_link_libraries(accepttest PRIVATE Qt::Network Qt::Test KF6::KIOCore)

add_executable(referrertest referrertest.cpp)
target_link_libraries(referrertest PRIVATE Qt::Network Qt::Test KF6::KIOCore)

add_executable(customheadertest customheadertest.cpp)
target_link_libraries(customheadertest PRIVATE Qt::Network Qt::Test KF6::KIOCore)

add_executable(responsecodetest responsecodetest.cpp)
target_link_libraries(responsecodetest PRIVATE Qt::Network Qt::Test KF6::KIOCore)

add_executable(contentlengthtest contentlengthtest.cpp)
target_link_libraries(contentlengthtest PRIVATE Qt::Network Qt::Test KF6::KIOCore)


# The tests above all talk to httpserver.py. ctest brings it up once for the lot of them and takes
# it down afterwards, so that they run the way the rest of the suite does rather than only by hand.
# Where flask is not installed there is nothing to run them against, so they are left out instead
# of failing.
find_package(Python3 COMPONENTS Interpreter)

set(HAVE_HTTP_TEST_SERVER FALSE)
if(Python3_Interpreter_FOUND AND CMAKE_SYSTEM_NAME STREQUAL "Linux")
    execute_process(
        COMMAND ${Python3_EXECUTABLE} -c "import flask, flask_httpauth"
        RESULT_VARIABLE _flaskResult
        OUTPUT_QUIET
        ERROR_QUIET)
    if(_flaskResult EQUAL 0)
        set(HAVE_HTTP_TEST_SERVER TRUE)
    endif()
endif()

add_feature_info(HttpTestServer HAVE_HTTP_TEST_SERVER "python flask and flask_httpauth, for the http worker tests")

if(HAVE_HTTP_TEST_SERVER)
    set(httpTestServerControl ${CMAKE_CURRENT_SOURCE_DIR}/httpserver-control.py)
    set(httpTestServerPidFile ${CMAKE_CURRENT_BINARY_DIR}/httpserver.pid)
    set(httpTestServerPort 5000)

    add_test(NAME http-server-start COMMAND ${Python3_EXECUTABLE} ${httpTestServerControl} start ${httpTestServerPort} ${httpTestServerPidFile})
    set_tests_properties(http-server-start PROPERTIES FIXTURES_SETUP httpTestServer)

    add_test(NAME http-server-stop COMMAND ${Python3_EXECUTABLE} ${httpTestServerControl} stop ${httpTestServerPidFile})
    set_tests_properties(http-server-stop PROPERTIES FIXTURES_CLEANUP httpTestServer)

    # Left out on purpose: authtest, which needs a password from kpasswdserver and gets none in a
    # test environment, and responsecodetest, which asks for a 404 to arrive without a job error
    # while KIO reports ERR_DOES_NOT_EXIST. Both fail the same way without any of this.
    set(HTTP_TESTS
        accepttest
        contentlengthtest
        cookiestest
        customheadertest
        errortest
        gettest
        headtest
        posttest
        propagateheaderstest
        puttest
        redirecttest
        referrertest
        stattest
        useragenttest)

    foreach(httpTest ${HTTP_TESTS})
        add_test(NAME ${httpTest} COMMAND ${httpTest})
        set_tests_properties(${httpTest} PROPERTIES
            FIXTURES_REQUIRED httpTestServer
            # One server, one port, and cookiestest leaves cookies behind, so they take their turn.
            RESOURCE_LOCK httpTestServer
            # Talk to the worker that was just built, not to one that happens to be installed.
            ENVIRONMENT "QT_PLUGIN_PATH=${CMAKE_BINARY_DIR}/bin")
    endforeach()
endif()
