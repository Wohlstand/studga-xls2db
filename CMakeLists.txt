cmake_minimum_required(VERSION 2.8)
set(CMAKE_CXX_STANDARD 14)

project(studga-xls2db CXX)

include(DirManager/dirman.cmake)

include_directories(
    "/usr/local/libxls/include"
    "${CMAKE_BINARY_DIR}/include"
)
link_directories(
    "/usr/local/libxls/lib"
    "${CMAKE_BINARY_DIR}/lib"
)

include(ExternalProject)
ExternalProject_Add(xlnt_external
    GIT_REPOSITORY    "https://github.com/tfussell/xlnt.git"
    GIT_TAG           "master"
    SOURCE_DIR        "${CMAKE_BINARY_DIR}/xlnt-src"
    BINARY_DIR        "${CMAKE_BINARY_DIR}/xlnt-build"
    CMAKE_ARGS
        "-DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}"
        "-DCMAKE_INSTALL_PREFIX=${CMAKE_BINARY_DIR}"
        "-DSTATIC=ON"
)

add_executable(${PROJECT_NAME}
    studga-xls2db.cpp
    schedule_file.cpp
    strings.cpp
    data_base.cpp
    schedule_manager.cpp
    files.cpp
    ${DIRMANAGER_SRCS}
)
add_dependencies(${PROJECT_NAME} xlnt_external)
target_link_libraries(${PROJECT_NAME}
    xlsreader
    xlnt$<$<CONFIG:Debug>:d>
    mysqlclient
    ssl
    crypto
)
