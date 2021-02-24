ecm_declare_qloggingcategory(
    CATEGORY_NAME kf.kio.core
    OLD_CATEGORY_NAMES kf5.kio.core
    IDENTIFIER KIO_CORE
    DESCRIPTION "KIOCore (KIO)"
    GROUP KIOCORE
)

ecm_declare_qloggingcategory(
    CATEGORY_NAME kf.kio.core.copyjob
    OLD_CATEGORY_NAMES kf5.kio.core.copyjob
    IDENTIFIER KIO_COPYJOB_DEBUG
    DEFAULT_SEVERITY Warning
    DESCRIPTION "KIO::CopyJob (KIO)"
    GROUP KIOCORE
)

ecm_declare_qloggingcategory(
    CATEGORY_NAME kf.kio.core.dirlister
    OLD_CATEGORY_NAMES kf5.kio.core.dirlister
    IDENTIFIER KIO_CORE_DIRLISTER
    DEFAULT_SEVERITY Warning
    DESCRIPTION "KCoreDirLister (KIO)"
    GROUP KIOCORE
)

ecm_declare_qloggingcategory(
    CATEGORY_NAME kf.kio.core.sambashare
    OLD_CATEGORY_NAMES kf5.kio.core.sambashare
    IDENTIFIER KIO_CORE_SAMBASHARE
    DEFAULT_SEVERITY Warning
    DESCRIPTION "sambashare (KIO)"
    GROUP KIOCORE
)

ecm_declare_qloggingcategory(
    CATEGORY_NAME kf.kio.gui
    OLD_CATEGORY_NAMES kf5.kio.gui
    IDENTIFIER KIO_GUI
    DESCRIPTION "KIOGui (KIO)"
    GROUP KIOGUI
)

ecm_declare_qloggingcategory(
    IDENTIFIER FAVICONS_LOG
    CATEGORY_NAME kf.kio.gui.favicons
    OLD_CATEGORY_NAMES kf5.kio.favicons
    DESCRIPTION "FavIcons (KIO)"
    GROUP KIOGUI
)

ecm_declare_qloggingcategory(
    CATEGORY_NAME kf.kio.filewidgets
    OLD_CATEGORY_NAMES kf5.kio.filewidgets
    IDENTIFIER KFILEWIDGETS_LOG
    DESCRIPTION "KFileWidgets (KIO)"
    GROUP KIOFILEWIDGETS
)

ecm_declare_qloggingcategory(
    CATEGORY_NAME kf.kio.filewidgets.kfilewidget
    OLD_CATEGORY_NAMES kf5.kio.filewidgets.kfilewidget
    IDENTIFIER KIO_KFILEWIDGETS_FW
    DEFAULT_SEVERITY Info
    DESCRIPTION "KFileWidgets (KIO)"
    GROUP KIOFILEWIDGETS
)

ecm_declare_qloggingcategory(
    CATEGORY_NAME kf.kio.filewidgets.kfilefiltercombo
    OLD_CATEGORY_NAMES kf5.kio.filewidgets.kfilefiltercombo
    IDENTIFIER KIO_KFILEWIDGETS_KFILEFILTERCOMBO
    DEFAULT_SEVERITY Warning
    DESCRIPTION "KFileFilterCombo (KIO)"
    GROUP KIOFILEWIDGETS
)

ecm_declare_qloggingcategory(
    CATEGORY_NAME kf.kio.widgets
    OLD_CATEGORY_NAMES kf5.kio.widgets
    IDENTIFIER KIO_WIDGETS
    DESCRIPTION "KIOWidgets (KIO)"
    GROUP KIOWIDGETS
)

ecm_declare_qloggingcategory(
    CATEGORY_NAME kf.kio.widgets.kdirmodel
    OLD_CATEGORY_NAMES kf5.kio.kdirmodel
    IDENTIFIER category
    DESCRIPTION "KDirModel (KIO)"
    GROUP KIOWIDGETS
)

ecm_declare_qloggingcategory(
    CATEGORY_NAME kf.kio.slaves.file
    OLD_CATEGORY_NAMES kf5.kio.kio_file
    IDENTIFIER KIO_FILE
    DESCRIPTION "kiofile (KIO)"
    GROUP KIOFILE
)

ecm_declare_qloggingcategory(
    CATEGORY_NAME kf.kio.slaves.ftp
    OLD_CATEGORY_NAMES kf5.kio.kio_ftp
    IDENTIFIER KIO_FTP
    DEFAULT_SEVERITY Warning
    DESCRIPTION "kio ftp (KIO)"
    GROUP KIOFTP
)

