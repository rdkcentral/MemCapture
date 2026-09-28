find_package(GTest CONFIG REQUIRED)

enable_testing()

add_executable(test_cgroup_parsing
    test_cgroup_parsing.cpp
)

target_include_directories(test_cgroup_parsing PRIVATE
    ${CMAKE_SOURCE_DIR}
)

target_link_libraries(test_cgroup_parsing
    GTest::gtest
    GTest::gtest_main
)

set_target_properties(test_cgroup_parsing PROPERTIES
    CXX_STANDARD 17
)

add_test(NAME CgroupParsingTests COMMAND test_cgroup_parsing)
