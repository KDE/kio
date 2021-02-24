add_subdirectory( kdedmodule ) 

set(kio_remote_SRCS
    kio_remote.cpp
    remoteimpl.cpp
    )

kcoreaddons_add_plugin(kio_remote
    SOURCES ${kio_remote_SRCS}
    INSTALL_NAMESPACE kf5/kio)
ecm_generate_qloggingcategories_source(kio_remote
    HEADER debug.h
    GROUP KIOREMOTE
)
target_link_libraries(kio_remote KF5::KIOCore KF5::I18n Qt5::Network)

set_target_properties(kio_remote PROPERTIES OUTPUT_NAME "remote")
