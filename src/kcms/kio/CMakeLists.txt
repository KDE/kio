
add_subdirectory( uasproviders )

find_package(KF5TextWidgets ${KF_DEP_VERSION} REQUIRED)

########### next target ###############

set(kcm_kio_PART_SRCS
    main.cpp
    kcookiesmain.cpp
    kcookiespolicies.cpp
    kcookiesmanagement.cpp
    kcookiespolicyselectiondlg.cpp
    smbrodlg.cpp
    useragentdlg.cpp
    kproxydlg.cpp
    useragentinfo.cpp
    useragentselectordlg.cpp
    netpref.cpp
    cache.cpp
    ksaveioconfig.cpp)

ki18n_wrap_ui(kcm_kio_PART_SRCS
    cache.ui
    useragentselectordlg.ui
    useragentdlg.ui
    kproxydlg.ui
    kcookiespolicies.ui
    kcookiesmanagement.ui
    kcookiespolicyselectiondlg.ui)

ecm_qt_export_logging_category(
    IDENTIFIER KIO_USERAGENTDLG
    CATEGORY_NAME kf.configwidgets.cms.kf.kio.useragentdlg
    OLD_CATEGORY_NAMES kf5.kio.useragentdlg
    DESCRIPTION "kio useragentdialog (KIO)"
    EXPORT KIO
)

add_library(kcm_kio MODULE ${kcm_kio_PART_SRCS})

target_link_libraries(kcm_kio
  PUBLIC
    Qt5::DBus
    KF5::ConfigCore
    KF5::ConfigGui
    KF5::ConfigWidgets
    KF5::Completion
    KF5::KIOCore
    KF5::KIOWidgets
    KF5::IconThemes
    KF5::ItemViews   # KTreeWidgetSearchLine
    KF5::TextWidgets # KPluralHandlingSpinBox
  PRIVATE
    KF5::I18n)

install(TARGETS kcm_kio  DESTINATION ${KDE_INSTALL_PLUGINDIR} )

########### install files ###############

install( FILES smb.desktop cookies.desktop useragent.desktop cache.desktop
    netpref.desktop proxy.desktop  DESTINATION  ${KDE_INSTALL_KSERVICES5DIR} )
install( FILES uasprovider.desktop  DESTINATION  ${KDE_INSTALL_KSERVICETYPES5DIR} )
