# Name without version postfix, as namespace by installation in kf-versioned subdir on shared systems
add_executable(kioworker)
ecm_mark_nongui_executable(kioworker)

target_sources(kioworker PRIVATE
    kioworker.cpp
)

target_link_libraries(kioworker
   KF6::KIOCore
)

install(TARGETS kioworker DESTINATION ${KDE_INSTALL_LIBEXECDIR_KF})
