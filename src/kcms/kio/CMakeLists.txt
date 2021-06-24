
find_package(KF5TextWidgets ${KF_DEP_VERSION} REQUIRED)

add_library(kcm_kio MODULE)

target_sources(kcm_kio PRIVATE
    main.cpp
    kcookiesmain.cpp
    kcookiespolicies.cpp
    kcookiesmanagement.cpp
    kcookiespolicyselectiondlg.cpp
    smbrodlg.cpp
    kproxydlg.cpp
    netpref.cpp
    ksaveioconfig.cpp)

ki18n_wrap_ui(kcm_kio
    kproxydlg.ui
    kcookiespolicies.ui
    kcookiesmanagement.ui
    kcookiespolicyselectiondlg.ui)

target_link_libraries(kcm_kio
  PUBLIC
    Qt5::DBus
    KF5::ConfigCore
    KF5::ConfigGui
    KF5::ConfigWidgets
    KF5::KIOCore
    KF5::KIOWidgets
    KF5::ItemViews   # KTreeWidgetSearchLine
    KF5::TextWidgets # KPluralHandlingSpinBox
  PRIVATE
    KF5::I18n)

install(TARGETS kcm_kio  DESTINATION ${KDE_INSTALL_PLUGINDIR} )

########### install files ###############

install( FILES smb.desktop cookies.desktop
    netpref.desktop proxy.desktop  DESTINATION  ${KDE_INSTALL_KSERVICES5DIR} )
