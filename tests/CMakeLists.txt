set(VLOGSIM_TESTS
    test_lexer
    test_parser
    test_eval_adder
    test_eval_mux
)

foreach(test_name ${VLOGSIM_TESTS})
    add_executable(${test_name} ${test_name}.cpp)
    target_link_libraries(${test_name} PRIVATE vlogsim_core)
    target_include_directories(${test_name} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    add_test(NAME ${test_name} COMMAND ${test_name})
endforeach()
