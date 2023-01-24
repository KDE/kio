include(ECMAddTests)

include_directories( ${CMAKE_CURRENT_SOURCE_DIR}/.. ${CMAKE_CURRENT_BINARY_DIR}/.. )

set (kpasswdservertest_dbus_SRCS)
qt_add_dbus_adaptor(kpasswdservertest_dbus_SRCS ../../core/org.kde.KPasswdServer.xml kpasswdserver.h KPasswdServer)

ecm_add_test(
    ${kpasswdservertest_dbus_SRCS}
    kpasswdservertest.cpp
    ../kpasswdserver.cpp
    TEST_NAME kpasswdservertest
    LINK_LIBRARIES
        KF6::DBusAddons
        KF6::KIOCore
        KF6::WidgetsAddons
        KF6::WindowSystem
        KF6::I18n
        Qt6::Core
        Qt6::Test
        ${WALLET_LIB}
)