ecm_declare_qloggingcategory(
    CATEGORY_NAME kf.kio.slaves.http
    OLD_CATEGORY_NAMES kf5.kio.kio_http
    IDENTIFIER KIO_HTTP
    DEFAULT_SEVERITY Warning
    DESCRIPTION "KIO HTTP slave (KIO)"
    GROUP KIOHTTP
)

ecm_declare_qloggingcategory(
    CATEGORY_NAME kf.kio.slaves.http.auth
    OLD_CATEGORY_NAMES kf5.kio.kio_http.auth
    IDENTIFIER KIO_HTTP_AUTH
    DESCRIPTION "kio http auth (KIO)"
    GROUP KIOHTTP
)

ecm_declare_qloggingcategory(
    CATEGORY_NAME kf.kio.slaves.http.filter
    OLD_CATEGORY_NAMES kf5.kio.kio_http.filter
    IDENTIFIER KIO_HTTP_FILTER
    DESCRIPTION "kio http filter (KIO)"
    GROUP KIOHTTP
)

ecm_declare_qloggingcategory(
    CATEGORY_NAME kf.kio.slaves.http.cookiejar
    OLD_CATEGORY_NAMES kf5.kio.cookiejar
    IDENTIFIER KIO_COOKIEJAR
    DESCRIPTION "kcookiejar (KIO)"
    GROUP KCOOKIEJAR
)

ecm_declare_qloggingcategory(
    CATEGORY_NAME kf.kio.slaves.remote
    OLD_CATEGORY_NAMES kf5.kio.kio_remote
    IDENTIFIER KIOREMOTE_LOG
    DEFAULT_SEVERITY Info
    DESCRIPTION "kio_remote (KIO)"
    GROUP KIOREMOTE
)

ecm_declare_qloggingcategory(
    CATEGORY_NAME kf.kio.slaves.trash
    OLD_CATEGORY_NAMES kf5.kio.trash
    IDENTIFIER KIO_TRASH
    DESCRIPTION "kio trash (KIO)"
    GROUP KIOTRASH
)

ecm_declare_qloggingcategory(
    IDENTIFIER KIO_USERAGENTDLG
    CATEGORY_NAME kf.configwidgets.cms.kf.kio.useragentdlg
    OLD_CATEGORY_NAMES kf5.kio.useragentdlg
    DESCRIPTION "kio useragentdialog (KIO)"
    GROUP KCM_KIO
)

ecm_declare_qloggingcategory(
    CATEGORY_NAME kf.kio.kiod
    OLD_CATEGORY_NAMES kf5.kiod
    IDENTIFIER KIOD_CATEGORY
    DESCRIPTION "KIO Daemon (KIO)"
    GROUP KIOD
)

ecm_declare_qloggingcategory(
    CATEGORY_NAME kf.kio.execd
    OLD_CATEGORY_NAMES kf5.kio.execd
    IDENTIFIER KIOEXEC
    DESCRIPTION "kioexecd (KIO)"
    GROUP KIOEXEC
)

ecm_declare_qloggingcategory(
    CATEGORY_NAME kf.kio.kpasswdserver
    OLD_CATEGORY_NAMES org.kde.kio.kpasswdserver
    IDENTIFIER category
    DESCRIPTION "KPasswdServer (KIO)"
    GROUP KPASSWDSERVER
)

ecm_declare_qloggingcategory(
    CATEGORY_NAME kf.kio.urifilters.ikws
    OLD_CATEGORY_NAMES org.kde.kurifilter-ikws
    IDENTIFIER category
    DEFAULT_SEVERITY Warning
    DESCRIPTION "KUriFilter IKWS (KIO)"
    GROUP KURIIKWSFILTER
)

ecm_declare_qloggingcategory(
    CATEGORY_NAME kf.kio.urifilters.localdomain
    OLD_CATEGORY_NAMES org.kde.kurifilter-localdomain
    IDENTIFIER category
    DEFAULT_SEVERITY Warning
    DESCRIPTION "KUriFilter Local Domain (KIO)"
    GROUP LOCALDOMAINURIFILTER
)

ecm_declare_qloggingcategory(
    CATEGORY_NAME kf.kio.urifilters.shorturi
    OLD_CATEGORY_NAMES org.kde.kurifilter-shorturi
    IDENTIFIER category
    DEFAULT_SEVERITY Warning
    DESCRIPTION "KUriFilter Shorturi (KIO)"
    GROUP KSHORTURIFILTER
)
