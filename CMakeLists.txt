cmake_minimum_required(VERSION 3.16)

project(wifiscan VERSION 0.1 LANGUAGES CXX)
set(APP_NAME wifiscanner)

set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOMOC ON)

set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(WIN32)
    set(APP_ICON_RC_FILE "${CMAKE_CURRENT_SOURCE_DIR}/wifiscan.rc")
    list(APPEND APP_SOURCES ${APP_ICON_RC_FILE})
elseif(APPLE)
    # 設置 macOS 專屬的 .icns 檔案
    set(MACOSX_BUNDLE_ICON_FILE "wifiscan.icns")
endif()

set(ICON_FILE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/images/wifiscan.ico")

# (可選) 檢查圖示檔案是否存在
if(NOT EXISTS ${ICON_FILE_PATH})
    message(FATAL_ERROR "wifi.ico not found : ${ICON_FILE_PATH}")
endif()

find_package(Qt6 REQUIRED COMPONENTS Qml Quick)
find_package(Qt6 REQUIRED COMPONENTS Core)
# find_package(Qt6 REQUIRED COMPONENTS AndroidExtras)
qt_standard_project_setup(REQUIRES 6.5)
if (Qt6Core_FOUND)
    if (Qt6Core_VERSION VERSION_LESS 6.5.0)
        message(FATAL_ERROR "Minimum supported Qt6 version is 6.5.0!")
    endif()
else()
    message(SEND_ERROR "The Qt6Core library could not be found!")
endif()

# qt_add_resources(${APP_NAME} "wifiscan.qrc")
qt_add_executable(${APP_NAME} WIN32
    src/main.cpp
    src/wifiscanner.h src/wifiscanner.cpp
    src/beacondetail.h
    src/beaconmodel.cpp src/beaconmodel.h
    src/interfacemodel.h
    src/imageprovider.h
    src/utf8utils.cpp src/utf8utils.h
    src/wifiutils.cpp src/wifiutils.h
    ${APP_SOURCES}
)


target_link_libraries(wifiscanner PRIVATE Qt6::Core)
if(ANDROID)
    set_property(TARGET ${APP_NAME} PROPERTY QT_ANDROID_PACKAGE_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/android)
    set(ANDROID_JAVA_SOURCES
        android/src/tw/idv/coolshou/WifiService.java #  Use the new path!
    )
    target_sources(${APP_NAME} PRIVATE
        src/qmlbeacondetail.h src/qmlbeacondetail.cpp
    )
    # qt_add_extra_resources_android(
    #     ${PROJECT_NAME}
    #     JAVA_SOURCES ${ANDROID_JAVA_SOURCES}
    # )
endif()

qt_add_qml_module(${APP_NAME}
    URI ${APP_NAME}
    VERSION 1.0
    QML_FILES src/Main.qml
    QML_FILES src/Detail.qml
    RESOURCES wifiscan.qrc
    SOURCES src/beaconfilterproxymodel.h src/beaconfilterproxymodel.cpp
)


if (UNIX AND NOT ANDROID)
    # --- Find libnl and libnl-genl ---
    # 1. Find the PkgConfig module
    find_package(PkgConfig REQUIRED)

    # 2. Use PkgConfig to find libnl-3.0 and libnl-genl-3.0
    # The variables PKG_NL_CINCLUDE_DIRS and PKG_NL_LIBRARIES will be populated
    pkg_check_modules(NL REQUIRED libnl-3.0 libnl-genl-3.0)
    # Link Netlink components
    target_include_directories(${APP_NAME} PRIVATE ${NL_INCLUDE_DIRS})
    target_link_libraries(${APP_NAME} PRIVATE ${NL_LIBRARIES})

    target_compile_definitions(${APP_NAME} PRIVATE IS_DESKTOP_LINUX)
    # install icon
    install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/images/wifiscan.png"
                DESTINATION "/usr/share/icons/hicolor/256x256/apps"
                RENAME "${PROJECT_NAME}.png"
        )
    # install .desktop
    configure_file(
        "${CMAKE_CURRENT_SOURCE_DIR}/wifiscan.desktop.in"
        "${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}.desktop"
        @ONLY
    )

    # 將生成的 .desktop 檔案安裝到標準應用程式路徑
    install(FILES "${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}.desktop"
        DESTINATION "share/applications"
    )

endif()
if (ANDROID)
    target_compile_definitions(${APP_NAME} PRIVATE IS_ANDROID)
endif()

# Qt for iOS sets MACOSX_BUNDLE_GUI_IDENTIFIER automatically since Qt 6.1.
# If you are developing for iOS or macOS you should consider setting an
# explicit, fixed bundle identifier manually though.
set_target_properties(${APP_NAME} PROPERTIES
#    MACOSX_BUNDLE_GUI_IDENTIFIER com.example.appwifiscan
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
    MACOSX_BUNDLE TRUE
    WIN32_EXECUTABLE TRUE
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

target_link_libraries(${APP_NAME}
    PRIVATE
      Qt6::Core
      Qt6::Qml
      Qt6::Quick
)
# target_link_libraries(${APP_NAME}
#     PRIVATE
#       Qt6::AndroidExtras
#)

include(GNUInstallDirs)
install(TARGETS ${APP_NAME}
    BUNDLE DESTINATION .
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
install(DIRECTORY images/ DESTINATION ${CMAKE_INSTALL_PREFIX}/images)
# Optional: Set C++ standard
#target_compile_features(${PROJECT_NAME} PRIVATE cxx_17)
