include(ECMMarkAsTest)

remove_definitions(-DQT_NO_CAST_FROM_ASCII)
remove_definitions(-DQT_NO_CAST_FROM_BYTEARRAY)

macro(KIOWIDGETS_EXECUTABLE_TESTS)
  foreach(_testname ${ARGN})
    add_executable(${_testname} ${_testname}.cpp)
    target_link_libraries(${_testname}
       KF6::KIOCore
       KF6::KIOGui
       KF6::KIOWidgets
       KF6::KIOFileWidgets
       Qt6::Test
       KF6::WidgetsAddons
       KF6::IconThemes)
    ecm_mark_as_test(${_testname})
  endforeach()
endmacro(KIOWIDGETS_EXECUTABLE_TESTS)

if (HAVE_QTDBUS)
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
