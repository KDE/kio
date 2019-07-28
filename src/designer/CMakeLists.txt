include(ECMAddQtDesignerPlugin)

ecm_qtdesignerplugin_widget(KImageFilePreview
    TOOLTIP "A widget that provides a image preview of a URL"
    GROUP "Display (KF5)"
)
ecm_qtdesignerplugin_widget(KUrlComboRequester
    INCLUDE_FILE "kurlrequester.h"
    TOOLTIP "URL Requester (KF5)"
    WHATSTHIS "A widget holding a combobox and a pushbutton. A \"file open\" dialog opened when clicking on the pushbutton."
    GROUP "Input (KF5)"
)
ecm_qtdesignerplugin_widget(KUrlRequester
    TOOLTIP "URL Requester (KF5)"
    WHATSTHIS "A widget holding a lineedit and a pushbutton. A \"file open\" dialog opened when clicking on the pushbutton."
    GROUP "Input (KF5)"
)

ecm_add_qtdesignerplugin(kiowidgets
    NAME KIOWidgets
    OUTPUT_NAME kio5widgets
    WIDGETS
        KImageFilePreview
        KUrlComboRequester
        KUrlRequester
    LINK_LIBRARIES
        KF5::KIOWidgets
        KF5::KIOFileWidgets
    INSTALL_DESTINATION "${KDE_INSTALL_QTPLUGINDIR}/designer"
    COMPONENT Devel
)
