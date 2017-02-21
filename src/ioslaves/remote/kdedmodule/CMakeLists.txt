add_library(remotedirnotify MODULE remotedirnotify.cpp remotedirnotifymodule.cpp ../debug.cpp)
kcoreaddons_desktop_to_json(remotedirnotify remotedirnotify.desktop)

target_link_libraries(remotedirnotify KF5::DBusAddons KF5::KIOCore)
# Rename target to allow co-installability with plasma-workspace <= 5.9
set_target_properties(remotedirnotify PROPERTIES OUTPUT_NAME "remotenotifier")

install(TARGETS remotedirnotify DESTINATION ${KDE_INSTALL_PLUGINDIR}/kf5/kded )
