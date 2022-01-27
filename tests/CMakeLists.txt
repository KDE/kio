include(ECMMarkAsTest)

find_package(KF5XmlGui ${KF_DEP_VERSION} REQUIRED)

remove_definitions(-DQT_NO_CAST_FROM_ASCII)
remove_definitions(-DQT_NO_CAST_FROM_BYTEARRAY)

macro(KIOWIDGETS_EXECUTABLE_TESTS)
  foreach(_testname ${ARGN})
    add_executable(${_testname} ${_testname}.cpp)
    target_link_libraries(${_testname}
       KF5::KIOCore
       KF5::KIOGui
       KF5::KIOWidgets
       KF5::KIOFileWidgets
       Qt${QT_MAJOR_VERSION}::Test
       KF5::WidgetsAddons
       KF5::IconThemes)
    ecm_mark_as_test(${_testname})
  endforeach()
endmacro(KIOWIDGETS_EXECUTABLE_TESTS)

if(NOT EXCLUDE_DEPRECATED_BEFORE_AND_AT STREQUAL "CURRENT" AND
   EXCLUDE_DEPRECATED_BEFORE_AND_AT VERSION_LESS 5.71.0)
    set(runapplication_EXE runapplication)
endif()

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
    kioslavetest
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

add_executable(kauth_tester kauth_tester.cpp)
target_link_libraries(kauth_tester
    KF5::KIOCore
    KF5::KIOGui
    KF5::KIOWidgets
    KF5::KIOFileWidgets
    KF5::WidgetsAddons
    KF5::IconThemes
)

endif()
