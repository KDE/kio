include(ECMAddQtDesignerPlugin)

ecm_qtdesignerplugin_widget(KImageFilePreview
    TOOLTIP "A widget that provides a image preview of a URL"
    GROUP "Display (KF6)"
)
ecm_qtdesignerplugin_widget(KUrlComboRequester
    INCLUDE_FILE "kurlrequester.h"
    TOOLTIP "URL Requester (KF6)"
    WHATSTHIS "A widget holding a combobox and a pushbutton. A \"file open\" dialog opened when clicking on the pushbutton."
    GROUP "Input (KF6)"
)
ecm_qtdesignerplugin_widget(KUrlRequester
    TOOLTIP "URL Requester (KF6)"
    WHATSTHIS "A widget holding a lineedit and a pushbutton. A \"file open\" dialog opened when clicking on the pushbutton."
    GROUP "Input (KF6)"
)

ecm_add_qtdesignerplugin(kiowidgets
    NAME KIOWidgets
    OUTPUT_NAME kio6widgets
    WIDGETS
        KImageFilePreview
        KUrlComboRequester
        KUrlRequester
    LINK_LIBRARIES
        KF6::KIOWidgets
        KF6::KIOFileWidgets
    INSTALL_DESTINATION "${KDE_INSTALL_QTPLUGINDIR}/designer"
    COMPONENT Devel
)
