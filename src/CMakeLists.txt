add_subdirectory(core)

# KIOCore-only executables
if (NOT ANDROID)
    add_subdirectory(kioworkers)
    add_subdirectory(schemehandlers)
endif()

if (HAVE_QTDBUS)
    add_subdirectory(kiod)
    add_subdirectory(kssld)
endif()
add_subdirectory(kioworker)

if (NOT KIOCORE_ONLY)
add_subdirectory(gui)
add_subdirectory(widgets)

if (HAVE_QTDBUS)
    add_subdirectory(kpasswdserver)
    add_subdirectory(kioexec)
endif()

if (NOT ANDROID)
    add_subdirectory(filewidgets)
    add_subdirectory(urifilters)
endif()

set(NON_KIOCORE_LINK_QCHS
    Qt6Widgets_QCH
    Qt6Network_QCH
    KF6Completion_QCH
    KF6WidgetsAddons_QCH
    KF6JobWidgets_QCH
    KF6Bookmarks_QCH
    KF6ItemViews_QCH
    KF6Solid_QCH
    KF6WindowSystem_QCH
)

if(BUILD_DESIGNERPLUGIN AND HAVE_QTDBUS)
    add_subdirectory(designer)
endif()

endif()

ecm_qt_install_logging_categories(
    EXPORT KIO
    FILE kio.categories
    DESTINATION ${KDE_INSTALL_LOGGINGCATEGORIESDIR}
)

if (BUILD_QCH)
    macro(_make_absolute var_name base_path)
        set(_result)
        foreach(_path ${${var_name}})
            if(IS_ABSOLUTE "${_path}")
                list(APPEND _result "${_path}")
            else()
                list(APPEND _result "${base_path}/${_path}")
            endif()
        endforeach()
        set(${var_name} ${_result})
    endmacro()

    _make_absolute(KIOCore_QCH_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/core")
    _make_absolute(KIOGui_QCH_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/gui")
    _make_absolute(KIOWidgets_QCH_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/widgets")
    _make_absolute(KIOFileWidgets_QCH_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/filewidgets")

    ecm_add_qch(
        KF6KIO_QCH
        NAME KIO
        BASE_NAME KF6KIO
        VERSION ${KF_VERSION}
        ORG_DOMAIN org.kde
        SOURCES # using only public headers, to cover only public API
            ${KIOCore_QCH_SOURCES}
            ${KIOGui_QCH_SOURCES}
            ${KIOWidgets_QCH_SOURCES}
            ${KIOFileWidgets_QCH_SOURCES}
        MD_MAINPAGE "${CMAKE_SOURCE_DIR}/README.md"
        IMAGE_DIRS "${CMAKE_SOURCE_DIR}/docs/pics"
        LINK_QCHS
            KF6CoreAddons_QCH
            KF6Service_QCH
            ${NON_KIOCORE_LINK_QCHS}
        BLANK_MACROS
            KIOCORE_EXPORT
            KIOCORE_DEPRECATED_EXPORT
            KIOCORE_DEPRECATED
            "KIOCORE_DEPRECATED_VERSION(x, y, t)"
            "KIOCORE_DEPRECATED_VERSION_BELATED(x, y, xt, yt, t)"
            "KIOCORE_ENUMERATOR_DEPRECATED_VERSION(x, y, t)"
            "KIOCORE_ENUMERATOR_DEPRECATED_VERSION_BELATED(x, y, xt, yt, t)"
            KIOGUI_EXPORT
            KIOGUI_DEPRECATED_EXPORT
            KIOGUI_DEPRECATED
            "KIOGUI_DEPRECATED_VERSION(x, y, t)"
            "KIOGUI_DEPRECATED_VERSION_BELATED(x, y, xt, yt, t))"
            "KIOGUI_ENUMERATOR_DEPRECATED_VERSION(x, y, t)"
            "KIOGUI_ENUMERATOR_DEPRECATED_VERSION_BELATED(x, y, xt, yt, t))"
            KIOWIDGETS_EXPORT
            KIOWIDGETS_DEPRECATED_EXPORT
            KIOWIDGETS_DEPRECATED
            "KIOWIDGETS_DEPRECATED_VERSION(x, y, t)"
            "KIOWIDGETS_DEPRECATED_VERSION_BELATED(x, y, xt, yt, t)"
            "KIOWIDGETS_ENUMERATOR_DEPRECATED_VERSION(x, y, t)"
            "KIOWIDGETS_ENUMERATOR_DEPRECATED_VERSION_BELATED(x, y, xt, yt, t)"
            KIOFILEWIDGETS_EXPORT
            KIOFILEWIDGETS_DEPRECATED_EXPORT
            KIOFILEWIDGETS_DEPRECATED
            "KIOFILEWIDGETS_DEPRECATED_VERSION(x, y, t)"
            "KIOFILEWIDGETS_DEPRECATED_VERSION_BELATED(x, y, xt, yt, t))"
            "KIOFILEWIDGETS_ENUMERATOR_DEPRECATED_VERSION(x, y, t)"
            "KIOFILEWIDGETS_ENUMERATOR_DEPRECATED_VERSION_BELATED(x, y, xt, yt, t))"
        PREDEFINED_MACROS
            "KIOCORE_ENABLE_DEPRECATED_SINCE(x, y)=1"
            "KIOCORE_BUILD_DEPRECATED_SINCE(x, y)=1"
            "KIOGUI_ENABLE_DEPRECATED_SINCE(x, y)=1"
            "KIOGUI_BUILD_DEPRECATED_SINCE(x, y)=1"
            "KIOWIDGETS_ENABLE_DEPRECATED_SINCE(x, y)=1"
            "KIOWIDGETS_BUILD_DEPRECATED_SINCE(x, y)=1"
            "KIOFILEWIDGETS_ENABLE_DEPRECATED_SINCE(x, y)=1"
            "KIOFILEWIDGETS_BUILD_DEPRECATED_SINCE(x, y)=1"
        TAGFILE_INSTALL_DESTINATION ${KDE_INSTALL_QTQCHDIR}
        QCH_INSTALL_DESTINATION ${KDE_INSTALL_QTQCHDIR}
        COMPONENT Devel
    )
endif()
