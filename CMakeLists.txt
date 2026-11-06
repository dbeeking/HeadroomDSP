cmake_minimum_required(VERSION 3.20)
project(HeadroomDSP VERSION 0.1.0 LANGUAGES CXX)

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)
option(HEADROOMDSP_BUILD_TESTS "Build unit tests" ON)
option(HEADROOMDSP_BUILD_EXAMPLES "Build example" ON)
option(HEADROOMDSP_BUILD_PYTHON "Build optional pybind11 module" OFF)

add_library(headroomdsp_core STATIC src/analysis.cpp)
add_library(HeadroomDSP::core ALIAS headroomdsp_core)
target_compile_features(headroomdsp_core PUBLIC cxx_std_17)
target_include_directories(headroomdsp_core PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)
set_target_properties(headroomdsp_core PROPERTIES POSITION_INDEPENDENT_CODE ON EXPORT_NAME core)

add_library(headroomdsp_c STATIC src/c_api.cpp)
add_library(HeadroomDSP::c ALIAS headroomdsp_c)
target_compile_features(headroomdsp_c PUBLIC cxx_std_17)
target_include_directories(headroomdsp_c PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)
target_link_libraries(headroomdsp_c PUBLIC headroomdsp_core)
set_target_properties(headroomdsp_c PROPERTIES POSITION_INDEPENDENT_CODE ON EXPORT_NAME c)

if(HEADROOMDSP_BUILD_PYTHON)
    find_package(pybind11 CONFIG REQUIRED)
    pybind11_add_module(headroom_dsp python/bindings.cpp)
    target_link_libraries(headroom_dsp PRIVATE headroomdsp_core)
    install(TARGETS headroom_dsp LIBRARY DESTINATION .)
endif()

if(HEADROOMDSP_BUILD_EXAMPLES)
    add_executable(headroomdsp_example examples/analyze_synthetic.cpp)
    target_link_libraries(headroomdsp_example PRIVATE HeadroomDSP::core)
endif()

if(HEADROOMDSP_BUILD_TESTS)
    enable_testing()
    add_executable(headroomdsp_tests tests/test_analysis.cpp)
    target_link_libraries(headroomdsp_tests PRIVATE HeadroomDSP::c)
    add_test(NAME headroomdsp_unit COMMAND headroomdsp_tests)
endif()

install(TARGETS headroomdsp_core headroomdsp_c
    EXPORT HeadroomDSPTargets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
install(EXPORT HeadroomDSPTargets FILE HeadroomDSPTargets.cmake
    NAMESPACE HeadroomDSP:: DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/HeadroomDSP)
configure_package_config_file(cmake/HeadroomDSPConfig.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/HeadroomDSPConfig.cmake
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/HeadroomDSP)
write_basic_package_version_file(
    ${CMAKE_CURRENT_BINARY_DIR}/HeadroomDSPConfigVersion.cmake
    VERSION ${PROJECT_VERSION} COMPATIBILITY SameMajorVersion)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/HeadroomDSPConfig.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/HeadroomDSPConfigVersion.cmake
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/HeadroomDSP)
