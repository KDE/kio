find_package(Qt${QT_MAJOR_VERSION}Test REQUIRED)

include(ECMAddTests)

########### next target ###############


# linking to Qt${QT_MAJOR_VERSION}::Gui is only needed for the include paths
ecm_add_test(kcookiejartest.cpp
    NAME_PREFIX "kioslave-"
    LINK_LIBRARIES  Qt${QT_MAJOR_VERSION}::Test Qt${QT_MAJOR_VERSION}::Gui KF5::KIOCore KF5::ConfigCore
)

########### install files ###############





