
set(dropnewintonewfolderPlugin_SRCS dropnewintonewfolderPlugin.cpp)

kcoreaddons_add_plugin(dropnewintonewfolder
    SOURCES ${dropnewintonewfolderPlugin_SRCS}
    INSTALL_NAMESPACE kf6/kio_dnd)

target_link_libraries(dropnewintonewfolder KF6::KIOWidgets KF6::KIOFileWidgets KF6::I18n)
