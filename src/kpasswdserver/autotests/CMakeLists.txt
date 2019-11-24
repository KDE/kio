include_directories( ${CMAKE_CURRENT_SOURCE_DIR}/.. ${CMAKE_CURRENT_BINARY_DIR}/.. )

set (kpasswdserver_test_SRCS
 kpasswdservertest.cpp
 ../kpasswdserver.cpp
)
qt5_add_dbus_adaptor(kpasswdserver_test_SRCS ../../core/org.kde.KPasswdServer.xml kpasswdserver.h KPasswdServer)

include(ECMAddTests)

ecm_add_test(${kpasswdserver_test_SRCS}
             TEST_NAME kpasswdservertest
             LINK_LIBRARIES KF5::DBusAddons KF5::KIOCore KF5::WidgetsAddons KF5::WindowSystem KF5::I18n Qt5::Core Qt5::Test ${WALLET_LIB})
