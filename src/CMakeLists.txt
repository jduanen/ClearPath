cmake_minimum_required(VERSION 3.16)
project(cpview LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)  # build/compile_commands.json, for clangd

find_package(Qt6 REQUIRED COMPONENTS Widgets Charts)

# Root of the Teknic sFoundation package (contains inc/ and sFoundation/)
set(SFND_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../Linux_Software/sFoundation"
    CACHE PATH "Path to the sFoundation package")
set(SFND_LIB_DIR "${SFND_ROOT}/sFoundation")

find_library(SFND_LIB sFoundation20 PATHS "${SFND_LIB_DIR}" /usr/local/lib REQUIRED)
get_filename_component(SFND_LIB_REALDIR "${SFND_LIB}" DIRECTORY)
find_package(Threads REQUIRED)

# The library's soname is libsFoundation20.so.1; the SDK build only produces
# libsFoundation20.so. Create the soname link beside it (what `ldconfig -n`
# would do) so the loader finds it, and so the library finds
# MNuserDriver20.xml in its own directory.
if(NOT EXISTS "${SFND_LIB_REALDIR}/libsFoundation20.so.1")
    message(STATUS "Creating ${SFND_LIB_REALDIR}/libsFoundation20.so.1")
    file(CREATE_LINK libsFoundation20.so "${SFND_LIB_REALDIR}/libsFoundation20.so.1" SYMBOLIC)
endif()

add_executable(cpview
    main.cpp
    MainWindow.cpp
    MotorWorker.cpp
    tabs/InfoStatusTab.cpp
    tabs/MotionTab.cpp
    tabs/ConfigTab.cpp
    tabs/HomingDialog.cpp
    tabs/PlotTab.cpp
)

target_include_directories(cpview PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}"
    "${SFND_ROOT}/inc/inc-pub")
target_link_libraries(cpview PRIVATE Qt6::Widgets Qt6::Charts "${SFND_LIB}" Threads::Threads)
set_target_properties(cpview PROPERTIES BUILD_RPATH "${SFND_LIB_REALDIR}")
