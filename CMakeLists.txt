cmake_minimum_required(VERSION 3.16) 
project(CastIt LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Tell CMake where to find Qt6
set(CMAKE_PREFIX_PATH "D:/.CODING/QtFramework/6.9.1/msvc2022_64/lib/cmake")

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

# Optional but recommended for Qt projects
include(GNUInstallDirs)
find_package(Qt6 COMPONENTS Widgets Network WebSockets HttpServer REQUIRED)

qt_standard_project_setup()

qt_add_executable(CastIt 
	src/main.cpp
	src/ui/main_window.cpp
	src/ui/main_window.h
	src/ui/main_window.ui
	src/core/device_discovery.cpp
	src/core/device_discovery.h
	src/core/cast_controller.cpp
	src/core/cast_controller.h
	src/core/dlna_controller.cpp
	src/core/dlna_controller.h
	src/core/dlna_discovery.cpp
	src/core/dlna_discovery.h
)

set(QT_BIN_DIR "D:/.CODING/QtFramework/6.9.1/msvc2022_64/bin")

add_custom_command(TARGET CastIt POST_BUILD
    COMMAND "${QT_BIN_DIR}/windeployqt.exe"
        $<$<CONFIG:Debug>:--debug>
        $<$<CONFIG:Release>:--release>
        --no-translations
        "$<TARGET_FILE:CastIt>"
    COMMENT "Deploying Qt dependencies..."
)

# Link againts Qt
target_link_libraries(CastIt PRIVATE 
	Qt6::Widgets
	Qt6::Network
	Qt6::WebSockets
	Qt6::HttpServer
)

# Add your include directories (both source and generated)
target_include_directories(CastIt PRIVATE
	src 
	${CMAKE_CURRENT_BINARY_DIR}
)
