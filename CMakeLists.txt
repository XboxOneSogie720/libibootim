cmake_minimum_required(VERSION 3.15)
project(libibootim VERSION 1.1 LANGUAGES C)
add_compile_options(-Wall -Wextra -Wpedantic)

set(CMAKE_C_STANDARD 11)

include(GNUInstallDirs)

find_package(PNG REQUIRED)

# Library
add_library(ibootim src/libibootim.c src/lzss.c)

target_include_directories(ibootim
    PUBLIC  ${CMAKE_SOURCE_DIR}/include
    PRIVATE ${PNG_INCLUDE_DIRS}
)

target_link_libraries(ibootim
    PRIVATE ${PNG_LIBRARIES}
)

target_compile_definitions(ibootim PRIVATE
    LIBIBOOTIM_VERSION="${PROJECT_VERSION}"
)

# CLI tool
add_executable(ibootimutil tools/ibootimutil.c)

target_link_libraries(ibootimutil
    PRIVATE ibootim
)

# Install
install(TARGETS ibootim
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

install(FILES include/libibootim.h
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

install(TARGETS ibootimutil
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

# pkg-config
configure_file(
    ${CMAKE_SOURCE_DIR}/libibootim.pc.in
    ${CMAKE_BINARY_DIR}/libibootim.pc
    @ONLY
)

install(FILES ${CMAKE_BINARY_DIR}/libibootim.pc
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/pkgconfig
)

# Uninstall
configure_file(
    ${CMAKE_SOURCE_DIR}/cmake/cmake_uninstall.cmake.in
    ${CMAKE_BINARY_DIR}/cmake_uninstall.cmake
    IMMEDIATE @ONLY
)

add_custom_target(uninstall
    COMMAND ${CMAKE_COMMAND} -P ${CMAKE_BINARY_DIR}/cmake_uninstall.cmake
)