find_package(GTest CONFIG REQUIRED)

enable_testing()

add_executable(test_js_escape
    test_js_escape.cpp
)

target_include_directories(test_js_escape PRIVATE
    ${CMAKE_SOURCE_DIR}
)

target_link_libraries(test_js_escape
    GTest::gtest
    GTest::gtest_main
)

set_target_properties(test_js_escape PROPERTIES
    CXX_STANDARD 17
)

add_test(NAME JsEscapeTests COMMAND test_js_escape)
