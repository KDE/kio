include(ECMMarkAsTest)

find_package(KF6XmlGui ${KF_DEP_VERSION} REQUIRED)

remove_definitions(-DQT_NO_CAST_FROM_ASCII)
remove_definitions(-DQT_NO_CAST_FROM_BYTEARRAY)

if (ENABLE_PCH)
    set_directory_properties(PROPERTIES PRECOMPILE_HEADERS_REUSE_FROM KF6KIOCore)
endif()

macro(KIOWIDGETS_EXECUTABLE_TESTS)
  foreach(_testname ${ARGN})
    add_executable(${_testname} ${_testname}.cpp)
    target_link_libraries(${_testname}
       KF6::KIOCore
       KF6::KIOGui
       KF6::KIOWidgets
       KF6::KIOFileWidgets
       KF6::XmlGui
       Qt6::Test
       KF6::WidgetsAddons
       KF6::IconThemes)
    if (ENABLE_PCH)
      target_precompile_headers(${_testname} REUSE_FROM KF6KIOWidgets)
    endif()
    ecm_mark_as_test(${_testname})
  endforeach()
endmacro(KIOWIDGETS_EXECUTABLE_TESTS)

if (NOT ANDROID)
KIOWIDGETS_EXECUTABLE_TESTS(
    getalltest
    kdirlistertest_gui
    kdirmodeltest_gui
    kemailclientlauncherjobtest_gui
    kencodingfiledialogtest_gui
    kfilecustomdialogtest_gui
    kfilecustomdialogtest_gui_select_dir_mode
    kfilewidgettest_gui
    kfilewidgettest_saving_gui
    kionetrctest
    kioworkertest
    kmountpoint_debug
    kopenwithtest
    kpropertiesdialogtest
    kprotocolinfo_dumper
    kruntest
    ksycocaupdatetest
    kterminallauncherjobtest_gui
    kurlnavigatortest_gui
    kurlrequestertest_gui
    listjobtest
    openfilemanagerwindowtest
    previewtest
    ${runapplication_EXE}
)

add_subdirectory(messageboxworker)
endif()
