add_subdirectory(cookies)
add_subdirectory(webshortcuts)
add_subdirectory(useragent)

if ( Q_WS_X11 )
    add_subdirectory(cache)
    add_subdirectory(netpref)
    add_subdirectory(proxy)
endif ()

if ( NOT Q_WS_WIN )
    add_subdirectory(smb)
endif ()

