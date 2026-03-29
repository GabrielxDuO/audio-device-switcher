cmake_minimum_required(VERSION 3.20)

project(AudioDeviceSwitcher
    VERSION 2.3.0
    DESCRIPTION "Windows Audio Device Switcher"
    LANGUAGES CXX RC
)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Source files
set(SOURCES
    src/main.cpp
    src/audio.cpp
    src/tray.cpp
    src/startup.cpp
)

# Generate resource.rc from template, injecting the project version automatically.
# The output goes to the build directory; icon/manifest paths in the template are absolute.
configure_file(res/resource.rc.in ${CMAKE_CURRENT_BINARY_DIR}/resource.rc @ONLY)

set(RESOURCES
    ${CMAKE_CURRENT_BINARY_DIR}/resource.rc
)

# WIN32 = /SUBSYSTEM:WINDOWS (no console window)
add_executable(AudioDeviceSwitcher WIN32 ${SOURCES} ${RESOURCES})

target_include_directories(AudioDeviceSwitcher PRIVATE src res)

target_link_libraries(AudioDeviceSwitcher PRIVATE
    Ole32
    Propsys
    Shell32
    User32
    Advapi32
    Comctl32
)

if(MSVC)
    # Use static CRT (/MT) instead of CMake default /MD
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded")

    # Replace CMake's default /O2 with /O1 (optimize for size)
    foreach(flag_var CMAKE_CXX_FLAGS_RELEASE CMAKE_C_FLAGS_RELEASE)
        string(REPLACE "/O2" "/O1" ${flag_var} "${${flag_var}}")
    endforeach()

    target_compile_options(AudioDeviceSwitcher PRIVATE
        /W3
        /utf-8
        /GS-
        /Gy
    )
    target_link_options(AudioDeviceSwitcher PRIVATE
        /SUBSYSTEM:WINDOWS
        /OPT:REF
        /OPT:ICF
        /MANIFEST:NO  # manifest is embedded via resource.rc (RT_MANIFEST), avoid duplicate
    )
endif()

# Output the exe directly to the project root under build/Release/
set_target_properties(AudioDeviceSwitcher PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/build"
)
