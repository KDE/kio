
set(dropnewintonewfolderPlugin_SRCS dropintonewfolderPlugin.cpp)

kcoreaddons_add_plugin(dropintonewfolder
    SOURCES ${dropintonewfolderPlugin_SRCS}
    INSTALL_NAMESPACE kf6/kio_dnd)

target_link_libraries(dropintonewfolder KF6::KIOWidgets KF6::KIOFileWidgets KF6::I18n)
