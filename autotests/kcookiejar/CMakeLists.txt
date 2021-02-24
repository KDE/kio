find_package(Qt5Test REQUIRED)

include(ECMAddTests)

########### next target ###############


# linking to Qt5::Gui is only needed for the include paths
ecm_add_test(kcookiejartest.cpp
    NAME_PREFIX "kioslave-"
    LINK_LIBRARIES  Qt5::Test Qt5::Gui KF5::KIOCore
)
ecm_generate_qloggingcategories_source(kcookiejartest
    HEADER kcookiejar_debug.h
    GROUP KCOOKIEJAR
)

########### install files ###############





