# Unit tests.
#
# Drop a `test_<something>.cpp` file in this directory and it is picked up
# automatically: each one is compiled into its own executable, linked against
# m3sp_core, and registered with CTest. A test passes when it returns 0.
#
#     ctest --test-dir build --output-on-failure
#
# No test framework is wired in, so the project stays buildable with no network
# access. To use GoogleTest or Catch2 instead, fetch it here and add it to
# target_link_libraries() in m3sp_add_test() below.

function(m3sp_add_test source)
    get_filename_component(name "${source}" NAME_WE)
    add_executable(${name} "${source}")
    target_link_libraries(${name} PRIVATE m3sp_core)
    target_include_directories(${name} PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
    m3sp_set_warnings(${name})
    add_test(NAME ${name} COMMAND ${name})
endfunction()

file(GLOB M3SP_TEST_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/test_*.cpp")

foreach(test_source ${M3SP_TEST_SOURCES})
    m3sp_add_test("${test_source}")
endforeach()

if(NOT M3SP_TEST_SOURCES)
    message(STATUS "tests/: no test_*.cpp files yet")
endif()
