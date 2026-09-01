cmake_minimum_required(VERSION 3.16)

project(
    UniSysInfo
    VERSION 0.1.0
    DESCRIPTION "Cross-platform system information library"
    LANGUAGES C
)

include(GNUInstallDirs)
include(GenerateExportHeader)

option(UNI_SYSINFO_BUILD_CLI "Build the uni-sysinfo command-line application" ON)

set(UNI_SYSINFO_SOURCES
    src/uni_sysinfo_cpu.c
    src/uni_sysinfo_cpu_aarch64.c
    src/uni_sysinfo_cpu_x86.c
)

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    list(APPEND UNI_SYSINFO_SOURCES src/uni_sysinfo_cpu_linux.c)
elseif(WIN32)
    list(APPEND UNI_SYSINFO_SOURCES src/uni_sysinfo_cpu_windows.c)
elseif(APPLE)
    list(APPEND UNI_SYSINFO_SOURCES src/uni_sysinfo_cpu_darwin.c)
else()
    list(APPEND UNI_SYSINFO_SOURCES src/uni_sysinfo_cpu_unsupported.c)
endif()

add_library(uni_sysinfo ${UNI_SYSINFO_SOURCES})
add_library(UniSysInfo::uni_sysinfo ALIAS uni_sysinfo)

generate_export_header(
    uni_sysinfo
    EXPORT_FILE_NAME ${CMAKE_CURRENT_BINARY_DIR}/include/uni_sysinfo_export.h
)

target_compile_features(uni_sysinfo PUBLIC c_std_11)
target_include_directories(
    uni_sysinfo
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/include>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

if(WIN32)
    target_compile_definitions(uni_sysinfo PRIVATE _WIN32_WINNT=0x0601)
    target_link_libraries(uni_sysinfo PRIVATE advapi32)
endif()

if(MSVC)
    target_compile_options(uni_sysinfo PRIVATE /W4)
else()
    target_compile_options(uni_sysinfo PRIVATE -Wall -Wextra -Wpedantic)
endif()

set_target_properties(
    uni_sysinfo
    PROPERTIES
        C_STANDARD 11
        C_STANDARD_REQUIRED YES
        C_EXTENSIONS NO
        C_VISIBILITY_PRESET hidden
        VERSION ${PROJECT_VERSION}
        SOVERSION ${PROJECT_VERSION_MAJOR}
)

if(UNI_SYSINFO_BUILD_CLI)
    add_executable(uni_sysinfo_cli src_cli/uni_sysinfo_cli.c)
    target_compile_features(uni_sysinfo_cli PRIVATE c_std_11)
    target_compile_definitions(uni_sysinfo_cli PRIVATE UNI_SYSINFO_VERSION="${PROJECT_VERSION}")
    target_link_libraries(uni_sysinfo_cli PRIVATE UniSysInfo::uni_sysinfo)
    set_target_properties(
        uni_sysinfo_cli
        PROPERTIES
            C_STANDARD 11
            C_STANDARD_REQUIRED YES
            C_EXTENSIONS NO
            OUTPUT_NAME uni-sysinfo
    )
    if(MSVC)
        target_compile_options(uni_sysinfo_cli PRIVATE /W4)
    else()
        target_compile_options(uni_sysinfo_cli PRIVATE -Wall -Wextra -Wpedantic)
    endif()
endif()

install(
    TARGETS uni_sysinfo
    EXPORT UniSysInfoTargets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)
if(UNI_SYSINFO_BUILD_CLI)
    install(TARGETS uni_sysinfo_cli RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
endif()

include(CMakePackageConfigHelpers)
configure_package_config_file(
    cmake/UniSysInfoConfig.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/UniSysInfoConfig.cmake
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/UniSysInfo
)
write_basic_package_version_file(
    ${CMAKE_CURRENT_BINARY_DIR}/UniSysInfoConfigVersion.cmake
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)
install(
    FILES
        ${CMAKE_CURRENT_BINARY_DIR}/UniSysInfoConfig.cmake
        ${CMAKE_CURRENT_BINARY_DIR}/UniSysInfoConfigVersion.cmake
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/UniSysInfo
)
install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
install(
    FILES ${CMAKE_CURRENT_BINARY_DIR}/include/uni_sysinfo_export.h
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)
install(
    EXPORT UniSysInfoTargets
    FILE UniSysInfoTargets.cmake
    NAMESPACE UniSysInfo::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/UniSysInfo
)

include(CTest)
if(BUILD_TESTING AND
   (CMAKE_SYSTEM_NAME STREQUAL "Linux" OR WIN32 OR APPLE))
    add_executable(uni_sysinfo_api_test tests/test_api.c)
    target_compile_features(uni_sysinfo_api_test PRIVATE c_std_11)
    target_link_libraries(uni_sysinfo_api_test PRIVATE UniSysInfo::uni_sysinfo)
    add_test(NAME uni_sysinfo.api COMMAND uni_sysinfo_api_test)
endif()
if(BUILD_TESTING AND UNI_SYSINFO_BUILD_CLI AND
   (CMAKE_SYSTEM_NAME STREQUAL "Linux" OR WIN32 OR APPLE))
    add_test(NAME uni_sysinfo.cli COMMAND uni_sysinfo_cli)
endif()
