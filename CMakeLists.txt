cmake_minimum_required(VERSION 3.16)
project(wsalauncher VERSION 0.3.8 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets Network)

set(WSALAUNCHER_SOURCES
    src/main.cpp
    src/MainWindow.cpp
    src/Installer.cpp
    src/GameLauncher.cpp
    src/Paths.cpp
    src/Platform.cpp
    src/Modrinth.cpp
    src/Download.cpp
    src/Releases.cpp
    src/WsamiawApi.cpp
    src/Hwid.cpp
    src/Bootstrap.cpp
    src/LauncherUpdate.cpp
    src/MainWindow.h
    src/Installer.h
    src/GameLauncher.h
    src/Paths.h
    src/Platform.h
    src/Modrinth.h
    src/Download.h
    src/Releases.h
    src/WsamiawApi.h
    src/Hwid.h
    src/Bootstrap.h
    src/LauncherUpdate.h
    src/Constants.h
)

if(APPLE)
    add_executable(wsalauncher MACOSX_BUNDLE ${WSALAUNCHER_SOURCES})
    set_target_properties(wsalauncher PROPERTIES
        MACOSX_BUNDLE_BUNDLE_NAME "Wsalauncher"
        MACOSX_BUNDLE_GUI_IDENTIFIER "wsamiaw.wsalauncher"
        MACOSX_BUNDLE_BUNDLE_VERSION "${PROJECT_VERSION}"
        MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}"
    )
elseif(WIN32)
    add_executable(wsalauncher WIN32 ${WSALAUNCHER_SOURCES})
else()
    add_executable(wsalauncher ${WSALAUNCHER_SOURCES})
endif()

target_link_libraries(wsalauncher PRIVATE Qt6::Widgets Qt6::Network)

target_compile_definitions(wsalauncher PRIVATE WSALAUNCHER_VERSION="${PROJECT_VERSION}")

# Windows: after building, run package-windows.bat to ship a portable folder
# (exe + Qt DLLs via windeployqt — end users do not install Qt).
if(WIN32)
    find_program(WINDEPLOYQT_EXECUTABLE windeployqt HINTS
        "${Qt6_DIR}/../../../bin"
        "$ENV{QTDIR}/bin"
    )
    if(WINDEPLOYQT_EXECUTABLE)
        add_custom_target(portable-windows
            COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_SOURCE_DIR}/dist/Wsalauncher-Windows"
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "$<TARGET_FILE:wsalauncher>"
                "${CMAKE_SOURCE_DIR}/dist/Wsalauncher-Windows/wsalauncher.exe"
            COMMAND "${WINDEPLOYQT_EXECUTABLE}" --release --compiler-runtime --network
                "${CMAKE_SOURCE_DIR}/dist/Wsalauncher-Windows/wsalauncher.exe"
            DEPENDS wsalauncher
            COMMENT "Creating portable Windows package in dist/Wsalauncher-Windows"
        )
    endif()
endif()

if(APPLE)
    install(TARGETS wsalauncher BUNDLE DESTINATION .)
else()
    install(TARGETS wsalauncher RUNTIME DESTINATION bin)
endif()
