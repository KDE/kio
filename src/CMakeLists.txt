add_subdirectory(core)

# KIOCore-only executables
add_subdirectory(kntlm)
add_subdirectory(ioslaves)
add_subdirectory(kiod)
add_subdirectory(kssld)
add_subdirectory(kioslave)

if (NOT KIOCORE_ONLY)
add_subdirectory(kpasswdserver)
add_subdirectory(widgets)
add_subdirectory(filewidgets)
add_subdirectory(kioexec)
add_subdirectory(urifilters)
add_subdirectory(kcms)
add_subdirectory(new_file_templates)

if(NOT WIN32)
  # arpa/nameser.h
  add_subdirectory(kpac)
endif()
endif()
