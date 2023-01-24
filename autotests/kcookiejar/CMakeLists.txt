find_package(Qt6Test REQUIRED)

include(ECMAddTests)

########### next target ###############


# linking to Qt6::Gui is only needed for the include paths
ecm_add_test(kcookiejartest.cpp
    NAME_PREFIX "kioworker-"
    LINK_LIBRARIES  Qt6::Test Qt6::Gui KF6::KIOCore KF6::ConfigCore
)

########### install files ###############





