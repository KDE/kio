find_package(Qt5Test REQUIRED)

include(ECMAddTests)

########### next target ###############

ecm_add_test(kcookiejartest.cpp
    NAME_PREFIX "kioslave-"
    LINK_LIBRARIES  Qt5::Test KF5::KIOCore
)

########### install files ###############





