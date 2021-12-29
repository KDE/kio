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
        KF5::DBusAddons
        KF5::KIOCore
        KF5::WidgetsAddons
        KF5::WindowSystem
        KF5::I18n
        Qt${QT_MAJOR_VERSION}::Core
        Qt${QT_MAJOR_VERSION}::Test
        ${WALLET_LIB}
)
