cmake_minimum_required(VERSION 3.16)

project(twentytwentytwentyreminder)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

include(GNUInstallDirs)

find_package(Qt6 COMPONENTS Core Gui Widgets REQUIRED)

add_executable(twentytwentytwentyreminder main.cpp)

target_link_libraries(twentytwentytwentyreminder PRIVATE Qt6::Core Qt6::Gui Qt6::Widgets)

install(TARGETS twentytwentytwentyreminder
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)


set(DESKTOP_FILE_NAME "twentytwentytwentyreminder.desktop")
set(DESKTOP_FILE_PATH "${CMAKE_CURRENT_BINARY_DIR}/${DESKTOP_FILE_NAME}")


set(DESKTOP_FILE_CONTENT 
"[Desktop Entry]
Type=Application
Name=20-20-20 Reminder
GenericName=Eye Strain Reminder
Comment=Reminds you to look away every 20 minutes
Exec=twentytwentytwentyreminder
Icon=alarm-symbolic
Terminal=false
Categories=Utility;Qt;
Keywords=timer;health;reminder;
StartupNotify=true
")

file(WRITE ${DESKTOP_FILE_PATH} ${DESKTOP_FILE_CONTENT})

install(FILES ${DESKTOP_FILE_PATH}
    DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/applications
)

# install(FILES app_icon.png 
#     DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/icons/hicolor/128x128/apps/
#     RENAME twentytwentytwentyreminder.png
# )