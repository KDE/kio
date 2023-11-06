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
